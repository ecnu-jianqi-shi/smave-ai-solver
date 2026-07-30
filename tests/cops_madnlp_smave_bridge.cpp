#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

extern "C" int smave_cops_solve_symmetric_csc(
    std::int32_t size,
    const std::int32_t* column_offsets,
    const std::int32_t* row_indices,
    const double* values,
    double* right_hand_side,
    double* relative_residual,
    std::int32_t* backend_code) {
    if (size <= 0 || column_offsets == nullptr || row_indices == nullptr ||
        values == nullptr || right_hand_side == nullptr) {
        return 1;
    }
    const auto dimension = static_cast<std::size_t>(size);
    if (column_offsets[0] != 0 || column_offsets[size] < 0) return 2;

    std::vector<std::vector<std::pair<std::size_t, double>>> rows(dimension);
    for (std::int32_t column = 0; column < size; ++column) {
        if (column_offsets[column] > column_offsets[column + 1]) return 3;
        for (std::int32_t offset = column_offsets[column];
             offset < column_offsets[column + 1]; ++offset) {
            const auto row = row_indices[offset];
            const auto value = values[offset];
            if (row < 0 || row >= size || !std::isfinite(value)) return 4;
            rows[static_cast<std::size_t>(row)].emplace_back(
                static_cast<std::size_t>(column), value);
            if (row != column) {
                rows[static_cast<std::size_t>(column)].emplace_back(
                    static_cast<std::size_t>(row), value);
            }
        }
    }

    smave::LinearSystem system;
    system.unknowns.resize(dimension);
    system.sparsity.row_count = dimension;
    system.sparsity.column_count = dimension;
    system.sparsity.row_offsets.reserve(dimension + 1);
    system.sparsity.row_offsets.push_back(0);
    for (auto& row : rows) {
        std::sort(row.begin(), row.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        for (const auto& [column, value] : row) {
            system.sparsity.column_indices.push_back(column);
            system.sparse_values.push_back(value);
        }
        system.sparsity.row_offsets.push_back(system.sparse_values.size());
    }
    system.right_hand_side.assign(right_hand_side, right_hand_side + dimension);
    system.symmetric = true;

    auto result = smave::industrial_sparse_direct_solve(system);
    if (backend_code != nullptr) *backend_code = result.solved ? 1 : 0;
    if (!result.solved) {
        result = smave::superlu_sparse_direct_solve(system);
        if (backend_code != nullptr && result.solved) *backend_code = 2;
    }
    if (!result.solved || result.solution.size() != dimension) return 5;
    const auto product = system.multiply(result.solution);
    double residual_norm = 0.0;
    double rhs_norm = 0.0;
    for (std::size_t index = 0; index < dimension; ++index) {
        residual_norm = std::max(residual_norm,
            std::abs(product[index] - system.right_hand_side[index]));
        rhs_norm = std::max(rhs_norm, std::abs(system.right_hand_side[index]));
    }
    const auto verified_residual = residual_norm / std::max(1.0, rhs_norm);
    if (relative_residual != nullptr) *relative_residual = verified_residual;
    if (!std::isfinite(verified_residual) || verified_residual > 1.0e-8) {
        if (backend_code != nullptr) *backend_code = 0;
        return 6;
    }
    std::copy(result.solution.begin(), result.solution.end(), right_hand_side);
    return 0;
}
