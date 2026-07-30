#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

#if defined(__APPLE__) && defined(__aarch64__)
#include <arm_neon.h>
#endif


namespace smave {
namespace {

constexpr double maximum_sparse_routing_ratio = 1.0e32;

double bounded_positive_ratio(double numerator, double denominator, double fallback) {
    if (!(numerator >= 0.0) || !(denominator > 0.0) ||
        !std::isfinite(numerator) || !std::isfinite(denominator)) {
        return std::isinf(numerator) && denominator > 0.0
            ? maximum_sparse_routing_ratio
            : fallback;
    }
    if (numerator > maximum_sparse_routing_ratio * denominator) {
        return maximum_sparse_routing_ratio;
    }
    return std::clamp(
        numerator / denominator, fallback, maximum_sparse_routing_ratio);
}

double dot(const std::vector<double>& left, const std::vector<double>& right) {
    double result = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result += left[index] * right[index];
    }
    return result;
}

double norm_inf(const std::vector<double>& values) {
    double result = 0.0;
    for (const double value : values) result = std::max(result, std::abs(value));
    return result;
}

double norm_two(const std::vector<double>& values) {
    return std::sqrt(std::max(0.0, dot(values, values)));
}

bool finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool cholesky_positive_definite(const std::vector<std::vector<double>>& matrix) {
    const std::size_t size = matrix.size();
    std::vector<std::vector<double>> lower(size, std::vector<double>(size));
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double value = matrix[row][column];
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= lower[row][inner] * lower[column][inner];
            }
            if (row == column) {
                if (!(value > 1.0e-14) || !std::isfinite(value)) return false;
                lower[row][column] = std::sqrt(value);
            } else {
                lower[row][column] = value / lower[column][column];
            }
        }
    }
    return true;
}

}  // namespace

std::size_t LinearSystem::size() const {
    if (!right_hand_side.empty()) return right_hand_side.size();
    if (!unknowns.empty()) return unknowns.size();
    if (!matrix.empty()) return matrix.size();
    return sparsity.row_count;
}

bool LinearSystem::has_dense_matrix() const {
    const auto dimension = size();
    return dimension != 0 && matrix.size() == dimension &&
        std::all_of(matrix.begin(), matrix.end(), [&](const auto& row) {
            return row.size() == dimension;
        });
}

bool LinearSystem::has_sparse_matrix() const {
    const auto dimension = size();
    return dimension != 0 && sparsity.row_count == dimension &&
        sparsity.column_count == dimension &&
        sparse_values.size() == sparsity.nonzeros();
}

std::size_t LinearSystem::nonzeros() const {
    if (has_sparse_matrix()) return sparsity.nonzeros();
    std::size_t count{};
    for (const auto& row : matrix) {
        count += static_cast<std::size_t>(std::count_if(
            row.begin(), row.end(), [](double value) { return value != 0.0; }));
    }
    return count;
}

std::size_t LinearSystem::dense_storage_bytes() const {
    std::size_t bytes{};
    for (const auto& row : matrix) bytes += row.size() * sizeof(double);
    return bytes;
}

std::size_t LinearSystem::sparse_storage_bytes() const {
    return sparsity.row_offsets.size() * sizeof(std::size_t) +
        sparsity.column_indices.size() * sizeof(std::size_t) +
        sparse_values.size() * sizeof(double);
}

double LinearSystem::coefficient(std::size_t row_index, std::size_t column) const {
    if (has_sparse_matrix()) {
        if (row_index >= sparsity.row_count || column >= sparsity.column_count) return 0.0;
        const auto begin = sparsity.row_offsets[row_index];
        const auto end = sparsity.row_offsets[row_index + 1];
        const auto iterator = std::lower_bound(
            sparsity.column_indices.begin() + static_cast<std::ptrdiff_t>(begin),
            sparsity.column_indices.begin() + static_cast<std::ptrdiff_t>(end), column);
        if (iterator == sparsity.column_indices.begin() + static_cast<std::ptrdiff_t>(end) ||
            *iterator != column) return 0.0;
        return sparse_values[static_cast<std::size_t>(
            std::distance(sparsity.column_indices.begin(), iterator))];
    }
    if (row_index >= matrix.size() || column >= matrix[row_index].size()) return 0.0;
    return matrix[row_index][column];
}

std::vector<double> LinearSystem::multiply(const std::vector<double>& vector) const {
    const auto dimension = size();
    if (vector.size() != dimension) return {};
    std::vector<double> result(dimension);
    if (has_sparse_matrix()) {
        for (std::size_t row_index = 0; row_index < dimension; ++row_index) {
            for (std::size_t offset = sparsity.row_offsets[row_index];
                 offset < sparsity.row_offsets[row_index + 1]; ++offset) {
                result[row_index] += sparse_values[offset] *
                    vector[sparsity.column_indices[offset]];
            }
        }
        return result;
    }
    if (!has_dense_matrix()) return {};
    for (std::size_t row_index = 0; row_index < dimension; ++row_index) {
        result[row_index] = dot(matrix[row_index], vector);
    }
    return result;
}

std::vector<double> LinearSystem::multiply_transpose(
    const std::vector<double>& vector) const {
    const auto dimension = size();
    if (vector.size() != dimension) return {};
    std::vector<double> result(dimension);
    if (has_sparse_matrix()) {
        for (std::size_t row_index = 0; row_index < dimension; ++row_index) {
            for (std::size_t offset = sparsity.row_offsets[row_index];
                 offset < sparsity.row_offsets[row_index + 1]; ++offset) {
                result[sparsity.column_indices[offset]] +=
                    sparse_values[offset] * vector[row_index];
            }
        }
        return result;
    }
    if (!has_dense_matrix()) return {};
    for (std::size_t row_index = 0; row_index < dimension; ++row_index) {
        for (std::size_t column = 0; column < dimension; ++column) {
            result[column] += matrix[row_index][column] * vector[row_index];
        }
    }
    return result;
}

void classify_linear_system(
    LinearSystem& system,
    double symmetry_relative_tolerance) {
    const auto size = system.size();
    if (size == 0 || (!system.has_dense_matrix() && !system.has_sparse_matrix())) {
        system.symmetric = false;
        system.positive_definite = false;
        system.diagonal_condition_estimate = std::numeric_limits<double>::infinity();
        system.coefficient_dynamic_range = 1.0;
        system.row_nonzero_coefficient_of_variation = 0.0;
        system.row_l1_condition_estimate = 1.0;
        system.diagonal_dominance_fraction = 0.0;
        system.mean_diagonal_row_l1_fraction = 0.0;
        system.normalized_mean_bandwidth = 0.0;
        return;
    }
    system.symmetric = true;
    double minimum_diagonal = std::numeric_limits<double>::infinity();
    double maximum_diagonal = 0.0;
    double minimum_coefficient = std::numeric_limits<double>::infinity();
    double maximum_coefficient = 0.0;
    double minimum_row_l1 = std::numeric_limits<double>::infinity();
    double maximum_row_l1 = 0.0;
    double row_nonzero_sum{};
    double row_nonzero_square_sum{};
    double diagonal_row_l1_fraction_sum{};
    double bandwidth_sum{};
    std::size_t coefficient_count{};
    std::size_t diagonally_dominant_rows{};
    for (std::size_t row = 0; row < size; ++row) {
        const auto diagonal = std::abs(system.coefficient(row, row));
        minimum_diagonal = std::min(minimum_diagonal, diagonal);
        maximum_diagonal = std::max(maximum_diagonal, diagonal);
        double row_l1{};
        double off_diagonal_l1{};
        std::size_t row_nonzeros{};
        const auto accumulate_statistics = [&](std::size_t column) {
            const double absolute = std::abs(system.coefficient(row, column));
            if (absolute == 0.0) return;
            minimum_coefficient = std::min(minimum_coefficient, absolute);
            maximum_coefficient = std::max(maximum_coefficient, absolute);
            row_l1 += absolute;
            if (column != row) off_diagonal_l1 += absolute;
            ++row_nonzeros;
            bandwidth_sum += static_cast<double>(
                row > column ? row - column : column - row);
            ++coefficient_count;
        };
        const auto inspect_symmetry = [&](std::size_t column) {
            if (column <= row) return;
            const auto left = system.coefficient(row, column);
            const auto right = system.coefficient(column, row);
            const double scale = 1.0 + std::max(std::abs(left), std::abs(right));
            if (std::abs(left - right) > symmetry_relative_tolerance * scale) {
                system.symmetric = false;
            }
        };
        if (system.has_sparse_matrix()) {
            for (const auto column : system.sparsity.row(row)) {
                accumulate_statistics(column);
                inspect_symmetry(column);
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                accumulate_statistics(column);
            }
            for (std::size_t column = row + 1; column < size; ++column) {
                inspect_symmetry(column);
            }
        }
        if (row_l1 > 0.0) minimum_row_l1 = std::min(minimum_row_l1, row_l1);
        maximum_row_l1 = std::max(maximum_row_l1, row_l1);
        row_nonzero_sum += static_cast<double>(row_nonzeros);
        row_nonzero_square_sum += static_cast<double>(row_nonzeros) *
            static_cast<double>(row_nonzeros);
        diagonal_row_l1_fraction_sum += row_l1 > 0.0 ? diagonal / row_l1 : 0.0;
        diagonally_dominant_rows += diagonal >= off_diagonal_l1 ? 1U : 0U;
    }
    if (system.has_dense_matrix()) {
        system.positive_definite =
            system.symmetric && cholesky_positive_definite(system.matrix);
    } else {
        bool positive_m_matrix = system.symmetric;
        std::vector<bool> strict_row(size);
        std::vector<std::vector<std::size_t>> adjacency(size);
        for (std::size_t row = 0; row < size; ++row) {
            double off_diagonal_sum{};
            for (const auto column : system.sparsity.row(row)) {
                if (column == row) continue;
                const auto value = system.coefficient(row, column);
                off_diagonal_sum += std::abs(value);
                if (value > 0.0) positive_m_matrix = false;
                if (value != 0.0) adjacency[row].push_back(column);
            }
            const auto diagonal = system.coefficient(row, row);
            if (!(diagonal > 0.0) || diagonal + 1.0e-12 < off_diagonal_sum) {
                positive_m_matrix = false;
            }
            strict_row[row] = diagonal > off_diagonal_sum + 1.0e-12;
        }
        std::vector<bool> visited(size);
        for (std::size_t start = 0; start < size && positive_m_matrix; ++start) {
            if (visited[start]) continue;
            bool component_strict{};
            std::vector<std::size_t> pending{start};
            visited[start] = true;
            while (!pending.empty()) {
                const auto row = pending.back();
                pending.pop_back();
                component_strict = component_strict || strict_row[row];
                for (const auto column : adjacency[row]) {
                    if (!visited[column]) {
                        visited[column] = true;
                        pending.push_back(column);
                    }
                }
            }
            if (!component_strict) positive_m_matrix = false;
        }
        system.positive_definite = positive_m_matrix;
    }
    system.diagonal_condition_estimate = minimum_diagonal > 0.0
        ? maximum_diagonal / minimum_diagonal
        : std::numeric_limits<double>::infinity();
    system.coefficient_dynamic_range = minimum_coefficient <
            std::numeric_limits<double>::infinity() && minimum_coefficient > 0.0
        ? bounded_positive_ratio(maximum_coefficient, minimum_coefficient, 1.0)
        : 1.0;
    const double mean_row_nonzeros = row_nonzero_sum / static_cast<double>(size);
    const double row_nonzero_variance = std::max(
        0.0, row_nonzero_square_sum / static_cast<double>(size) -
            mean_row_nonzeros * mean_row_nonzeros);
    system.row_nonzero_coefficient_of_variation = mean_row_nonzeros > 0.0
        ? std::sqrt(row_nonzero_variance) / mean_row_nonzeros
        : 0.0;
    system.row_l1_condition_estimate = minimum_row_l1 <
            std::numeric_limits<double>::infinity() && minimum_row_l1 > 0.0
        ? bounded_positive_ratio(maximum_row_l1, minimum_row_l1, 1.0)
        : 1.0;
    system.diagonal_dominance_fraction =
        static_cast<double>(diagonally_dominant_rows) / static_cast<double>(size);
    system.mean_diagonal_row_l1_fraction =
        diagonal_row_l1_fraction_sum / static_cast<double>(size);
    system.normalized_mean_bandwidth = coefficient_count > 0 && size > 1
        ? bandwidth_sum /
            (static_cast<double>(coefficient_count) * static_cast<double>(size - 1))
        : 0.0;
}

PeriodicTridiagonalFactorization::PeriodicTridiagonalFactorization(
    std::size_t size,
    double diagonal,
    double off_diagonal)
    : size_(size),
      lower_(off_diagonal),
      upper_(off_diagonal),
      corner_upper_(off_diagonal),
      corner_lower_(off_diagonal),
      gamma_(-diagonal) {
    if (size_ < 3 || !std::isfinite(diagonal) || !std::isfinite(off_diagonal) ||
        diagonal == 0.0 || gamma_ == 0.0) {
        return;
    }
    inverse_pivots_.resize(size_);
    upper_factors_.resize(size_ - 1);
    const auto first_diagonal = diagonal - gamma_;
    if (!std::isfinite(first_diagonal) || first_diagonal == 0.0) return;
    inverse_pivots_[0] = 1.0 / first_diagonal;
    upper_factors_[0] = upper_ * inverse_pivots_[0];
    for (std::size_t index = 1; index + 1 < size_; ++index) {
        const auto pivot = diagonal - lower_ * upper_factors_[index - 1];
        if (!std::isfinite(pivot) || pivot == 0.0) return;
        inverse_pivots_[index] = 1.0 / pivot;
        upper_factors_[index] = upper_ * inverse_pivots_[index];
    }
    const auto final_diagonal = diagonal -
        corner_upper_ * corner_lower_ / gamma_;
    const auto final_pivot = final_diagonal -
        lower_ * upper_factors_[size_ - 2];
    if (!std::isfinite(final_pivot) || final_pivot == 0.0) return;
    inverse_pivots_[size_ - 1] = 1.0 / final_pivot;

    std::vector<double> update(size_);
    update[0] = gamma_;
    update[size_ - 1] = corner_upper_;
    correction_.resize(size_);
    std::vector<double> workspace(size_);
    if (!solve_nonperiodic(
            update.data(), correction_.data(), workspace.data())) return;
    correction_denominator_ = 1.0 + correction_[0] +
        corner_lower_ * correction_[size_ - 1] / gamma_;
    valid_ = std::isfinite(correction_denominator_) &&
        correction_denominator_ != 0.0 &&
        std::all_of(correction_.begin(), correction_.end(), [](double value) {
            return std::isfinite(value);
        });
}

std::size_t PeriodicTridiagonalFactorization::size() const {
    return size_;
}

bool PeriodicTridiagonalFactorization::valid() const {
    return valid_;
}

bool PeriodicTridiagonalFactorization::solve_nonperiodic(
    const double* right_hand_side,
    double* solution,
    double* workspace) const {
    if (right_hand_side == nullptr || solution == nullptr || workspace == nullptr ||
        inverse_pivots_.size() != size_ || upper_factors_.size() + 1 != size_) {
        return false;
    }
    workspace[0] = right_hand_side[0] * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        workspace[index] =
            (right_hand_side[index] - lower_ * workspace[index - 1]) *
            inverse_pivots_[index];
    }
    solution[size_ - 1] = workspace[size_ - 1];
    for (std::size_t index = size_ - 1; index-- > 0;) {
        solution[index] = workspace[index] -
            upper_factors_[index] * solution[index + 1];
    }
    return true;
}

bool PeriodicTridiagonalFactorization::solve(
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) const {
    if (!valid_ || right_hand_side.size() != size_) return false;
    solution.resize(size_);
    std::vector<double> workspace(size_);
    if (!solve_nonperiodic(
            right_hand_side.data(), solution.data(), workspace.data())) return false;
    const auto scale = (solution[0] +
        corner_lower_ * solution[size_ - 1] / gamma_) /
        correction_denominator_;
    for (std::size_t index = 0; index < size_; ++index) {
        solution[index] -= scale * correction_[index];
    }
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool PeriodicTridiagonalFactorization::solve_batch(
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::vector<double>& solutions) const {
    if (!valid_ || batch == 0 || right_hand_sides.size() != batch * size_) {
        return false;
    }
    solutions.resize(right_hand_sides.size());
    std::vector<double> workspace(size_);
    for (std::size_t item = 0; item < batch; ++item) {
        const auto offset = item * size_;
        auto* solution = solutions.data() + offset;
        if (!solve_nonperiodic(
                right_hand_sides.data() + offset, solution, workspace.data())) {
            return false;
        }
        const auto scale = (solution[0] +
            corner_lower_ * solution[size_ - 1] / gamma_) /
            correction_denominator_;
        for (std::size_t index = 0; index < size_; ++index) {
            solution[index] -= scale * correction_[index];
        }
    }
    return std::all_of(solutions.begin(), solutions.end(), [](double value) {
        return std::isfinite(value);
    });
}

PeriodicLowerBidiagonalFactorization::PeriodicLowerBidiagonalFactorization(
    std::size_t size,
    double diagonal,
    double lower)
    : size_(size),
      inverse_diagonal_(diagonal != 0.0 ? 1.0 / diagonal : 0.0),
      lower_(lower) {
    if (size_ < 2 || !std::isfinite(diagonal) || !std::isfinite(lower) ||
        diagonal == 0.0) {
        return;
    }
    wrap_coefficients_.resize(size_);
    wrap_coefficients_[0] = -lower_ * inverse_diagonal_;
    for (std::size_t index = 1; index < size_; ++index) {
        wrap_coefficients_[index] =
            -lower_ * wrap_coefficients_[index - 1] * inverse_diagonal_;
    }
    wrap_denominator_ = 1.0 - wrap_coefficients_[size_ - 1];
    valid_ = std::isfinite(wrap_denominator_) && wrap_denominator_ != 0.0 &&
        std::all_of(wrap_coefficients_.begin(), wrap_coefficients_.end(),
                    [](double value) { return std::isfinite(value); });
}

bool PeriodicLowerBidiagonalFactorization::valid() const {
    return valid_;
}

bool PeriodicLowerBidiagonalFactorization::solve(
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) const {
    if (!valid_ || right_hand_side.size() != size_) return false;
    solution.resize(size_);
    solution[0] = right_hand_side[0] * inverse_diagonal_;
    for (std::size_t index = 1; index < size_; ++index) {
        solution[index] =
            (right_hand_side[index] - lower_ * solution[index - 1]) *
            inverse_diagonal_;
    }
    const auto last = solution[size_ - 1] / wrap_denominator_;
    for (std::size_t index = 0; index < size_; ++index) {
        solution[index] += wrap_coefficients_[index] * last;
    }
    return finite(solution);
}

bool PeriodicLowerBidiagonalFactorization::solve_interleaved(
    const std::vector<double>& right_hand_sides,
    std::size_t batch,
    std::vector<double>& solutions) const {
    if (!valid_ || batch == 0 || right_hand_sides.size() != size_ * batch) {
        return false;
    }
    solutions.resize(right_hand_sides.size());
    if (batch == 3) {
#if defined(__APPLE__) && defined(__aarch64__)
        const auto feedback = -lower_ * inverse_diagonal_;
        const float64x2_t previous_scales{feedback, feedback * feedback};
        std::array<double, 3> previous{};
        std::size_t index{};
        for (; index + 1 < size_; index += 2) {
            const auto offset = index * 3;
            const auto right = vld3q_f64(right_hand_sides.data() + offset);
            float64x2x3_t result{};
            for (std::size_t lane = 0; lane < 3; ++lane) {
                auto values = vmulq_n_f64(right.val[lane], inverse_diagonal_);
                values = vfmaq_n_f64(values, previous_scales, previous[lane]);
                values = vsetq_lane_f64(
                    vgetq_lane_f64(values, 1) +
                        inverse_diagonal_ * feedback *
                            vgetq_lane_f64(right.val[lane], 0),
                    values, 1);
                result.val[lane] = values;
                previous[lane] = vgetq_lane_f64(values, 1);
            }
            vst3q_f64(solutions.data() + offset, result);
        }
        if (index < size_) {
            const auto offset = index * 3;
            for (std::size_t lane = 0; lane < 3; ++lane) {
                solutions[offset + lane] =
                    right_hand_sides[offset + lane] * inverse_diagonal_ +
                    feedback * previous[lane];
            }
        }
#else
        solutions[0] = right_hand_sides[0] * inverse_diagonal_;
        solutions[1] = right_hand_sides[1] * inverse_diagonal_;
        solutions[2] = right_hand_sides[2] * inverse_diagonal_;
        for (std::size_t index = 1; index < size_; ++index) {
            const auto offset = index * 3;
            const auto previous = offset - 3;
            solutions[offset] =
                (right_hand_sides[offset] - lower_ * solutions[previous]) *
                inverse_diagonal_;
            solutions[offset + 1] =
                (right_hand_sides[offset + 1] -
                 lower_ * solutions[previous + 1]) * inverse_diagonal_;
            solutions[offset + 2] =
                (right_hand_sides[offset + 2] -
                 lower_ * solutions[previous + 2]) * inverse_diagonal_;
        }
#endif
    } else {
        for (std::size_t lane = 0; lane < batch; ++lane) {
            solutions[lane] = right_hand_sides[lane] * inverse_diagonal_;
        }
        for (std::size_t index = 1; index < size_; ++index) {
            const auto offset = index * batch;
            const auto previous = offset - batch;
            for (std::size_t lane = 0; lane < batch; ++lane) {
                solutions[offset + lane] =
                    (right_hand_sides[offset + lane] -
                     lower_ * solutions[previous + lane]) * inverse_diagonal_;
            }
        }
    }
    const auto last_offset = (size_ - 1) * batch;
    if (batch == 3) {
        const std::array<double, 3> last{
            solutions[last_offset] / wrap_denominator_,
            solutions[last_offset + 1] / wrap_denominator_,
            solutions[last_offset + 2] / wrap_denominator_};
#if defined(__APPLE__) && defined(__aarch64__)
        std::size_t index{};
        for (; index + 1 < size_; index += 2) {
            const auto offset = index * 3;
            const auto coefficients =
                vld1q_f64(wrap_coefficients_.data() + index);
            auto corrected = vld3q_f64(solutions.data() + offset);
            for (std::size_t lane = 0; lane < 3; ++lane) {
                corrected.val[lane] = vfmaq_n_f64(
                    corrected.val[lane], coefficients, last[lane]);
            }
            vst3q_f64(solutions.data() + offset, corrected);
        }
        if (index < size_) {
            const auto offset = index * 3;
            for (std::size_t lane = 0; lane < 3; ++lane) {
                solutions[offset + lane] +=
                    wrap_coefficients_[index] * last[lane];
            }
        }
#else
        for (std::size_t index = 0; index < size_; ++index) {
            const auto offset = index * 3;
            const auto coefficient = wrap_coefficients_[index];
            solutions[offset] += coefficient * last[0];
            solutions[offset + 1] += coefficient * last[1];
            solutions[offset + 2] += coefficient * last[2];
        }
#endif
    } else {
        for (std::size_t lane = 0; lane < batch; ++lane) {
            const auto last = solutions[last_offset + lane] / wrap_denominator_;
            for (std::size_t index = 0; index < size_; ++index) {
                solutions[index * batch + lane] +=
                    wrap_coefficients_[index] * last;
            }
        }
    }
    return finite(solutions);
}

bool periodic_lower_bidiagonal_relative_residual_interleaved(
    const std::vector<double>& right_hand_sides,
    const std::vector<double>& solutions,
    std::size_t batch,
    double diagonal,
    double lower,
    std::vector<double>& relative_residuals) {
    if (batch == 0 || right_hand_sides.empty() ||
        right_hand_sides.size() != solutions.size() ||
        right_hand_sides.size() % batch != 0 ||
        !std::isfinite(diagonal) || !std::isfinite(lower)) {
        return false;
    }
    const auto size = right_hand_sides.size() / batch;
    relative_residuals.assign(batch, 0.0);
#if defined(__APPLE__) && defined(__aarch64__)
    if (batch == 3 && size >= 2) {
        float64x2_t residual_squares[3]{vdupq_n_f64(0.0), vdupq_n_f64(0.0),
                                       vdupq_n_f64(0.0)};
        float64x2_t right_squares[3]{vdupq_n_f64(0.0), vdupq_n_f64(0.0),
                                    vdupq_n_f64(0.0)};
        double previous[3]{solutions[(size - 1) * 3],
                           solutions[(size - 1) * 3 + 1],
                           solutions[(size - 1) * 3 + 2]};
        const auto diagonal_vector = vdupq_n_f64(diagonal);
        const auto lower_vector = vdupq_n_f64(lower);
        std::size_t index{};
        for (; index + 1 < size; index += 2) {
            const auto offset = index * 3;
            const auto right = vld3q_f64(right_hand_sides.data() + offset);
            const auto solution = vld3q_f64(solutions.data() + offset);
            for (std::size_t lane = 0; lane < 3; ++lane) {
                const auto preceding = vextq_f64(
                    vdupq_n_f64(previous[lane]), solution.val[lane], 1);
                auto product = vmulq_f64(diagonal_vector, solution.val[lane]);
                product = vfmaq_f64(product, lower_vector, preceding);
                const auto residual = vsubq_f64(right.val[lane], product);
                residual_squares[lane] = vfmaq_f64(
                    residual_squares[lane], residual, residual);
                right_squares[lane] = vfmaq_f64(
                    right_squares[lane], right.val[lane], right.val[lane]);
                previous[lane] = vgetq_lane_f64(solution.val[lane], 1);
            }
        }
        std::array<double, 3> residual_norm_squared{};
        std::array<double, 3> right_hand_side_norm_squared{};
        for (std::size_t lane = 0; lane < 3; ++lane) {
            residual_norm_squared[lane] = vaddvq_f64(residual_squares[lane]);
            right_hand_side_norm_squared[lane] = vaddvq_f64(right_squares[lane]);
        }
        if (index < size) {
            const auto offset = index * 3;
            for (std::size_t lane = 0; lane < 3; ++lane) {
                const auto residual = right_hand_sides[offset + lane] -
                    (diagonal * solutions[offset + lane] +
                     lower * previous[lane]);
                residual_norm_squared[lane] += residual * residual;
                right_hand_side_norm_squared[lane] +=
                    right_hand_sides[offset + lane] *
                    right_hand_sides[offset + lane];
            }
        }
        for (std::size_t lane = 0; lane < 3; ++lane) {
            relative_residuals[lane] = std::sqrt(residual_norm_squared[lane]) /
                std::max(1.0, std::sqrt(right_hand_side_norm_squared[lane]));
        }
        return finite(relative_residuals);
    }
#endif
    std::vector<double> residual_norm_squared(batch);
    std::vector<double> right_hand_side_norm_squared(batch);
    for (std::size_t index = 0; index < size; ++index) {
        const auto previous = index == 0 ? size - 1 : index - 1;
        for (std::size_t lane = 0; lane < batch; ++lane) {
            const auto position = index * batch + lane;
            const auto residual = right_hand_sides[position] -
                (diagonal * solutions[position] +
                 lower * solutions[previous * batch + lane]);
            residual_norm_squared[lane] += residual * residual;
            right_hand_side_norm_squared[lane] +=
                right_hand_sides[position] * right_hand_sides[position];
        }
    }
    for (std::size_t lane = 0; lane < batch; ++lane) {
        relative_residuals[lane] = std::sqrt(residual_norm_squared[lane]) /
            std::max(1.0, std::sqrt(right_hand_side_norm_squared[lane]));
    }
    return finite(relative_residuals);
}

bool frozen_burgers_relative_residual_interleaved(
    const std::vector<double>& states,
    const std::vector<double>& solutions,
    std::size_t batch,
    double diffusion_number,
    double convection_scale,
    std::vector<double>& relative_residuals) {
    if (batch == 0 || states.empty() || states.size() != solutions.size() ||
        states.size() % batch != 0 || !std::isfinite(diffusion_number) ||
        !std::isfinite(convection_scale)) {
        return false;
    }
    const auto size = states.size() / batch;
    if (size < 3) return false;
    relative_residuals.assign(batch, 0.0);
#if defined(__APPLE__) && defined(__aarch64__)
    if (batch == 3) {
        auto pair_residual_squares = vdupq_n_f64(0.0);
        auto pair_state_squares = vdupq_n_f64(0.0);
        double third_residual_squared{};
        double third_state_squared{};
        const auto diagonal = vdupq_n_f64(1.0 + 2.0 * diffusion_number);
        const auto negative_diffusion = vdupq_n_f64(-diffusion_number);
        const auto convection_factor = vdupq_n_f64(convection_scale);
        for (std::size_t index = 0; index < size; ++index) {
            const auto previous = index == 0 ? size - 1 : index - 1;
            const auto next = index + 1 == size ? 0 : index + 1;
            const auto offset = index * 3;
            const auto state_pair = vld1q_f64(states.data() + offset);
            const auto solution_pair = vld1q_f64(solutions.data() + offset);
            const auto previous_pair =
                vld1q_f64(solutions.data() + previous * 3);
            const auto next_pair = vld1q_f64(solutions.data() + next * 3);
            const auto convection = vmulq_f64(convection_factor, state_pair);
            auto product = vmulq_f64(diagonal, solution_pair);
            product = vfmaq_f64(
                product, vsubq_f64(negative_diffusion, convection),
                previous_pair);
            product = vfmaq_f64(
                product, vaddq_f64(negative_diffusion, convection), next_pair);
            const auto residual = vsubq_f64(state_pair, product);
            pair_residual_squares = vfmaq_f64(
                pair_residual_squares, residual, residual);
            pair_state_squares = vfmaq_f64(
                pair_state_squares, state_pair, state_pair);

            const auto state = states[offset + 2];
            const auto scalar_convection = convection_scale * state;
            const auto scalar_product =
                (1.0 + 2.0 * diffusion_number) * solutions[offset + 2] +
                (-diffusion_number - scalar_convection) *
                    solutions[previous * 3 + 2] +
                (-diffusion_number + scalar_convection) *
                    solutions[next * 3 + 2];
            const auto scalar_residual = state - scalar_product;
            third_residual_squared += scalar_residual * scalar_residual;
            third_state_squared += state * state;
        }
        std::array<double, 2> pair_residuals{};
        std::array<double, 2> pair_states{};
        vst1q_f64(pair_residuals.data(), pair_residual_squares);
        vst1q_f64(pair_states.data(), pair_state_squares);
        relative_residuals[0] = std::sqrt(pair_residuals[0]) /
            std::max(1.0, std::sqrt(pair_states[0]));
        relative_residuals[1] = std::sqrt(pair_residuals[1]) /
            std::max(1.0, std::sqrt(pair_states[1]));
        relative_residuals[2] = std::sqrt(third_residual_squared) /
            std::max(1.0, std::sqrt(third_state_squared));
        return finite(relative_residuals);
    }
#endif
    std::vector<double> residual_norm_squared(batch);
    std::vector<double> state_norm_squared(batch);
    for (std::size_t index = 0; index < size; ++index) {
        const auto previous = index == 0 ? size - 1 : index - 1;
        const auto next = index + 1 == size ? 0 : index + 1;
        for (std::size_t lane = 0; lane < batch; ++lane) {
            const auto position = index * batch + lane;
            const auto convection = convection_scale * states[position];
            const auto product =
                (1.0 + 2.0 * diffusion_number) * solutions[position] +
                (-diffusion_number - convection) *
                    solutions[previous * batch + lane] +
                (-diffusion_number + convection) *
                    solutions[next * batch + lane];
            const auto residual = states[position] - product;
            residual_norm_squared[lane] += residual * residual;
            state_norm_squared[lane] += states[position] * states[position];
        }
    }
    for (std::size_t lane = 0; lane < batch; ++lane) {
        relative_residuals[lane] = std::sqrt(residual_norm_squared[lane]) /
            std::max(1.0, std::sqrt(state_norm_squared[lane]));
    }
    return finite(relative_residuals);
}

VariableTridiagonalWorkspace::VariableTridiagonalWorkspace(std::size_t size)
    : size_(size),
      inverse_pivots_(size),
      modified_upper_(size),
      modified_right_hand_side_(size),
      modified_diagonal_(size),
      primary_(size),
      update_(size),
      correction_(size) {}

std::size_t VariableTridiagonalWorkspace::size() const {
    return size_;
}

bool VariableTridiagonalWorkspace::solve_impl(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ == 0 || lower.size() != size_ || diagonal.size() != size_ ||
        upper.size() != size_ || right_hand_side.size() != size_) {
        return false;
    }
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    auto pivot = diagonal[0];
    if (!std::isfinite(pivot) ||
        std::abs(pivot) <= pivot_scale(diagonal[0], lower[0], upper[0])) {
        return false;
    }
    modified_upper_[0] = size_ > 1 ? upper[0] / pivot : 0.0;
    modified_right_hand_side_[0] = right_hand_side[0] / pivot;
    for (std::size_t index = 1; index < size_; ++index) {
        pivot = diagonal[index] - lower[index] * modified_upper_[index - 1];
        if (!std::isfinite(pivot) ||
            std::abs(pivot) <= pivot_scale(
                diagonal[index], lower[index], upper[index])) {
            return false;
        }
        modified_upper_[index] =
            index + 1 < size_ ? upper[index] / pivot : 0.0;
        modified_right_hand_side_[index] =
            (right_hand_side[index] -
             lower[index] * modified_right_hand_side_[index - 1]) / pivot;
    }
    solution.resize(size_);
    solution[size_ - 1] = modified_right_hand_side_[size_ - 1];
    if (!std::isfinite(solution[size_ - 1])) return false;
    for (std::size_t index = size_ - 1; index-- > 0;) {
        solution[index] = modified_right_hand_side_[index] -
            modified_upper_[index] * solution[index + 1];
        if (!std::isfinite(solution[index])) return false;
    }
    return true;
}

bool VariableTridiagonalWorkspace::solve(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    return solve_impl(lower, diagonal, upper, right_hand_side, solution);
}

bool VariableTridiagonalWorkspace::solve_strictly_diagonally_dominant_m_matrix(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ == 0 || lower.size() != size_ || diagonal.size() != size_ ||
        upper.size() != size_ || right_hand_side.size() != size_) {
        return false;
    }
    auto pivot = diagonal[0];
    if (!(pivot > 0.0)) return false;
    modified_upper_[0] = size_ > 1 ? upper[0] / pivot : 0.0;
    modified_right_hand_side_[0] = right_hand_side[0] / pivot;
    for (std::size_t index = 1; index < size_; ++index) {
        pivot = diagonal[index] - lower[index] * modified_upper_[index - 1];
        if (!(pivot > 0.0)) return false;
        modified_upper_[index] =
            index + 1 < size_ ? upper[index] / pivot : 0.0;
        modified_right_hand_side_[index] =
            (right_hand_side[index] -
             lower[index] * modified_right_hand_side_[index - 1]) / pivot;
    }
    solution.resize(size_);
    solution.back() = modified_right_hand_side_.back();
    for (std::size_t index = size_ - 1; index-- > 0;) {
        solution[index] = modified_right_hand_side_[index] -
            modified_upper_[index] * solution[index + 1];
    }
    return true;
}

bool VariableTridiagonalWorkspace::
solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal(
    double lower,
    const std::vector<double>& diagonal,
    double upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ == 0 || diagonal.size() != size_ ||
        right_hand_side.size() != size_ || !std::isfinite(lower) ||
        !std::isfinite(upper) || lower > 0.0 || upper > 0.0) {
        return false;
    }
    auto pivot = diagonal[0];
    if (!(pivot > 0.0)) return false;
    modified_upper_[0] = size_ > 1 ? upper / pivot : 0.0;
    modified_right_hand_side_[0] = right_hand_side[0] / pivot;
    if (!std::isfinite(modified_right_hand_side_[0])) return false;
    for (std::size_t index = 1; index < size_; ++index) {
        pivot = diagonal[index] - lower * modified_upper_[index - 1];
        if (!(pivot > 0.0)) return false;
        modified_upper_[index] = index + 1 < size_ ? upper / pivot : 0.0;
        modified_right_hand_side_[index] =
            (right_hand_side[index] -
             lower * modified_right_hand_side_[index - 1]) / pivot;
        if (!std::isfinite(modified_right_hand_side_[index])) return false;
    }
    solution.resize(size_);
    solution.back() = modified_right_hand_side_.back();
    for (std::size_t index = size_ - 1; index-- > 0;) {
        solution[index] = modified_right_hand_side_[index] -
            modified_upper_[index] * solution[index + 1];
        if (!std::isfinite(solution[index])) return false;
    }
    return std::isfinite(solution.back());
}

BatchedVariableTridiagonalWorkspace::BatchedVariableTridiagonalWorkspace(
    std::size_t size, std::size_t batch)
    : size_(size),
      batch_(batch),
      inverse_pivots_(size * batch),
      modified_upper_(size * batch),
      modified_right_hand_side_(size * batch),
      correction_(size * batch),
      factors_(batch) {}

std::size_t BatchedVariableTridiagonalWorkspace::size() const {
    return size_;
}

std::size_t BatchedVariableTridiagonalWorkspace::batch() const {
    return batch_;
}

bool BatchedVariableTridiagonalWorkspace::
solve_strictly_diagonally_dominant_m_matrix_interleaved(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    const auto values = size_ * batch_;
    if (size_ == 0 || batch_ == 0 || lower.size() != values ||
        diagonal.size() != values || upper.size() != values ||
        right_hand_side.size() != values) {
        return false;
    }
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        const auto pivot = diagonal[lane];
        if (!(pivot > 0.0)) return false;
        modified_upper_[lane] = size_ > 1 ? upper[lane] / pivot : 0.0;
        modified_right_hand_side_[lane] = right_hand_side[lane] / pivot;
    }
    for (std::size_t index = 1; index < size_; ++index) {
        const auto offset = index * batch_;
        const auto previous = offset - batch_;
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            const auto position = offset + lane;
            const auto pivot = diagonal[position] -
                lower[position] * modified_upper_[previous + lane];
            if (!(pivot > 0.0)) return false;
            modified_upper_[position] = index + 1 < size_
                ? upper[position] / pivot
                : 0.0;
            modified_right_hand_side_[position] =
                (right_hand_side[position] - lower[position] *
                 modified_right_hand_side_[previous + lane]) / pivot;
        }
    }
    solution.resize(values);
    const auto last = (size_ - 1) * batch_;
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        solution[last + lane] = modified_right_hand_side_[last + lane];
    }
    for (std::size_t index = size_ - 1; index-- > 0;) {
        const auto offset = index * batch_;
        const auto next = offset + batch_;
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            const auto position = offset + lane;
            solution[position] = modified_right_hand_side_[position] -
                modified_upper_[position] * solution[next + lane];
        }
    }
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool BatchedVariableTridiagonalWorkspace::
solve_strictly_diagonally_dominant_m_matrix_constant_off_diagonal_interleaved(
    double lower,
    const std::vector<double>& diagonal,
    double upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    const auto values = size_ * batch_;
    if (size_ == 0 || batch_ == 0 || diagonal.size() != values ||
        right_hand_side.size() != values || !std::isfinite(lower) ||
        !std::isfinite(upper) || lower > 0.0 || upper > 0.0) {
        return false;
    }
    if (batch_ == 3) {
#if defined(__APPLE__) && defined(__aarch64__)
        const auto ones = vdupq_n_f64(1.0);
        const auto lower_vector = vdupq_n_f64(lower);
        const auto upper_vector = vdupq_n_f64(upper);
        const auto first_diagonal = vld1q_f64(diagonal.data());
        double first_pivots[2];
        vst1q_f64(first_pivots, first_diagonal);
        if (!(first_pivots[0] > 0.0) || !(first_pivots[1] > 0.0) ||
            !(diagonal[2] > 0.0)) {
            return false;
        }
        const auto first_inverse = vdivq_f64(ones, first_diagonal);
        vst1q_f64(inverse_pivots_.data(), first_inverse);
        vst1q_f64(modified_upper_.data(),
                  size_ > 1 ? vmulq_f64(upper_vector, first_inverse)
                            : vdupq_n_f64(0.0));
        vst1q_f64(modified_right_hand_side_.data(),
                  vmulq_f64(vld1q_f64(right_hand_side.data()), first_inverse));
        inverse_pivots_[2] = 1.0 / diagonal[2];
        modified_upper_[2] = size_ > 1 ? upper * inverse_pivots_[2] : 0.0;
        modified_right_hand_side_[2] =
            right_hand_side[2] * inverse_pivots_[2];
        for (std::size_t index = 1; index < size_; ++index) {
            const auto offset = index * 3;
            const auto previous = offset - 3;
            const auto pivot_vector = vsubq_f64(
                vld1q_f64(diagonal.data() + offset),
                vmulq_f64(lower_vector,
                          vld1q_f64(modified_upper_.data() + previous)));
            double pivot_values[2];
            vst1q_f64(pivot_values, pivot_vector);
            const auto pivot2 = diagonal[offset + 2] -
                lower * modified_upper_[previous + 2];
            if (!(pivot_values[0] > 0.0) || !(pivot_values[1] > 0.0) ||
                !(pivot2 > 0.0)) {
                return false;
            }
            const auto inverse_vector = vdivq_f64(ones, pivot_vector);
            vst1q_f64(inverse_pivots_.data() + offset, inverse_vector);
            const auto next_upper = index + 1 < size_ ? upper_vector
                                                      : vdupq_n_f64(0.0);
            vst1q_f64(modified_upper_.data() + offset,
                      vmulq_f64(next_upper, inverse_vector));
            const auto reduced_right = vsubq_f64(
                vld1q_f64(right_hand_side.data() + offset),
                vmulq_f64(
                    lower_vector,
                    vld1q_f64(modified_right_hand_side_.data() + previous)));
            vst1q_f64(modified_right_hand_side_.data() + offset,
                      vmulq_f64(reduced_right, inverse_vector));
            inverse_pivots_[offset + 2] = 1.0 / pivot2;
            modified_upper_[offset + 2] =
                index + 1 < size_ ? upper * inverse_pivots_[offset + 2] : 0.0;
            modified_right_hand_side_[offset + 2] =
                (right_hand_side[offset + 2] - lower *
                 modified_right_hand_side_[previous + 2]) *
                inverse_pivots_[offset + 2];
        }
#else
        for (std::size_t lane = 0; lane < 3; ++lane) {
            const auto pivot = diagonal[lane];
            if (!(pivot > 0.0)) return false;
            const auto inverse = 1.0 / pivot;
            inverse_pivots_[lane] = inverse;
            modified_upper_[lane] = size_ > 1 ? upper * inverse : 0.0;
            modified_right_hand_side_[lane] =
                right_hand_side[lane] * inverse;
        }
        for (std::size_t index = 1; index < size_; ++index) {
            const auto offset = index * 3;
            const auto previous = offset - 3;
            const auto pivot0 = diagonal[offset] -
                lower * modified_upper_[previous];
            const auto pivot1 = diagonal[offset + 1] -
                lower * modified_upper_[previous + 1];
            const auto pivot2 = diagonal[offset + 2] -
                lower * modified_upper_[previous + 2];
            if (!(pivot0 > 0.0) || !(pivot1 > 0.0) || !(pivot2 > 0.0)) {
                return false;
            }
            const auto inverse0 = 1.0 / pivot0;
            const auto inverse1 = 1.0 / pivot1;
            const auto inverse2 = 1.0 / pivot2;
            inverse_pivots_[offset] = inverse0;
            inverse_pivots_[offset + 1] = inverse1;
            inverse_pivots_[offset + 2] = inverse2;
            const auto next_upper = index + 1 < size_ ? upper : 0.0;
            modified_upper_[offset] = next_upper * inverse0;
            modified_upper_[offset + 1] = next_upper * inverse1;
            modified_upper_[offset + 2] = next_upper * inverse2;
            modified_right_hand_side_[offset] =
                (right_hand_side[offset] - lower *
                 modified_right_hand_side_[previous]) * inverse0;
            modified_right_hand_side_[offset + 1] =
                (right_hand_side[offset + 1] - lower *
                 modified_right_hand_side_[previous + 1]) * inverse1;
            modified_right_hand_side_[offset + 2] =
                (right_hand_side[offset + 2] - lower *
                 modified_right_hand_side_[previous + 2]) * inverse2;
        }
#endif
    } else {
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            const auto pivot = diagonal[lane];
            if (!(pivot > 0.0)) return false;
            const auto inverse = 1.0 / pivot;
            inverse_pivots_[lane] = inverse;
            modified_upper_[lane] = size_ > 1 ? upper * inverse : 0.0;
            modified_right_hand_side_[lane] =
                right_hand_side[lane] * inverse;
        }
        for (std::size_t index = 1; index < size_; ++index) {
            const auto offset = index * batch_;
            const auto previous = offset - batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                const auto pivot = diagonal[position] -
                    lower * modified_upper_[previous + lane];
                if (!(pivot > 0.0)) return false;
                const auto inverse = 1.0 / pivot;
                inverse_pivots_[position] = inverse;
                modified_upper_[position] =
                    index + 1 < size_ ? upper * inverse : 0.0;
                modified_right_hand_side_[position] =
                    (right_hand_side[position] - lower *
                     modified_right_hand_side_[previous + lane]) * inverse;
            }
        }
    }
    solution.resize(values);
    const auto last = (size_ - 1) * batch_;
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        solution[last + lane] = modified_right_hand_side_[last + lane];
    }
    if (batch_ == 3) {
        for (std::size_t index = size_ - 1; index-- > 0;) {
            const auto offset = index * 3;
            const auto next = offset + 3;
#if defined(__APPLE__) && defined(__aarch64__)
            vst1q_f64(solution.data() + offset, vsubq_f64(
                vld1q_f64(modified_right_hand_side_.data() + offset),
                vmulq_f64(vld1q_f64(modified_upper_.data() + offset),
                          vld1q_f64(solution.data() + next))));
#else
            solution[offset] = modified_right_hand_side_[offset] -
                modified_upper_[offset] * solution[next];
            solution[offset + 1] = modified_right_hand_side_[offset + 1] -
                modified_upper_[offset + 1] * solution[next + 1];
#endif
            solution[offset + 2] = modified_right_hand_side_[offset + 2] -
                modified_upper_[offset + 2] * solution[next + 2];
        }
    } else {
        for (std::size_t index = size_ - 1; index-- > 0;) {
            const auto offset = index * batch_;
            const auto next = offset + batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                solution[position] = modified_right_hand_side_[position] -
                    modified_upper_[position] * solution[next + lane];
            }
        }
    }
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool BatchedVariableTridiagonalWorkspace::
solve_cyclic_constant_diagonal_affine_off_diagonal_interleaved(
    double diagonal,
    double lower_offset,
    double lower_state_scale,
    double upper_offset,
    double upper_state_scale,
    const std::vector<double>& state,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    const auto values = size_ * batch_;
    if (size_ < 3 || batch_ == 0 || state.size() != values ||
        right_hand_side.size() != values || !std::isfinite(diagonal) ||
        diagonal == 0.0 || !std::isfinite(lower_offset) ||
        !std::isfinite(lower_state_scale) || !std::isfinite(upper_offset) ||
        !std::isfinite(upper_state_scale)) {
        return false;
    }
    const auto gamma = -diagonal;
    const auto first_diagonal = diagonal - gamma;
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        const auto top_right = lower_offset + lower_state_scale * state[lane];
        const auto first_upper = upper_offset + upper_state_scale * state[lane];
        const auto bottom_position = (size_ - 1) * batch_ + lane;
        const auto bottom_left =
            upper_offset + upper_state_scale * state[bottom_position];
        if (!std::isfinite(state[lane]) ||
            !std::isfinite(right_hand_side[lane]) ||
            !std::isfinite(top_right) || !std::isfinite(first_upper) ||
            !std::isfinite(bottom_left) || first_diagonal == 0.0) {
            return false;
        }
        inverse_pivots_[lane] = 1.0 / first_diagonal;
        modified_upper_[lane] = first_upper * inverse_pivots_[lane];
        modified_right_hand_side_[lane] =
            right_hand_side[lane] * inverse_pivots_[lane];
        correction_[lane] = gamma * inverse_pivots_[lane];
    }
    for (std::size_t index = 1; index + 1 < size_; ++index) {
        const auto offset = index * batch_;
        const auto previous = offset - batch_;
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            const auto position = offset + lane;
            const auto lower =
                lower_offset + lower_state_scale * state[position];
            const auto upper =
                upper_offset + upper_state_scale * state[position];
            const auto pivot = diagonal -
                lower * modified_upper_[previous + lane];
            if (!std::isfinite(state[position]) ||
                !std::isfinite(right_hand_side[position]) ||
                !std::isfinite(lower) || !std::isfinite(upper) ||
                !std::isfinite(pivot) ||
                std::abs(pivot) <=
                    pivot_scale(diagonal, lower, upper)) {
                return false;
            }
            inverse_pivots_[position] = 1.0 / pivot;
            modified_upper_[position] = upper * inverse_pivots_[position];
            modified_right_hand_side_[position] =
                (right_hand_side[position] - lower *
                 modified_right_hand_side_[previous + lane]) *
                inverse_pivots_[position];
            correction_[position] =
                (-lower * correction_[previous + lane]) *
                inverse_pivots_[position];
        }
    }
    const auto last = (size_ - 1) * batch_;
    const auto previous = last - batch_;
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        const auto position = last + lane;
        const auto lower =
            lower_offset + lower_state_scale * state[position];
        const auto bottom_left =
            upper_offset + upper_state_scale * state[position];
        const auto top_right =
            lower_offset + lower_state_scale * state[lane];
        const auto diagonal_value =
            diagonal - top_right * bottom_left / gamma;
        const auto pivot = diagonal_value -
            lower * modified_upper_[previous + lane];
        if (!std::isfinite(state[position]) ||
            !std::isfinite(right_hand_side[position]) ||
            !std::isfinite(lower) || !std::isfinite(bottom_left) ||
            !std::isfinite(pivot) ||
            std::abs(pivot) <=
                pivot_scale(diagonal_value, lower, 0.0)) {
            return false;
        }
        inverse_pivots_[position] = 1.0 / pivot;
        modified_upper_[position] = 0.0;
        modified_right_hand_side_[position] =
            (right_hand_side[position] - lower *
             modified_right_hand_side_[previous + lane]) *
            inverse_pivots_[position];
        correction_[position] =
            (bottom_left - lower * correction_[previous + lane]) *
            inverse_pivots_[position];
    }
    if (batch_ == 3) {
        for (std::size_t index = size_ - 1; index-- > 0;) {
            const auto offset = index * 3;
            const auto next = offset + 3;
#if defined(__APPLE__) && defined(__aarch64__)
            const auto upper_pair =
                vld1q_f64(modified_upper_.data() + offset);
            vst1q_f64(modified_right_hand_side_.data() + offset,
                      vfmsq_f64(
                          vld1q_f64(
                              modified_right_hand_side_.data() + offset),
                          upper_pair,
                          vld1q_f64(
                              modified_right_hand_side_.data() + next)));
            vst1q_f64(correction_.data() + offset,
                      vfmsq_f64(
                          vld1q_f64(correction_.data() + offset), upper_pair,
                          vld1q_f64(correction_.data() + next)));
#else
            modified_right_hand_side_[offset] -=
                modified_upper_[offset] * modified_right_hand_side_[next];
            modified_right_hand_side_[offset + 1] -=
                modified_upper_[offset + 1] *
                modified_right_hand_side_[next + 1];
            correction_[offset] -=
                modified_upper_[offset] * correction_[next];
            correction_[offset + 1] -=
                modified_upper_[offset + 1] * correction_[next + 1];
#endif
            modified_right_hand_side_[offset + 2] -=
                modified_upper_[offset + 2] *
                modified_right_hand_side_[next + 2];
            correction_[offset + 2] -=
                modified_upper_[offset + 2] * correction_[next + 2];
        }
    } else {
        for (std::size_t index = size_ - 1; index-- > 0;) {
            const auto offset = index * batch_;
            const auto next = offset + batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                modified_right_hand_side_[position] -=
                    modified_upper_[position] *
                    modified_right_hand_side_[next + lane];
                correction_[position] -=
                    modified_upper_[position] * correction_[next + lane];
            }
        }
    }
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        const auto top_right = lower_offset + lower_state_scale * state[lane];
        const auto denominator = 1.0 + correction_[lane] +
            top_right * correction_[last + lane] / gamma;
        if (!std::isfinite(denominator) ||
            std::abs(denominator) <=
                std::numeric_limits<double>::epsilon() * 64.0) {
            return false;
        }
        factors_[lane] = (modified_right_hand_side_[lane] +
            top_right * modified_right_hand_side_[last + lane] / gamma) /
            denominator;
    }
    solution.resize(values);
    if (batch_ == 3) {
        const auto factor0 = factors_[0];
        const auto factor1 = factors_[1];
        const auto factor2 = factors_[2];
#if defined(__APPLE__) && defined(__aarch64__)
        const float64x2_t factor_pair{factor0, factor1};
#endif
        for (std::size_t index = 0; index < size_; ++index) {
            const auto offset = index * 3;
#if defined(__APPLE__) && defined(__aarch64__)
            vst1q_f64(solution.data() + offset,
                      vfmsq_f64(
                          vld1q_f64(
                              modified_right_hand_side_.data() + offset),
                          factor_pair,
                          vld1q_f64(correction_.data() + offset)));
#else
            solution[offset] = modified_right_hand_side_[offset] -
                factor0 * correction_[offset];
            solution[offset + 1] = modified_right_hand_side_[offset + 1] -
                factor1 * correction_[offset + 1];
#endif
            solution[offset + 2] = modified_right_hand_side_[offset + 2] -
                factor2 * correction_[offset + 2];
        }
    } else {
        for (std::size_t index = 0; index < size_; ++index) {
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = index * batch_ + lane;
                solution[position] = modified_right_hand_side_[position] -
                    factors_[lane] * correction_[position];
            }
        }
    }
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

BatchedFivePointSsorPcgWorkspace::BatchedFivePointSsorPcgWorkspace(
    std::size_t grid_width, std::size_t batch)
    : grid_width_(grid_width),
      batch_(batch),
      residual_(grid_width * grid_width * batch),
      preconditioned_(grid_width * grid_width * batch),
      direction_(grid_width * grid_width * batch),
      matrix_direction_(grid_width * grid_width * batch),
      forward_(grid_width * grid_width * batch) {}

std::size_t BatchedFivePointSsorPcgWorkspace::grid_width() const {
    return grid_width_;
}

std::size_t BatchedFivePointSsorPcgWorkspace::batch() const {
    return batch_;
}

BatchedKrylovResult BatchedFivePointSsorPcgWorkspace::solve_interleaved(
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& diagonal,
    const std::vector<double>& right_hand_side,
    double relaxation,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations) {
    BatchedKrylovResult result;
    const auto points = grid_width_ * grid_width_;
    const auto values = points * batch_;
    if (grid_width_ == 0 || batch_ == 0 || west.size() != values ||
        east.size() != values || south.size() != values ||
        north.size() != values || diagonal.size() != values ||
        right_hand_side.size() != values || !(relaxation > 0.0) ||
        !(relaxation < 2.0) || !(absolute_tolerance >= 0.0) ||
        !(relative_tolerance >= 0.0) || maximum_iterations <= 0) {
        result.reason = "invalid batched five-point SSOR-PCG input";
        return result;
    }
    if (!std::all_of(diagonal.begin(), diagonal.end(), [](double value) {
            return value > 0.0 && std::isfinite(value);
        }) || !finite(right_hand_side)) {
        result.reason = "non-finite or non-positive batched stencil";
        return result;
    }
    result.valid = true;
    result.solution.assign(values, 0.0);
    result.iterations.assign(batch_, 0);
    result.residual_inf.assign(batch_, 0.0);
    result.converged.assign(batch_, false);
    residual_ = right_hand_side;
    std::vector<double> thresholds(batch_);
    std::vector<double> residual_dot_preconditioned(batch_);
    std::vector<double> next_dot(batch_);
    std::vector<double> curvature(batch_);
    std::vector<double> alpha(batch_);
    std::vector<double> beta(batch_);
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        double right_norm{};
        for (std::size_t point = 0; point < points; ++point) {
            right_norm = std::max(
                right_norm, std::abs(right_hand_side[point * batch_ + lane]));
        }
        thresholds[lane] = absolute_tolerance +
            relative_tolerance * std::max(1.0, right_norm);
        result.converged[lane] = right_norm <= thresholds[lane];
    }
    const auto apply_ssor = [&]() {
        for (std::size_t row = 0; row < grid_width_; ++row) {
            for (std::size_t column = 0; column < grid_width_; ++column) {
                const auto point = row * grid_width_ + column;
                const auto offset = point * batch_;
                for (std::size_t lane = 0; lane < batch_; ++lane) {
                    const auto position = offset + lane;
                    auto value = residual_[position];
                    if (column > 0) {
                        value += west[position] *
                            forward_[(point - 1) * batch_ + lane];
                    }
                    if (row > 0) {
                        value += south[position] *
                            forward_[(point - grid_width_) * batch_ + lane];
                    }
                    forward_[position] =
                        relaxation * value / diagonal[position];
                }
            }
        }
        const auto diagonal_scale = (2.0 - relaxation) / relaxation;
        for (std::size_t reverse_row = 0; reverse_row < grid_width_; ++reverse_row) {
            const auto row = grid_width_ - reverse_row - 1;
            for (std::size_t reverse_column = 0;
                 reverse_column < grid_width_; ++reverse_column) {
                const auto column = grid_width_ - reverse_column - 1;
                const auto point = row * grid_width_ + column;
                const auto offset = point * batch_;
                for (std::size_t lane = 0; lane < batch_; ++lane) {
                    const auto position = offset + lane;
                    auto value = diagonal_scale *
                        forward_[position] * diagonal[position];
                    if (column + 1 < grid_width_) {
                        value += east[position] *
                            preconditioned_[(point + 1) * batch_ + lane];
                    }
                    if (row + 1 < grid_width_) {
                        value += north[position] *
                            preconditioned_[(point + grid_width_) * batch_ + lane];
                    }
                    preconditioned_[position] =
                        relaxation * value / diagonal[position];
                }
            }
        }
    };
    apply_ssor();
    direction_ = preconditioned_;
    for (std::size_t point = 0; point < points; ++point) {
        const auto offset = point * batch_;
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            residual_dot_preconditioned[lane] +=
                residual_[offset + lane] * preconditioned_[offset + lane];
        }
    }
    for (int iteration = 1; iteration <= maximum_iterations; ++iteration) {
        std::fill(curvature.begin(), curvature.end(), 0.0);
        for (std::size_t row = 0; row < grid_width_; ++row) {
            for (std::size_t column = 0; column < grid_width_; ++column) {
                const auto point = row * grid_width_ + column;
                const auto offset = point * batch_;
                for (std::size_t lane = 0; lane < batch_; ++lane) {
                    const auto position = offset + lane;
                    auto value = diagonal[position] * direction_[position];
                    if (column > 0) value -= west[position] *
                        direction_[(point - 1) * batch_ + lane];
                    if (column + 1 < grid_width_) value -= east[position] *
                        direction_[(point + 1) * batch_ + lane];
                    if (row > 0) value -= south[position] *
                        direction_[(point - grid_width_) * batch_ + lane];
                    if (row + 1 < grid_width_) value -= north[position] *
                        direction_[(point + grid_width_) * batch_ + lane];
                    matrix_direction_[position] = value;
                    curvature[lane] += direction_[position] * value;
                }
            }
        }
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            if (result.converged[lane]) {
                alpha[lane] = 0.0;
                continue;
            }
            if (!(curvature[lane] > 0.0) ||
                !(residual_dot_preconditioned[lane] > 0.0) ||
                !std::isfinite(curvature[lane]) ||
                !std::isfinite(residual_dot_preconditioned[lane])) {
                result.valid = false;
                result.reason = "batched SSOR-PCG lost positive definiteness";
                return result;
            }
            alpha[lane] = residual_dot_preconditioned[lane] / curvature[lane];
            result.iterations[lane] = iteration;
        }
        std::fill(result.residual_inf.begin(), result.residual_inf.end(), 0.0);
        for (std::size_t point = 0; point < points; ++point) {
            const auto offset = point * batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                result.solution[position] += alpha[lane] * direction_[position];
                residual_[position] -= alpha[lane] * matrix_direction_[position];
                result.residual_inf[lane] = std::max(
                    result.residual_inf[lane], std::abs(residual_[position]));
            }
        }
        bool all_converged = true;
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            if (result.residual_inf[lane] <= thresholds[lane]) {
                result.converged[lane] = true;
            }
            all_converged = all_converged && result.converged[lane];
        }
        if (all_converged) break;
        apply_ssor();
        std::fill(next_dot.begin(), next_dot.end(), 0.0);
        for (std::size_t point = 0; point < points; ++point) {
            const auto offset = point * batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                next_dot[lane] += residual_[offset + lane] *
                    preconditioned_[offset + lane];
            }
        }
        for (std::size_t lane = 0; lane < batch_; ++lane) {
            beta[lane] = result.converged[lane]
                ? 0.0
                : next_dot[lane] / residual_dot_preconditioned[lane];
            residual_dot_preconditioned[lane] = next_dot[lane];
        }
        for (std::size_t point = 0; point < points; ++point) {
            const auto offset = point * batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                direction_[position] = result.converged[lane]
                    ? 0.0
                    : preconditioned_[position] + beta[lane] * direction_[position];
            }
        }
    }
    std::fill(result.residual_inf.begin(), result.residual_inf.end(), 0.0);
    for (std::size_t row = 0; row < grid_width_; ++row) {
        for (std::size_t column = 0; column < grid_width_; ++column) {
            const auto point = row * grid_width_ + column;
            const auto offset = point * batch_;
            for (std::size_t lane = 0; lane < batch_; ++lane) {
                const auto position = offset + lane;
                auto value = diagonal[position] * result.solution[position];
                if (column > 0) value -= west[position] *
                    result.solution[(point - 1) * batch_ + lane];
                if (column + 1 < grid_width_) value -= east[position] *
                    result.solution[(point + 1) * batch_ + lane];
                if (row > 0) value -= south[position] *
                    result.solution[(point - grid_width_) * batch_ + lane];
                if (row + 1 < grid_width_) value -= north[position] *
                    result.solution[(point + grid_width_) * batch_ + lane];
                result.residual_inf[lane] = std::max(
                    result.residual_inf[lane],
                    std::abs(value - right_hand_side[position]));
            }
        }
    }
    for (std::size_t lane = 0; lane < batch_; ++lane) {
        result.converged[lane] = result.residual_inf[lane] <= thresholds[lane];
    }
    result.reason = std::all_of(
        result.converged.begin(), result.converged.end(), [](bool value) {
            return value;
        }) ? "all batched SSOR-PCG lanes passed true residual"
           : "one or more batched SSOR-PCG lanes missed true residual";
    return result;
}

bool VariableTridiagonalWorkspace::solve_constant_diagonal_impl(
    const std::vector<double>& lower,
    double diagonal,
    double first_diagonal,
    double last_diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ == 0 || lower.size() != size_ || upper.size() != size_ ||
        right_hand_side.size() != size_ || !std::isfinite(diagonal) ||
        !std::isfinite(first_diagonal) || !std::isfinite(last_diagonal)) {
        return false;
    }
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    auto pivot = first_diagonal;
    if (std::abs(pivot) <= pivot_scale(first_diagonal, lower[0], upper[0])) {
        return false;
    }
    modified_upper_[0] = size_ > 1 ? upper[0] / pivot : 0.0;
    modified_right_hand_side_[0] = right_hand_side[0] / pivot;
    for (std::size_t index = 1; index < size_; ++index) {
        const auto diagonal_value = index + 1 == size_ ? last_diagonal : diagonal;
        pivot = diagonal_value - lower[index] * modified_upper_[index - 1];
        if (!std::isfinite(pivot) ||
            std::abs(pivot) <= pivot_scale(
                diagonal_value, lower[index], upper[index])) {
            return false;
        }
        modified_upper_[index] =
            index + 1 < size_ ? upper[index] / pivot : 0.0;
        modified_right_hand_side_[index] =
            (right_hand_side[index] -
             lower[index] * modified_right_hand_side_[index - 1]) / pivot;
    }
    solution.resize(size_);
    solution.back() = modified_right_hand_side_.back();
    if (!std::isfinite(solution.back())) return false;
    for (std::size_t index = size_ - 1; index-- > 0;) {
        solution[index] = modified_right_hand_side_[index] -
            modified_upper_[index] * solution[index + 1];
        if (!std::isfinite(solution[index])) return false;
    }
    return true;
}

bool VariableTridiagonalWorkspace::solve_cyclic(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    double top_right,
    double bottom_left,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ < 3 || lower.size() != size_ || diagonal.size() != size_ ||
        upper.size() != size_ || right_hand_side.size() != size_ ||
        !std::isfinite(top_right) || !std::isfinite(bottom_left) ||
        !std::isfinite(diagonal.front()) || diagonal.front() == 0.0) {
        return false;
    }
    const auto gamma = -diagonal.front();
    std::copy(diagonal.begin(), diagonal.end(), modified_diagonal_.begin());
    modified_diagonal_.front() -= gamma;
    modified_diagonal_.back() -= top_right * bottom_left / gamma;
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    auto pivot = modified_diagonal_[0];
    if (!std::isfinite(pivot) || !std::isfinite(lower[0]) ||
        !std::isfinite(upper[0]) ||
        std::abs(pivot) <= pivot_scale(pivot, lower[0], upper[0])) {
        return false;
    }
    inverse_pivots_[0] = 1.0 / pivot;
    modified_upper_[0] = upper[0] * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        if (!std::isfinite(lower[index]) ||
            !std::isfinite(modified_diagonal_[index]) ||
            !std::isfinite(upper[index])) {
            return false;
        }
        pivot = modified_diagonal_[index] -
            lower[index] * modified_upper_[index - 1];
        if (!std::isfinite(pivot) ||
            std::abs(pivot) <= pivot_scale(
                modified_diagonal_[index], lower[index], upper[index])) {
            return false;
        }
        inverse_pivots_[index] = 1.0 / pivot;
        modified_upper_[index] =
            index + 1 < size_ ? upper[index] * inverse_pivots_[index] : 0.0;
    }
    if (!std::isfinite(right_hand_side[0])) return false;
    primary_[0] = right_hand_side[0] * inverse_pivots_[0];
    correction_[0] = gamma * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        if (!std::isfinite(right_hand_side[index])) return false;
        primary_[index] = (right_hand_side[index] -
            lower[index] * primary_[index - 1]) * inverse_pivots_[index];
        const auto update_value = index + 1 == size_ ? bottom_left : 0.0;
        correction_[index] = (update_value -
            lower[index] * correction_[index - 1]) * inverse_pivots_[index];
    }
    for (std::size_t index = size_ - 1; index-- > 0;) {
        primary_[index] -= modified_upper_[index] * primary_[index + 1];
        correction_[index] -=
            modified_upper_[index] * correction_[index + 1];
    }
    const auto denominator = 1.0 + correction_.front() +
        top_right * correction_.back() / gamma;
    if (!std::isfinite(denominator) ||
        std::abs(denominator) <=
            std::numeric_limits<double>::epsilon() * 64.0) {
        return false;
    }
    const auto factor = (primary_.front() +
        top_right * primary_.back() / gamma) / denominator;
    solution.resize(size_);
    for (std::size_t index = 0; index < size_; ++index) {
        solution[index] = primary_[index] - factor * correction_[index];
        if (!std::isfinite(solution[index])) return false;
    }
    return true;
}

bool VariableTridiagonalWorkspace::solve_cyclic_constant_diagonal(
    const std::vector<double>& lower,
    double diagonal,
    const std::vector<double>& upper,
    double top_right,
    double bottom_left,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ < 3 || lower.size() != size_ || upper.size() != size_ ||
        right_hand_side.size() != size_ || !std::isfinite(diagonal) ||
        diagonal == 0.0 || !std::isfinite(top_right) ||
        !std::isfinite(bottom_left)) {
        return false;
    }
    const auto gamma = -diagonal;
    const auto first_diagonal = diagonal - gamma;
    const auto last_diagonal = diagonal - top_right * bottom_left / gamma;
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    auto pivot = first_diagonal;
    if (!std::isfinite(lower[0]) || !std::isfinite(upper[0]) ||
        std::abs(pivot) <= pivot_scale(pivot, lower[0], upper[0])) {
        return false;
    }
    inverse_pivots_[0] = 1.0 / pivot;
    modified_upper_[0] = upper[0] * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        const auto diagonal_value = index + 1 == size_ ? last_diagonal : diagonal;
        if (!std::isfinite(lower[index]) || !std::isfinite(upper[index])) return false;
        pivot = diagonal_value - lower[index] * modified_upper_[index - 1];
        if (!std::isfinite(pivot) ||
            std::abs(pivot) <= pivot_scale(
                diagonal_value, lower[index], upper[index])) {
            return false;
        }
        inverse_pivots_[index] = 1.0 / pivot;
        modified_upper_[index] =
            index + 1 < size_ ? upper[index] * inverse_pivots_[index] : 0.0;
    }
    if (!std::isfinite(right_hand_side[0])) return false;
    primary_[0] = right_hand_side[0] * inverse_pivots_[0];
    correction_[0] = gamma * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        if (!std::isfinite(right_hand_side[index])) return false;
        primary_[index] = (right_hand_side[index] -
            lower[index] * primary_[index - 1]) * inverse_pivots_[index];
        const auto update_value = index + 1 == size_ ? bottom_left : 0.0;
        correction_[index] = (update_value -
            lower[index] * correction_[index - 1]) * inverse_pivots_[index];
    }
    for (std::size_t index = size_ - 1; index-- > 0;) {
        primary_[index] -= modified_upper_[index] * primary_[index + 1];
        correction_[index] -=
            modified_upper_[index] * correction_[index + 1];
    }
    const auto denominator = 1.0 + correction_.front() +
        top_right * correction_.back() / gamma;
    if (!std::isfinite(denominator) ||
        std::abs(denominator) <=
            std::numeric_limits<double>::epsilon() * 64.0) {
        return false;
    }
    const auto factor = (primary_.front() +
        top_right * primary_.back() / gamma) / denominator;
    solution.resize(size_);
    for (std::size_t index = 0; index < size_; ++index) {
        solution[index] = primary_[index] - factor * correction_[index];
        if (!std::isfinite(solution[index])) return false;
    }
    return true;
}

bool VariableTridiagonalWorkspace::
solve_cyclic_constant_diagonal_affine_off_diagonal(
    double diagonal,
    double lower_offset,
    double lower_state_scale,
    double upper_offset,
    double upper_state_scale,
    const std::vector<double>& state,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    if (size_ < 3 || state.size() != size_ || right_hand_side.size() != size_ ||
        !std::isfinite(diagonal) || diagonal == 0.0 ||
        !std::isfinite(lower_offset) || !std::isfinite(lower_state_scale) ||
        !std::isfinite(upper_offset) || !std::isfinite(upper_state_scale)) {
        return false;
    }
    const auto lower_at = [&](std::size_t index) {
        return lower_offset + lower_state_scale * state[index];
    };
    const auto upper_at = [&](std::size_t index) {
        return upper_offset + upper_state_scale * state[index];
    };
    const auto top_right = lower_at(0);
    const auto bottom_left = upper_at(size_ - 1);
    const auto gamma = -diagonal;
    const auto first_diagonal = diagonal - gamma;
    const auto last_diagonal = diagonal - top_right * bottom_left / gamma;
    const auto pivot_scale = [](double diagonal_value, double lower_value,
                                double upper_value) {
        return std::numeric_limits<double>::epsilon() * 64.0 *
            std::max({1.0, std::abs(diagonal_value),
                      std::abs(lower_value), std::abs(upper_value)});
    };
    auto pivot = first_diagonal;
    const auto first_upper = upper_at(0);
    if (!std::isfinite(state[0]) || !std::isfinite(right_hand_side[0]) ||
        !std::isfinite(top_right) || !std::isfinite(first_upper) ||
        !std::isfinite(bottom_left) ||
        std::abs(pivot) <= pivot_scale(pivot, 0.0, first_upper)) {
        return false;
    }
    inverse_pivots_[0] = 1.0 / pivot;
    modified_upper_[0] = first_upper * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        if (!std::isfinite(state[index]) ||
            !std::isfinite(right_hand_side[index])) {
            return false;
        }
        const auto lower = lower_at(index);
        const auto upper = index + 1 < size_ ? upper_at(index) : 0.0;
        const auto diagonal_value = index + 1 == size_ ? last_diagonal : diagonal;
        if (!std::isfinite(lower) || !std::isfinite(upper)) return false;
        pivot = diagonal_value - lower * modified_upper_[index - 1];
        if (!std::isfinite(pivot) ||
            std::abs(pivot) <= pivot_scale(diagonal_value, lower, upper)) {
            return false;
        }
        inverse_pivots_[index] = 1.0 / pivot;
        modified_upper_[index] = upper * inverse_pivots_[index];
    }
    primary_[0] = right_hand_side[0] * inverse_pivots_[0];
    correction_[0] = gamma * inverse_pivots_[0];
    for (std::size_t index = 1; index < size_; ++index) {
        const auto lower = lower_at(index);
        primary_[index] = (right_hand_side[index] -
            lower * primary_[index - 1]) * inverse_pivots_[index];
        const auto update_value = index + 1 == size_ ? bottom_left : 0.0;
        correction_[index] = (update_value -
            lower * correction_[index - 1]) * inverse_pivots_[index];
    }
    for (std::size_t index = size_ - 1; index-- > 0;) {
        primary_[index] -= modified_upper_[index] * primary_[index + 1];
        correction_[index] -=
            modified_upper_[index] * correction_[index + 1];
    }
    const auto denominator = 1.0 + correction_.front() +
        top_right * correction_.back() / gamma;
    if (!std::isfinite(denominator) ||
        std::abs(denominator) <=
            std::numeric_limits<double>::epsilon() * 64.0) {
        return false;
    }
    const auto factor = (primary_.front() +
        top_right * primary_.back() / gamma) / denominator;
    solution.resize(size_);
    for (std::size_t index = 0; index < size_; ++index) {
        solution[index] = primary_[index] - factor * correction_[index];
        if (!std::isfinite(solution[index])) return false;
    }
    return true;
}

bool tridiagonal_direct_solve(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    VariableTridiagonalWorkspace workspace(diagonal.size());
    return workspace.solve(
        lower, diagonal, upper, right_hand_side, solution);
}

bool cyclic_tridiagonal_direct_solve(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    double top_right,
    double bottom_left,
    const std::vector<double>& right_hand_side,
    std::vector<double>& solution) {
    VariableTridiagonalWorkspace workspace(diagonal.size());
    return workspace.solve_cyclic(
        lower, diagonal, upper, top_right, bottom_left,
        right_hand_side, solution);
}

StructuredDirectResult structured_tridiagonal_direct_solve(
    const LinearSystem& system,
    double structural_zero_tolerance) {
    StructuredDirectResult result;
    result.backend = "structured-tridiagonal-direct-cpu-v1";
    const auto size = system.size();
    if (size < 3 || system.right_hand_side.size() != size ||
        structural_zero_tolerance < 0.0 ||
        !std::isfinite(structural_zero_tolerance)) {
        result.reason = "system is too small or has an invalid RHS/tolerance";
        return result;
    }
    std::vector<double> lower(size);
    std::vector<double> diagonal(size);
    std::vector<double> upper(size);
    double top_right{};
    double bottom_left{};
    for (std::size_t row = 0; row < size; ++row) {
        diagonal[row] = system.coefficient(row, row);
        if (row > 0) lower[row] = system.coefficient(row, row - 1);
        if (row + 1 < size) upper[row] = system.coefficient(row, row + 1);
        if (row == 0) top_right = system.coefficient(0, size - 1);
        if (row + 1 == size) bottom_left = system.coefficient(size - 1, 0);
        const auto allowed_column = [&](std::size_t column) {
            const bool allowed = column == row ||
                (row > 0 && column + 1 == row) ||
                (row + 1 < size && column == row + 1) ||
                (row == 0 && column + 1 == size) ||
                (row + 1 == size && column == 0);
            return allowed;
        };
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                if (!allowed_column(system.sparsity.column_indices[offset]) &&
                    std::abs(system.sparse_values[offset]) > structural_zero_tolerance) {
                    result.reason = "matrix has a non-tridiagonal coefficient";
                    return result;
                }
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                if (!allowed_column(column) &&
                    std::abs(system.matrix[row][column]) > structural_zero_tolerance) {
                    result.reason = "matrix has a non-tridiagonal coefficient";
                    return result;
                }
            }
        }
    }
    result.periodic = std::abs(top_right) > structural_zero_tolerance ||
        std::abs(bottom_left) > structural_zero_tolerance;
    result.eligible = true;
    result.solved = result.periodic
        ? cyclic_tridiagonal_direct_solve(
              lower, diagonal, upper, top_right, bottom_left,
              system.right_hand_side, result.solution)
        : tridiagonal_direct_solve(
              lower, diagonal, upper, system.right_hand_side, result.solution);
    if (!result.solved) {
        result.reason = result.periodic
            ? "cyclic tridiagonal factorization failed"
            : "tridiagonal factorization failed";
        return result;
    }
    const auto product = system.multiply(result.solution);
    if (product.size() != size) {
        result.solved = false;
        result.reason = "structured residual multiplication failed";
        return result;
    }
    for (std::size_t row = 0; row < size; ++row) {
        result.residual_inf = std::max(
            result.residual_inf,
            std::abs(product[row] - system.right_hand_side[row]));
    }
    result.reason = result.periodic
        ? "periodic tridiagonal topology and residual verified"
        : "tridiagonal topology and residual verified";
    return result;
}

LinearSystem assemble_linear_system(
    const ModelIR& model,
    const BlockIR& block,
    const std::unordered_map<std::string, Expression>& residuals,
    const std::unordered_map<std::string, double>& context) {
    if (!block.linear) throw std::invalid_argument("cannot assemble nonlinear block as linear");
    if (block.unknowns.size() != block.equation_ids.size()) {
        throw std::invalid_argument("linear block is not square");
    }
    auto base_values = context;
    for (const auto& variable : model.variables) {
        if (variable.kind == "parameter" && !base_values.contains(variable.name)) {
            base_values[variable.name] = variable.start;
        }
    }
    for (const auto& unknown : block.unknowns) base_values[unknown] = 0.0;
    LinearSystem system;
    system.unknowns = block.unknowns;
    const std::size_t size = block.unknowns.size();
    constexpr std::size_t dense_storage_limit = 1024;
    const bool retain_dense = size <= dense_storage_limit;
    if (retain_dense) system.matrix.assign(size, std::vector<double>(size));
    system.sparsity = block.jacobian_sparsity;
    system.sparse_values.resize(system.sparsity.nonzeros());
    system.right_hand_side.resize(size);
    for (std::size_t row = 0; row < size; ++row) {
        const auto& expression = residuals.at(block.equation_ids[row]);
        std::vector<std::string> local_unknowns;
        local_unknowns.reserve(system.sparsity.row(row).size());
        for (const auto column : system.sparsity.row(row)) {
            local_unknowns.push_back(block.unknowns[column]);
        }
        const auto linear_coefficients =
            expression.constant_linear_coefficients(local_unknowns);
        const double offset_value = expression.evaluate(base_values);
        system.right_hand_side[row] = -offset_value;
        std::size_t local_index{};
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            const auto column = system.sparsity.column_indices[offset];
            double value{};
            if (linear_coefficients.has_value()) {
                value = (*linear_coefficients)[local_index];
            } else {
                auto unit_values = base_values;
                unit_values[block.unknowns[column]] = 1.0;
                value = expression.evaluate(unit_values) - offset_value;
            }
            system.sparse_values[offset] = value;
            if (retain_dense) system.matrix[row][column] = value;
            ++local_index;
        }
    }
    classify_linear_system(system);
    return system;
}

void update_linear_right_hand_side(
    LinearSystem& system,
    const ModelIR& model,
    const BlockIR& block,
    const std::unordered_map<std::string, Expression>& residuals,
    const std::unordered_map<std::string, double>& context) {
    if (!block.linear || system.size() != block.unknowns.size() ||
        system.unknowns != block.unknowns) {
        throw std::invalid_argument("cached linear system does not match block");
    }
    auto values = context;
    for (const auto& variable : model.variables) {
        if (variable.kind == "parameter" && !values.contains(variable.name)) {
            values[variable.name] = variable.start;
        }
    }
    for (const auto& unknown : block.unknowns) values[unknown] = 0.0;
    system.right_hand_side.resize(block.equation_ids.size());
    for (std::size_t row = 0; row < block.equation_ids.size(); ++row) {
        system.right_hand_side[row] = -residuals.at(block.equation_ids[row]).evaluate(values);
    }
}

bool dense_direct_solve(const LinearSystem& system, std::vector<double>& solution) {
    if (!system.has_dense_matrix()) return false;
    auto matrix = system.matrix;
    auto right = system.right_hand_side;
    const std::size_t size = right.size();
    solution.assign(size, 0.0);
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1.0e-14) return false;
        std::swap(matrix[pivot], matrix[column]);
        std::swap(right[pivot], right[column]);
        for (std::size_t row = column + 1; row < size; ++row) {
            const double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t item = column; item < size; ++item) {
                matrix[row][item] -= factor * matrix[column][item];
            }
            right[row] -= factor * right[column];
        }
    }
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const std::size_t row = size - reverse - 1;
        double value = right[row];
        for (std::size_t column = row + 1; column < size; ++column) {
            value -= matrix[row][column] * solution[column];
        }
        solution[row] = value / matrix[row][row];
    }
    return finite(solution);
}

SparseDirectResult sparse_ordered_threshold_pivot_solve(
    const LinearSystem& system,
    double pivot_threshold) {
    SparseDirectResult result;
    const std::size_t size = system.right_hand_side.size();
    if (size == 0 || (!system.has_dense_matrix() && !system.has_sparse_matrix()) ||
        !std::isfinite(pivot_threshold) || pivot_threshold <= 0.0 ||
        pivot_threshold > 1.0) return result;
    std::vector<std::set<std::size_t>> adjacency(size);
    for (std::size_t row = 0; row < size; ++row) {
        std::vector<std::size_t> columns;
        if (system.has_sparse_matrix()) {
            for (const auto column : system.sparsity.row(row)) {
                const auto value = system.coefficient(row, column);
                if (!std::isfinite(value)) return result;
                if (value != 0.0) columns.push_back(column);
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                if (system.matrix[row][column] != 0.0) columns.push_back(column);
            }
        }
        for (const auto column : columns) {
            const double value = system.coefficient(row, column);
            if (!std::isfinite(value)) return result;
        }
        result.initial_nonzeros += columns.size();
        for (std::size_t left = 0; left < columns.size(); ++left) {
            for (std::size_t right = left + 1; right < columns.size(); ++right) {
                adjacency[columns[left]].insert(columns[right]);
                adjacency[columns[right]].insert(columns[left]);
            }
        }
    }
    std::vector<bool> eliminated(size, false);
    result.column_order.reserve(size);
    for (std::size_t step = 0; step < size; ++step) {
        std::size_t selected = size;
        std::size_t minimum_degree = size + 1;
        for (std::size_t column = 0; column < size; ++column) {
            if (eliminated[column]) continue;
            std::size_t degree = 0;
            for (const auto neighbor : adjacency[column]) {
                if (!eliminated[neighbor]) ++degree;
            }
            if (degree < minimum_degree) {
                selected = column;
                minimum_degree = degree;
            }
        }
        if (selected == size) return result;
        std::vector<std::size_t> active_neighbors;
        for (const auto neighbor : adjacency[selected]) {
            if (!eliminated[neighbor]) active_neighbors.push_back(neighbor);
        }
        for (std::size_t left = 0; left < active_neighbors.size(); ++left) {
            for (std::size_t right = left + 1; right < active_neighbors.size(); ++right) {
                if (adjacency[active_neighbors[left]].insert(active_neighbors[right]).second) {
                    adjacency[active_neighbors[right]].insert(active_neighbors[left]);
                    ++result.ordering_fill_edges;
                }
            }
        }
        eliminated[selected] = true;
        result.column_order.push_back(selected);
    }
    std::vector<std::set<std::size_t>> natural_adjacency(size);
    for (std::size_t row = 0; row < size; ++row) {
        std::vector<std::size_t> columns;
        if (system.has_sparse_matrix()) {
            columns.assign(system.sparsity.row(row).begin(), system.sparsity.row(row).end());
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                if (system.matrix[row][column] != 0.0) columns.push_back(column);
            }
        }
        for (std::size_t left = 0; left < columns.size(); ++left) {
            for (std::size_t right = left + 1; right < columns.size(); ++right) {
                natural_adjacency[columns[left]].insert(columns[right]);
                natural_adjacency[columns[right]].insert(columns[left]);
            }
        }
    }
    std::vector<bool> naturally_eliminated(size, false);
    for (std::size_t selected = 0; selected < size; ++selected) {
        std::vector<std::size_t> active_neighbors;
        for (const auto neighbor : natural_adjacency[selected]) {
            if (!naturally_eliminated[neighbor]) active_neighbors.push_back(neighbor);
        }
        for (std::size_t left = 0; left < active_neighbors.size(); ++left) {
            for (std::size_t right = left + 1; right < active_neighbors.size(); ++right) {
                if (natural_adjacency[active_neighbors[left]].insert(
                        active_neighbors[right]).second) {
                    natural_adjacency[active_neighbors[right]].insert(active_neighbors[left]);
                    ++result.natural_fill_edges;
                }
            }
        }
        naturally_eliminated[selected] = true;
    }
    std::vector<std::map<std::size_t, double>> rows(size);
    std::vector<std::size_t> ordered_position(size);
    for (std::size_t position = 0; position < size; ++position) {
        ordered_position[result.column_order[position]] = position;
    }
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_sparse_matrix()) {
            for (const auto column : system.sparsity.row(row)) {
                const auto value = system.coefficient(row, column);
                if (value != 0.0) rows[row].emplace(ordered_position[column], value);
            }
        } else {
            for (std::size_t position = 0; position < size; ++position) {
                const double value = system.matrix[row][result.column_order[position]];
                if (value != 0.0) rows[row].emplace(position, value);
            }
        }
    }
    auto right = system.right_hand_side;
    if (!finite(right)) return result;
    result.minimum_scaled_pivot = 1.0;
    for (std::size_t column = 0; column < size; ++column) {
        std::vector<double> row_scales(size, 0.0);
        double maximum_score = 0.0;
        for (std::size_t row = column; row < size; ++row) {
            for (auto entry = rows[row].lower_bound(column);
                 entry != rows[row].end(); ++entry) {
                row_scales[row] = std::max(row_scales[row], std::abs(entry->second));
            }
            const auto entry = rows[row].find(column);
            if (entry == rows[row].end() || !(row_scales[row] > 0.0)) continue;
            maximum_score = std::max(
                maximum_score, std::abs(entry->second) / row_scales[row]);
        }
        if (!(maximum_score > std::numeric_limits<double>::epsilon() * size) ||
            !std::isfinite(maximum_score)) return result;
        std::size_t pivot = size;
        const double accepted_score = pivot_threshold * maximum_score;
        for (std::size_t row = column; row < size; ++row) {
            const auto entry = rows[row].find(column);
            if (entry == rows[row].end() || !(row_scales[row] > 0.0)) continue;
            const double score = std::abs(entry->second) / row_scales[row];
            if (score >= accepted_score) {
                pivot = row;
                result.minimum_scaled_pivot = std::min(result.minimum_scaled_pivot, score);
                break;
            }
        }
        if (pivot == size) return result;
        if (pivot != column) ++result.row_swaps;
        std::swap(rows[pivot], rows[column]);
        std::swap(right[pivot], right[column]);
        const double diagonal = rows[column].at(column);
        for (std::size_t row = column + 1; row < size; ++row) {
            const auto entry = rows[row].find(column);
            if (entry == rows[row].end()) continue;
            const double factor = entry->second / diagonal;
            if (!std::isfinite(factor)) return result;
            rows[row].erase(entry);
            for (auto upper = rows[column].upper_bound(column);
                 upper != rows[column].end(); ++upper) {
                const double updated = rows[row][upper->first] - factor * upper->second;
                if (!std::isfinite(updated)) return result;
                if (updated == 0.0) rows[row].erase(upper->first);
                else rows[row][upper->first] = updated;
            }
            right[row] -= factor * right[column];
            if (!std::isfinite(right[row])) return result;
        }
    }
    std::vector<double> ordered_solution(size, 0.0);
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const std::size_t row = size - reverse - 1;
        const auto diagonal = rows[row].find(row);
        if (diagonal == rows[row].end()) return result;
        double value = right[row];
        for (auto entry = rows[row].upper_bound(row); entry != rows[row].end(); ++entry) {
            value -= entry->second * ordered_solution[entry->first];
        }
        ordered_solution[row] = value / diagonal->second;
    }
    if (!finite(ordered_solution)) return result;
    result.solution.assign(size, 0.0);
    for (std::size_t position = 0; position < size; ++position) {
        result.solution[result.column_order[position]] = ordered_solution[position];
        result.upper_nonzeros += rows[position].size();
    }
    result.solved = finite(result.solution);
    return result;
}

Preconditioner jacobi_preconditioner(const LinearSystem& system) {
    std::vector<double> inverse_diagonal(system.size());
    for (std::size_t index = 0; index < system.size(); ++index) {
        const double diagonal = system.coefficient(index, index);
        if (std::abs(diagonal) < 1.0e-14 || !std::isfinite(diagonal)) return {};
        inverse_diagonal[index] = 1.0 / diagonal;
    }
    return [inverse_diagonal = std::move(inverse_diagonal)](
               const std::vector<double>& residual,
               std::vector<double>& result) {
        if (residual.size() != inverse_diagonal.size()) return false;
        result.resize(residual.size());
        for (std::size_t index = 0; index < residual.size(); ++index) {
            result[index] = inverse_diagonal[index] * residual[index];
        }
        return finite(result);
    };
}

Preconditioner symmetric_gauss_seidel_preconditioner(
    const LinearSystem& system,
    double relaxation) {
    const auto size = system.size();
    if (size == 0 || !(relaxation > 0.0) || !(relaxation < 2.0) ||
        !std::isfinite(relaxation)) {
        return {};
    }
    std::vector<double> diagonal(size);
    for (std::size_t row = 0; row < size; ++row) {
        diagonal[row] = system.coefficient(row, row);
        if (!std::isfinite(diagonal[row]) || diagonal[row] == 0.0) return {};
    }
    const auto row_entries = [system](std::size_t row, auto&& visitor) {
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                visitor(system.sparsity.column_indices[offset],
                        system.sparse_values[offset]);
            }
        } else {
            for (std::size_t column = 0; column < system.size(); ++column) {
                if (system.matrix[row][column] != 0.0) {
                    visitor(column, system.matrix[row][column]);
                }
            }
        }
    };
    return [size, diagonal = std::move(diagonal), relaxation,
            row_entries = std::move(row_entries)](
               const std::vector<double>& residual,
               std::vector<double>& output) {
        if (residual.size() != size) return false;
        std::vector<double> forward(size);
        for (std::size_t row = 0; row < size; ++row) {
            auto value = residual[row];
            row_entries(row, [&](std::size_t column, double coefficient) {
                if (column < row) value -= coefficient * forward[column];
            });
            forward[row] = relaxation * value / diagonal[row];
        }
        std::vector<double> scaled(size);
        const auto diagonal_scale = (2.0 - relaxation) / relaxation;
        for (std::size_t row = 0; row < size; ++row) {
            scaled[row] = diagonal_scale * diagonal[row] * forward[row];
        }
        output.assign(size, 0.0);
        for (std::size_t reverse = 0; reverse < size; ++reverse) {
            const auto row = size - reverse - 1;
            auto value = scaled[row];
            row_entries(row, [&](std::size_t column, double coefficient) {
                if (column > row) value -= coefficient * output[column];
            });
            output[row] = relaxation * value / diagonal[row];
        }
        return finite(output);
    };
}

Preconditioner incomplete_cholesky_zero_preconditioner(
    const LinearSystem& system,
    const SparsityPattern& sparsity) {
    const std::size_t size = system.size();
    if (!system.symmetric || !system.positive_definite ||
        sparsity.row_count != size || sparsity.column_count != size) return {};
    std::vector<std::vector<std::pair<std::size_t, double>>> lower(size);
    std::vector<double> diagonal(size);
    for (std::size_t row = 0; row < size; ++row) {
        for (const auto column : sparsity.row(row)) {
            if (column >= row) continue;
            double value = system.coefficient(row, column);
            std::size_t left{};
            std::size_t right{};
            while (left < lower[row].size() && right < lower[column].size()) {
                const auto left_column = lower[row][left].first;
                const auto right_column = lower[column][right].first;
                if (left_column >= column || right_column >= column) break;
                if (left_column == right_column) {
                    value -= lower[row][left].second * lower[column][right].second;
                    ++left;
                    ++right;
                } else if (left_column < right_column) {
                    ++left;
                } else {
                    ++right;
                }
            }
            if (!(diagonal[column] > 0.0)) return {};
            value /= diagonal[column];
            if (!std::isfinite(value)) return {};
            lower[row].emplace_back(column, value);
        }
        double diagonal_value = system.coefficient(row, row);
        for (const auto& __entry : lower[row]) {

            const auto& column = __entry.first;

            const auto& value = __entry.second;
            (void)column;
            diagonal_value -= value * value;
        }
        if (!(diagonal_value > 1.0e-14) || !std::isfinite(diagonal_value)) return {};
        diagonal[row] = std::sqrt(diagonal_value);
    }
    std::vector<std::vector<std::pair<std::size_t, double>>> upper(size);
    for (std::size_t row = 0; row < size; ++row) {
        for (const auto& __entry : lower[row]) {

            const auto& column = __entry.first;

            const auto& value = __entry.second;
            upper[column].emplace_back(row, value);
        }
    }
    return [lower = std::move(lower), upper = std::move(upper),
            diagonal = std::move(diagonal),
            intermediate = std::vector<double>(size)](
               const std::vector<double>& residual,
               std::vector<double>& result) mutable {
        const std::size_t size = lower.size();
        if (residual.size() != size) return false;
        for (std::size_t row = 0; row < size; ++row) {
            double value = residual[row];
            for (const auto& __entry : lower[row]) {

                const auto& column = __entry.first;

                const auto& factor = __entry.second;
                value -= factor * intermediate[column];
            }
            intermediate[row] = value / diagonal[row];
        }
        result.assign(size, 0.0);
        for (std::size_t reverse = 0; reverse < size; ++reverse) {
            const std::size_t row = size - reverse - 1;
            double value = intermediate[row];
            for (const auto& __entry : upper[row]) {

                const auto& column = __entry.first;

                const auto& factor = __entry.second;
                value -= factor * result[column];
            }
            result[row] = value / diagonal[row];
        }
        return finite(result);
    };
}

Preconditioner incomplete_lu_zero_preconditioner(
    const LinearSystem& system,
    const SparsityPattern& sparsity) {
    const std::size_t size = system.size();
    if (size == 0 || sparsity.row_count != size || sparsity.column_count != size) return {};
    using Entry = std::pair<std::size_t, double>;
    std::vector<std::vector<Entry>> factors(size);
    const auto find_entry = [](std::vector<Entry>& row, std::size_t column) {
        const auto iterator = std::lower_bound(
            row.begin(), row.end(), column,
            [](const Entry& entry, std::size_t value) { return entry.first < value; });
        return iterator != row.end() && iterator->first == column ? iterator : row.end();
    };
    for (std::size_t row = 0; row < size; ++row) {
        factors[row].reserve(sparsity.row(row).size() + 1);
        for (const auto column : sparsity.row(row)) {
            factors[row].emplace_back(column, system.coefficient(row, column));
        }
        if (find_entry(factors[row], row) == factors[row].end()) {
            factors[row].emplace_back(row, system.coefficient(row, row));
            std::sort(factors[row].begin(), factors[row].end());
        }
    }
    for (std::size_t row = 0; row < size; ++row) {
        for (auto entry = factors[row].begin();
             entry != factors[row].end() && entry->first < row; ++entry) {
            const auto pivot_row = entry->first;
            const auto pivot = find_entry(factors[pivot_row], pivot_row);
            if (pivot == factors[pivot_row].end() ||
                std::abs(pivot->second) < 1.0e-14 || !std::isfinite(pivot->second)) {
                return {};
            }
            entry->second /= pivot->second;
            if (!std::isfinite(entry->second)) return {};
            for (auto upper = std::next(pivot);
                 upper != factors[pivot_row].end(); ++upper) {
                const auto target = find_entry(factors[row], upper->first);
                if (target != factors[row].end()) {
                    target->second -= entry->second * upper->second;
                    if (!std::isfinite(target->second)) return {};
                }
            }
        }
        const auto diagonal = find_entry(factors[row], row);
        if (diagonal == factors[row].end() ||
            std::abs(diagonal->second) < 1.0e-14 || !std::isfinite(diagonal->second)) return {};
    }
    return [factors = std::move(factors)](
               const std::vector<double>& residual,
               std::vector<double>& result) {
        const std::size_t size = factors.size();
        if (residual.size() != size) return false;
        std::vector<double> intermediate(size);
        for (std::size_t row = 0; row < size; ++row) {
            double value = residual[row];
            for (const auto& __entry : factors[row]) {

                const auto& column = __entry.first;

                const auto& factor = __entry.second;
                if (column >= row) break;
                value -= factor * intermediate[column];
            }
            intermediate[row] = value;
        }
        result.assign(size, 0.0);
        for (std::size_t reverse = 0; reverse < size; ++reverse) {
            const std::size_t row = size - reverse - 1;
            double value = intermediate[row];
            double diagonal{};
            for (const auto& __entry : factors[row]) {

                const auto& column = __entry.first;

                const auto& factor = __entry.second;
                if (column == row) diagonal = factor;
                else if (column > row) value -= factor * result[column];
            }
            if (std::abs(diagonal) < 1.0e-14) return false;
            result[row] = value / diagonal;
        }
        return finite(result);
    };
}

Preconditioner incomplete_lu_threshold_preconditioner(
    const LinearSystem& system,
    double drop_tolerance,
    std::size_t maximum_entries_per_triangle) {
    const std::size_t size = system.size();
    if (size == 0 || (!system.has_dense_matrix() && !system.has_sparse_matrix()) ||
        !std::isfinite(drop_tolerance) || drop_tolerance < 0.0 ||
        maximum_entries_per_triangle == 0) return {};
    using SparseRow = std::map<std::size_t, double>;
    std::vector<SparseRow> factors(size);
    const auto retain_largest = [maximum_entries_per_triangle](
                                    SparseRow& row,
                                    std::size_t begin,
                                    std::size_t end) {
        std::vector<std::pair<double, std::size_t>> entries;
        for (auto iterator = row.lower_bound(begin);
             iterator != row.end() && iterator->first < end; ++iterator) {
            entries.emplace_back(std::abs(iterator->second), iterator->first);
        }
        if (entries.size() <= maximum_entries_per_triangle) return;
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            if (left.first != right.first) return left.first > right.first;
            return left.second < right.second;
        });
        for (std::size_t index = maximum_entries_per_triangle;
             index < entries.size(); ++index) {
            row.erase(entries[index].second);
        }
    };
    const auto row_values = [&](std::size_t row) {
        SparseRow values;
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const double value = system.sparse_values[offset];
                if (value != 0.0) {
                    values[system.sparsity.column_indices[offset]] = value;
                }
            }
        } else {
            for (std::size_t column = 0; column < size; ++column) {
                const double value = system.matrix[row][column];
                if (value != 0.0) values[column] = value;
            }
        }
        return values;
    };
    for (std::size_t row = 0; row < size; ++row) {
        if (system.has_dense_matrix() &&
            (system.matrix[row].size() != size || !finite(system.matrix[row]))) {
            return {};
        }
        SparseRow working = row_values(row);
        double row_scale = 0.0;
        for (const auto& __entry : working) {

            const auto& column = __entry.first;

            const auto& value = __entry.second;
            (void)column;
            if (!std::isfinite(value)) return {};
            row_scale = std::max(row_scale, std::abs(value));
        }
        if (!(row_scale > 0.0)) return {};
        const double threshold = drop_tolerance * row_scale;
        auto pivot_entry = working.begin();
        while (pivot_entry != working.end() && pivot_entry->first < row) {
            const std::size_t pivot = pivot_entry->first;
            if (std::abs(pivot_entry->second) <= threshold) {
                pivot_entry = working.erase(pivot_entry);
                continue;
            }
            const auto diagonal = factors[pivot].find(pivot);
            if (diagonal == factors[pivot].end() || diagonal->second == 0.0 ||
                !std::isfinite(diagonal->second)) return {};
            pivot_entry->second /= diagonal->second;
            if (!std::isfinite(pivot_entry->second)) return {};
            const double multiplier = pivot_entry->second;
            for (auto upper = std::next(diagonal); upper != factors[pivot].end(); ++upper) {
                auto [target, inserted] = working.try_emplace(upper->first, 0.0);
                (void)inserted;
                target->second -= multiplier * upper->second;
                if (!std::isfinite(target->second)) return {};
                if (target->first != row && std::abs(target->second) <= threshold) {
                    working.erase(target);
                }
            }
            pivot_entry = working.upper_bound(pivot);
        }
        for (auto iterator = working.begin(); iterator != working.end();) {
            if (iterator->first != row && std::abs(iterator->second) <= threshold) {
                iterator = working.erase(iterator);
            } else {
                ++iterator;
            }
        }
        retain_largest(working, 0, row);
        retain_largest(working, row + 1, size);
        const auto diagonal = working.find(row);
        if (diagonal == working.end() || diagonal->second == 0.0 ||
            !std::isfinite(diagonal->second)) return {};
        factors[row] = std::move(working);
    }
    return [factors = std::move(factors)](
               const std::vector<double>& residual,
               std::vector<double>& result) {
        const std::size_t size = factors.size();
        if (residual.size() != size || !finite(residual)) return false;
        std::vector<double> intermediate(size);
        for (std::size_t row = 0; row < size; ++row) {
            double value = residual[row];
            for (const auto& [column, factor] : factors[row]) {
                if (column >= row) break;
                value -= factor * intermediate[column];
            }
            intermediate[row] = value;
        }
        result.assign(size, 0.0);
        for (std::size_t reverse = 0; reverse < size; ++reverse) {
            const std::size_t row = size - reverse - 1;
            double value = intermediate[row];
            const auto diagonal = factors[row].find(row);
            if (diagonal == factors[row].end() || diagonal->second == 0.0) return false;
            for (auto upper = std::next(diagonal); upper != factors[row].end(); ++upper) {
                value -= upper->second * result[upper->first];
            }
            result[row] = value / diagonal->second;
        }
        return finite(result);
    };
}

KrylovResult preconditioned_conjugate_gradient(
    const LinearSystem& system,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations) {
    if (!system.symmetric || !system.positive_definite) {
        KrylovResult result;
        result.breakdown = true;
        result.reason = "PCG requires an SPD system";
        return result;
    }
    return preconditioned_conjugate_gradient(
        system.size(),
        [&](const std::vector<double>& input, std::vector<double>& output) {
            output = system.multiply(input);
            return output.size() == system.size() && finite(output);
        },
        system.right_hand_side, initial, preconditioner,
        absolute_tolerance, relative_tolerance, maximum_iterations);
}

KrylovResult preconditioned_conjugate_gradient(
    std::size_t size,
    const LinearOperator& linear_operator,
    const std::vector<double>& right_hand_side,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations) {
    KrylovResult result;
    if (!linear_operator || !preconditioner || initial.size() != size ||
        right_hand_side.size() != size || maximum_iterations <= 0) {
        result.breakdown = true;
        result.reason = "invalid PCG input";
        return result;
    }
    result.solution = initial;
    result.residual_history.reserve(
        static_cast<std::size_t>(maximum_iterations) + 1);
    std::vector<double> product(size);
    if (!linear_operator(result.solution, product) || product.size() != size ||
        !finite(product)) {
        result.breakdown = true;
        result.reason = "linear operator failed";
        return result;
    }
    std::vector<double> residual(size);
    for (std::size_t index = 0; index < size; ++index) {
        residual[index] = right_hand_side[index] - product[index];
    }
    const double right_norm = norm_inf(right_hand_side);
    const double threshold = absolute_tolerance + relative_tolerance * std::max(1.0, right_norm);
    result.residual_history.push_back(norm_inf(residual));
    if (result.residual_history.back() <= threshold) {
        result.converged = true;
        result.reason = "initial guess satisfies true residual";
        return result;
    }
    std::vector<double> preconditioned(size);
    if (!preconditioner(residual, preconditioned)) {
        result.breakdown = true;
        result.reason = "preconditioner failed";
        return result;
    }
    std::vector<double> direction = preconditioned;
    std::vector<double> matrix_direction(size);
    double residual_dot_preconditioned = dot(residual, preconditioned);
    if (!(residual_dot_preconditioned > 0.0) || !std::isfinite(residual_dot_preconditioned)) {
        result.breakdown = true;
        result.reason = "preconditioner is not positive definite";
        return result;
    }
    int non_improving = 0;
    for (int iteration = 1; iteration <= maximum_iterations; ++iteration) {
        if (!linear_operator(direction, matrix_direction) ||
            matrix_direction.size() != size || !finite(matrix_direction)) {
            result.breakdown = true;
            result.iterations = iteration - 1;
            result.reason = "linear operator failed";
            return result;
        }
        const double curvature = dot(direction, matrix_direction);
        if (!(curvature > 0.0) || !std::isfinite(curvature)) {
            result.breakdown = true;
            result.iterations = iteration - 1;
            result.reason = "non-positive Krylov curvature";
            return result;
        }
        const double alpha = residual_dot_preconditioned / curvature;
        for (std::size_t index = 0; index < size; ++index) {
            result.solution[index] += alpha * direction[index];
            residual[index] -= alpha * matrix_direction[index];
        }
        const double residual_norm = norm_inf(residual);
        if (residual_norm >=
            result.residual_history.back() * (1.0 - 1.0e-12)) {
            ++non_improving;
        } else {
            non_improving = 0;
        }
        result.residual_history.push_back(residual_norm);
        result.iterations = iteration;
        if (residual_norm <= threshold) {
            if (!linear_operator(result.solution, product) ||
                product.size() != size || !finite(product)) {
                result.breakdown = true;
                result.reason = "linear operator failed during residual gate";
                return result;
            }
            for (std::size_t index = 0; index < size; ++index) {
                residual[index] = right_hand_side[index] - product[index];
            }
            const auto true_residual_norm = norm_inf(residual);
            result.residual_history.back() = true_residual_norm;
            if (true_residual_norm <= threshold) {
                result.converged = true;
                result.reason = "true residual converged";
                return result;
            }
        }
        if (non_improving >= 4) {
            result.stagnated = true;
            result.reason = "true residual stagnated";
            return result;
        }
        if (!preconditioner(residual, preconditioned)) {
            result.breakdown = true;
            result.reason = "preconditioner failed";
            return result;
        }
        const double next_dot = dot(residual, preconditioned);
        if (!(next_dot > 0.0) || !std::isfinite(next_dot)) {
            result.breakdown = true;
            result.reason = "preconditioner lost positive definiteness";
            return result;
        }
        const double beta = next_dot / residual_dot_preconditioned;
        for (std::size_t index = 0; index < size; ++index) {
            direction[index] = preconditioned[index] + beta * direction[index];
        }
        residual_dot_preconditioned = next_dot;
    }
    result.reason = "maximum Krylov iterations reached";
    return result;
}

KrylovResult restarted_gmres(
    const LinearSystem& system,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations,
    int restart_dimension) {
    return restarted_gmres(
        system.size(),
        [&](const std::vector<double>& input, std::vector<double>& output) {
            output = system.multiply(input);
            return output.size() == system.size() && finite(output);
        },
        system.right_hand_side, initial, preconditioner,
        absolute_tolerance, relative_tolerance,
        maximum_iterations, restart_dimension);
}

KrylovResult restarted_gmres(
    std::size_t size,
    const LinearOperator& linear_operator,
    const std::vector<double>& right_hand_side,
    const std::vector<double>& initial,
    const Preconditioner& preconditioner,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations,
    int restart_dimension) {
    KrylovResult result;
    if (!linear_operator || !preconditioner || initial.size() != size ||
        maximum_iterations <= 0 || restart_dimension <= 0 ||
        right_hand_side.size() != size) {
        result.breakdown = true;
        result.reason = "invalid GMRES input";
        return result;
    }
    result.solution = initial;
    const double threshold = absolute_tolerance + relative_tolerance *
        std::max(1.0, norm_inf(right_hand_side));
    int non_improving = 0;
    while (result.iterations < maximum_iterations) {
        std::vector<double> product;
        if (!linear_operator(result.solution, product) || product.size() != size ||
            !finite(product)) {
            result.breakdown = true;
            result.reason = "linear operator failed";
            return result;
        }
        std::vector<double> residual(size);
        for (std::size_t index = 0; index < size; ++index) {
            residual[index] = right_hand_side[index] - product[index];
        }
        const double true_norm = norm_inf(residual);
        if (result.residual_history.empty() ||
            true_norm != result.residual_history.back()) {
            result.residual_history.push_back(true_norm);
        }
        if (true_norm <= threshold) {
            result.converged = true;
            result.reason = "true residual converged";
            return result;
        }
        std::vector<double> preconditioned_residual;
        if (!preconditioner(residual, preconditioned_residual)) {
            result.breakdown = true;
            result.reason = "preconditioner failed";
            return result;
        }
        const double beta = norm_two(preconditioned_residual);
        if (!(beta > 0.0) || !std::isfinite(beta)) {
            result.breakdown = true;
            result.reason = "preconditioned residual breakdown";
            return result;
        }
        const int cycle = std::min({
            restart_dimension,
            maximum_iterations - result.iterations,
            static_cast<int>(size)});
        std::vector<std::vector<double>> basis(
            static_cast<std::size_t>(cycle + 1), std::vector<double>(size));
        for (std::size_t index = 0; index < size; ++index) {
            basis[0][index] = preconditioned_residual[index] / beta;
        }
        std::vector<std::vector<double>> hessenberg(
            static_cast<std::size_t>(cycle + 1),
            std::vector<double>(static_cast<std::size_t>(cycle)));
        std::vector<double> cosines(static_cast<std::size_t>(cycle));
        std::vector<double> sines(static_cast<std::size_t>(cycle));
        std::vector<double> transformed_rhs(static_cast<std::size_t>(cycle + 1));
        transformed_rhs[0] = beta;
        auto cycle_solution = result.solution;
        for (int column = 0; column < cycle; ++column) {
            std::vector<double> action;
            if (!linear_operator(
                    basis[static_cast<std::size_t>(column)], action) ||
                action.size() != size || !finite(action)) {
                result.breakdown = true;
                result.reason = "linear operator failed";
                return result;
            }
            std::vector<double> preconditioned_action;
            if (!preconditioner(action, preconditioned_action)) {
                result.breakdown = true;
                result.reason = "preconditioner failed";
                return result;
            }
            for (int row = 0; row <= column; ++row) {
                hessenberg[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] =
                    dot(preconditioned_action, basis[static_cast<std::size_t>(row)]);
                for (std::size_t index = 0; index < size; ++index) {
                    preconditioned_action[index] -=
                        hessenberg[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] *
                        basis[static_cast<std::size_t>(row)][index];
                }
            }
            const double next_norm = norm_two(preconditioned_action);
            hessenberg[static_cast<std::size_t>(column + 1)]
                       [static_cast<std::size_t>(column)] = next_norm;
            if (next_norm > 1.0e-14) {
                for (std::size_t index = 0; index < size; ++index) {
                    basis[static_cast<std::size_t>(column + 1)][index] =
                        preconditioned_action[index] / next_norm;
                }
            }
            for (int row = 0; row < column; ++row) {
                const double upper =
                    hessenberg[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
                const double lower =
                    hessenberg[static_cast<std::size_t>(row + 1)][static_cast<std::size_t>(column)];
                hessenberg[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] =
                    cosines[static_cast<std::size_t>(row)] * upper +
                    sines[static_cast<std::size_t>(row)] * lower;
                hessenberg[static_cast<std::size_t>(row + 1)][static_cast<std::size_t>(column)] =
                    -sines[static_cast<std::size_t>(row)] * upper +
                    cosines[static_cast<std::size_t>(row)] * lower;
            }
            const double diagonal =
                hessenberg[static_cast<std::size_t>(column)][static_cast<std::size_t>(column)];
            const double subdiagonal =
                hessenberg[static_cast<std::size_t>(column + 1)][static_cast<std::size_t>(column)];
            const double rotation_norm = std::hypot(diagonal, subdiagonal);
            if (!(rotation_norm > 1.0e-14) || !std::isfinite(rotation_norm)) {
                result.breakdown = true;
                result.reason = "Arnoldi breakdown";
                return result;
            }
            cosines[static_cast<std::size_t>(column)] = diagonal / rotation_norm;
            sines[static_cast<std::size_t>(column)] = subdiagonal / rotation_norm;
            hessenberg[static_cast<std::size_t>(column)][static_cast<std::size_t>(column)] =
                rotation_norm;
            hessenberg[static_cast<std::size_t>(column + 1)][static_cast<std::size_t>(column)] = 0.0;
            const double rhs_upper = transformed_rhs[static_cast<std::size_t>(column)];
            transformed_rhs[static_cast<std::size_t>(column)] =
                cosines[static_cast<std::size_t>(column)] * rhs_upper;
            transformed_rhs[static_cast<std::size_t>(column + 1)] =
                -sines[static_cast<std::size_t>(column)] * rhs_upper;
            std::vector<double> coefficients(static_cast<std::size_t>(column + 1));
            for (int reverse = column; reverse >= 0; --reverse) {
                double value = transformed_rhs[static_cast<std::size_t>(reverse)];
                for (int inner = reverse + 1; inner <= column; ++inner) {
                    value -= hessenberg[static_cast<std::size_t>(reverse)]
                                        [static_cast<std::size_t>(inner)] *
                        coefficients[static_cast<std::size_t>(inner)];
                }
                coefficients[static_cast<std::size_t>(reverse)] = value /
                    hessenberg[static_cast<std::size_t>(reverse)]
                               [static_cast<std::size_t>(reverse)];
            }
            cycle_solution = result.solution;
            for (int basis_index = 0; basis_index <= column; ++basis_index) {
                for (std::size_t index = 0; index < size; ++index) {
                    cycle_solution[index] +=
                        coefficients[static_cast<std::size_t>(basis_index)] *
                        basis[static_cast<std::size_t>(basis_index)][index];
                }
            }
            std::vector<double> candidate_product;
            if (!linear_operator(cycle_solution, candidate_product) ||
                candidate_product.size() != size || !finite(candidate_product)) {
                result.breakdown = true;
                result.reason = "linear operator failed";
                return result;
            }
            double candidate_residual = 0.0;
            for (std::size_t index = 0; index < size; ++index) {
                candidate_residual = std::max(
                    candidate_residual,
                    std::abs(right_hand_side[index] - candidate_product[index]));
            }
            if (candidate_residual >= result.residual_history.back() * (1.0 - 1.0e-12)) {
                ++non_improving;
            } else {
                non_improving = 0;
            }
            result.residual_history.push_back(candidate_residual);
            ++result.iterations;
            if (candidate_residual <= threshold) {
                result.solution = std::move(cycle_solution);
                result.converged = true;
                result.reason = "true residual converged";
                return result;
            }
            if (non_improving >= 6) {
                result.solution = std::move(cycle_solution);
                result.stagnated = true;
                result.reason = "true residual stagnated";
                return result;
            }
            if (next_norm <= 1.0e-14) break;
        }
        result.solution = std::move(cycle_solution);
    }
    result.reason = "maximum Krylov iterations reached";
    return result;
}

KrylovResult least_squares_qr(
    const LinearSystem& system,
    const std::vector<double>& initial,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations) {
    KrylovResult result;
    const std::size_t size = system.size();
    result.solution = initial;
    if (size == 0 || initial.size() != size ||
        system.right_hand_side.size() != size || maximum_iterations <= 0 ||
        (!system.has_sparse_matrix() && !system.has_dense_matrix()) ||
        !finite(initial) || !finite(system.right_hand_side)) {
        result.breakdown = true;
        result.reason = "invalid LSQR input";
        return result;
    }

    double matrix_norm_inf{};
    std::vector<double> row_maximum(size);
    for (std::size_t row = 0; row < size; ++row) {
        double row_sum{};
        if (system.has_sparse_matrix()) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const double magnitude = std::abs(system.sparse_values[offset]);
                row_sum += magnitude;
                row_maximum[row] = std::max(row_maximum[row], magnitude);
            }
        } else {
            for (const double value : system.matrix[row]) {
                const double magnitude = std::abs(value);
                row_sum += magnitude;
                row_maximum[row] = std::max(row_maximum[row], magnitude);
            }
        }
        matrix_norm_inf = std::max(matrix_norm_inf, row_sum);
    }
    std::vector<double> column_scale(size, 1.0);
    std::vector<double> row_scale(size, 1.0);
    constexpr int equilibration_sweeps = 6;
    for (int sweep = 0; sweep < equilibration_sweeps; ++sweep) {
        std::fill(row_maximum.begin(), row_maximum.end(), 0.0);
        for (std::size_t row = 0; row < size; ++row) {
            if (system.has_sparse_matrix()) {
                for (std::size_t offset = system.sparsity.row_offsets[row];
                     offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                    const std::size_t column = system.sparsity.column_indices[offset];
                    row_maximum[row] = std::max(
                        row_maximum[row],
                        std::abs(row_scale[row] * system.sparse_values[offset] *
                                 column_scale[column]));
                }
            } else {
                for (std::size_t column = 0; column < size; ++column) {
                    row_maximum[row] = std::max(
                        row_maximum[row],
                        std::abs(row_scale[row] * system.matrix[row][column] *
                                 column_scale[column]));
                }
            }
            if (row_maximum[row] > 0.0) {
                const double updated = row_scale[row] /
                    std::sqrt(row_maximum[row]);
                if (std::isfinite(updated)) row_scale[row] = updated;
            }
        }

        std::vector<double> column_maximum(size);
        for (std::size_t row = 0; row < size; ++row) {
            if (system.has_sparse_matrix()) {
                for (std::size_t offset = system.sparsity.row_offsets[row];
                     offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                    const std::size_t column = system.sparsity.column_indices[offset];
                    column_maximum[column] = std::max(
                        column_maximum[column],
                        std::abs(row_scale[row] * system.sparse_values[offset] *
                                 column_scale[column]));
                }
            } else {
                for (std::size_t column = 0; column < size; ++column) {
                    column_maximum[column] = std::max(
                        column_maximum[column],
                        std::abs(row_scale[row] * system.matrix[row][column] *
                                 column_scale[column]));
                }
            }
        }
        for (std::size_t column = 0; column < size; ++column) {
            if (column_maximum[column] > 0.0) {
                const double updated = column_scale[column] /
                    std::sqrt(column_maximum[column]);
                if (std::isfinite(updated)) column_scale[column] = updated;
            }
        }
    }
    const auto scaled_forward = [&](const std::vector<double>& input) {
        std::vector<double> unscaled_input(size);
        for (std::size_t column = 0; column < size; ++column) {
            unscaled_input[column] = column_scale[column] * input[column];
        }
        auto output = system.multiply(unscaled_input);
        if (output.size() == size) {
            for (std::size_t row = 0; row < size; ++row) {
                output[row] *= row_scale[row];
            }
        }
        return output;
    };
    const auto scaled_transpose = [&](const std::vector<double>& input) {
        std::vector<double> scaled_input(size);
        for (std::size_t row = 0; row < size; ++row) {
            scaled_input[row] = row_scale[row] * input[row];
        }
        auto output = system.multiply_transpose(scaled_input);
        if (output.size() == size) {
            for (std::size_t column = 0; column < size; ++column) {
                output[column] *= column_scale[column];
            }
        }
        return output;
    };
    const auto restore_solution = [&](const std::vector<double>& scaled_solution) {
        std::vector<double> restored(size);
        for (std::size_t column = 0; column < size; ++column) {
            restored[column] = column_scale[column] * scaled_solution[column];
        }
        return restored;
    };
    const double right_norm_inf = norm_inf(system.right_hand_side);
    const auto true_residual = [&](const std::vector<double>& solution,
                                   double& residual_inf) {
        const auto product = system.multiply(solution);
        if (product.size() != size || !finite(product)) return false;
        residual_inf = 0.0;
        for (std::size_t index = 0; index < size; ++index) {
            residual_inf = std::max(
                residual_inf,
                std::abs(system.right_hand_side[index] - product[index]));
        }
        return std::isfinite(residual_inf);
    };
    const auto accepted = [&](const std::vector<double>& solution,
                              double residual_inf) {
        const double scale = matrix_norm_inf * norm_inf(solution) + right_norm_inf;
        return residual_inf <= absolute_tolerance + relative_tolerance * scale;
    };

    std::vector<double> scaled_solution(size);
    for (std::size_t column = 0; column < size; ++column) {
        scaled_solution[column] = initial[column] / column_scale[column];
    }
    auto initial_product = scaled_forward(scaled_solution);
    if (initial_product.size() != size || !finite(initial_product)) {
        result.breakdown = true;
        result.reason = "LSQR initial operator application failed";
        return result;
    }
    std::vector<double> left(size);
    for (std::size_t index = 0; index < size; ++index) {
        left[index] = row_scale[index] * system.right_hand_side[index] -
            initial_product[index];
    }
    double beta = norm_two(left);
    double initial_residual_inf{};
    if (!true_residual(result.solution, initial_residual_inf)) {
        result.breakdown = true;
        result.reason = "LSQR initial true residual evaluation failed";
        return result;
    }
    result.residual_history.push_back(initial_residual_inf);
    if (accepted(result.solution, result.residual_history.back())) {
        result.converged = true;
        result.reason = "initial LSQR residual passed";
        return result;
    }
    if (!(beta > 0.0) || !std::isfinite(beta)) {
        result.breakdown = true;
        result.reason = "LSQR left bidiagonalization breakdown";
        return result;
    }
    for (double& value : left) value /= beta;

    std::vector<double> right = scaled_transpose(left);
    if (right.size() != size || !finite(right)) {
        result.breakdown = true;
        result.reason = "LSQR transpose operator application failed";
        return result;
    }
    double alpha = norm_two(right);
    if (!(alpha > 0.0) || !std::isfinite(alpha)) {
        result.breakdown = true;
        result.reason = "LSQR right bidiagonalization breakdown";
        return result;
    }
    for (double& value : right) value /= alpha;
    std::vector<double> direction = right;
    double rho_bar = alpha;
    double phi_bar = beta;
    int non_improving{};

    for (int iteration = 0; iteration < maximum_iterations; ++iteration) {
        auto next_left = scaled_forward(right);
        if (next_left.size() != size || !finite(next_left)) {
            result.breakdown = true;
            result.reason = "LSQR forward operator application failed";
            return result;
        }
        for (std::size_t index = 0; index < size; ++index) {
            next_left[index] -= alpha * left[index];
        }
        beta = norm_two(next_left);
        if (!std::isfinite(beta)) {
            result.breakdown = true;
            result.reason = "LSQR non-finite left norm";
            return result;
        }
        if (beta > 0.0) {
            for (double& value : next_left) value /= beta;
        }

        auto next_right = scaled_transpose(next_left);
        if (next_right.size() != size || !finite(next_right)) {
            result.breakdown = true;
            result.reason = "LSQR transpose operator application failed";
            return result;
        }
        for (std::size_t index = 0; index < size; ++index) {
            next_right[index] -= beta * right[index];
        }
        alpha = norm_two(next_right);
        if (!std::isfinite(alpha)) {
            result.breakdown = true;
            result.reason = "LSQR non-finite right norm";
            return result;
        }
        if (alpha > 0.0) {
            for (double& value : next_right) value /= alpha;
        }

        const double rho = std::hypot(rho_bar, beta);
        if (!(rho > 0.0) || !std::isfinite(rho)) {
            result.breakdown = true;
            result.reason = "LSQR plane-rotation breakdown";
            return result;
        }
        const double cosine = rho_bar / rho;
        const double sine = beta / rho;
        const double theta = sine * alpha;
        rho_bar = -cosine * alpha;
        const double phi = cosine * phi_bar;
        phi_bar = sine * phi_bar;
        const double solution_scale = phi / rho;
        const double direction_scale = theta / rho;
        for (std::size_t index = 0; index < size; ++index) {
            scaled_solution[index] += solution_scale * direction[index];
            direction[index] = next_right[index] - direction_scale * direction[index];
        }
        result.solution = restore_solution(scaled_solution);
        left = std::move(next_left);
        right = std::move(next_right);
        ++result.iterations;

        double residual_inf{};
        if (!true_residual(result.solution, residual_inf)) {
            result.breakdown = true;
            result.reason = "LSQR true residual evaluation failed";
            return result;
        }
        if (residual_inf >= result.residual_history.back() * (1.0 - 1.0e-13)) {
            ++non_improving;
        } else {
            non_improving = 0;
        }
        result.residual_history.push_back(residual_inf);
        if (accepted(result.solution, residual_inf)) {
            result.converged = true;
            result.reason = "true residual converged";
            return result;
        }
        if ((alpha <= 1.0e-15 && beta <= 1.0e-15) || non_improving >= 100) {
            result.stagnated = true;
            result.reason = "LSQR true residual stagnated";
            return result;
        }
    }
    result.reason = "maximum LSQR iterations reached";
    return result;
}

std::unordered_map<std::string, double> linear_solution_values(
    const LinearSystem& system,
    const std::vector<double>& solution,
    const std::unordered_map<std::string, double>& context) {
    if (solution.size() != system.unknowns.size()) {
        throw std::invalid_argument("linear solution shape mismatch");
    }
    auto values = context;
    for (std::size_t index = 0; index < solution.size(); ++index) {
        values[system.unknowns[index]] = solution[index];
    }
    return values;
}

}  // namespace smave
