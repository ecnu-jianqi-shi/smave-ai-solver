#include "smave/linear.hpp"
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

std::vector<double> read_field(
    hid_t file, const std::string& field, hsize_t sample, hsize_t time,
    std::size_t width) {
    Handle dataset(H5Dopen2(file, ("/" + field).c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {sample, time, 0};
    const hsize_t count[] = {1, 1, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("CFD field hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("CFD field read failed");
    }
    return {raw.begin(), raw.end()};
}

struct HelmholtzOperator {
    std::size_t width;
    double diffusion_number;

    bool operator()(const std::vector<double>& input, std::vector<double>& output) const {
        if (input.size() != width) return false;
        output.resize(width);
        for (std::size_t index = 0; index < width; ++index) {
            const auto left = index == 0 ? width - 1 : index - 1;
            const auto right = index + 1 == width ? 0 : index + 1;
            output[index] = (1.0 + 2.0 * diffusion_number) * input[index] -
                diffusion_number * (input[left] + input[right]);
        }
        return true;
    }
};

struct ClassicalResult {
    std::vector<double> solution;
    int iterations{};
    bool converged{};
};

ClassicalResult classical_pcg(
    const HelmholtzOperator& operation, const std::vector<double>& right_hand_side,
    int maximum_iterations, double tolerance) {
    ClassicalResult result;
    result.solution.assign(right_hand_side.size(), 0.0);
    auto residual = right_hand_side;
    const auto inverse_diagonal = 1.0 /
        (1.0 + 2.0 * operation.diffusion_number);
    auto preconditioned = residual;
    for (auto& value : preconditioned) value *= inverse_diagonal;
    auto direction = preconditioned;
    auto residual_preconditioned = dot(residual, preconditioned);
    const auto initial_norm = std::sqrt(dot(residual, residual));
    for (int iteration = 0; iteration < maximum_iterations; ++iteration) {
        std::vector<double> product;
        operation(direction, product);
        const auto curvature = dot(direction, product);
        if (!(curvature > 0.0)) return result;
        const auto alpha = residual_preconditioned / curvature;
        for (std::size_t index = 0; index < residual.size(); ++index) {
            result.solution[index] += alpha * direction[index];
            residual[index] -= alpha * product[index];
        }
        result.iterations = iteration + 1;
        if (std::sqrt(dot(residual, residual)) <=
            tolerance * std::max(1.0, initial_norm)) {
            result.converged = true;
            return result;
        }
        for (std::size_t index = 0; index < residual.size(); ++index) {
            preconditioned[index] = inverse_diagonal * residual[index];
        }
        const auto next = dot(residual, preconditioned);
        const auto beta = next / residual_preconditioned;
        for (std::size_t index = 0; index < direction.size(); ++index) {
            direction[index] = preconditioned[index] + beta * direction[index];
        }
        residual_preconditioned = next;
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::invalid_argument(
                "usage: smave_pdebench_cfd_1d_benchmark FILE OUTPUT");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t width = 1024;
        constexpr std::size_t samples = 3;
        constexpr std::size_t time_slices = 10;
        const std::vector<std::string> fields = {"density", "Vx", "pressure"};
        constexpr double dissipation = 0.01;
        constexpr double delta_time = 0.01;
        constexpr double domain_width = 1.0;
        const auto delta_x = domain_width / width;
        const auto diffusion_number = dissipation * delta_time / (delta_x * delta_x);
        const HelmholtzOperator operation{width, diffusion_number};
        const auto solves = samples * time_slices * fields.size();
        std::vector<std::vector<double>> right_hand_sides;
        right_hand_sides.reserve(solves);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            for (std::size_t time = 0; time < time_slices; ++time) {
                for (const auto& field : fields) {
                    right_hand_sides.push_back(read_field(
                        file.get(), field, sample, time, width));
                }
            }
        }
        double maximum_cross_error{};
        double maximum_smave_relative_residual{};
        double classical_seconds{};
        double smave_setup_seconds{};
        double smave_kernel_seconds{};
        double smave_gate_seconds{};
        std::size_t classical_iterations{};
        const auto started = std::chrono::steady_clock::now();
        std::vector<std::vector<double>> traditional_solutions;
        traditional_solutions.reserve(solves);
        std::vector<double> packed_right_hand_sides;
        packed_right_hand_sides.reserve(solves * width);
        for (const auto& right_hand_side : right_hand_sides) {
            packed_right_hand_sides.insert(
                packed_right_hand_sides.end(),
                right_hand_side.begin(), right_hand_side.end());
        }
        std::vector<double> packed_solutions;
        const auto run_classical = [&]() {
            for (const auto& right_hand_side : right_hand_sides) {
                const auto classical_started = std::chrono::steady_clock::now();
                auto traditional = classical_pcg(
                    operation, right_hand_side, 1000, 1.0e-10);
                classical_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - classical_started).count();
                if (!traditional.converged) {
                    throw std::runtime_error("classical CFD Helmholtz PCG failed");
                }
                classical_iterations += traditional.iterations;
                traditional_solutions.push_back(std::move(traditional.solution));
            }
        };
        const auto run_smave = [&]() {
            const auto setup_started = std::chrono::steady_clock::now();
            const smave::PeriodicTridiagonalFactorization factorization(
                width, 1.0 + 2.0 * diffusion_number, -diffusion_number);
            smave_setup_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - setup_started).count();
            if (!factorization.valid()) {
                throw std::runtime_error("SMAVE periodic tridiagonal setup failed");
            }
            const auto kernel_started = std::chrono::steady_clock::now();
            if (!factorization.solve_batch(
                    packed_right_hand_sides, solves, packed_solutions)) {
                throw std::runtime_error(
                    "SMAVE periodic tridiagonal batch solve failed");
            }
            smave_kernel_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - kernel_started).count();

            const auto gate_started = std::chrono::steady_clock::now();
            for (std::size_t solve = 0; solve < solves; ++solve) {
                const auto offset = solve * width;
                double residual_norm_squared{};
                double right_hand_side_norm_squared{};
                for (std::size_t index = 0; index < width; ++index) {
                    const auto left = index == 0 ? width - 1 : index - 1;
                    const auto right = index + 1 == width ? 0 : index + 1;
                    const auto solution = packed_solutions[offset + index];
                    const auto product =
                        (1.0 + 2.0 * diffusion_number) * solution -
                        diffusion_number * (packed_solutions[offset + left] +
                                            packed_solutions[offset + right]);
                    const auto residual = right_hand_sides[solve][index] - product;
                    residual_norm_squared += residual * residual;
                    right_hand_side_norm_squared +=
                        right_hand_sides[solve][index] *
                        right_hand_sides[solve][index];
                }
                const auto relative_residual = std::sqrt(residual_norm_squared) /
                    std::max(1.0, std::sqrt(right_hand_side_norm_squared));
                maximum_smave_relative_residual = std::max(
                    maximum_smave_relative_residual, relative_residual);
            }
            smave_gate_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - gate_started).count();
        };
        if (solver_order == smave::test::BenchmarkSolverOrder::smave_first) {
            run_smave();
            run_classical();
        } else {
            run_classical();
            run_smave();
        }
        for (std::size_t solve = 0; solve < solves; ++solve) {
            const auto offset = solve * width;
            double cross_numerator{};
            double cross_denominator{1.0};
            for (std::size_t index = 0; index < width; ++index) {
                const auto solution = packed_solutions[offset + index];
                cross_numerator = std::max(
                    cross_numerator,
                    std::abs(solution - traditional_solutions[solve][index]));
                cross_denominator = std::max(
                    cross_denominator,
                    std::abs(traditional_solutions[solve][index]));
            }
            maximum_cross_error = std::max(
                maximum_cross_error, cross_numerator / cross_denominator);
        }
        const auto smave_seconds =
            smave_setup_seconds + smave_kernel_seconds + smave_gate_seconds;
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_CFD_1D 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SOURCE_SHAPE 10000 101 1024\n"
               << "FIELDS \"density,Vx,pressure\"\n"
               << "MODEL \"implicit-dissipative-three-field-helmholtz-subsystem\"\n"
               << "DISCRETIZATION_GRID " << width << "\n"
               << "SAMPLES " << samples << "\n"
               << "TIME_SLICES " << time_slices << "\n"
               << "SOLVES " << solves << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "CLASSICAL_PCG_TOTAL_ITERATIONS " << classical_iterations << "\n"
               << "SMAVE_BACKEND \"periodic-tridiagonal-factor-reuse-batch-cpu-v1\"\n"
               << "SMAVE_SETUP_SECONDS " << smave_setup_seconds << "\n"
               << "SMAVE_KERNEL_SECONDS " << smave_kernel_seconds << "\n"
               << "SMAVE_GATE_SECONDS " << smave_gate_seconds << "\n"
               << "CLASSICAL_SOLVE_SECONDS " << classical_seconds << "\n"
               << "SMAVE_SOLVE_SECONDS " << smave_seconds << "\n"
               << "CLASSICAL_MEAN_SOLVE_US " << classical_seconds * 1.0e6 / solves << "\n"
               << "SMAVE_MEAN_SOLVE_US " << smave_seconds * 1.0e6 / solves << "\n"
               << "SMAVE_VS_CLASSICAL_SPEEDUP " << classical_seconds / smave_seconds << "\n"
               << "CROSS_SOLVER_RELATIVE_INF_ERROR " << maximum_cross_error << "\n"
               << "SMAVE_MAXIMUM_RELATIVE_RESIDUAL "
               << maximum_smave_relative_residual << "\n"
               << "ELAPSED_SECONDS " << elapsed << "\n"
               << "CROSS_SOLVER_AGREEMENT "
               << (maximum_cross_error <= 1.0e-7 &&
                   maximum_smave_relative_residual <= 1.0e-10) << "\n"
               << "END\n";
        std::cout << "PDEBench 1D CFD solves=" << solves
                  << " cross_error=" << maximum_cross_error << '\n';
        return maximum_cross_error <= 1.0e-7 &&
            maximum_smave_relative_residual <= 1.0e-10 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench 1D CFD benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
