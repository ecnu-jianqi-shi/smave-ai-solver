#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <new>
#include <queue>
#include <string>
#include <vector>

#if defined(SMAVE_HAVE_SUPERLU_SPARSE)
extern "C" {
#include <slu_ddefs.h>
}
#endif

namespace smave {
namespace {

std::size_t structural_rank(const LinearSystem& system) {
    const std::size_t size = system.size();
    std::vector<std::vector<std::size_t>> adjacency(size);
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                if (system.sparse_values[offset] != 0.0) {
                    adjacency[row].push_back(system.sparsity.column_indices[offset]);
                }
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                if (system.matrix[row][column] != 0.0) {
                    adjacency[row].push_back(column);
                }
            }
        }
    }

    const std::size_t unmatched = size;
    const std::size_t unreachable = size + 1;
    std::vector<std::size_t> row_match(size, unmatched);
    std::vector<std::size_t> column_match(size, unmatched);
    std::vector<std::size_t> distance(size, unreachable);
    const auto build_layers = [&]() {
        std::queue<std::size_t> pending;
        bool augmenting_path_exists{};
        for (std::size_t row = 0; row < size; ++row) {
            if (row_match[row] == unmatched) {
                distance[row] = 0;
                pending.push(row);
            } else {
                distance[row] = unreachable;
            }
        }
        while (!pending.empty()) {
            const std::size_t row = pending.front();
            pending.pop();
            for (const std::size_t column : adjacency[row]) {
                const std::size_t matched_row = column_match[column];
                if (matched_row == unmatched) {
                    augmenting_path_exists = true;
                } else if (distance[matched_row] == unreachable) {
                    distance[matched_row] = distance[row] + 1;
                    pending.push(matched_row);
                }
            }
        }
        return augmenting_path_exists;
    };
    std::function<bool(std::size_t)> augment = [&](std::size_t row) {
        for (const std::size_t column : adjacency[row]) {
            const std::size_t matched_row = column_match[column];
            if (matched_row == unmatched ||
                (distance[matched_row] == distance[row] + 1 &&
                 augment(matched_row))) {
                row_match[row] = column;
                column_match[column] = row;
                return true;
            }
        }
        distance[row] = unreachable;
        return false;
    };

    std::size_t matched{};
    while (build_layers()) {
        for (std::size_t row = 0; row < size; ++row) {
            if (row_match[row] == unmatched && augment(row)) ++matched;
        }
    }
    return matched;
}

}  // namespace

bool superlu_sparse_direct_available() {
#if defined(SMAVE_HAVE_SUPERLU_SPARSE)
    return true;
#else
    return false;
#endif
}

std::string superlu_sparse_direct_backend() {
#if defined(SMAVE_HAVE_SUPERLU_SPARSE)
    return "superlu-dgssv-cpu-v1";
#else
    return "unavailable";
#endif
}

IndustrialSparseDirectResult superlu_sparse_direct_solve(
    const LinearSystem& system) {
    IndustrialSparseDirectResult result;
    result.available = superlu_sparse_direct_available();
    result.backend = superlu_sparse_direct_backend();
    const auto size = system.size();
    if (!result.available) {
        result.reason = "SuperLU sparse backend is not compiled";
        return result;
    }
    if (size == 0 || system.right_hand_side.size() != size ||
        (!system.has_sparse_matrix() && !system.has_dense_matrix()) ||
        size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.reason = "invalid SuperLU sparse direct input";
        return result;
    }
    const std::size_t rank = structural_rank(system);
    if (rank != size) {
        result.reason = "SuperLU structural-rank gate rejected matrix with deficiency " +
            std::to_string(size - rank);
        return result;
    }
#if defined(SMAVE_HAVE_SUPERLU_SPARSE)
    std::vector<int_t> column_counts(size);
    std::size_t nonzeros{};
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = system.sparsity.column_indices[offset];
                const auto value = system.sparse_values[offset];
                if (column >= size || !std::isfinite(value)) {
                    result.reason = "SuperLU matrix contains invalid entries";
                    return result;
                }
                if (value != 0.0) {
                    ++column_counts[column];
                    ++nonzeros;
                }
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                const auto value = system.matrix[row][column];
                if (!std::isfinite(value)) {
                    result.reason = "SuperLU matrix contains NaN/Inf";
                    return result;
                }
                if (value != 0.0) {
                    ++column_counts[column];
                    ++nonzeros;
                }
            }
        }
    }
    if (nonzeros > static_cast<std::size_t>(std::numeric_limits<int_t>::max())) {
        result.reason = "SuperLU matrix exceeds configured index range";
        return result;
    }
    std::vector<int_t> column_offsets(size + 1);
    for (std::size_t column = 0; column < size; ++column) {
        column_offsets[column + 1] = column_offsets[column] + column_counts[column];
    }
    std::vector<int_t> positions = column_offsets;
    auto* values = doubleMalloc(static_cast<int_t>(nonzeros));
    auto* row_indices = intMalloc(static_cast<int_t>(nonzeros));
    auto* offsets = intMalloc(static_cast<int_t>(size + 1));
    auto* right_hand_side = doubleMalloc(static_cast<int_t>(size));
    if (values == nullptr || row_indices == nullptr || offsets == nullptr ||
        right_hand_side == nullptr) {
        SUPERLU_FREE(values);
        SUPERLU_FREE(row_indices);
        SUPERLU_FREE(offsets);
        SUPERLU_FREE(right_hand_side);
        throw std::bad_alloc();
    }
    std::copy(column_offsets.begin(), column_offsets.end(), offsets);
    auto insert = [&](std::size_t row, std::size_t column, double value) {
        if (value == 0.0) return;
        const auto destination = positions[column]++;
        values[destination] = value;
        row_indices[destination] = static_cast<int_t>(row);
    };
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                insert(row, system.sparsity.column_indices[offset],
                       system.sparse_values[offset]);
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                insert(row, column, system.matrix[row][column]);
            }
        }
    }
    std::copy(system.right_hand_side.begin(), system.right_hand_side.end(),
              right_hand_side);
    SuperMatrix matrix{};
    SuperMatrix dense_rhs{};
    SuperMatrix lower{};
    SuperMatrix upper{};
    dCreate_CompCol_Matrix(
        &matrix, static_cast<int_t>(size), static_cast<int_t>(size),
        static_cast<int_t>(nonzeros), values, row_indices, offsets,
        SLU_NC, SLU_D, SLU_GE);
    dCreate_Dense_Matrix(
        &dense_rhs, static_cast<int_t>(size), 1, right_hand_side,
        static_cast<int_t>(size), SLU_DN, SLU_D, SLU_GE);
    std::vector<int> column_permutation(size);
    std::vector<int> row_permutation(size);
    superlu_options_t options{};
    set_default_options(&options);
    options.PrintStat = NO;
    SuperLUStat_t statistics{};
    StatInit(&statistics);
    int_t information{};
    dgssv(
        &options, &matrix, column_permutation.data(), row_permutation.data(),
        &lower, &upper, &dense_rhs, &statistics, &information);
    result.matrix_nonzeros = nonzeros;
    if (information == 0) {
        result.solution.assign(right_hand_side, right_hand_side + size);
        double right_norm{};
        for (const auto value : system.right_hand_side) {
            right_norm = std::max(right_norm, std::abs(value));
        }
        const auto product = system.multiply(result.solution);
        double residual_norm{};
        for (std::size_t row = 0; row < size; ++row) {
            residual_norm = std::max(
                residual_norm,
                std::abs(system.right_hand_side[row] - product[row]));
        }
        result.residual_inf = residual_norm / std::max(1.0, right_norm);
        result.solved = std::isfinite(result.residual_inf) &&
            result.residual_inf <= 1.0e-8;
        result.reason = result.solved
            ? "SuperLU dgssv and original linear residual gate passed"
            : "SuperLU dgssv failed original linear residual gate";
    } else {
        result.reason = "SuperLU dgssv failed with info=" +
            std::to_string(information);
    }
    StatFree(&statistics);
    Destroy_CompCol_Matrix(&matrix);
    Destroy_SuperMatrix_Store(&dense_rhs);
    SUPERLU_FREE(right_hand_side);
    if (lower.Store != nullptr) Destroy_SuperNode_Matrix(&lower);
    if (upper.Store != nullptr) Destroy_CompCol_Matrix(&upper);
#endif
    return result;
}

}  // namespace smave
