#include "smave/linear.hpp"
#include "smave/pdebench_training.hpp"
#include "pdebench_benchmark_order.hpp"

#include <hdf5.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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

std::vector<double> read_vector(hid_t dataset) {
    Handle space(H5Dget_space(dataset), H5Sclose);
    hsize_t dimensions[1]{};
    if (H5Sget_simple_extent_dims(space.get(), dimensions, nullptr) != 1) {
        throw std::runtime_error("unexpected vector shape");
    }
    std::vector<float> raw(dimensions[0]);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 vector read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> read_state(hid_t dataset, hsize_t time, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {time, 0, 0};
    const hsize_t count[] = {1, width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 state read failed");
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
    double pivot = diagonal.front();
    modified_upper.front() = upper.front() / pivot;
    modified_rhs.front() = right_hand_side.front() / pivot;
    for (std::size_t index = 1; index < size; ++index) {
        pivot = diagonal[index] - lower[index] * modified_upper[index - 1];
        if (std::abs(pivot) < 1.0e-14) throw std::runtime_error("zero tridiagonal pivot");
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

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_diffusion_sorption_benchmark FILE OUTPUT "
                "[LEARNED_OPERATOR]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t samples = 3;
        constexpr std::size_t steps = 50;
        constexpr double diffusion = 0.0005;
        constexpr double porosity = 0.29;
        constexpr double solid_density = 2880.0;
        constexpr double freundlich_coefficient = 0.00035;
        constexpr double freundlich_exponent = 0.874;
        std::optional<smave::LearnedFrozenRetardationArtifact> learned_operator;
        if (argc == 4) {
            learned_operator =
                smave::LearnedFrozenRetardationArtifact::read(argv[3]);
        }
        double maximum_cross_error{};
        double maximum_smave_relative_residual{};
        double maximum_data_error{};
        double classical_solve_seconds{};
        double smave_setup_seconds{};
        double smave_kernel_seconds{};
        double smave_gate_seconds{};
        double smave_solve_seconds{};
        std::vector<std::vector<std::vector<double>>> authoritative_states(samples);
        std::vector<double> times;
        std::vector<double> grid;
        for (std::size_t sample = 0; sample < samples; ++sample) {
            std::ostringstream group_name;
            group_name << '/' << std::setw(4) << std::setfill('0') << sample;
            Handle data(H5Dopen2(file.get(), (group_name.str() + "/data").c_str(),
                                 H5P_DEFAULT), H5Dclose);
            Handle time_dataset(H5Dopen2(file.get(), (group_name.str() + "/grid/t").c_str(),
                                         H5P_DEFAULT), H5Dclose);
            Handle space_dataset(H5Dopen2(file.get(), (group_name.str() + "/grid/x").c_str(),
                                          H5P_DEFAULT), H5Dclose);
            const auto sample_times = read_vector(time_dataset.get());
            const auto sample_grid = read_vector(space_dataset.get());
            if (sample == 0) {
                times = sample_times;
                grid = sample_grid;
            } else if (sample_times != times || sample_grid != grid) {
                throw std::runtime_error(
                    "diffusion-sorption batch grids are inconsistent");
            }
            authoritative_states[sample].reserve(steps + 1);
            for (std::size_t step = 0; step <= steps; ++step) {
                authoritative_states[sample].push_back(
                    read_state(data.get(), step, grid.size()));
            }
        }
        const auto width = grid.size();
        if (learned_operator && learned_operator->width != width) {
            throw std::invalid_argument(
                "learned diffusion-sorption operator width mismatch");
        }
        const auto unknowns = width - 1;
        const auto values = unknowns * samples;
        const auto smave_setup_started = std::chrono::steady_clock::now();
        smave::BatchedVariableTridiagonalWorkspace smave_workspace(
            unknowns, samples);
        smave_setup_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - smave_setup_started).count();
        std::vector<double> lower(values);
        std::vector<double> diagonal(values);
        std::vector<double> upper(values);
        std::vector<double> right_hand_side(values);
        std::vector<double> smave_lower(values);
        std::vector<double> smave_diagonal(values);
        std::vector<double> smave_upper(values);
        std::vector<double> smave_right_hand_side(values);
        std::vector<double> smave_unknowns(values);
        std::vector<std::vector<double>> states(samples);
        std::vector<std::vector<double>> traditional_unknowns(samples);
        std::vector<double> traditional(width, 1.0);
        std::vector<double> smave_solution(width, 1.0);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            states[sample] = authoritative_states[sample][0];
        }
        const double delta_x = grid[1] - grid[0];
        for (std::size_t step = 0; step < steps; ++step) {
            const double delta_time = times[step + 1] - times[step];
            const double number = diffusion * delta_time / (delta_x * delta_x);
            for (std::size_t local = 0; local < unknowns; ++local) {
                const auto index = local + 1;
                const auto offset = local * samples;
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    const auto position = offset + sample;
                    const double concentration =
                        std::max(states[sample][index], 1.0e-8);
                    const double retardation = porosity + solid_density *
                        freundlich_coefficient * freundlich_exponent *
                        std::pow(concentration, freundlich_exponent - 1.0);
                    lower[position] = local > 0 ? -number : 0.0;
                    diagonal[position] = retardation + 2.0 * number;
                    upper[position] = local + 1 < unknowns ? -number : 0.0;
                    right_hand_side[position] =
                        retardation * states[sample][index];
                    if (learned_operator) {
                        const auto ratio = learned_operator->constant_ratio +
                            learned_operator->power_ratio * std::pow(
                                concentration,
                                learned_operator->concentration_exponent);
                        smave_lower[position] = local > 0 ? -1.0 : 0.0;
                        smave_diagonal[position] = ratio + 2.0;
                        smave_upper[position] =
                            local + 1 < unknowns ? -1.0 : 0.0;
                        smave_right_hand_side[position] =
                            ratio * states[sample][index];
                    } else {
                        smave_lower[position] = lower[position];
                        smave_diagonal[position] = diagonal[position];
                        smave_upper[position] = upper[position];
                        smave_right_hand_side[position] =
                            right_hand_side[position];
                    }
                }
            }
            for (std::size_t sample = 0; sample < samples; ++sample) {
                right_hand_side[sample] += number;
                diagonal[(unknowns - 1) * samples + sample] -= number;
                if (learned_operator) {
                    smave_right_hand_side[sample] += 1.0;
                    smave_diagonal[(unknowns - 1) * samples + sample] -= 1.0;
                } else {
                    smave_right_hand_side[sample] += number;
                    smave_diagonal[(unknowns - 1) * samples + sample] -= number;
                }
            }
            const auto run_classical = [&]() {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                std::vector<double> sample_lower(unknowns);
                std::vector<double> sample_diagonal(unknowns);
                std::vector<double> sample_upper(unknowns);
                std::vector<double> sample_right(unknowns);
                for (std::size_t local = 0; local < unknowns; ++local) {
                    const auto position = local * samples + sample;
                    sample_lower[local] = lower[position];
                    sample_diagonal[local] = diagonal[position];
                    sample_upper[local] = upper[position];
                    sample_right[local] = right_hand_side[position];
                }
                const auto classical_started = std::chrono::steady_clock::now();
                traditional_unknowns[sample] = solve_tridiagonal(
                    sample_lower, sample_diagonal, sample_upper, sample_right);
                classical_solve_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - classical_started).count();
                }
            };
            const auto run_smave = [&]() {
                const auto smave_started = std::chrono::steady_clock::now();
                if (!smave_workspace.
                        solve_strictly_diagonally_dominant_m_matrix_interleaved(
                            smave_lower, smave_diagonal, smave_upper,
                            smave_right_hand_side, smave_unknowns)) {
                    throw std::runtime_error(
                        "SMAVE batched diffusion-sorption solve failed");
                }
                smave_kernel_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - smave_started).count();
                const auto gate_started = std::chrono::steady_clock::now();
                std::array<double, samples> residual_norm_squared{};
                std::array<double, samples> right_hand_side_norm_squared{};
                for (std::size_t local = 0; local < unknowns; ++local) {
                    for (std::size_t sample = 0; sample < samples; ++sample) {
                        const auto position = local * samples + sample;
                        auto product = diagonal[position] * smave_unknowns[position];
                        if (local > 0) {
                            product += lower[position] *
                                smave_unknowns[position - samples];
                        }
                        if (local + 1 < unknowns) {
                            product += upper[position] *
                                smave_unknowns[position + samples];
                        }
                        const auto residual = right_hand_side[position] - product;
                        residual_norm_squared[sample] += residual * residual;
                        right_hand_side_norm_squared[sample] +=
                            right_hand_side[position] * right_hand_side[position];
                    }
                }
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    maximum_smave_relative_residual = std::max(
                        maximum_smave_relative_residual,
                        std::sqrt(residual_norm_squared[sample]) /
                            std::max(
                                1.0, std::sqrt(right_hand_side_norm_squared[sample])));
                }
                smave_gate_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - gate_started).count();
            };
            if (solver_order == smave::test::BenchmarkSolverOrder::smave_first) {
                run_smave();
                run_classical();
            } else {
                run_classical();
                run_smave();
            }
            for (std::size_t sample = 0; sample < samples; ++sample) {
                std::fill(traditional.begin(), traditional.end(), 1.0);
                std::fill(smave_solution.begin(), smave_solution.end(), 1.0);
                std::copy(traditional_unknowns[sample].begin(),
                          traditional_unknowns[sample].end(), traditional.begin() + 1);
                for (std::size_t local = 0; local < unknowns; ++local) {
                        smave_solution[local + 1] =
                        smave_unknowns[local * samples + sample];
                }
                maximum_cross_error = std::max(maximum_cross_error,
                    relative_infinity_error(smave_solution, traditional));
                maximum_data_error = std::max(maximum_data_error,
                    relative_infinity_error(
                        smave_solution, authoritative_states[sample][step + 1]));
                states[sample] = smave_solution;
            }
        }
        const auto solves = samples * steps;
        smave_solve_seconds =
            smave_setup_seconds + smave_kernel_seconds + smave_gate_seconds;
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_DIFFUSION_SORPTION 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "MODEL \"frozen-retardation-implicit-diffusion\"\n"
               << "SAMPLES " << samples << "\n"
               << "STEPS_PER_SAMPLE " << steps << "\n"
               << "SOLVES " << solves << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "SMAVE_BACKEND \""
               << (learned_operator
                   ? "learned-frozen-retardation-scaled-tridiagonal-fp64-v1"
                   : "interleaved-batch-strict-dd-m-matrix-cpu-v2")
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
               << classical_solve_seconds * 1.0e6 / solves << "\n"
               << "SMAVE_MEAN_SOLVE_US "
               << smave_solve_seconds * 1.0e6 / solves << "\n"
               << "SMAVE_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / smave_solve_seconds << "\n"
               << "CROSS_SOLVER_RELATIVE_INF_ERROR " << maximum_cross_error << "\n"
               << "SMAVE_MAXIMUM_RELATIVE_RESIDUAL "
               << maximum_smave_relative_residual << "\n"
               << "PDEBENCH_TRAJECTORY_RELATIVE_INF_ERROR " << maximum_data_error << "\n"
               << "CROSS_SOLVER_AGREEMENT "
               << (maximum_cross_error <= 1.0e-7 &&
                   maximum_smave_relative_residual <= 1.0e-10) << "\n"
               << "END\n";
        std::cout << "PDEBench diffusion-sorption solves=" << solves
                  << " cross_error=" << maximum_cross_error
                  << " data_error=" << maximum_data_error << '\n';
        return maximum_cross_error <= 1.0e-7 &&
            maximum_smave_relative_residual <= 1.0e-10 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench diffusion-sorption benchmark failure: "
                  << error.what() << '\n';
        return 2;
    }
}
