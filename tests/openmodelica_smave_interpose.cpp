#include "smave/linear.hpp"

extern "C" {
#include <simulation_data.h>
#include <simulation/solver/nonlinearSystem.h>
}

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <vector>

namespace {

using OriginalSolve = int (*)(DATA*, threadData_t*, int);

OriginalSolve original_solver() {
    static auto function = reinterpret_cast<OriginalSolve>(
        dlsym(RTLD_NEXT, "solve_nonlinear_system"));
    return function;
}

bool enabled() {
    const char* value = std::getenv("SMAVE_OMC_NLS");
    return value && std::strcmp(value, "1") == 0;
}

std::size_t attempts{};
std::size_t accepted{};
std::size_t fallbacks{};
std::size_t iterations{};
std::size_t calls{};
std::size_t initial_calls{};
std::size_t mixed_calls{};
std::size_t tearing_calls{};

struct ReportWriter {
    ~ReportWriter() {
        const char* path = std::getenv("SMAVE_OMC_REPORT");
        if (!path) return;
        std::ofstream output(path);
        output << "SMAVE_OPENMODELICA_NLS 1\n"
               << "CALLS " << calls << "\n"
               << "INITIAL_CALLS " << initial_calls << "\n"
               << "MIXED_CALLS " << mixed_calls << "\n"
               << "TEARING_CALLS " << tearing_calls << "\n"
               << "ATTEMPTS " << attempts << "\n"
               << "ACCEPTED " << accepted << "\n"
               << "FALLBACKS " << fallbacks << "\n"
               << "LINEAR_ITERATIONS " << iterations << "\nEND\n";
    }
} report_writer;

double infinity_norm(const std::vector<double>& values) {
    double norm{};
    for (double value : values) norm = std::max(norm, std::abs(value));
    return norm;
}

}  // namespace

extern "C" int solve_nonlinear_system(
    DATA* data, threadData_t* thread_data, int system_number) {
    auto fallback = original_solver();
    if (!fallback) return 0;
    NONLINEAR_SYSTEM_DATA* system =
        &data->simulationInfo->nonlinearSystemData[system_number];
    ++calls;
    initial_calls += data->simulationInfo->initial ? 1 : 0;
    mixed_calls += system->mixedSystem ? 1 : 0;
    tearing_calls += system->strictTearingFunctionCall ? 1 : 0;
    if (!enabled() || data->simulationInfo->initial ||
        system->mixedSystem || system->strictTearingFunctionCall ||
        !system->residualFunc || system->size <= 0 || system->size > 512) {
        return fallback(data, thread_data, system_number);
    }
    ++attempts;

    const std::size_t size = static_cast<std::size_t>(system->size);
    std::vector<double> state(size);
    system->getIterationVars(data, state.data());
    std::vector<double> residual(size);
    RESIDUAL_USERDATA user_data{data, thread_data, nullptr};
    int flag = static_cast<int>(size);
    auto evaluate = [&](const std::vector<double>& input,
                        std::vector<double>& output) {
        output.assign(size, 0.0);
        system->residualFunc(&user_data, input.data(), output.data(), &flag);
        system->numberOfFEval++;
        return std::all_of(output.begin(), output.end(),
            [](double value) { return std::isfinite(value); });
    };

    if (!evaluate(state, residual)) return fallback(data, thread_data, system_number);
    std::size_t total_iterations{};
    bool converged = false;
    for (int newton = 0; newton < 16; ++newton) {
        const double state_norm = infinity_norm(state);
        const double residual_norm = infinity_norm(residual);
        if (residual_norm <= 1.0e-9 * std::max(1.0, state_norm)) {
            converged = true;
            break;
        }
        const auto base_state = state;
        const auto base_residual = residual;
        const smave::LinearOperator jacobian =
            [&](const std::vector<double>& direction,
                std::vector<double>& output) {
                const double direction_norm = infinity_norm(direction);
                if (direction_norm == 0.0) {
                    output.assign(size, 0.0);
                    return true;
                }
                const double epsilon = 1.0e-7 *
                    (1.0 + infinity_norm(base_state)) / direction_norm;
                std::vector<double> perturbed = base_state;
                for (std::size_t index = 0; index < size; ++index)
                    perturbed[index] += epsilon * direction[index];
                std::vector<double> perturbed_residual;
                if (!evaluate(perturbed, perturbed_residual)) return false;
                output.resize(size);
                for (std::size_t index = 0; index < size; ++index)
                    output[index] = (perturbed_residual[index] -
                        base_residual[index]) / epsilon;
                return true;
            };
        std::vector<double> right_hand_side(size);
        for (std::size_t index = 0; index < size; ++index)
            right_hand_side[index] = -residual[index];
        const auto correction = smave::restarted_gmres(
            size, jacobian, right_hand_side, std::vector<double>(size),
            [](const std::vector<double>& input, std::vector<double>& output) {
                output = input;
                return true;
            }, 1.0e-11, 1.0e-9,
            std::max<int>(80, static_cast<int>(size) * 3),
            std::min<int>(40, static_cast<int>(size)));
        if (!correction.converged) break;
        total_iterations += static_cast<std::size_t>(correction.iterations);
        double scale = 1.0;
        bool accepted = false;
        while (scale >= 1.0 / 128.0) {
            std::vector<double> trial = base_state;
            for (std::size_t index = 0; index < size; ++index)
                trial[index] += scale * correction.solution[index];
            std::vector<double> trial_residual;
            if (evaluate(trial, trial_residual) &&
                infinity_norm(trial_residual) < residual_norm) {
                state = std::move(trial);
                residual = std::move(trial_residual);
                accepted = true;
                break;
            }
            scale *= 0.5;
        }
        if (!accepted) break;
    }
    if (!converged) {
        system->numberOfFailures++;
        ++fallbacks;
        return fallback(data, thread_data, system_number);
    }
    evaluate(state, residual);
    std::copy(state.begin(), state.end(), system->nlsx);
    system->solved = NLS_SOLVED;
    system->lastTimeSolved = data->localData[0]->timeValue;
    system->numberOfCall++;
    system->numberOfIterations += total_iterations;
    iterations += total_iterations;
    ++accepted;
    return 1;
}
