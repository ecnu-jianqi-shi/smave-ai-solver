#include "smave/linear.hpp"
#include "smave/device.hpp"
#include "smave/pdebench_training.hpp"
#include "pdebench_benchmark_order.hpp"

#include <hdf5.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Handle {
public:
    Handle(hid_t value, herr_t (*closer)(hid_t)) : value_(value), closer_(closer) {
        if (value_ < 0) throw std::runtime_error("HDF5 handle creation failed");
    }
    ~Handle() { if (value_ >= 0) closer_(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    [[nodiscard]] hid_t get() const { return value_; }
private:
    hid_t value_;
    herr_t (*closer_)(hid_t);
};

double dot(const std::vector<double>& left, const std::vector<double>& right) {
    return std::inner_product(left.begin(), left.end(), right.begin(), 0.0);
}

double relative_infinity_error(
    const std::vector<double>& left, const std::vector<double>& right) {
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

std::vector<double> read_field(
    hid_t file, const std::string& sample, hsize_t time,
    hsize_t height, hsize_t width) {
    Handle dataset(H5Dopen2(file, ("/" + sample + "/data").c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {time, 0, 0, 0};
    const hsize_t count[] = {1, height, width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("shallow-water hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(4, count, nullptr), H5Sclose);
    std::vector<float> raw(height * width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("shallow-water field read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> downsample_square(
    const std::vector<double>& field, std::size_t source_width,
    std::size_t target_width) {
    std::vector<double> sampled(target_width * target_width);
    for (std::size_t row = 0; row < target_width; ++row) {
        const auto source_row = row * source_width / target_width;
        for (std::size_t column = 0; column < target_width; ++column) {
            const auto source_column = column * source_width / target_width;
            sampled[row * target_width + column] =
                field[source_row * source_width + source_column];
        }
    }
    return sampled;
}

struct WaveOperator {
    std::size_t width;
    double wave_number;

    bool operator()(const std::vector<double>& input, std::vector<double>& output) const {
        if (input.size() != width * width) return false;
        output.resize(input.size());
        for (std::size_t row = 0; row < width; ++row) {
            const auto south = row == 0 ? width - 1 : row - 1;
            const auto north = row + 1 == width ? 0 : row + 1;
            for (std::size_t column = 0; column < width; ++column) {
                const auto west = column == 0 ? width - 1 : column - 1;
                const auto east = column + 1 == width ? 0 : column + 1;
                const auto index = row * width + column;
                output[index] = (1.0 + 4.0 * wave_number) * input[index] -
                    wave_number * (input[row * width + west] +
                                   input[row * width + east] +
                                   input[south * width + column] +
                                   input[north * width + column]);
            }
        }
        return true;
    }
};

struct ClassicalResult {
    int iterations{};
    bool converged{};
};

struct ClassicalWorkspace {
    std::vector<double> solution;
    std::vector<double> residual;
    std::vector<double> preconditioned;
    std::vector<double> direction;
    std::vector<double> product;
};

ClassicalResult classical_pcg(
    const WaveOperator& operation, const std::vector<double>& right_hand_side,
    int maximum_iterations, double tolerance, ClassicalWorkspace& workspace) {
    ClassicalResult result;
    workspace.solution.assign(right_hand_side.size(), 0.0);
    workspace.residual = right_hand_side;
    const auto inverse_diagonal = 1.0 / (1.0 + 4.0 * operation.wave_number);
    workspace.preconditioned = workspace.residual;
    for (auto& value : workspace.preconditioned) value *= inverse_diagonal;
    workspace.direction = workspace.preconditioned;
    auto residual_preconditioned = dot(
        workspace.residual, workspace.preconditioned);
    const auto initial_norm = std::sqrt(dot(workspace.residual, workspace.residual));
    for (int iteration = 0; iteration < maximum_iterations; ++iteration) {
        operation(workspace.direction, workspace.product);
        const auto curvature = dot(workspace.direction, workspace.product);
        if (!(curvature > 0.0)) return result;
        const auto alpha = residual_preconditioned / curvature;
        for (std::size_t index = 0; index < workspace.solution.size(); ++index) {
            workspace.solution[index] += alpha * workspace.direction[index];
            workspace.residual[index] -= alpha * workspace.product[index];
        }
        result.iterations = iteration + 1;
        if (std::sqrt(dot(workspace.residual, workspace.residual)) <=
            tolerance * std::max(1.0, initial_norm)) {
            result.converged = true;
            return result;
        }
        for (std::size_t index = 0; index < workspace.residual.size(); ++index) {
            workspace.preconditioned[index] =
                inverse_diagonal * workspace.residual[index];
        }
        const auto next = dot(workspace.residual, workspace.preconditioned);
        const auto beta = next / residual_preconditioned;
        for (std::size_t index = 0; index < workspace.direction.size(); ++index) {
            workspace.direction[index] = workspace.preconditioned[index] +
                beta * workspace.direction[index];
        }
        residual_preconditioned = next;
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_shallow_water_benchmark FILE OUTPUT "
                "[LEARNED_OPERATOR]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        std::optional<smave::LearnedPeriodicHelmholtzArtifact> learned_operator;
        if (argc == 4) {
            learned_operator =
                smave::LearnedPeriodicHelmholtzArtifact::read(argv[3]);
            if (learned_operator->family != "shallow-water" ||
                learned_operator->discrete_operator_id !=
                    "periodic-wave-helmholtz-v1") {
                throw std::invalid_argument("invalid shallow-water learned operator");
            }
        }
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t source_width = 128;
        constexpr std::size_t width = 32;
        constexpr std::size_t samples = 3;
        constexpr std::size_t steps = 20;
        constexpr double delta_time = 0.01;
        constexpr double domain_width = 5.0;
        constexpr double gravity = 1.0;
        const double delta_x = domain_width / width;
        const double wave_number = gravity * delta_time * delta_time /
            (delta_x * delta_x);
        const WaveOperator operation{width, wave_number};
        if (learned_operator && learned_operator->width != width) {
            throw std::invalid_argument("shallow-water learned width mismatch");
        }
        smave::AcceleratePeriodicHelmholtz2DPlan spectral_plan(
            width, learned_operator
                ? learned_operator->stencil_number : wave_number);
        if (!spectral_plan.available()) {
            throw std::runtime_error(
                "SMAVE shallow-water spectral plan failed: " + spectral_plan.reason());
        }
        double maximum_cross_error{};
        double maximum_smave_relative_residual{};
        double maximum_data_error{};
        std::size_t classical_iterations{};
        double classical_solve_seconds{};
        const auto smave_setup_seconds = spectral_plan.setup_us() * 1.0e-6;
        double batch_kernel_us{};
        double batch_gate_seconds{};
        ClassicalWorkspace classical_workspace;
        std::vector<double> right_hand_side(width * width);
        std::vector<double> smave_solution(width * width);
        std::vector<double> product(width * width);
        const auto started = std::chrono::steady_clock::now();
        std::vector<std::vector<double>> previous_states(samples);
        std::vector<std::vector<double>> current_states(samples);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            std::ostringstream name;
            name << std::setw(4) << std::setfill('0') << sample;
            previous_states[sample] = downsample_square(read_field(
                file.get(), name.str(), 0, source_width, source_width), source_width, width);
            current_states[sample] = downsample_square(read_field(
                file.get(), name.str(), 1, source_width, source_width), source_width, width);
        }
        std::vector<double> batch_right_hand_sides(samples * width * width);
        std::vector<double> batch_solutions;
        std::vector<ClassicalWorkspace> classical_workspaces(samples);
        for (std::size_t step = 0; step < steps; ++step) {
            for (std::size_t sample = 0; sample < samples; ++sample) {
                auto& previous = previous_states[sample];
                auto& current = current_states[sample];
                for (std::size_t index = 0; index < current.size(); ++index) {
                    right_hand_side[index] = 2.0 * current[index] - previous[index];
                    batch_right_hand_sides[sample * width * width + index] =
                        right_hand_side[index];
                }
            }
            const auto run_classical = [&]() {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    std::copy_n(
                        batch_right_hand_sides.begin() + sample * width * width,
                        width * width, right_hand_side.begin());
                    const auto classical_started = std::chrono::steady_clock::now();
                    const auto traditional = classical_pcg(
                        operation, right_hand_side, 500, 1.0e-10,
                        classical_workspaces[sample]);
                    classical_solve_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - classical_started).count();
                    if (!traditional.converged) {
                        throw std::runtime_error("classical shallow-water PCG failed");
                    }
                    classical_iterations +=
                        static_cast<std::size_t>(traditional.iterations);
                }
            };
            const auto run_smave = [&]() {
                batch_solutions.resize(batch_right_hand_sides.size());
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    std::copy_n(
                        batch_right_hand_sides.begin() + sample * width * width,
                        width * width, right_hand_side.begin());
                    if (!spectral_plan.solve(
                            right_hand_side, smave_solution, &batch_kernel_us)) {
                        throw std::runtime_error(
                            "SMAVE shallow-water spectral scalar plan solve failed");
                    }
                    std::copy_n(
                        smave_solution.begin(), width * width,
                        batch_solutions.begin() + sample * width * width);
                }
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    std::copy_n(
                        batch_solutions.begin() + sample * width * width,
                        width * width, smave_solution.begin());
                    std::copy_n(
                        batch_right_hand_sides.begin() + sample * width * width,
                        width * width, right_hand_side.begin());
                    const auto gate_started = std::chrono::steady_clock::now();
                    if (!operation(smave_solution, product)) {
                        throw std::runtime_error(
                            "SMAVE shallow-water residual gate failed");
                    }
                    double residual_norm_squared{};
                    double right_hand_side_norm_squared{};
                    for (std::size_t index = 0;
                         index < right_hand_side.size(); ++index) {
                        const auto residual =
                            right_hand_side[index] - product[index];
                        residual_norm_squared += residual * residual;
                        right_hand_side_norm_squared +=
                            right_hand_side[index] * right_hand_side[index];
                    }
                    maximum_smave_relative_residual = std::max(
                        maximum_smave_relative_residual,
                        std::sqrt(residual_norm_squared) /
                            std::max(1.0, std::sqrt(right_hand_side_norm_squared)));
                    batch_gate_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - gate_started).count();
                }
            };
            smave::test::run_in_benchmark_solver_order(
                solver_order, run_classical, run_smave);
            for (std::size_t sample = 0; sample < samples; ++sample) {
                auto& previous = previous_states[sample];
                auto& current = current_states[sample];
                std::copy_n(
                    batch_solutions.begin() + sample * width * width,
                    width * width, smave_solution.begin());
                maximum_cross_error = std::max(maximum_cross_error,
                    relative_infinity_error(
                        smave_solution, classical_workspaces[sample].solution));
                std::ostringstream name;
                name << std::setw(4) << std::setfill('0') << sample;
                const auto authoritative = downsample_square(read_field(
                    file.get(), name.str(), step + 2, source_width, source_width),
                    source_width, width);
                maximum_data_error = std::max(maximum_data_error,
                    relative_infinity_error(smave_solution, authoritative));
                previous = current;
                current = smave_solution;
            }
        }
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        const auto smave_kernel_seconds = batch_kernel_us * 1.0e-6;
        const auto smave_solve_seconds =
            smave_setup_seconds + smave_kernel_seconds + batch_gate_seconds;
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_SHALLOW_WATER 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SOURCE_SHAPE 1000 101 128 128 1\n"
               << "MODEL \"linearized-scalar-gravity-wave-subsystem\"\n"
               << "DISCRETIZATION_GRID " << width << ' ' << width << "\n"
               << "SAMPLES " << samples << "\n"
               << "STEPS_PER_SAMPLE " << steps << "\n"
               << "SOLVES " << samples * steps << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "SMAVE_BACKEND \"accelerate-vdsp-real-periodic-helmholtz-plan-fp64-v2\"\n"
               << "SMAVE_BATCH_SIZE " << samples << "\n"
               << "SMAVE_BATCH_ROUTED 0\n"
               << "SMAVE_BATCH_REJECTION \"32x32 batch=3 dispatch crossover favors persistent scalar plan\"\n"
               << "LEARNED_OPERATOR " << learned_operator.has_value() << "\n"
               << "LEARNED_STENCIL_NUMBER "
               << (learned_operator ? learned_operator->stencil_number : 0.0)
               << "\n"
               << "LEARNED_TRAINING_RESIDUAL "
               << (learned_operator
                   ? learned_operator->training_maximum_relative_residual : 0.0)
               << "\n"
               << "LEARNED_HELDOUT_RESIDUAL "
               << (learned_operator
                   ? learned_operator->heldout_maximum_relative_residual : 0.0)
               << "\n"
               << "SMAVE_SETUP_SECONDS " << smave_setup_seconds << "\n"
               << "SMAVE_KERNEL_SECONDS " << smave_kernel_seconds << "\n"
               << "SMAVE_GATE_SECONDS " << batch_gate_seconds << "\n"
               << "CLASSICAL_PCG_TOTAL_ITERATIONS " << classical_iterations << "\n"
               << "CLASSICAL_SOLVE_SECONDS " << classical_solve_seconds << "\n"
               << "SMAVE_SOLVE_SECONDS " << smave_solve_seconds << "\n"
               << "CLASSICAL_MEAN_SOLVE_US "
               << classical_solve_seconds * 1.0e6 / (samples * steps) << "\n"
               << "SMAVE_MEAN_SOLVE_US "
               << smave_solve_seconds * 1.0e6 / (samples * steps) << "\n"
               << "SMAVE_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / smave_solve_seconds << "\n"
               << "CROSS_SOLVER_RELATIVE_INF_ERROR " << maximum_cross_error << "\n"
               << "SMAVE_MAXIMUM_RELATIVE_RESIDUAL "
               << maximum_smave_relative_residual << "\n"
               << "PDEBENCH_TRAJECTORY_RELATIVE_INF_ERROR " << maximum_data_error << "\n"
               << "ELAPSED_SECONDS " << elapsed << "\n"
               << "CROSS_SOLVER_AGREEMENT "
               << (maximum_cross_error <= 1.0e-7 &&
                   maximum_smave_relative_residual <= 1.0e-10) << "\n"
               << "END\n";
        std::cout << "PDEBench shallow-water solves=" << samples * steps
                  << " cross_error=" << maximum_cross_error
                  << " data_error=" << maximum_data_error << '\n';
        return maximum_cross_error <= 1.0e-7 &&
            maximum_smave_relative_residual <= 1.0e-10 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench shallow-water benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
