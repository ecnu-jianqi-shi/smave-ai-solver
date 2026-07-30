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
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

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

std::vector<double> read_velocity(
    hid_t file, hsize_t sample, hsize_t time, hsize_t component,
    std::size_t source_width, std::size_t target_width) {
    Handle dataset(H5Dopen2(file, "/velocity", H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {sample, time, 0, 0, component};
    const hsize_t count[] = {1, 1, source_width, source_width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("NS velocity hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(5, count, nullptr), H5Sclose);
    std::vector<float> raw(source_width * source_width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("NS velocity read failed");
    }
    std::vector<double> sampled(target_width * target_width);
    for (std::size_t row = 0; row < target_width; ++row) {
        const auto source_row = row * source_width / target_width;
        for (std::size_t column = 0; column < target_width; ++column) {
            const auto source_column = column * source_width / target_width;
            sampled[row * target_width + column] =
                raw[source_row * source_width + source_column];
        }
    }
    return sampled;
}

struct HelmholtzOperator {
    std::size_t width;
    double diffusion_number;

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
                output[index] = (1.0 + 4.0 * diffusion_number) * input[index] -
                    diffusion_number * (input[row * width + west] +
                                        input[row * width + east] +
                                        input[south * width + column] +
                                        input[north * width + column]);
            }
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
        (1.0 + 4.0 * operation.diffusion_number);
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
        if (argc < 3 || argc > 6) {
            throw std::invalid_argument(
                "usage: smave_pdebench_ns_incompressible_benchmark FILE OUTPUT "
                "[LEARNED_OPERATOR [WORKERS [TIME_SLICES]]]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t source_width = 512;
        constexpr std::size_t width = 64;
        constexpr std::size_t samples = 2;
        std::size_t time_slices = 10;
        if (argc == 6) {
            time_slices = std::stoull(argv[5]);
            if (time_slices == 0 || time_slices > 1000) {
                throw std::invalid_argument("TIME_SLICES must be in [1, 1000]");
            }
        }
        constexpr std::size_t components = 2;
        constexpr double viscosity = 1.0e-3;
        constexpr double delta_time = 0.01;
        constexpr double domain_width = 1.0;
        const auto delta_x = domain_width / width;
        const auto diffusion_number = viscosity * delta_time / (delta_x * delta_x);
        std::optional<smave::LearnedPeriodicHelmholtzArtifact> learned_operator;
        if (argc >= 4) {
            learned_operator =
                smave::LearnedPeriodicHelmholtzArtifact::read(argv[3]);
            if (learned_operator->family != "ns-incompressible" ||
                learned_operator->discrete_operator_id !=
                    "periodic-viscous-helmholtz-v1" ||
                learned_operator->width != width ||
                learned_operator->training_maximum_relative_residual > 1.0e-10 ||
                learned_operator->heldout_maximum_relative_residual > 1.0e-10) {
                throw std::runtime_error(
                    "NS learned operator does not satisfy the deployment contract");
            }
        }
        const HelmholtzOperator operation{width, diffusion_number};
        const auto solves = samples * time_slices * components;
        const auto hardware_workers = std::max(1U, std::thread::hardware_concurrency());
        std::size_t requested_workers = hardware_workers;
        if (argc >= 5) {
            requested_workers = std::stoull(argv[4]);
            if (requested_workers == 0) {
                throw std::invalid_argument("WORKERS must be positive");
            }
        }
        const auto worker_count = std::min<std::size_t>(solves, requested_workers);
        const auto plane = width * width;
        std::vector<std::vector<double>> right_hand_sides;
        right_hand_sides.reserve(solves);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            for (std::size_t time = 0; time < time_slices; ++time) {
                for (std::size_t component = 0; component < components; ++component) {
                    right_hand_sides.push_back(read_velocity(
                        file.get(), sample, time, component, source_width, width));
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
        std::vector<std::vector<double>> traditional_solutions(solves);
        std::vector<double> packed_right_hand_sides;
        packed_right_hand_sides.reserve(solves * plane);
        for (const auto& right_hand_side : right_hand_sides) {
            packed_right_hand_sides.insert(
                packed_right_hand_sides.end(),
                right_hand_side.begin(), right_hand_side.end());
        }
        std::vector<double> spectral_output;
        const auto run_classical = [&]() {
            std::vector<std::size_t> worker_iterations(worker_count);
            std::vector<unsigned char> worker_converged(worker_count, 1);
            std::vector<std::thread> classical_workers;
            classical_workers.reserve(worker_count);
            const auto classical_started = std::chrono::steady_clock::now();
            for (std::size_t worker = 0; worker < worker_count; ++worker) {
                classical_workers.emplace_back([&, worker] {
                    for (std::size_t solve = worker; solve < solves;
                         solve += worker_count) {
                        auto traditional = classical_pcg(
                            operation, right_hand_sides[solve], 500, 1.0e-10);
                        worker_converged[worker] = static_cast<unsigned char>(
                            worker_converged[worker] != 0 && traditional.converged);
                        worker_iterations[worker] += traditional.iterations;
                        traditional_solutions[solve] = std::move(traditional.solution);
                    }
                });
            }
            for (auto& worker : classical_workers) worker.join();
            classical_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - classical_started).count();
            if (!std::all_of(worker_converged.begin(), worker_converged.end(),
                             [](unsigned char value) { return value != 0; })) {
                throw std::runtime_error("classical NS Helmholtz PCG failed");
            }
            classical_iterations = std::accumulate(
                worker_iterations.begin(), worker_iterations.end(), std::size_t{});
        };
        const auto run_smave = [&]() {
            smave::AcceleratePeriodicHelmholtz2DPlan spectral_plan(
                width, learned_operator
                    ? learned_operator->stencil_number : diffusion_number);
            double spectral_kernel_us{};
            if (!spectral_plan.available() || !spectral_plan.solve_batch(
                    packed_right_hand_sides, solves, spectral_output,
                    &spectral_kernel_us)) {
                throw std::runtime_error(
                    "SMAVE persistent Accelerate spectral Helmholtz failed: " +
                    spectral_plan.reason());
            }
            smave_setup_seconds = spectral_plan.setup_us() * 1.0e-6;
            smave_kernel_seconds = spectral_kernel_us * 1.0e-6;
            const auto gate_started = std::chrono::steady_clock::now();
            std::vector<double> worker_residuals(worker_count);
            const auto gate_worker = [&](std::size_t worker) {
                double worker_residual{};
                for (std::size_t solve = worker; solve < solves;
                     solve += worker_count) {
                    const auto offset = solve * plane;
                    double residual_norm_squared{};
                    double right_hand_side_norm_squared{};
                    for (std::size_t row = 0; row < width; ++row) {
                        const auto south = row == 0 ? width - 1 : row - 1;
                        const auto north = row + 1 == width ? 0 : row + 1;
                        for (std::size_t column = 0; column < width; ++column) {
                            const auto west = column == 0 ? width - 1 : column - 1;
                            const auto east = column + 1 == width ? 0 : column + 1;
                            const auto index = row * width + column;
                            const auto solution = spectral_output[offset + index];
                            const auto product =
                                (1.0 + 4.0 * diffusion_number) * solution -
                                diffusion_number * (
                                    spectral_output[offset + row * width + west] +
                                    spectral_output[offset + row * width + east] +
                                    spectral_output[offset + south * width + column] +
                                    spectral_output[offset + north * width + column]);
                            const auto residual =
                                right_hand_sides[solve][index] - product;
                            residual_norm_squared += residual * residual;
                            right_hand_side_norm_squared +=
                                right_hand_sides[solve][index] *
                                right_hand_sides[solve][index];
                        }
                    }
                    worker_residual = std::max(
                        worker_residual,
                        std::sqrt(residual_norm_squared) /
                            std::max(1.0, std::sqrt(right_hand_side_norm_squared)));
                }
                worker_residuals[worker] = worker_residual;
            };
#if defined(__APPLE__)
            struct GateContext {
                const decltype(gate_worker)* worker;
            } gate_context{&gate_worker};
            dispatch_apply_f(
                worker_count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                &gate_context, [](void* raw_context, std::size_t worker) {
                    const auto& context = *static_cast<GateContext*>(raw_context);
                    (*context.worker)(worker);
                });
#else
            std::vector<std::thread> gate_workers;
            gate_workers.reserve(worker_count);
            for (std::size_t worker = 0; worker < worker_count; ++worker) {
                gate_workers.emplace_back(gate_worker, worker);
            }
            for (auto& worker : gate_workers) worker.join();
#endif
            maximum_smave_relative_residual = *std::max_element(
                worker_residuals.begin(), worker_residuals.end());
            smave_gate_seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - gate_started).count();
        };
        smave::test::run_in_benchmark_solver_order(
            solver_order, run_classical, run_smave);
        const auto cross_started = std::chrono::steady_clock::now();
        for (std::size_t solve = 0; solve < solves; ++solve) {
            const auto offset = solve * plane;
            double cross_numerator{};
            double cross_denominator{1.0};
            for (std::size_t index = 0; index < plane; ++index) {
                cross_numerator = std::max(
                    cross_numerator,
                    std::abs(spectral_output[offset + index] -
                             traditional_solutions[solve][index]));
                cross_denominator = std::max(
                    cross_denominator,
                    std::abs(traditional_solutions[solve][index]));
            }
            maximum_cross_error = std::max(
                maximum_cross_error, cross_numerator / cross_denominator);
        }
        smave_gate_seconds += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - cross_started).count();
        const auto smave_seconds =
            smave_setup_seconds + smave_kernel_seconds + smave_gate_seconds;
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_NS_INCOMPRESSIBLE 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SOURCE_SHAPE 4 1000 512 512 2\n"
               << "MODEL \"implicit-viscous-velocity-helmholtz-subsystem\"\n"
               << "DISCRETIZATION_GRID " << width << ' ' << width << "\n"
               << "SAMPLES " << samples << "\n"
               << "TIME_SLICES " << time_slices << "\n"
               << "VELOCITY_COMPONENTS " << components << "\n"
               << "SOLVES " << solves << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "REQUESTED_WORKERS " << requested_workers << "\n"
               << "PARALLEL_WORKERS " << worker_count << "\n"
               << "SMAVE_GATE_WORKERS " << worker_count << "\n"
               << "CLASSICAL_PCG_TOTAL_ITERATIONS " << classical_iterations << "\n"
               << "LEARNED_OPERATOR_USED " << learned_operator.has_value() << "\n"
               << "LEARNED_STENCIL_NUMBER "
               << (learned_operator ? learned_operator->stencil_number : 0.0)
               << "\n"
               << "SMAVE_BACKEND \"accelerate-vdsp-real-persistent-parallel-periodic-helmholtz-2d-fp64-v4\"\n"
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
        std::cout << "PDEBench incompressible NS solves=" << solves
                  << " cross_error=" << maximum_cross_error << '\n';
        return maximum_cross_error <= 1.0e-7 &&
            maximum_smave_relative_residual <= 1.0e-10 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench incompressible NS benchmark failure: "
                  << error.what() << '\n';
        return 2;
    }
}
