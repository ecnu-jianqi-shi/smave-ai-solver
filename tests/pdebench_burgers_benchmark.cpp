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

double relative_infinity_error(
    const std::vector<double>& left, const std::vector<double>& right) {
    if (left.size() != right.size()) throw std::invalid_argument("error shape mismatch");
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

std::vector<double> read_slice(
    hid_t dataset, hsize_t sample, hsize_t time, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, time, 0};
    const hsize_t count[] = {1, 1, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
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

std::vector<double> solve_tridiagonal(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side) {
    const auto size = diagonal.size();
    std::vector<double> modified_upper(size);
    std::vector<double> modified_rhs(size);
    double pivot = diagonal[0];
    if (std::abs(pivot) < 1.0e-14) throw std::runtime_error("tridiagonal zero pivot");
    modified_upper[0] = upper[0] / pivot;
    modified_rhs[0] = right_hand_side[0] / pivot;
    for (std::size_t index = 1; index < size; ++index) {
        pivot = diagonal[index] - lower[index] * modified_upper[index - 1];
        if (std::abs(pivot) < 1.0e-14) throw std::runtime_error("tridiagonal zero pivot");
        modified_upper[index] = index + 1 < size ? upper[index] / pivot : 0.0;
        modified_rhs[index] =
            (right_hand_side[index] - lower[index] * modified_rhs[index - 1]) / pivot;
    }
    std::vector<double> solution(size);
    solution.back() = modified_rhs.back();
    for (std::size_t index = size - 1; index-- > 0;) {
        solution[index] = modified_rhs[index] - modified_upper[index] * solution[index + 1];
    }
    return solution;
}

std::vector<double> solve_cyclic_tridiagonal(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    double top_right, double bottom_left,
    const std::vector<double>& right_hand_side) {
    auto modified_diagonal = diagonal;
    const double gamma = -diagonal.front();
    modified_diagonal.front() -= gamma;
    modified_diagonal.back() -= top_right * bottom_left / gamma;
    auto primary = solve_tridiagonal(
        lower, modified_diagonal, upper, right_hand_side);
    std::vector<double> update(diagonal.size());
    update.front() = gamma;
    update.back() = bottom_left;
    const auto correction = solve_tridiagonal(
        lower, modified_diagonal, upper, update);
    const auto factor = (primary.front() + top_right * primary.back() / gamma) /
        (1.0 + correction.front() + top_right * correction.back() / gamma);
    for (std::size_t index = 0; index < primary.size(); ++index) {
        primary[index] -= factor * correction[index];
    }
    return primary;
}

struct FrozenBurgersOperator {
    const std::vector<double>& state;
    double diffusion_number;
    double convection_scale;

    bool operator()(const std::vector<double>& input, std::vector<double>& output) const {
        if (input.size() != state.size()) return false;
        output.resize(input.size());
        for (std::size_t index = 0; index < input.size(); ++index) {
            const auto previous = index == 0 ? input.size() - 1 : index - 1;
            const auto next = index + 1 == input.size() ? 0 : index + 1;
            const auto convection = convection_scale * state[index];
            output[index] = (1.0 + 2.0 * diffusion_number) * input[index] +
                (-diffusion_number - convection) * input[previous] +
                (-diffusion_number + convection) * input[next];
        }
        return true;
    }
};

std::vector<double> solve_frozen_burgers_direct(
    const std::vector<double>& state,
    double diffusion_number, double convection_scale) {
    const auto size = state.size();
    std::vector<double> lower(size);
    std::vector<double> diagonal(size, 1.0 + 2.0 * diffusion_number);
    std::vector<double> upper(size);
    for (std::size_t index = 0; index < size; ++index) {
        const auto convection = convection_scale * state[index];
        lower[index] = -diffusion_number - convection;
        upper[index] = -diffusion_number + convection;
    }
    const auto top_right = lower.front();
    const auto bottom_left = upper.back();
    lower.front() = 0.0;
    upper.back() = 0.0;
    return solve_cyclic_tridiagonal(
        lower, diagonal, upper, top_right, bottom_left, state);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_burgers_benchmark FILE OUTPUT [LEARNED_OPERATOR]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle tensor(H5Dopen2(file.get(), "tensor", H5P_DEFAULT), H5Dclose);
        Handle tensor_space(H5Dget_space(tensor.get()), H5Sclose);
        hsize_t dimensions[3]{};
        if (H5Sget_simple_extent_dims(tensor_space.get(), dimensions, nullptr) != 3 ||
            dimensions[1] < 2 || dimensions[2] < 3) {
            throw std::runtime_error("unexpected PDEBench Burgers tensor shape");
        }
        Handle viscosity_attribute(H5Aopen(file.get(), "Nu", H5P_DEFAULT), H5Aclose);
        double viscosity{};
        if (H5Aread(viscosity_attribute.get(), H5T_NATIVE_DOUBLE, &viscosity) < 0) {
            throw std::runtime_error("cannot read Burgers viscosity");
        }
        constexpr double delta_time = 0.01;
        const double delta_x = 1.0 / static_cast<double>(dimensions[2]);
        const double diffusion_number = viscosity * delta_time / (delta_x * delta_x);
        const double convection_scale = delta_time / (2.0 * delta_x);
        std::optional<smave::LearnedFrozenBurgersArtifact> learned_operator;
        if (argc == 4) {
            learned_operator = smave::LearnedFrozenBurgersArtifact::read(argv[3]);
            if (learned_operator->width != dimensions[2]) {
                throw std::invalid_argument(
                    "learned Burgers operator width mismatch");
            }
        }
        const auto smave_diffusion_number = learned_operator
            ? learned_operator->diffusion_number : diffusion_number;
        const auto smave_convection_scale = learned_operator
            ? learned_operator->convection_scale : convection_scale;
        const std::size_t samples = std::min<hsize_t>(3, dimensions[0]);
        const std::size_t steps = std::min<hsize_t>(50, dimensions[1] - 1);
        double maximum_cross_error{};
        double maximum_smave_relative_residual{};
        double maximum_data_error{};
        double classical_solve_seconds{};
        double smave_kernel_seconds{};
        double smave_gate_seconds{};
        const auto size = static_cast<std::size_t>(dimensions[2]);
        const auto setup_started = std::chrono::steady_clock::now();
        smave::BatchedVariableTridiagonalWorkspace smave_workspace(size, samples);
        const auto smave_setup_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - setup_started).count();
        std::vector<std::vector<std::vector<double>>> authoritative_states(
            samples, std::vector<std::vector<double>>(steps + 1));
        std::vector<std::vector<double>> states(samples);
        std::vector<std::vector<double>> traditional_solutions(samples);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            for (std::size_t step = 0; step <= steps; ++step) {
                authoritative_states[sample][step] =
                    read_slice(tensor.get(), sample, step, dimensions[2]);
            }
            states[sample] = authoritative_states[sample][0];
        }
        std::vector<double> interleaved_states(size * samples);
        std::vector<double> interleaved_solutions(size * samples);
        std::vector<double> smave_solution(size);
        std::vector<double> relative_residuals;
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t step = 0; step < steps; ++step) {
            for (std::size_t index = 0; index < size; ++index) {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    interleaved_states[index * samples + sample] =
                        states[sample][index];
                }
            }
            const auto run_classical = [&]() {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    const auto classical_started = std::chrono::steady_clock::now();
                    traditional_solutions[sample] = solve_frozen_burgers_direct(
                        states[sample], diffusion_number, convection_scale);
                    classical_solve_seconds += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - classical_started).count();
                }
            };
            const auto run_smave = [&]() {
                const auto smave_started = std::chrono::steady_clock::now();
                if (!smave_workspace.
                        solve_cyclic_constant_diagonal_affine_off_diagonal_interleaved(
                            1.0 + 2.0 * smave_diffusion_number,
                            -smave_diffusion_number, -smave_convection_scale,
                            -smave_diffusion_number, smave_convection_scale,
                            interleaved_states,
                            interleaved_states, interleaved_solutions)) {
                    throw std::runtime_error(
                        "SMAVE batched Burgers cyclic tridiagonal solve failed");
                }
                smave_kernel_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - smave_started).count();
                const auto gate_started = std::chrono::steady_clock::now();
                if (!smave::frozen_burgers_relative_residual_interleaved(
                        interleaved_states, interleaved_solutions, samples,
                        diffusion_number, convection_scale, relative_residuals)) {
                    throw std::runtime_error(
                        "SMAVE Burgers vectorized residual gate failed");
                }
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    maximum_smave_relative_residual = std::max(
                        maximum_smave_relative_residual,
                        relative_residuals[sample]);
                }
                smave_gate_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - gate_started).count();
            };
            smave::test::run_in_benchmark_solver_order(
                solver_order, run_classical, run_smave);
            for (std::size_t sample = 0; sample < samples; ++sample) {
                for (std::size_t index = 0; index < size; ++index) {
                    smave_solution[index] =
                        interleaved_solutions[index * samples + sample];
                }
                maximum_cross_error = std::max(maximum_cross_error,
                    relative_infinity_error(
                        smave_solution, traditional_solutions[sample]));
                maximum_data_error = std::max(maximum_data_error,
                    relative_infinity_error(
                        smave_solution, authoritative_states[sample][step + 1]));
                states[sample] = smave_solution;
            }
        }
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        const auto smave_solve_seconds =
            smave_setup_seconds + smave_kernel_seconds + smave_gate_seconds;
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_BURGERS 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SHAPE " << dimensions[0] << ' ' << dimensions[1] << ' '
               << dimensions[2] << "\n"
               << "VISCOSITY " << viscosity << "\n"
               << "TIME_STEP " << delta_time << "\n"
               << "SAMPLES " << samples << "\n"
               << "STEPS_PER_SAMPLE " << steps << "\n"
               << "SOLVES " << samples * steps << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "SMAVE_BACKEND \""
               << (learned_operator
                   ? "learned-frozen-burgers-interleaved-fp64-v1"
                   : "interleaved-batch-affine-cyclic-tridiagonal-neon-cpu-v5")
               << "\"\n"
               << "LEARNED_OPERATOR " << learned_operator.has_value() << '\n'
               << "LEARNED_TRAINING_RESIDUAL "
               << (learned_operator
                   ? learned_operator->training_maximum_relative_residual : 0.0)
               << "\n"
               << "LEARNED_HELDOUT_RESIDUAL "
               << (learned_operator
                   ? learned_operator->heldout_maximum_relative_residual : 0.0)
               << "\n"
               << "SMAVE_BATCH_SIZE " << samples << "\n"
               << "SMAVE_SETUP_SECONDS " << smave_setup_seconds << "\n"
               << "SMAVE_KERNEL_SECONDS " << smave_kernel_seconds << "\n"
               << "SMAVE_GATE_SECONDS " << smave_gate_seconds << "\n"
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
                   maximum_smave_relative_residual <= 1.0e-9) << "\n"
               << "END\n";
        std::cout << "PDEBench Burgers solves=" << samples * steps
                  << " cross_error=" << maximum_cross_error
                  << " data_error=" << maximum_data_error << '\n';
        return maximum_cross_error <= 1.0e-7 &&
            maximum_smave_relative_residual <= 1.0e-9 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench Burgers benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
