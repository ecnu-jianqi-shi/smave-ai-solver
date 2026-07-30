#include "smave/linear.hpp"
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
#include <optional>
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

double infinity_error(const std::vector<double>& left, const std::vector<double>& right) {
    if (left.size() != right.size()) throw std::invalid_argument("error shape mismatch");
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

bool solve_periodic_implicit_upwind(
    const std::vector<double>& right_hand_side,
    double courant,
    std::vector<double>& affine,
    std::vector<double>& coefficient,
    std::vector<double>& solution) {
    if (right_hand_side.empty()) return false;
    const auto diagonal = 1.0 + courant;
    affine.resize(right_hand_side.size());
    coefficient.resize(right_hand_side.size());
    affine[0] = right_hand_side[0] / diagonal;
    coefficient[0] = courant / diagonal;
    for (std::size_t index = 1; index < right_hand_side.size(); ++index) {
        affine[index] = (right_hand_side[index] + courant * affine[index - 1]) / diagonal;
        coefficient[index] = courant * coefficient[index - 1] / diagonal;
    }
    const auto last = affine.back() / (1.0 - coefficient.back());
    solution.resize(right_hand_side.size());
    for (std::size_t index = 0; index < solution.size(); ++index) {
        solution[index] = affine[index] + coefficient[index] * last;
    }
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

std::vector<double> read_slice(hid_t dataset, hsize_t sample, hsize_t time, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, time, 0};
    const hsize_t count[] = {1, 1, width};
    if (H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 tensor read failed");
    }
    return {raw.begin(), raw.end()};
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_advection_benchmark FILE OUTPUT [LEARNED_OPERATOR]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle tensor(H5Dopen2(file.get(), "tensor", H5P_DEFAULT), H5Dclose);
        Handle tensor_space(H5Dget_space(tensor.get()), H5Sclose);
        hsize_t dimensions[3]{};
        if (H5Sget_simple_extent_dims(tensor_space.get(), dimensions, nullptr) != 3 ||
            dimensions[1] < 2 || dimensions[2] < 2) {
            throw std::runtime_error("unexpected PDEBench advection tensor shape");
        }
        Handle beta_attribute(H5Aopen(file.get(), "beta", H5P_DEFAULT), H5Aclose);
        double beta{};
        if (H5Aread(beta_attribute.get(), H5T_NATIVE_DOUBLE, &beta) < 0) {
            throw std::runtime_error("cannot read advection beta");
        }
        const double delta_time = 0.01;
        const double delta_x = 1.0 / static_cast<double>(dimensions[2]);
        const double courant = beta * delta_time / delta_x;
        std::optional<smave::LearnedPeriodicRecurrenceArtifact> learned_operator;
        if (argc == 4) {
            learned_operator =
                smave::LearnedPeriodicRecurrenceArtifact::read(argv[3]);
            if (learned_operator->family != "advection" ||
                learned_operator->discrete_operator_id !=
                    "periodic-implicit-upwind-v1" ||
                learned_operator->width != dimensions[2]) {
                throw std::invalid_argument(
                    "learned advection operator is incompatible with benchmark");
            }
        }
        const auto smave_diagonal = learned_operator
            ? 1.0 / learned_operator->inverse_diagonal
            : 1.0 + courant;
        const auto smave_lower = learned_operator
            ? -learned_operator->feedback /
                learned_operator->inverse_diagonal
            : -courant;
        const std::size_t samples = std::min<hsize_t>(3, dimensions[0]);
        const std::size_t steps = std::min<hsize_t>(50, dimensions[1] - 1);
        double maximum_cross_error{};
        double maximum_smave_relative_residual{};
        double maximum_data_error{};
        double classical_solve_seconds{};
        const auto setup_started = std::chrono::steady_clock::now();
        const smave::PeriodicLowerBidiagonalFactorization factorization(
            dimensions[2], smave_diagonal, smave_lower);
        const auto smave_setup_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - setup_started).count();
        if (!factorization.valid()) {
            throw std::runtime_error("SMAVE advection periodic bidiagonal setup failed");
        }
        double smave_kernel_seconds{};
        double smave_gate_seconds{};
        double batch_kernel_seconds{};
        double batch_gate_seconds{};
        double batch_maximum_relative_residual{};
        double batch_maximum_cross_error{};
        double batch_maximum_data_error{};
        std::vector<double> classical_affine(dimensions[2]);
        std::vector<double> classical_coefficient(dimensions[2]);
        std::vector<double> traditional(dimensions[2]);
        std::vector<double> smave_solution(dimensions[2]);
        double batch_setup_seconds{};
        const auto started = std::chrono::steady_clock::now();
        const auto run_classical_phase = [&]() {
            for (std::size_t sample = 0; sample < samples; ++sample) {
                auto state = read_slice(tensor.get(), sample, 0, dimensions[2]);
                for (std::size_t step = 0; step < steps; ++step) {
                    const auto classical_started = std::chrono::steady_clock::now();
                    if (!solve_periodic_implicit_upwind(
                            state, courant, classical_affine,
                            classical_coefficient, traditional)) {
                        throw std::runtime_error("classical advection solve failed");
                    }
                    classical_solve_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - classical_started).count();
                    const auto smave_started = std::chrono::steady_clock::now();
                    if (!factorization.solve(state, smave_solution)) {
                        throw std::runtime_error(
                            "SMAVE advection bidiagonal solve failed");
                    }
                    smave_kernel_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - smave_started).count();
                    const auto gate_started = std::chrono::steady_clock::now();
                    double residual_norm_squared{};
                    double right_hand_side_norm_squared{};
                    for (std::size_t index = 0; index < state.size(); ++index) {
                        const auto previous =
                            index == 0 ? state.size() - 1 : index - 1;
                        const auto residual = state[index] -
                            ((1.0 + courant) * smave_solution[index] -
                             courant * smave_solution[previous]);
                        residual_norm_squared += residual * residual;
                        right_hand_side_norm_squared += state[index] * state[index];
                    }
                    maximum_smave_relative_residual = std::max(
                        maximum_smave_relative_residual,
                        std::sqrt(residual_norm_squared) /
                            std::max(1.0, std::sqrt(right_hand_side_norm_squared)));
                    smave_gate_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    maximum_cross_error = std::max(maximum_cross_error,
                        infinity_error(smave_solution, traditional));
                    const auto authoritative = read_slice(
                        tensor.get(), sample, step + 1, dimensions[2]);
                    maximum_data_error = std::max(maximum_data_error,
                        infinity_error(smave_solution, authoritative));
                    state.swap(smave_solution);
                }
            }
        };
        const auto run_smave_phase = [&]() {
            const auto batch_setup_started = std::chrono::steady_clock::now();
            const smave::PeriodicLowerBidiagonalFactorization batch_factorization(
                dimensions[2], smave_diagonal, smave_lower);
            batch_setup_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - batch_setup_started).count();
            if (!batch_factorization.valid()) {
                throw std::runtime_error("SMAVE advection batch setup failed");
            }
            std::vector<double> batch_state(dimensions[2] * samples);
            std::vector<double> batch_solution;
            std::vector<double> batch_reference(dimensions[2]);
            std::vector<double> lane_right(dimensions[2]);
            std::vector<double> batch_lane_solution(dimensions[2]);
            std::vector<double> batch_relative_residuals;
            for (std::size_t sample = 0; sample < samples; ++sample) {
                const auto initial = read_slice(tensor.get(), sample, 0, dimensions[2]);
                for (std::size_t index = 0; index < dimensions[2]; ++index) {
                    batch_state[index * samples + sample] = initial[index];
                }
            }
            for (std::size_t step = 0; step < steps; ++step) {
                const auto batch_started = std::chrono::steady_clock::now();
                if (!batch_factorization.solve_interleaved(
                        batch_state, samples, batch_solution)) {
                    throw std::runtime_error("SMAVE advection batch solve failed");
                }
                batch_kernel_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - batch_started).count();
                const auto batch_gate_started = std::chrono::steady_clock::now();
                if (!smave::periodic_lower_bidiagonal_relative_residual_interleaved(
                        batch_state, batch_solution, samples, 1.0 + courant,
                        -courant, batch_relative_residuals)) {
                    throw std::runtime_error(
                        "SMAVE advection vectorized residual gate failed");
                }
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    batch_maximum_relative_residual = std::max(
                        batch_maximum_relative_residual,
                        batch_relative_residuals[sample]);
                }
                batch_gate_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - batch_gate_started).count();
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    for (std::size_t index = 0; index < dimensions[2]; ++index) {
                        lane_right[index] = batch_state[index * samples + sample];
                    }
                    if (!solve_periodic_implicit_upwind(
                            lane_right, courant, classical_affine,
                            classical_coefficient, batch_reference)) {
                        throw std::runtime_error(
                            "SMAVE advection traditional batch reference failed");
                    }
                    const auto authoritative = read_slice(
                        tensor.get(), sample, step + 1, dimensions[2]);
                    for (std::size_t index = 0; index < dimensions[2]; ++index) {
                        batch_lane_solution[index] =
                            batch_solution[index * samples + sample];
                    }
                    batch_maximum_cross_error = std::max(
                        batch_maximum_cross_error,
                        infinity_error(batch_lane_solution, batch_reference));
                    batch_maximum_data_error = std::max(
                        batch_maximum_data_error,
                        infinity_error(batch_lane_solution, authoritative));
                }
                batch_state.swap(batch_solution);
            }
        };
        smave::test::run_in_benchmark_solver_order(
            solver_order, run_classical_phase, run_smave_phase);
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        const auto scalar_candidate_seconds =
            smave_setup_seconds + smave_kernel_seconds + smave_gate_seconds;
        const auto smave_solve_seconds =
            batch_setup_seconds + batch_kernel_seconds + batch_gate_seconds;
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_ADVECTION 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SHAPE " << dimensions[0] << ' ' << dimensions[1] << ' '
               << dimensions[2] << "\n"
               << "BETA " << beta << "\n"
               << "COURANT " << courant << "\n"
               << "SAMPLES " << samples << "\n"
               << "STEPS_PER_SAMPLE " << steps << "\n"
               << "SOLVES " << samples * steps << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "SMAVE_BACKEND \""
               << (learned_operator
                   ? "learned-periodic-recurrence-interleaved-fp64-v1"
                   : "periodic-lower-bidiagonal-interleaved-cpu-v2")
               << "\"\n"
               << "LEARNED_OPERATOR " << learned_operator.has_value() << "\n"
               << "LEARNED_TRAINING_RESIDUAL "
               << (learned_operator
                   ? learned_operator->training_maximum_relative_residual : 0.0)
               << "\n"
               << "LEARNED_HELDOUT_RESIDUAL "
               << (learned_operator
                   ? learned_operator->heldout_maximum_relative_residual : 0.0)
               << "\n"
               << "SMAVE_SETUP_SECONDS " << batch_setup_seconds << "\n"
               << "SMAVE_KERNEL_SECONDS " << batch_kernel_seconds << "\n"
               << "SMAVE_GATE_SECONDS " << batch_gate_seconds << "\n"
               << "SCALAR_CANDIDATE_BACKEND "
                  "\"periodic-lower-bidiagonal-factor-reuse-cpu-v1\"\n"
               << "SCALAR_CANDIDATE_FULL_SECONDS "
               << scalar_candidate_seconds << "\n"
               << "BATCH_BACKEND \"periodic-lower-bidiagonal-interleaved-neon-gate-cpu-v3\"\n"
               << "BATCH_SIZE " << samples << "\n"
               << "BATCH_KERNEL_SECONDS " << batch_kernel_seconds << "\n"
               << "BATCH_GATE_SECONDS " << batch_gate_seconds << "\n"
               << "BATCH_SETUP_SECONDS " << batch_setup_seconds << "\n"
               << "BATCH_FULL_SECONDS "
               << batch_setup_seconds + batch_kernel_seconds + batch_gate_seconds << "\n"
               << "BATCH_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds /
                    (batch_setup_seconds + batch_kernel_seconds + batch_gate_seconds)
               << "\n"
               << "BATCH_MAXIMUM_RELATIVE_RESIDUAL "
               << batch_maximum_relative_residual << "\n"
               << "BATCH_CROSS_SCALAR_INF_ERROR "
               << batch_maximum_cross_error << "\n"
               << "CLASSICAL_SOLVE_SECONDS " << classical_solve_seconds << "\n"
               << "SMAVE_SOLVE_SECONDS " << smave_solve_seconds << "\n"
               << "CLASSICAL_MEAN_SOLVE_US "
               << classical_solve_seconds * 1.0e6 / (samples * steps) << "\n"
               << "SMAVE_MEAN_SOLVE_US "
               << smave_solve_seconds * 1.0e6 / (samples * steps) << "\n"
               << "SMAVE_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / smave_solve_seconds << "\n"
               << "CROSS_SOLVER_RELATIVE_INF_ERROR "
               << batch_maximum_cross_error << "\n"
               << "SMAVE_MAXIMUM_RELATIVE_RESIDUAL "
               << batch_maximum_relative_residual << "\n"
               << "PDEBENCH_TRAJECTORY_RELATIVE_INF_ERROR "
               << batch_maximum_data_error << "\n"
               << "ELAPSED_SECONDS " << elapsed << "\n"
               << "CROSS_SOLVER_AGREEMENT "
               << (batch_maximum_cross_error <= 1.0e-8 &&
                   batch_maximum_relative_residual <= 1.0e-10) << "\n"
               << "END\n";
        std::cout << "PDEBench advection solves=" << samples * steps
                  << " cross_error=" << maximum_cross_error
                  << " data_error=" << maximum_data_error << '\n';
        return batch_maximum_cross_error <= 1.0e-8 &&
            batch_maximum_relative_residual <= 1.0e-10 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench advection benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
