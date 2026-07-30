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
    if (left.size() != right.size()) throw std::invalid_argument("error shape mismatch");
    double numerator{};
    double denominator{1.0};
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator = std::max(numerator, std::abs(left[index] - right[index]));
        denominator = std::max(denominator, std::abs(right[index]));
    }
    return numerator / denominator;
}

std::vector<double> read_sample(
    hid_t dataset, hsize_t sample, hsize_t channels,
    hsize_t height, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, 0, 0, 0};
    const hsize_t count[] = {1, channels, height, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(4, count, nullptr), H5Sclose);
    std::vector<float> raw(channels * height * width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 tensor read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> read_coefficient(
    hid_t dataset, hsize_t sample, hsize_t height, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, 0, 0};
    const hsize_t count[] = {1, height, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 coefficient selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(height * width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 coefficient read failed");
    }
    return {raw.begin(), raw.end()};
}

double harmonic(double left, double right) {
    return 2.0 * left * right / (left + right);
}

std::vector<double> downsample_square(
    const std::vector<double>& field,
    std::size_t source_width,
    std::size_t target_width);


struct DarcyOperator {
    std::size_t width;
    double inverse_spacing_squared;
    std::vector<double> west;
    std::vector<double> east;
    std::vector<double> south;
    std::vector<double> north;
    std::vector<double> inverse_diagonal;

    DarcyOperator(
        const std::vector<double>& coefficient,
        std::size_t grid_width,
        double spacing_scale)
        : width(grid_width), inverse_spacing_squared(spacing_scale) {
        const auto interior = width - 2;
        const auto size = interior * interior;
        west.resize(size);
        east.resize(size);
        south.resize(size);
        north.resize(size);
        inverse_diagonal.resize(size);
        for (std::size_t row = 1; row + 1 < width; ++row) {
            for (std::size_t column = 1; column + 1 < width; ++column) {
                const auto full = row * width + column;
                const auto local = (row - 1) * interior + column - 1;
                const auto center = coefficient[full];
                west[local] = harmonic(center, coefficient[full - 1]) * spacing_scale;
                east[local] = harmonic(center, coefficient[full + 1]) * spacing_scale;
                south[local] = harmonic(center, coefficient[full - width]) * spacing_scale;
                north[local] = harmonic(center, coefficient[full + width]) * spacing_scale;
                inverse_diagonal[local] = 1.0 /
                    (west[local] + east[local] + south[local] + north[local]);
            }
        }
    }

    bool operator()(const std::vector<double>& input, std::vector<double>& output) const {
        const auto interior = width - 2;
        if (input.size() != interior * interior) return false;
        output.resize(input.size());
        for (std::size_t row = 1; row + 1 < width; ++row) {
            for (std::size_t column = 1; column + 1 < width; ++column) {
                const auto local = (row - 1) * interior + column - 1;
                double value = input[local] / inverse_diagonal[local];
                if (column > 1) value -= west[local] * input[local - 1];
                if (column + 2 < width) value -= east[local] * input[local + 1];
                if (row > 1) value -= south[local] * input[local - interior];
                if (row + 2 < width) value -= north[local] * input[local + interior];
                output[local] = value;
            }
        }
        return true;
    }

    [[nodiscard]] std::vector<double> diagonal_inverse() const {
        return inverse_diagonal;
    }

    [[nodiscard]] smave::LinearSystem linear_system(
        const std::vector<double>& right_hand_side) const {
        const auto interior = width - 2;
        const auto size = interior * interior;
        if (right_hand_side.size() != size) {
            throw std::invalid_argument("Darcy right-hand side shape mismatch");
        }
        smave::LinearSystem system;
        system.sparsity.row_count = size;
        system.sparsity.column_count = size;
        system.sparsity.row_offsets.reserve(size + 1);
        system.sparsity.column_indices.reserve(5 * size);
        system.sparse_values.reserve(5 * size);
        system.sparsity.row_offsets.push_back(0);
        for (std::size_t row = 0; row < interior; ++row) {
            for (std::size_t column = 0; column < interior; ++column) {
                const auto index = row * interior + column;
                if (row > 0) {
                    system.sparsity.column_indices.push_back(index - interior);
                    system.sparse_values.push_back(-south[index]);
                }
                if (column > 0) {
                    system.sparsity.column_indices.push_back(index - 1);
                    system.sparse_values.push_back(-west[index]);
                }
                system.sparsity.column_indices.push_back(index);
                system.sparse_values.push_back(1.0 / inverse_diagonal[index]);
                if (column + 1 < interior) {
                    system.sparsity.column_indices.push_back(index + 1);
                    system.sparse_values.push_back(-east[index]);
                }
                if (row + 1 < interior) {
                    system.sparsity.column_indices.push_back(index + interior);
                    system.sparse_values.push_back(-north[index]);
                }
                system.sparsity.row_offsets.push_back(
                    system.sparsity.column_indices.size());
            }
        }
        system.right_hand_side = right_hand_side;
        system.symmetric = true;
        system.positive_definite = true;
        return system;
    }
};

smave::Preconditioner ssor_preconditioner(
    const DarcyOperator& operation,
    double relaxation) {
    const auto interior = operation.width - 2;
    return [&operation, interior, relaxation,
            forward = std::vector<double>(operation.inverse_diagonal.size())](
               const std::vector<double>& residual,
               std::vector<double>& output) mutable {
        if (residual.size() != operation.inverse_diagonal.size()) return false;
        for (std::size_t row = 0; row < interior; ++row) {
            for (std::size_t column = 0; column < interior; ++column) {
                const auto index = row * interior + column;
                auto value = residual[index];
                if (column > 0) value += operation.west[index] * forward[index - 1];
                if (row > 0) value += operation.south[index] * forward[index - interior];
                forward[index] = relaxation * operation.inverse_diagonal[index] * value;
            }
        }
        output.resize(residual.size());
        const auto diagonal_scale = (2.0 - relaxation) / relaxation;
        for (std::size_t reverse_row = 0; reverse_row < interior; ++reverse_row) {
            const auto row = interior - reverse_row - 1;
            for (std::size_t reverse_column = 0; reverse_column < interior;
                 ++reverse_column) {
                const auto column = interior - reverse_column - 1;
                const auto index = row * interior + column;
                auto value = diagonal_scale *
                    forward[index] / operation.inverse_diagonal[index];
                if (column + 1 < interior) {
                    value += operation.east[index] * output[index + 1];
                }
                if (row + 1 < interior) {
                    value += operation.north[index] * output[index + interior];
                }
                output[index] = relaxation * operation.inverse_diagonal[index] * value;
            }
        }
        return true;
    };
}

smave::Preconditioner additive_line_preconditioner(
    const DarcyOperator& operation) {
    const auto width = operation.width - 2;
    return [&operation, width,
            row_workspace = smave::VariableTridiagonalWorkspace(width),
            column_workspace = smave::VariableTridiagonalWorkspace(width),
            lower = std::vector<double>(width),
            diagonal = std::vector<double>(width),
            upper = std::vector<double>(width),
            line_right = std::vector<double>(width),
            line_solution = std::vector<double>(width)](
               const std::vector<double>& residual,
               std::vector<double>& output) mutable {
        if (residual.size() != width * width) return false;
        output.assign(residual.size(), 0.0);
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                const auto index = row * width + column;
                lower[column] = column > 0 ? -operation.west[index] : 0.0;
                diagonal[column] = 1.0 / operation.inverse_diagonal[index];
                upper[column] =
                    column + 1 < width ? -operation.east[index] : 0.0;
                line_right[column] = residual[index];
            }
            if (!row_workspace.solve(
                    lower, diagonal, upper, line_right, line_solution)) return false;
            for (std::size_t column = 0; column < width; ++column) {
                output[row * width + column] += 0.5 * line_solution[column];
            }
        }
        for (std::size_t column = 0; column < width; ++column) {
            for (std::size_t row = 0; row < width; ++row) {
                const auto index = row * width + column;
                lower[row] = row > 0 ? -operation.south[index] : 0.0;
                diagonal[row] = 1.0 / operation.inverse_diagonal[index];
                upper[row] = row + 1 < width ? -operation.north[index] : 0.0;
                line_right[row] = residual[index];
            }
            if (!column_workspace.solve(
                    lower, diagonal, upper, line_right, line_solution)) return false;
            for (std::size_t row = 0; row < width; ++row) {
                output[row * width + column] += 0.5 * line_solution[row];
            }
        }
        return std::all_of(output.begin(), output.end(), [](double value) {
            return std::isfinite(value);
        });
    };
}

double fit_ssor_relaxation(
    hid_t coefficient_dataset,
    std::size_t source_width,
    std::size_t width,
    double inverse_spacing_squared,
    hsize_t total_samples) {
    const std::vector<double> candidates = {
        0.8, 1.0, 1.2, 1.4, 1.6, 1.7, 1.8, 1.9};
    const auto training_samples = std::min<hsize_t>(3, total_samples - 3);
    double best = candidates.front();
    double best_score = std::numeric_limits<double>::infinity();
    const auto unknowns = (width - 2) * (width - 2);
    const std::vector<double> right_hand_side(unknowns, 1.0);
    const std::vector<double> initial(unknowns);
    for (const auto candidate : candidates) {
        double score{};
        for (hsize_t offset = 0; offset < training_samples; ++offset) {
            const auto sample = total_samples - 1 - offset;
            const auto coefficient = downsample_square(read_coefficient(
                coefficient_dataset, sample, source_width, source_width),
                source_width, width);
            const DarcyOperator operation(
                coefficient, width, inverse_spacing_squared);
            const auto preconditioner = ssor_preconditioner(operation, candidate);
            const auto result = smave::preconditioned_conjugate_gradient(
                unknowns, operation, right_hand_side, initial,
                preconditioner, 1.0e-12, 1.0e-8, 2000);
            if (!result.converged) {
                score = std::numeric_limits<double>::infinity();
                break;
            }
            score += static_cast<double>(result.iterations);
        }
        if (score < best_score) {
            best_score = score;
            best = candidate;
        }
    }
    return best;
}

struct ClassicalCgResult {
    std::vector<double> solution;
    int iterations{};
    bool converged{};
};

ClassicalCgResult classical_conjugate_gradient(
    const DarcyOperator& operation, const std::vector<double>& right_hand_side,
    const std::vector<double>& diagonal_inverse,
    int maximum_iterations, double tolerance) {
    ClassicalCgResult result;
    result.solution.assign(right_hand_side.size(), 0.0);
    auto residual = right_hand_side;
    std::vector<double> preconditioned(residual.size());
    for (std::size_t index = 0; index < residual.size(); ++index) {
        preconditioned[index] = diagonal_inverse[index] * residual[index];
    }
    auto direction = preconditioned;
    const auto initial_norm = std::sqrt(dot(residual, residual));
    double residual_preconditioned = dot(residual, preconditioned);
    for (int iteration = 0; iteration < maximum_iterations; ++iteration) {
        std::vector<double> product;
        if (!operation(direction, product)) return result;
        const auto denominator = dot(direction, product);
        if (!(denominator > 0.0) || !std::isfinite(denominator)) return result;
        const auto alpha = residual_preconditioned / denominator;
        for (std::size_t index = 0; index < result.solution.size(); ++index) {
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
            preconditioned[index] = diagonal_inverse[index] * residual[index];
        }
        const auto next_residual_preconditioned = dot(residual, preconditioned);
        const auto beta = next_residual_preconditioned / residual_preconditioned;
        for (std::size_t index = 0; index < direction.size(); ++index) {
            direction[index] = preconditioned[index] + beta * direction[index];
        }
        residual_preconditioned = next_residual_preconditioned;
    }
    return result;
}

std::vector<double> interior(const std::vector<double>& field, std::size_t width) {
    std::vector<double> values;
    values.reserve((width - 2) * (width - 2));
    for (std::size_t row = 1; row + 1 < width; ++row) {
        values.insert(values.end(), field.begin() + row * width + 1,
                      field.begin() + (row + 1) * width - 1);
    }
    return values;
}

std::vector<double> downsample_square(
    const std::vector<double>& field, std::size_t source_width,
    std::size_t target_width) {
    std::vector<double> sampled(target_width * target_width);
    for (std::size_t row = 0; row < target_width; ++row) {
        const auto source_row = row * (source_width - 1) / (target_width - 1);
        for (std::size_t column = 0; column < target_width; ++column) {
            const auto source_column = column * (source_width - 1) / (target_width - 1);
            sampled[row * target_width + column] =
                field[source_row * source_width + source_column];
        }
    }
    return sampled;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_darcy_benchmark FILE OUTPUT "
                "[LEARNED_OPERATOR_PREFIX]");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        const auto solver_order = smave::test::benchmark_solver_order();
        std::optional<smave::LearnedDarcyNearestArtifact> learned_operator;
        if (argc == 4) {
            learned_operator = smave::LearnedDarcyNearestArtifact::read(argv[3]);
        }
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle coefficient_dataset(H5Dopen2(file.get(), "nu", H5P_DEFAULT), H5Dclose);
        Handle solution_dataset(H5Dopen2(file.get(), "tensor", H5P_DEFAULT), H5Dclose);
        Handle solution_space(H5Dget_space(solution_dataset.get()), H5Sclose);
        hsize_t dimensions[4]{};
        if (H5Sget_simple_extent_dims(solution_space.get(), dimensions, nullptr) != 4 ||
            dimensions[1] != 1 || dimensions[2] != dimensions[3] || dimensions[2] < 3) {
            throw std::runtime_error("unexpected PDEBench DarcyFlow tensor shape");
        }
        const auto source_width = static_cast<std::size_t>(dimensions[2]);
        const std::size_t width = 32;
        if (learned_operator && learned_operator->width != width) {
            throw std::invalid_argument("learned Darcy operator width mismatch");
        }
        const auto unknowns = (width - 2) * (width - 2);
        const auto samples = std::min<hsize_t>(3, dimensions[0]);
        const auto spacing = 1.0 / static_cast<double>(width - 1);
        const std::vector<double> right_hand_side(unknowns, 1.0);
        const auto training_started = std::chrono::steady_clock::now();
        const auto learned_relaxation = fit_ssor_relaxation(
            coefficient_dataset.get(), source_width, width,
            1.0 / (spacing * spacing), dimensions[0]);
        const auto training_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - training_started).count();
        double maximum_cross_error{};
        double maximum_data_error{};
        std::size_t smave_iterations{};
        std::size_t classical_iterations{};
        std::size_t multigrid_iterations{};
        std::size_t multigrid_levels{};
        std::size_t multigrid_storage_bytes{};
        double classical_solve_seconds{};
        double smave_solve_seconds{};
        double multigrid_setup_seconds{};
        double multigrid_solve_seconds{};
        double incomplete_cholesky_setup_seconds{};
        double incomplete_cholesky_solve_seconds{};
        double geometric_multigrid_setup_seconds{};
        double geometric_multigrid_solve_seconds{};
        std::size_t geometric_multigrid_iterations{};
        std::size_t geometric_multigrid_levels{};
        std::size_t geometric_multigrid_storage_bytes{};
        double maximum_geometric_multigrid_cross_error{};
        bool geometric_multigrid_agreement = true;
        double geometric_warmstart_solve_seconds{};
        std::size_t geometric_warmstart_iterations{};
        double maximum_geometric_warmstart_cross_error{};
        bool geometric_warmstart_agreement = true;
        double additive_line_solve_seconds{};
        std::size_t additive_line_iterations{};
        double maximum_additive_line_cross_error{};
        bool additive_line_agreement = true;
        std::size_t incomplete_cholesky_iterations{};
        double maximum_incomplete_cholesky_cross_error{};
        bool incomplete_cholesky_agreement = true;
        double maximum_multigrid_cross_error{};
        bool multigrid_agreement = true;
        std::string accelerate_cholesky_backend = "unavailable";
        std::string accelerate_cholesky_reason;
        bool accelerate_cholesky_available = false;
        bool accelerate_cholesky_solved = true;
        bool accelerate_cholesky_agreement = true;
        std::size_t accelerate_cholesky_nonzeros{};
        double accelerate_cholesky_full_seconds{};
        double accelerate_cholesky_factor_seconds{};
        double accelerate_cholesky_solve_seconds{};
        double accelerate_cholesky_gate_seconds{};
        double maximum_accelerate_cholesky_residual{};
        double maximum_accelerate_cholesky_cross_error{};
        std::string accelerate_band_backend = "unavailable";
        std::string accelerate_band_reason;
        bool accelerate_band_available = false;
        bool accelerate_band_solved = true;
        bool accelerate_band_agreement = true;
        double accelerate_band_full_seconds{};
        double accelerate_band_factor_seconds{};
        double accelerate_band_solve_seconds{};
        double accelerate_band_gate_seconds{};
        double maximum_accelerate_band_residual{};
        double maximum_accelerate_band_cross_error{};
        std::vector<double> metal_west;
        std::vector<double> metal_east;
        std::vector<double> metal_south;
        std::vector<double> metal_north;
        std::vector<double> metal_inverse_diagonal;
        std::vector<double> metal_right_hand_sides;
        std::vector<double> classical_solutions;
        double learned_warmstart_full_seconds{};
        double learned_warmstart_inference_seconds{};
        double learned_warmstart_solve_seconds{};
        std::size_t learned_warmstart_iterations{};
        double maximum_learned_warmstart_cross_error{};
        double maximum_learned_candidate_residual{};
        bool learned_warmstart_agreement = true;
        metal_west.reserve(samples * unknowns);
        metal_east.reserve(samples * unknowns);
        metal_south.reserve(samples * unknowns);
        metal_north.reserve(samples * unknowns);
        metal_inverse_diagonal.reserve(samples * unknowns);
        metal_right_hand_sides.reserve(samples * unknowns);
        classical_solutions.reserve(samples * unknowns);
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t sample = 0; sample < samples; ++sample) {
            const auto coefficient = downsample_square(read_coefficient(
                coefficient_dataset.get(), sample, dimensions[2], dimensions[3]),
                source_width, width);
            const DarcyOperator operation{coefficient, width, 1.0 / (spacing * spacing)};
            std::optional<smave::KrylovResult> learned_warmstart_result;
            if (learned_operator) {
                const auto full_started = std::chrono::steady_clock::now();
                const auto inference_started = std::chrono::steady_clock::now();
                const auto learned_full = learned_operator->predict(coefficient);
                learned_warmstart_inference_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - inference_started).count();
                const auto learned_initial = interior(learned_full, width);
                std::vector<double> product;
                if (!operation(learned_initial, product)) {
                    throw std::runtime_error("Darcy learned candidate residual failed");
                }
                double residual_squared{};
                for (std::size_t index = 0; index < unknowns; ++index) {
                    const auto residual = right_hand_side[index] - product[index];
                    residual_squared += residual * residual;
                }
                maximum_learned_candidate_residual = std::max(
                    maximum_learned_candidate_residual,
                    std::sqrt(residual_squared / static_cast<double>(unknowns)));
                const auto solve_started = std::chrono::steady_clock::now();
                learned_warmstart_result = smave::preconditioned_conjugate_gradient(
                    unknowns, operation, right_hand_side, learned_initial,
                    ssor_preconditioner(operation, learned_relaxation),
                    1.0e-12, 1.0e-8, 2000);
                learned_warmstart_solve_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - solve_started).count();
                learned_warmstart_full_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - full_started).count();
                if (!learned_warmstart_result->converged) {
                    throw std::runtime_error("Darcy learned warm-start PCG failed");
                }
                learned_warmstart_iterations +=
                    static_cast<std::size_t>(learned_warmstart_result->iterations);
            }
            metal_west.insert(
                metal_west.end(), operation.west.begin(), operation.west.end());
            metal_east.insert(
                metal_east.end(), operation.east.begin(), operation.east.end());
            metal_south.insert(
                metal_south.end(), operation.south.begin(), operation.south.end());
            metal_north.insert(
                metal_north.end(), operation.north.begin(), operation.north.end());
            metal_inverse_diagonal.insert(metal_inverse_diagonal.end(),
                operation.inverse_diagonal.begin(), operation.inverse_diagonal.end());
            metal_right_hand_sides.insert(metal_right_hand_sides.end(),
                right_hand_side.begin(), right_hand_side.end());
            const auto diagonal = operation.diagonal_inverse();
            const auto preconditioner =
                ssor_preconditioner(operation, learned_relaxation);
            smave::KrylovResult smave_result;
            ClassicalCgResult classical;
            int smave_sample_iterations{};
            const auto run_classical = [&]() {
                const auto classical_started = std::chrono::steady_clock::now();
                classical = classical_conjugate_gradient(
                    operation, right_hand_side, diagonal, 2000, 1.0e-8);
                classical_solve_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - classical_started).count();
            };
            const auto run_smave = [&]() {
                const auto smave_started = std::chrono::steady_clock::now();
                std::vector<double> initial(unknowns);
                for (int attempt = 0; attempt < 8; ++attempt) {
                    smave_result = smave::preconditioned_conjugate_gradient(
                        unknowns, operation, right_hand_side, initial,
                        preconditioner, 1.0e-12, 1.0e-8, 2000);
                    smave_sample_iterations += smave_result.iterations;
                    if (smave_result.converged || smave_result.solution.empty()) break;
                    initial = smave_result.solution;
                }
                smave_solve_seconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - smave_started).count();
            };
            smave::test::run_in_benchmark_solver_order(
                solver_order, run_classical, run_smave);

            const auto multigrid_setup_started = std::chrono::steady_clock::now();
            const auto system = operation.linear_system(right_hand_side);
            const auto accelerate_cholesky_started =
                std::chrono::steady_clock::now();
            const auto accelerate_cholesky_result =
                smave::accelerate_sparse_spd_direct_solve(system);
            accelerate_cholesky_full_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                accelerate_cholesky_started).count();
            accelerate_cholesky_backend = accelerate_cholesky_result.backend;
            accelerate_cholesky_reason = accelerate_cholesky_result.reason;
            accelerate_cholesky_available = accelerate_cholesky_result.available;
            accelerate_cholesky_solved = accelerate_cholesky_solved &&
                accelerate_cholesky_result.solved;
            accelerate_cholesky_nonzeros = std::max(
                accelerate_cholesky_nonzeros,
                accelerate_cholesky_result.matrix_nonzeros);
            accelerate_cholesky_factor_seconds +=
                accelerate_cholesky_result.factor_seconds;
            accelerate_cholesky_solve_seconds +=
                accelerate_cholesky_result.solve_seconds;
            accelerate_cholesky_gate_seconds +=
                accelerate_cholesky_result.gate_seconds;
            maximum_accelerate_cholesky_residual = std::max(
                maximum_accelerate_cholesky_residual,
                accelerate_cholesky_result.residual_inf);
            const auto accelerate_band_started =
                std::chrono::steady_clock::now();
            std::vector<double> accelerate_band_diagonal(unknowns);
            for (std::size_t index = 0; index < unknowns; ++index) {
                accelerate_band_diagonal[index] =
                    1.0 / operation.inverse_diagonal[index];
            }
            const auto accelerate_band_result =
                smave::accelerate_five_point_spd_direct_solve(
                    width - 2, operation.west, operation.east,
                    operation.south, operation.north,
                    accelerate_band_diagonal, right_hand_side);
            accelerate_band_full_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                accelerate_band_started).count();
            accelerate_band_backend = accelerate_band_result.backend;
            accelerate_band_reason = accelerate_band_result.reason;
            accelerate_band_available = accelerate_band_result.available;
            accelerate_band_solved = accelerate_band_solved &&
                accelerate_band_result.solved;
            accelerate_band_factor_seconds += accelerate_band_result.factor_seconds;
            accelerate_band_solve_seconds += accelerate_band_result.solve_seconds;
            accelerate_band_gate_seconds += accelerate_band_result.gate_seconds;
            maximum_accelerate_band_residual = std::max(
                maximum_accelerate_band_residual,
                accelerate_band_result.residual_inf);
            const auto incomplete_cholesky_setup_started =
                std::chrono::steady_clock::now();
            const auto incomplete_cholesky_preconditioner =
                smave::incomplete_cholesky_zero_preconditioner(
                    system, system.sparsity);
            incomplete_cholesky_setup_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                incomplete_cholesky_setup_started).count();
            if (!incomplete_cholesky_preconditioner) {
                throw std::runtime_error("DarcyFlow IC(0) setup failed");
            }
            const auto incomplete_cholesky_started =
                std::chrono::steady_clock::now();
            const auto incomplete_cholesky_result =
                smave::preconditioned_conjugate_gradient(
                    system, std::vector<double>(unknowns),
                    incomplete_cholesky_preconditioner,
                    1.0e-12, 1.0e-8, 2000);
            incomplete_cholesky_solve_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                incomplete_cholesky_started).count();
            const auto geometric_multigrid_setup_started =
                std::chrono::steady_clock::now();
            std::vector<double> stencil_diagonal(unknowns);
            for (std::size_t index = 0; index < unknowns; ++index) {
                stencil_diagonal[index] = 1.0 / operation.inverse_diagonal[index];
            }
            smave::GeometricFivePointMultigrid2D geometric_multigrid(
                width - 2, operation.west, operation.east,
                operation.south, operation.north, stencil_diagonal,
                16, 1, 1, learned_relaxation);
            geometric_multigrid_setup_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                geometric_multigrid_setup_started).count();
            if (!geometric_multigrid.valid()) {
                throw std::runtime_error(
                    "DarcyFlow geometric multigrid setup failed: " +
                    geometric_multigrid.reason());
            }
            geometric_multigrid_levels = std::max(
                geometric_multigrid_levels, geometric_multigrid.levels());
            geometric_multigrid_storage_bytes = std::max(
                geometric_multigrid_storage_bytes,
                geometric_multigrid.storage_bytes());
            const smave::Preconditioner geometric_multigrid_preconditioner =
                [&geometric_multigrid](const std::vector<double>& residual,
                                       std::vector<double>& correction) {
                    return geometric_multigrid.apply(residual, correction);
                };
            const auto geometric_multigrid_started =
                std::chrono::steady_clock::now();
            const auto geometric_multigrid_result =
                smave::preconditioned_conjugate_gradient(
                    unknowns, operation, right_hand_side,
                    std::vector<double>(unknowns),
                    geometric_multigrid_preconditioner,
                    1.0e-12, 1.0e-8, 2000);
            geometric_multigrid_solve_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                geometric_multigrid_started).count();
            const auto geometric_warmstart_started =
                std::chrono::steady_clock::now();
            std::vector<double> geometric_initial;
            if (!geometric_multigrid.apply(
                    right_hand_side, geometric_initial)) {
                throw std::runtime_error(
                    "DarcyFlow geometric multigrid warm-start failed");
            }
            const auto geometric_warmstart_result =
                smave::preconditioned_conjugate_gradient(
                    unknowns, operation, right_hand_side,
                    geometric_initial, preconditioner,
                    1.0e-12, 1.0e-8, 2000);
            geometric_warmstart_solve_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                geometric_warmstart_started).count();
            const auto line_preconditioner = additive_line_preconditioner(operation);
            const auto additive_line_started = std::chrono::steady_clock::now();
            smave::KrylovResult additive_line_result;
            int additive_line_sample_iterations{};
            std::vector<double> additive_line_initial(unknowns);
            for (int attempt = 0; attempt < 8; ++attempt) {
                additive_line_result = smave::preconditioned_conjugate_gradient(
                    unknowns, operation, right_hand_side,
                    additive_line_initial, line_preconditioner,
                    1.0e-12, 1.0e-8, 2000);
                additive_line_sample_iterations += additive_line_result.iterations;
                if (additive_line_result.converged ||
                    additive_line_result.solution.empty()) break;
                additive_line_initial = additive_line_result.solution;
            }
            additive_line_solve_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                additive_line_started).count();
            smave::AggregationMultigrid2D multigrid(
                system, width - 2, 16, 1, 1, 2.0 / 3.0);
            multigrid_setup_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - multigrid_setup_started).count();
            if (!multigrid.valid()) {
                throw std::runtime_error(
                    "DarcyFlow multigrid setup failed: " + multigrid.reason());
            }
            multigrid_levels = std::max(multigrid_levels, multigrid.levels());
            multigrid_storage_bytes = std::max(
                multigrid_storage_bytes, multigrid.storage_bytes());
            const smave::Preconditioner multigrid_preconditioner =
                [&multigrid](const std::vector<double>& residual,
                             std::vector<double>& correction) {
                    return multigrid.apply(residual, correction);
                };
            const auto multigrid_started = std::chrono::steady_clock::now();
            const auto multigrid_result = smave::preconditioned_conjugate_gradient(
                unknowns, operation, right_hand_side, std::vector<double>(unknowns),
                multigrid_preconditioner, 1.0e-12, 1.0e-8, 2000);
            multigrid_solve_seconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - multigrid_started).count();
            if (!smave_result.converged || !classical.converged ||
                !multigrid_result.converged ||
                !incomplete_cholesky_result.converged ||
                !geometric_multigrid_result.converged ||
                !geometric_warmstart_result.converged ||
                !additive_line_result.converged) {
                throw std::runtime_error(
                    "DarcyFlow iterative solver did not converge: smave=" +
                    std::to_string(smave_result.converged) + " (" +
                    smave_result.reason + ", iterations=" +
                    std::to_string(smave_result.iterations) + "), classical=" +
                    std::to_string(classical.converged) + " (iterations=" +
                    std::to_string(classical.iterations) + "), multigrid=" +
                    std::to_string(multigrid_result.converged) + " (" +
                    multigrid_result.reason + ", iterations=" +
                    std::to_string(multigrid_result.iterations) + "), line=" +
                    std::to_string(additive_line_result.converged) + " (" +
                    additive_line_result.reason + ", iterations=" +
                    std::to_string(additive_line_result.iterations) + ")");
            }
            smave_iterations += static_cast<std::size_t>(smave_sample_iterations);
            classical_iterations += static_cast<std::size_t>(classical.iterations);
            classical_solutions.insert(classical_solutions.end(),
                classical.solution.begin(), classical.solution.end());
            multigrid_iterations +=
                static_cast<std::size_t>(multigrid_result.iterations);
            incomplete_cholesky_iterations += static_cast<std::size_t>(
                incomplete_cholesky_result.iterations);
            geometric_multigrid_iterations += static_cast<std::size_t>(
                geometric_multigrid_result.iterations);
            geometric_warmstart_iterations += static_cast<std::size_t>(
                geometric_warmstart_result.iterations);
            additive_line_iterations += static_cast<std::size_t>(
                additive_line_sample_iterations);
            maximum_cross_error = std::max(maximum_cross_error,
                relative_infinity_error(
                    accelerate_band_result.solution, classical.solution));
            if (learned_warmstart_result) {
                const auto error = relative_infinity_error(
                    learned_warmstart_result->solution, classical.solution);
                maximum_learned_warmstart_cross_error = std::max(
                    maximum_learned_warmstart_cross_error, error);
                learned_warmstart_agreement =
                    learned_warmstart_agreement && error <= 1.0e-6;
            }
            const auto accelerate_cholesky_cross_error =
                accelerate_cholesky_result.solved
                ? relative_infinity_error(
                      accelerate_cholesky_result.solution, classical.solution)
                : std::numeric_limits<double>::infinity();
            maximum_accelerate_cholesky_cross_error = std::max(
                maximum_accelerate_cholesky_cross_error,
                accelerate_cholesky_cross_error);
            accelerate_cholesky_agreement = accelerate_cholesky_agreement &&
                accelerate_cholesky_cross_error <= 1.0e-6;
            const auto accelerate_band_cross_error =
                accelerate_band_result.solved
                ? relative_infinity_error(
                      accelerate_band_result.solution, classical.solution)
                : std::numeric_limits<double>::infinity();
            maximum_accelerate_band_cross_error = std::max(
                maximum_accelerate_band_cross_error,
                accelerate_band_cross_error);
            accelerate_band_agreement = accelerate_band_agreement &&
                accelerate_band_cross_error <= 1.0e-6;
            maximum_multigrid_cross_error = std::max(
                maximum_multigrid_cross_error,
                relative_infinity_error(
                    multigrid_result.solution, classical.solution));
            multigrid_agreement = multigrid_agreement &&
                maximum_multigrid_cross_error <= 1.0e-6;
            maximum_incomplete_cholesky_cross_error = std::max(
                maximum_incomplete_cholesky_cross_error,
                relative_infinity_error(
                    incomplete_cholesky_result.solution, classical.solution));
            incomplete_cholesky_agreement = incomplete_cholesky_agreement &&
                maximum_incomplete_cholesky_cross_error <= 1.0e-6;
            maximum_geometric_multigrid_cross_error = std::max(
                maximum_geometric_multigrid_cross_error,
                relative_infinity_error(
                    geometric_multigrid_result.solution, classical.solution));
            geometric_multigrid_agreement = geometric_multigrid_agreement &&
                maximum_geometric_multigrid_cross_error <= 1.0e-6;
            maximum_geometric_warmstart_cross_error = std::max(
                maximum_geometric_warmstart_cross_error,
                relative_infinity_error(
                    geometric_warmstart_result.solution, classical.solution));
            geometric_warmstart_agreement = geometric_warmstart_agreement &&
                maximum_geometric_warmstart_cross_error <= 1.0e-6;
            maximum_additive_line_cross_error = std::max(
                maximum_additive_line_cross_error,
                relative_infinity_error(
                    additive_line_result.solution, classical.solution));
            additive_line_agreement = additive_line_agreement &&
                maximum_additive_line_cross_error <= 1.0e-6;
            const auto authoritative = interior(downsample_square(read_sample(
                solution_dataset.get(), sample, 1, dimensions[2], dimensions[3]),
                source_width, width), width);
            maximum_data_error = std::max(maximum_data_error,
                relative_infinity_error(
                    accelerate_band_result.solution, authoritative));
        }
        const auto batched_candidate_started = std::chrono::steady_clock::now();
        std::vector<double> batched_west(samples * unknowns);
        std::vector<double> batched_east(samples * unknowns);
        std::vector<double> batched_south(samples * unknowns);
        std::vector<double> batched_north(samples * unknowns);
        std::vector<double> batched_diagonal(samples * unknowns);
        std::vector<double> batched_right(samples * unknowns);
        for (std::size_t point = 0; point < unknowns; ++point) {
            for (std::size_t sample = 0; sample < samples; ++sample) {
                const auto lane_major = sample * unknowns + point;
                const auto interleaved = point * samples + sample;
                batched_west[interleaved] = metal_west[lane_major];
                batched_east[interleaved] = metal_east[lane_major];
                batched_south[interleaved] = metal_south[lane_major];
                batched_north[interleaved] = metal_north[lane_major];
                batched_diagonal[interleaved] =
                    1.0 / metal_inverse_diagonal[lane_major];
                batched_right[interleaved] = metal_right_hand_sides[lane_major];
            }
        }
        smave::BatchedFivePointSsorPcgWorkspace batched_workspace(
            width - 2, samples);
        const auto batched_candidate = batched_workspace.solve_interleaved(
            batched_west, batched_east, batched_south, batched_north,
            batched_diagonal, batched_right, learned_relaxation,
            1.0e-12, 1.0e-8, 2000);
        const auto batched_candidate_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - batched_candidate_started).count();
        std::vector<double> batched_candidate_lane_major(samples * unknowns);
        if (batched_candidate.solution.size() == samples * unknowns) {
            for (std::size_t point = 0; point < unknowns; ++point) {
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    batched_candidate_lane_major[sample * unknowns + point] =
                        batched_candidate.solution[point * samples + sample];
                }
            }
        }
        const auto batched_candidate_cross_error =
            batched_candidate_lane_major.size() == classical_solutions.size()
            ? relative_infinity_error(
                  batched_candidate_lane_major, classical_solutions)
            : std::numeric_limits<double>::infinity();
        const auto batched_candidate_maximum_residual =
            batched_candidate.residual_inf.empty()
            ? std::numeric_limits<double>::infinity()
            : *std::max_element(
                  batched_candidate.residual_inf.begin(),
                  batched_candidate.residual_inf.end());
        const auto batched_candidate_iterations = std::accumulate(
            batched_candidate.iterations.begin(),
            batched_candidate.iterations.end(), std::size_t{});
        const auto batched_candidate_converged = batched_candidate.valid &&
            std::all_of(
                batched_candidate.converged.begin(),
                batched_candidate.converged.end(),
                [](bool value) { return value; });
        const auto metal_result = smave::metal_weighted_jacobi_2d_batch(
            metal_west, metal_east, metal_south, metal_north,
            metal_inverse_diagonal, metal_right_hand_sides,
            samples, width - 2, 4000, 2.0 / 3.0, 1.0e-6);
        const auto metal_cross_error = metal_result.executed &&
                metal_result.output.size() == classical_solutions.size()
            ? relative_infinity_error(metal_result.output, classical_solutions)
            : std::numeric_limits<double>::infinity();
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_DARCY 1\n"
               << "INPUT \"" << input_path.string() << "\"\n"
               << "SHAPE " << dimensions[0] << ' ' << dimensions[1] << ' '
               << dimensions[2] << ' ' << dimensions[3] << "\n"
               << "DISCRETIZATION_GRID " << width << ' ' << width << "\n"
               << "SAMPLES " << samples << "\n"
               << "UNKNOWNS_PER_SAMPLE " << unknowns << "\n"
               << "SOLVES " << samples << "\n"
               << "SOLVER_ORDER \""
               << smave::test::benchmark_solver_order_name(solver_order) << "\"\n"
               << "SMAVE_BACKEND \"accelerate-lapack-five-point-spd-band-cpu-v1\"\n"
               << "TRAINING_SAMPLES 3\n"
               << "TRAINING_SAMPLE_RANGE \"9997-9999\"\n"
               << "EVALUATION_SAMPLE_RANGE \"0-2\"\n"
               << "LEARNED_RELAXATION " << learned_relaxation << "\n"
               << "OFFLINE_TRAINING_SECONDS " << training_seconds << "\n"
               << "LEARNED_WARMSTART_AVAILABLE "
               << learned_operator.has_value() << "\n"
               << "LEARNED_WARMSTART_AUTHORITY \"warm-start-only\"\n"
               << "LEARNED_WARMSTART_PROTOTYPES "
               << (learned_operator ? learned_operator->prototypes : 0) << "\n"
               << "LEARNED_WARMSTART_HELDOUT_MEAN_ERROR "
               << (learned_operator
                   ? learned_operator->heldout_mean_relative_inf_error : 0.0)
               << "\n"
               << "LEARNED_WARMSTART_MAXIMUM_CANDIDATE_RESIDUAL "
               << maximum_learned_candidate_residual << "\n"
               << "LEARNED_WARMSTART_TOTAL_ITERATIONS "
               << learned_warmstart_iterations << "\n"
               << "LEARNED_WARMSTART_INFERENCE_SECONDS "
               << learned_warmstart_inference_seconds << "\n"
               << "LEARNED_WARMSTART_CORRECTION_SECONDS "
               << learned_warmstart_solve_seconds << "\n"
               << "LEARNED_WARMSTART_FULL_SECONDS "
               << learned_warmstart_full_seconds << "\n"
               << "LEARNED_WARMSTART_VS_CLASSICAL_SPEEDUP "
               << (learned_warmstart_full_seconds > 0.0
                   ? classical_solve_seconds / learned_warmstart_full_seconds : 0.0)
               << "\n"
               << "LEARNED_WARMSTART_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_learned_warmstart_cross_error << "\n"
               << "LEARNED_WARMSTART_AGREEMENT "
               << learned_warmstart_agreement << "\n"
               << "SMAVE_PCG_TOTAL_ITERATIONS 0\n"
               << "CLASSICAL_JACOBI_PCG_TOTAL_ITERATIONS " << classical_iterations << "\n"
               << "CLASSICAL_SOLVE_SECONDS " << classical_solve_seconds << "\n"
               << "SMAVE_SOLVE_SECONDS " << accelerate_band_full_seconds << "\n"
               << "SSOR_PCG_CANDIDATE_TOTAL_ITERATIONS "
               << smave_iterations << "\n"
               << "SSOR_PCG_CANDIDATE_SOLVE_SECONDS "
               << smave_solve_seconds << "\n"
               << "ACCELERATE_CHOLESKY_BACKEND \""
               << accelerate_cholesky_backend << "\"\n"
               << "ACCELERATE_CHOLESKY_AVAILABLE "
               << accelerate_cholesky_available << "\n"
               << "ACCELERATE_CHOLESKY_SOLVED "
               << accelerate_cholesky_solved << "\n"
               << "ACCELERATE_CHOLESKY_MATRIX_NONZEROS "
               << accelerate_cholesky_nonzeros << "\n"
               << "ACCELERATE_CHOLESKY_FULL_SECONDS "
               << accelerate_cholesky_full_seconds << "\n"
               << "ACCELERATE_CHOLESKY_FACTOR_SECONDS "
               << accelerate_cholesky_factor_seconds << "\n"
               << "ACCELERATE_CHOLESKY_SOLVE_SECONDS "
               << accelerate_cholesky_solve_seconds << "\n"
               << "ACCELERATE_CHOLESKY_GATE_SECONDS "
               << accelerate_cholesky_gate_seconds << "\n"
               << "ACCELERATE_CHOLESKY_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / accelerate_cholesky_full_seconds << "\n"
               << "ACCELERATE_CHOLESKY_MAXIMUM_RESIDUAL_INF "
               << maximum_accelerate_cholesky_residual << "\n"
               << "ACCELERATE_CHOLESKY_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_accelerate_cholesky_cross_error << "\n"
               << "ACCELERATE_CHOLESKY_CROSS_SOLVER_AGREEMENT "
               << accelerate_cholesky_agreement << "\n"
               << "ACCELERATE_CHOLESKY_REASON \""
               << accelerate_cholesky_reason << "\"\n"
               << "ACCELERATE_BAND_CHOLESKY_BACKEND \""
               << accelerate_band_backend << "\"\n"
               << "ACCELERATE_BAND_CHOLESKY_AVAILABLE "
               << accelerate_band_available << "\n"
               << "ACCELERATE_BAND_CHOLESKY_SOLVED "
               << accelerate_band_solved << "\n"
               << "ACCELERATE_BAND_CHOLESKY_HALF_BANDWIDTH "
               << width - 2 << "\n"
               << "ACCELERATE_BAND_CHOLESKY_FULL_SECONDS "
               << accelerate_band_full_seconds << "\n"
               << "ACCELERATE_BAND_CHOLESKY_FACTOR_SECONDS "
               << accelerate_band_factor_seconds << "\n"
               << "ACCELERATE_BAND_CHOLESKY_SOLVE_SECONDS "
               << accelerate_band_solve_seconds << "\n"
               << "ACCELERATE_BAND_CHOLESKY_GATE_SECONDS "
               << accelerate_band_gate_seconds << "\n"
               << "ACCELERATE_BAND_CHOLESKY_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / accelerate_band_full_seconds << "\n"
               << "ACCELERATE_BAND_CHOLESKY_MAXIMUM_RESIDUAL_INF "
               << maximum_accelerate_band_residual << "\n"
               << "ACCELERATE_BAND_CHOLESKY_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_accelerate_band_cross_error << "\n"
               << "ACCELERATE_BAND_CHOLESKY_CROSS_SOLVER_AGREEMENT "
               << accelerate_band_agreement << "\n"
               << "ACCELERATE_BAND_CHOLESKY_REASON \""
               << accelerate_band_reason << "\"\n"
               << "BATCHED_SSOR_PCG_BACKEND "
                  "\"interleaved-five-point-ssor-pcg-cpu-v1\"\n"
               << "BATCHED_SSOR_PCG_TOTAL_ITERATIONS "
               << batched_candidate_iterations << "\n"
               << "BATCHED_SSOR_PCG_FULL_SECONDS "
               << batched_candidate_seconds << "\n"
               << "BATCHED_SSOR_PCG_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / batched_candidate_seconds << "\n"
               << "BATCHED_SSOR_PCG_MAXIMUM_RESIDUAL_INF "
               << batched_candidate_maximum_residual << "\n"
               << "BATCHED_SSOR_PCG_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << batched_candidate_cross_error << "\n"
               << "BATCHED_SSOR_PCG_CONVERGED "
               << batched_candidate_converged << "\n"
               << "MULTIGRID_BACKEND \"pcg-aggregation-multigrid-cpu-v1\"\n"
               << "MULTIGRID_LEVELS " << multigrid_levels << "\n"
               << "MULTIGRID_STORAGE_BYTES " << multigrid_storage_bytes << "\n"
               << "MULTIGRID_PCG_TOTAL_ITERATIONS " << multigrid_iterations << "\n"
               << "MULTIGRID_SETUP_SECONDS " << multigrid_setup_seconds << "\n"
               << "MULTIGRID_SOLVE_SECONDS " << multigrid_solve_seconds << "\n"
               << "MULTIGRID_FULL_SECONDS "
               << multigrid_setup_seconds + multigrid_solve_seconds << "\n"
               << "MULTIGRID_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds /
                    (multigrid_setup_seconds + multigrid_solve_seconds) << "\n"
               << "MULTIGRID_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_multigrid_cross_error << "\n"
               << "MULTIGRID_CROSS_SOLVER_AGREEMENT " << multigrid_agreement << "\n"
               << "IC0_BACKEND \"pcg-incomplete-cholesky-zero-cpu-v1\"\n"
               << "IC0_PCG_TOTAL_ITERATIONS "
               << incomplete_cholesky_iterations << "\n"
               << "IC0_SETUP_SECONDS " << incomplete_cholesky_setup_seconds << "\n"
               << "IC0_SOLVE_SECONDS " << incomplete_cholesky_solve_seconds << "\n"
               << "IC0_FULL_SECONDS "
               << incomplete_cholesky_setup_seconds +
                    incomplete_cholesky_solve_seconds << "\n"
               << "IC0_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds /
                    (incomplete_cholesky_setup_seconds +
                     incomplete_cholesky_solve_seconds) << "\n"
               << "IC0_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_incomplete_cholesky_cross_error << "\n"
               << "IC0_CROSS_SOLVER_AGREEMENT "
               << incomplete_cholesky_agreement << "\n"
               << "GEOMETRIC_MG_BACKEND "
                  "\"pcg-matrix-free-geometric-five-point-cpu-v1\"\n"
               << "GEOMETRIC_MG_LEVELS "
               << geometric_multigrid_levels << "\n"
               << "GEOMETRIC_MG_STORAGE_BYTES "
               << geometric_multigrid_storage_bytes << "\n"
               << "GEOMETRIC_MG_PCG_TOTAL_ITERATIONS "
               << geometric_multigrid_iterations << "\n"
               << "GEOMETRIC_MG_SETUP_SECONDS "
               << geometric_multigrid_setup_seconds << "\n"
               << "GEOMETRIC_MG_SOLVE_SECONDS "
               << geometric_multigrid_solve_seconds << "\n"
               << "GEOMETRIC_MG_FULL_SECONDS "
               << geometric_multigrid_setup_seconds +
                    geometric_multigrid_solve_seconds << "\n"
               << "GEOMETRIC_MG_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds /
                    (geometric_multigrid_setup_seconds +
                     geometric_multigrid_solve_seconds) << "\n"
               << "GEOMETRIC_MG_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_geometric_multigrid_cross_error << "\n"
               << "GEOMETRIC_MG_CROSS_SOLVER_AGREEMENT "
               << geometric_multigrid_agreement << "\n"
               << "GEOMETRIC_WARMSTART_BACKEND "
                  "\"one-vcycle-warmstart-trained-ssor-pcg-cpu-v1\"\n"
               << "GEOMETRIC_WARMSTART_PCG_TOTAL_ITERATIONS "
               << geometric_warmstart_iterations << "\n"
               << "GEOMETRIC_WARMSTART_SOLVE_SECONDS "
               << geometric_warmstart_solve_seconds << "\n"
               << "GEOMETRIC_WARMSTART_FULL_SECONDS "
               << geometric_multigrid_setup_seconds +
                    geometric_warmstart_solve_seconds << "\n"
               << "GEOMETRIC_WARMSTART_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds /
                    (geometric_multigrid_setup_seconds +
                     geometric_warmstart_solve_seconds) << "\n"
               << "GEOMETRIC_WARMSTART_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_geometric_warmstart_cross_error << "\n"
               << "GEOMETRIC_WARMSTART_CROSS_SOLVER_AGREEMENT "
               << geometric_warmstart_agreement << "\n"
               << "ADDITIVE_LINE_BACKEND "
                  "\"pcg-additive-row-column-line-cpu-v1\"\n"
               << "ADDITIVE_LINE_PCG_TOTAL_ITERATIONS "
               << additive_line_iterations << "\n"
               << "ADDITIVE_LINE_SOLVE_SECONDS "
               << additive_line_solve_seconds << "\n"
               << "ADDITIVE_LINE_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / additive_line_solve_seconds << "\n"
               << "ADDITIVE_LINE_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << maximum_additive_line_cross_error << "\n"
               << "ADDITIVE_LINE_CROSS_SOLVER_AGREEMENT "
               << additive_line_agreement << "\n"
               << "METAL_BACKEND \"" << metal_result.backend << "\"\n"
               << "METAL_DEVICE \"" << metal_result.device_name << "\"\n"
               << "METAL_AVAILABLE " << metal_result.available << "\n"
               << "METAL_EXECUTED " << metal_result.executed << "\n"
               << "METAL_VERIFIED " << metal_result.verified << "\n"
               << "METAL_ITERATIONS " << metal_result.iterations << "\n"
               << "METAL_SETUP_US " << metal_result.setup_us << "\n"
               << "METAL_KERNEL_US " << metal_result.kernel_us << "\n"
               << "METAL_DOWNLOAD_US " << metal_result.download_us << "\n"
               << "METAL_MAXIMUM_RELATIVE_RESIDUAL "
               << metal_result.maximum_relative_residual << "\n"
               << "METAL_CROSS_SOLVER_RELATIVE_INF_ERROR "
               << metal_cross_error << "\n"
               << "METAL_REASON \"" << metal_result.reason << "\"\n"
               << "CLASSICAL_MEAN_SOLVE_US "
               << classical_solve_seconds * 1.0e6 / samples << "\n"
               << "SMAVE_MEAN_SOLVE_US "
               << accelerate_band_full_seconds * 1.0e6 / samples << "\n"
               << "SMAVE_VS_CLASSICAL_SPEEDUP "
               << classical_solve_seconds / accelerate_band_full_seconds << "\n"
               << "CROSS_SOLVER_RELATIVE_INF_ERROR " << maximum_cross_error << "\n"
               << "PDEBENCH_FIELD_RELATIVE_INF_ERROR " << maximum_data_error << "\n"
               << "ELAPSED_SECONDS " << elapsed << "\n"
               << "CROSS_SOLVER_AGREEMENT " << (maximum_cross_error <= 1.0e-6) << "\n"
               << "END\n";
        std::cout << "PDEBench DarcyFlow solves=" << samples
                  << " cross_error=" << maximum_cross_error
                  << " data_error=" << maximum_data_error << '\n';
        return maximum_cross_error <= 1.0e-6 ? 0 : 4;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench DarcyFlow benchmark failure: " << error.what() << '\n';
        return 2;
    }
}
