#include "smave/linear.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
#include <Accelerate/Accelerate.h>
#endif

namespace smave {
namespace {

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) {
        if (!std::isfinite(value)) return std::numeric_limits<double>::infinity();
        result = std::max(result, std::abs(value));
    }
    return result;
}

}  // namespace

bool industrial_sparse_direct_available() {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    return true;
#else
    return false;
#endif
}

std::string industrial_sparse_direct_backend() {
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    return "accelerate-sparse-qr-cpu-v1";
#else
    return "unavailable";
#endif
}

IndustrialSparseDirectResult industrial_sparse_direct_solve(
    const LinearSystem& system) {
    IndustrialSparseDirectResult result;
    result.available = industrial_sparse_direct_available();
    result.backend = industrial_sparse_direct_backend();
    const auto size = system.size();
    if (!result.available) {
        result.reason = "industrial sparse backend is not compiled for this platform";
        return result;
    }
    if (size == 0 || system.right_hand_side.size() != size ||
        (!system.has_sparse_matrix() && !system.has_dense_matrix()) ||
        size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.reason = "invalid industrial sparse direct input";
        return result;
    }
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    std::vector<long> column_starts(size + 1, 0);
    std::vector<int> row_indices;
    std::vector<double> values;
    std::vector<std::vector<std::pair<std::size_t, double>>> columns(size);
    std::vector<double> row_maximum(size, 0.0);
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = system.sparsity.column_indices[offset];
                const auto value = system.sparse_values[offset];
                if (!std::isfinite(value)) {
                    result.reason = "industrial sparse matrix contains NaN/Inf";
                    return result;
                }
                if (value != 0.0) {
                    columns[column].push_back({row, value});
                    row_maximum[row] = std::max(row_maximum[row], std::abs(value));
                }
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                const auto value = system.matrix[row][column];
                if (!std::isfinite(value)) {
                    result.reason = "industrial sparse matrix contains NaN/Inf";
                    return result;
                }
                if (value != 0.0) {
                    columns[column].push_back({row, value});
                    row_maximum[row] = std::max(row_maximum[row], std::abs(value));
                }
            }
        }
    }
    std::vector<double> row_scale(size);
    for (std::size_t row = 0; row < size; ++row) {
        if (!(row_maximum[row] > 0.0) || !std::isfinite(row_maximum[row])) {
            result.reason = "industrial sparse rank gate found an empty row";
            return result;
        }
        row_scale[row] = 1.0 / row_maximum[row];
    }
    for (std::size_t column = 0; column < size; ++column) {
        column_starts[column] = static_cast<long>(values.size());
        for (const auto& [row, value] : columns[column]) {
            row_indices.push_back(static_cast<int>(row));
            values.push_back(value * row_scale[row]);
        }
    }
    column_starts[size] = static_cast<long>(values.size());
    result.matrix_nonzeros = values.size();
    SparseAttributes_t attributes{};
    attributes.kind = SparseOrdinary;
    SparseMatrixStructure structure{
        static_cast<int>(size),
        static_cast<int>(size),
        column_starts.data(),
        row_indices.data(),
        attributes,
        1};
    SparseMatrix_Double matrix{structure, values.data()};
    auto factorization = SparseFactor(SparseFactorizationQR, matrix);
    if (factorization.status < 0) {
        result.reason = "Accelerate sparse QR factorization failed with status " +
            std::to_string(factorization.status);
        SparseCleanup(factorization);
        return result;
    }
    result.solution.assign(size, 0.0);
    auto right_hand_side = system.right_hand_side;
    for (std::size_t row = 0; row < size; ++row) {
        right_hand_side[row] *= row_scale[row];
    }
    const auto workspace_bytes = factorization.solveWorkspaceRequiredStatic +
        factorization.solveWorkspaceRequiredPerRHS;
    const auto workspace_items = std::max<std::size_t>(
        1, (workspace_bytes + sizeof(std::max_align_t) - 1) /
            sizeof(std::max_align_t));
    std::vector<std::max_align_t> workspace(workspace_items);
    DenseVector_Double solution_vector{
        static_cast<int>(size), result.solution.data()};
    DenseVector_Double right_vector{
        static_cast<int>(size), right_hand_side.data()};
    SparseSolve(factorization, right_vector, solution_vector, workspace.data());
    if (!std::all_of(result.solution.begin(), result.solution.end(), [](double value) {
            return std::isfinite(value);
        })) {
        result.reason = "Accelerate sparse QR produced NaN/Inf";
        SparseCleanup(factorization);
        return result;
    }
    std::vector<double> rank_probe_solution(size);
    for (std::size_t index = 0; index < size; ++index) {
        rank_probe_solution[index] = 1.0 +
            static_cast<double>((index * 17U + 3U) % 13U) / 13.0;
    }
    auto rank_probe = system.multiply(rank_probe_solution);
    auto scaled_rank_probe = rank_probe;
    for (std::size_t row = 0; row < size; ++row) {
        scaled_rank_probe[row] *= row_scale[row];
    }
    std::vector<double> rank_solution(size);
    DenseVector_Double rank_solution_vector{
        static_cast<int>(size), rank_solution.data()};
    DenseVector_Double rank_right_vector{
        static_cast<int>(size), scaled_rank_probe.data()};
    SparseSolve(
        factorization, rank_right_vector, rank_solution_vector, workspace.data());
    SparseCleanup(factorization);
    if (!std::all_of(rank_solution.begin(), rank_solution.end(), [](double value) {
            return std::isfinite(value);
        })) {
        result.reason = "industrial sparse numerical rank probe produced NaN/Inf";
        return result;
    }
    std::vector<double> rank_error(size);
    for (std::size_t index = 0; index < size; ++index) {
        rank_error[index] = rank_solution[index] - rank_probe_solution[index];
    }
    const double rank_relative_error = infinity_norm(rank_error) /
        std::max(1.0, infinity_norm(rank_probe_solution));
    if (!std::isfinite(rank_relative_error) || rank_relative_error > 1.0e-9) {
        result.reason = "industrial sparse deterministic numerical rank gate failed";
        return result;
    }
    const auto product = system.multiply(result.solution);
    std::vector<double> residual(size);
    for (std::size_t index = 0; index < size; ++index) {
        residual[index] = product[index] - system.right_hand_side[index];
    }
    result.residual_inf = infinity_norm(residual);
    double maximum_row_sum{};
    for (std::size_t row = 0; row < size; ++row) {
        double row_sum{};
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                row_sum += std::abs(system.sparse_values[offset]);
            }
        } else {
            for (const auto value : system.matrix[row]) row_sum += std::abs(value);
        }
        maximum_row_sum = std::max(maximum_row_sum, row_sum);
    }
    const double backward_scale = infinity_norm(system.right_hand_side) +
        maximum_row_sum * infinity_norm(result.solution);
    const double threshold = 1.0e-10 * std::max(1.0e-300, backward_scale);
    if (!std::isfinite(result.residual_inf) || result.residual_inf > threshold) {
        result.reason = "industrial sparse solution failed original linear residual gate";
        return result;
    }
    result.solved = true;
    result.reason = "Accelerate sparse QR and original linear residual gate passed";
#endif
    return result;
}

SparseSpdDirectResult accelerate_sparse_spd_direct_solve(
    const LinearSystem& system) {
    SparseSpdDirectResult result;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    result.available = true;
    result.backend = "accelerate-sparse-cholesky-spd-cpu-v1";
#else
    result.backend = "unavailable";
#endif
    const auto size = system.size();
    if (!result.available) {
        result.reason = "Accelerate sparse Cholesky is unavailable";
        return result;
    }
    if (!system.symmetric || !system.positive_definite || size == 0 ||
        system.right_hand_side.size() != size ||
        (!system.has_sparse_matrix() && !system.has_dense_matrix()) ||
        size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.reason = "invalid or non-SPD Accelerate sparse Cholesky input";
        return result;
    }
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    std::vector<std::vector<std::pair<std::size_t, double>>> columns(size);
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = system.sparsity.column_indices[offset];
                const auto value = system.sparse_values[offset];
                if (!std::isfinite(value)) {
                    result.reason = "SPD sparse matrix contains NaN/Inf";
                    return result;
                }
                if (row >= column && value != 0.0) {
                    columns[column].push_back({row, value});
                }
            }
        } else {
            for (std::size_t column = 0; column <= row; ++column) {
                const auto value = system.matrix[row][column];
                if (!std::isfinite(value)) {
                    result.reason = "SPD dense matrix contains NaN/Inf";
                    return result;
                }
                if (value != 0.0) columns[column].push_back({row, value});
            }
        }
    }
    std::vector<long> column_starts(size + 1);
    std::vector<int> row_indices;
    std::vector<double> values;
    for (std::size_t column = 0; column < size; ++column) {
        column_starts[column] = static_cast<long>(values.size());
        for (const auto& [row, value] : columns[column]) {
            row_indices.push_back(static_cast<int>(row));
            values.push_back(value);
        }
    }
    column_starts[size] = static_cast<long>(values.size());
    result.matrix_nonzeros = values.size();
    SparseAttributes_t attributes{};
    attributes.kind = SparseSymmetric;
    attributes.triangle = SparseLowerTriangle;
    SparseMatrixStructure structure{
        static_cast<int>(size), static_cast<int>(size),
        column_starts.data(), row_indices.data(), attributes, 1};
    SparseMatrix_Double matrix{structure, values.data()};
    const auto factor_started = std::chrono::steady_clock::now();
    auto factorization = SparseFactor(SparseFactorizationCholesky, matrix);
    result.factor_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - factor_started).count();
    if (factorization.status < 0) {
        result.reason = "Accelerate sparse Cholesky factorization failed with status " +
            std::to_string(factorization.status);
        SparseCleanup(factorization);
        return result;
    }
    const auto workspace_bytes = factorization.solveWorkspaceRequiredStatic +
        factorization.solveWorkspaceRequiredPerRHS;
    const auto workspace_items = std::max<std::size_t>(
        1, (workspace_bytes + sizeof(std::max_align_t) - 1) /
            sizeof(std::max_align_t));
    std::vector<std::max_align_t> workspace(workspace_items);
    result.solution.assign(size, 0.0);
    DenseVector_Double solution_vector{
        static_cast<int>(size), result.solution.data()};
    DenseVector_Double right_vector{
        static_cast<int>(size),
        const_cast<double*>(system.right_hand_side.data())};
    const auto solve_started = std::chrono::steady_clock::now();
    SparseSolve(factorization, right_vector, solution_vector, workspace.data());
    result.solve_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - solve_started).count();
    SparseCleanup(factorization);
    if (!std::all_of(
            result.solution.begin(), result.solution.end(),
            [](double value) { return std::isfinite(value); })) {
        result.reason = "Accelerate sparse Cholesky produced NaN/Inf";
        return result;
    }
    const auto gate_started = std::chrono::steady_clock::now();
    const auto product = system.multiply(result.solution);
    double maximum_row_sum{};
    double solution_inf{};
    double right_inf{};
    for (std::size_t row = 0; row < size; ++row) {
        result.residual_inf = std::max(
            result.residual_inf,
            std::abs(product[row] - system.right_hand_side[row]));
        solution_inf = std::max(solution_inf, std::abs(result.solution[row]));
        right_inf = std::max(right_inf, std::abs(system.right_hand_side[row]));
        double row_sum{};
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                row_sum += std::abs(system.sparse_values[offset]);
            }
        } else {
            for (const auto value : system.matrix[row]) row_sum += std::abs(value);
        }
        maximum_row_sum = std::max(maximum_row_sum, row_sum);
    }
    result.gate_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - gate_started).count();
    const auto threshold = 1.0e-10 * std::max(
        1.0e-300, right_inf + maximum_row_sum * solution_inf);
    if (!std::isfinite(result.residual_inf) || result.residual_inf > threshold) {
        result.reason = "Accelerate sparse Cholesky failed original residual gate";
        return result;
    }
    result.solved = true;
    result.reason = "Accelerate sparse Cholesky and original residual gate passed";
#endif
    return result;
}

SparseSpdDirectResult accelerate_spd_band_direct_solve(
    const LinearSystem& system,
    std::size_t half_bandwidth) {
    SparseSpdDirectResult result;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    result.available = true;
    result.backend = "accelerate-lapack-spd-band-cholesky-cpu-v1";
#else
    result.backend = "unavailable";
#endif
    const auto size = system.size();
    if (!result.available) {
        result.reason = "Accelerate SPD band Cholesky is unavailable";
        return result;
    }
    if (!system.symmetric || !system.positive_definite || size == 0 ||
        system.right_hand_side.size() != size ||
        (!system.has_sparse_matrix() && !system.has_dense_matrix()) ||
        size > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        half_bandwidth >= size ||
        half_bandwidth > static_cast<std::size_t>(
            std::numeric_limits<int>::max() - 1)) {
        result.reason = "invalid or non-SPD Accelerate band Cholesky input";
        return result;
    }
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    const auto leading_dimension = half_bandwidth + 1;
    std::vector<double> band(leading_dimension * size, 0.0);
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = system.sparsity.column_indices[offset];
                const auto value = system.sparse_values[offset];
                if (!std::isfinite(value)) {
                    result.reason = "SPD band matrix contains NaN/Inf";
                    return result;
                }
                if (value == 0.0 || row < column) continue;
                if (row - column > half_bandwidth) {
                    result.reason = "SPD matrix has nonzeros outside declared band";
                    return result;
                }
                band[(row - column) + column * leading_dimension] += value;
                ++result.matrix_nonzeros;
            }
        } else {
            for (std::size_t column = 0; column <= row; ++column) {
                const auto value = system.matrix[row][column];
                if (!std::isfinite(value)) {
                    result.reason = "SPD band matrix contains NaN/Inf";
                    return result;
                }
                if (value == 0.0) continue;
                if (row - column > half_bandwidth) {
                    result.reason = "SPD matrix has nonzeros outside declared band";
                    return result;
                }
                band[(row - column) + column * leading_dimension] = value;
                ++result.matrix_nonzeros;
            }
        }
    }
    auto order = static_cast<__CLPK_integer>(size);
    auto bandwidth = static_cast<__CLPK_integer>(half_bandwidth);
    auto band_leading_dimension = static_cast<__CLPK_integer>(leading_dimension);
    __CLPK_integer info{};
    char lower = 'L';
    const auto factor_started = std::chrono::steady_clock::now();
    dpbtrf_(&lower, &order, &bandwidth, band.data(),
            &band_leading_dimension, &info);
    result.factor_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - factor_started).count();
    if (info != 0) {
        result.reason = "Accelerate band Cholesky factorization failed with info " +
            std::to_string(info);
        return result;
    }
    result.solution = system.right_hand_side;
    __CLPK_integer right_hand_sides = 1;
    auto right_leading_dimension = order;
    const auto solve_started = std::chrono::steady_clock::now();
    dpbtrs_(&lower, &order, &bandwidth, &right_hand_sides,
            band.data(), &band_leading_dimension, result.solution.data(),
            &right_leading_dimension, &info);
    result.solve_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - solve_started).count();
    if (info != 0 || !std::all_of(
            result.solution.begin(), result.solution.end(),
            [](double value) { return std::isfinite(value); })) {
        result.reason = "Accelerate band Cholesky solve failed with info " +
            std::to_string(info);
        return result;
    }
    const auto gate_started = std::chrono::steady_clock::now();
    const auto product = system.multiply(result.solution);
    double maximum_row_sum{};
    double solution_inf{};
    double right_inf{};
    for (std::size_t row = 0; row < size; ++row) {
        result.residual_inf = std::max(
            result.residual_inf,
            std::abs(product[row] - system.right_hand_side[row]));
        solution_inf = std::max(solution_inf, std::abs(result.solution[row]));
        right_inf = std::max(right_inf, std::abs(system.right_hand_side[row]));
        double row_sum{};
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                row_sum += std::abs(system.sparse_values[offset]);
            }
        } else {
            for (const auto value : system.matrix[row]) row_sum += std::abs(value);
        }
        maximum_row_sum = std::max(maximum_row_sum, row_sum);
    }
    result.gate_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - gate_started).count();
    const auto threshold = 1.0e-10 * std::max(
        1.0e-300, right_inf + maximum_row_sum * solution_inf);
    if (!std::isfinite(result.residual_inf) || result.residual_inf > threshold) {
        result.reason = "Accelerate band Cholesky failed original residual gate";
        return result;
    }
    result.solved = true;
    result.reason = "Accelerate band Cholesky and original residual gate passed";
#endif
    return result;
}

SparseSpdDirectResult accelerate_five_point_spd_direct_solve(
    std::size_t width,
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& diagonal,
    const std::vector<double>& right_hand_side) {
    SparseSpdDirectResult result;
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    result.available = true;
    result.backend = "accelerate-lapack-five-point-spd-band-cpu-v1";
#else
    result.backend = "unavailable";
#endif
    if (!result.available) {
        result.reason = "Accelerate five-point SPD direct solve is unavailable";
        return result;
    }
    if (width == 0 || width > static_cast<std::size_t>(
            std::numeric_limits<int>::max() - 1) ||
        width > std::numeric_limits<std::size_t>::max() / width) {
        result.reason = "invalid five-point SPD width";
        return result;
    }
    const auto size = width * width;
    if (west.size() != size || east.size() != size || south.size() != size ||
        north.size() != size || diagonal.size() != size ||
        right_hand_side.size() != size ||
        size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.reason = "five-point SPD input shape mismatch";
        return result;
    }
#if defined(SMAVE_HAVE_ACCELERATE_SPARSE)
    const auto leading_dimension = width + 1;
    std::vector<double> band(leading_dimension * size, 0.0);
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            if (!std::isfinite(diagonal[index]) || !(diagonal[index] > 0.0)) {
                result.reason = "five-point SPD diagonal is invalid";
                return result;
            }
            band[index * leading_dimension] = diagonal[index];
            ++result.matrix_nonzeros;
            if (column + 1 < width) {
                if (!std::isfinite(east[index]) ||
                    !std::isfinite(west[index + 1]) ||
                    std::abs(east[index] - west[index + 1]) >
                        1.0e-12 * std::max({1.0, std::abs(east[index]),
                                           std::abs(west[index + 1])})) {
                    result.reason = "five-point horizontal coupling is not symmetric";
                    return result;
                }
                band[1 + index * leading_dimension] = -east[index];
                ++result.matrix_nonzeros;
            }
            if (row + 1 < width) {
                if (!std::isfinite(north[index]) ||
                    !std::isfinite(south[index + width]) ||
                    std::abs(north[index] - south[index + width]) >
                        1.0e-12 * std::max({1.0, std::abs(north[index]),
                                           std::abs(south[index + width])})) {
                    result.reason = "five-point vertical coupling is not symmetric";
                    return result;
                }
                band[width + index * leading_dimension] = -north[index];
                ++result.matrix_nonzeros;
            }
        }
    }
    auto order = static_cast<__CLPK_integer>(size);
    auto bandwidth = static_cast<__CLPK_integer>(width);
    auto band_leading_dimension = static_cast<__CLPK_integer>(leading_dimension);
    __CLPK_integer info{};
    char lower = 'L';
    const auto factor_started = std::chrono::steady_clock::now();
    dpbtrf_(&lower, &order, &bandwidth, band.data(),
            &band_leading_dimension, &info);
    result.factor_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - factor_started).count();
    if (info != 0) {
        result.reason = "five-point SPD factorization failed with info " +
            std::to_string(info);
        return result;
    }
    result.solution = right_hand_side;
    __CLPK_integer right_hand_sides = 1;
    auto right_leading_dimension = order;
    const auto solve_started = std::chrono::steady_clock::now();
    dpbtrs_(&lower, &order, &bandwidth, &right_hand_sides,
            band.data(), &band_leading_dimension, result.solution.data(),
            &right_leading_dimension, &info);
    result.solve_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - solve_started).count();
    if (info != 0 || !std::all_of(
            result.solution.begin(), result.solution.end(),
            [](double value) { return std::isfinite(value); })) {
        result.reason = "five-point SPD solve failed with info " +
            std::to_string(info);
        return result;
    }
    const auto gate_started = std::chrono::steady_clock::now();
    double maximum_row_sum{};
    double solution_inf{};
    double right_inf{};
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            auto value = diagonal[index] * result.solution[index];
            auto row_sum = std::abs(diagonal[index]);
            if (column > 0) {
                value -= west[index] * result.solution[index - 1];
                row_sum += std::abs(west[index]);
            }
            if (column + 1 < width) {
                value -= east[index] * result.solution[index + 1];
                row_sum += std::abs(east[index]);
            }
            if (row > 0) {
                value -= south[index] * result.solution[index - width];
                row_sum += std::abs(south[index]);
            }
            if (row + 1 < width) {
                value -= north[index] * result.solution[index + width];
                row_sum += std::abs(north[index]);
            }
            result.residual_inf = std::max(
                result.residual_inf, std::abs(value - right_hand_side[index]));
            maximum_row_sum = std::max(maximum_row_sum, row_sum);
            solution_inf = std::max(solution_inf, std::abs(result.solution[index]));
            right_inf = std::max(right_inf, std::abs(right_hand_side[index]));
        }
    }
    result.gate_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - gate_started).count();
    const auto threshold = 1.0e-10 * std::max(
        1.0e-300, right_inf + maximum_row_sum * solution_inf);
    if (!std::isfinite(result.residual_inf) || result.residual_inf > threshold) {
        result.reason = "five-point SPD solve failed original residual gate";
        return result;
    }
    result.solved = true;
    result.reason = "five-point SPD Cholesky and original residual gate passed";
#endif
    return result;
}

}  // namespace smave
