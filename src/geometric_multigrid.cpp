#include "smave/linear.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace smave {
namespace {

struct Level {
    std::size_t width{};
    std::vector<std::size_t> row_offsets;
    std::vector<std::size_t> columns;
    std::vector<double> values;
    std::vector<double> inverse_diagonal;
    std::vector<std::size_t> aggregate;
    std::size_t coarse_size{};
};

bool finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool multiply(
    const Level& level,
    const std::vector<double>& input,
    std::vector<double>& output) {
    const auto size = level.inverse_diagonal.size();
    if (input.size() != size) return false;
    output.assign(size, 0.0);
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t offset = level.row_offsets[row];
             offset < level.row_offsets[row + 1]; ++offset) {
            output[row] += level.values[offset] * input[level.columns[offset]];
        }
    }
    return finite(output);
}

bool make_level(
    std::size_t width,
    const std::vector<std::map<std::size_t, double>>& rows,
    Level& level) {
    const auto size = rows.size();
    if (width * width != size || size == 0) return false;
    level.width = width;
    level.row_offsets.resize(size + 1);
    level.inverse_diagonal.resize(size);
    for (std::size_t row = 0; row < size; ++row) {
        level.row_offsets[row] = level.columns.size();
        const auto diagonal = rows[row].find(row);
        if (diagonal == rows[row].end() || !(diagonal->second > 0.0) ||
            !std::isfinite(diagonal->second)) {
            return false;
        }
        level.inverse_diagonal[row] = 1.0 / diagonal->second;
        for (const auto& [column, value] : rows[row]) {
            if (value == 0.0) continue;
            level.columns.push_back(column);
            level.values.push_back(value);
        }
    }
    level.row_offsets[size] = level.columns.size();
    return finite(level.values) && finite(level.inverse_diagonal);
}

std::vector<std::map<std::size_t, double>> rows_from_system(
    const LinearSystem& system) {
    const auto size = system.size();
    std::vector<std::map<std::size_t, double>> rows(size);
    if (system.has_sparse_matrix()) {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                rows[row][system.sparsity.column_indices[offset]] +=
                    system.sparse_values[offset];
            }
        }
    } else if (system.has_dense_matrix()) {
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                if (system.matrix[row][column] != 0.0) {
                    rows[row][column] = system.matrix[row][column];
                }
            }
        }
    }
    return rows;
}

std::vector<std::map<std::size_t, double>> aggregate_rows(
    const Level& fine,
    std::size_t coarse_width) {
    std::vector<std::map<std::size_t, double>> coarse(
        coarse_width * coarse_width);
    const auto size = fine.inverse_diagonal.size();
    for (std::size_t row = 0; row < size; ++row) {
        const auto coarse_row = fine.aggregate[row];
        for (std::size_t offset = fine.row_offsets[row];
             offset < fine.row_offsets[row + 1]; ++offset) {
            const auto coarse_column = fine.aggregate[fine.columns[offset]];
            coarse[coarse_row][coarse_column] += fine.values[offset];
        }
    }
    return coarse;
}

bool cholesky_factor(
    const Level& level,
    std::vector<double>& lower) {
    const auto size = level.inverse_diagonal.size();
    std::vector<double> dense(size * size);
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t offset = level.row_offsets[row];
             offset < level.row_offsets[row + 1]; ++offset) {
            dense[row * size + level.columns[offset]] = level.values[offset];
        }
    }
    lower.assign(size * size, 0.0);
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            auto value = dense[row * size + column];
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= lower[row * size + inner] * lower[column * size + inner];
            }
            if (row == column) {
                if (!(value > std::numeric_limits<double>::epsilon()) ||
                    !std::isfinite(value)) return false;
                lower[row * size + column] = std::sqrt(value);
            } else {
                lower[row * size + column] =
                    value / lower[column * size + column];
            }
        }
    }
    return true;
}

bool cholesky_solve(
    const std::vector<double>& lower,
    const std::vector<double>& right,
    std::vector<double>& result,
    std::vector<double>& intermediate) {
    const auto size = right.size();
    if (lower.size() != size * size) return false;
    intermediate.resize(size);
    for (std::size_t row = 0; row < size; ++row) {
        auto value = right[row];
        for (std::size_t column = 0; column < row; ++column) {
            value -= lower[row * size + column] * intermediate[column];
        }
        intermediate[row] = value / lower[row * size + row];
    }
    result.assign(size, 0.0);
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const auto row = size - reverse - 1;
        auto value = intermediate[row];
        for (std::size_t column = row + 1; column < size; ++column) {
            value -= lower[column * size + row] * result[column];
        }
        result[row] = value / lower[row * size + row];
    }
    return finite(result);
}

}  // namespace

struct AggregationMultigrid2D::Impl {
    struct Workspace {
        std::vector<double> product;
        std::vector<double> coarse_right;
        std::vector<double> correction;
        std::vector<double> intermediate;
    };

    std::vector<Level> levels;
    std::vector<double> coarse_cholesky;
    mutable std::vector<Workspace> workspaces;
    std::size_t pre_steps{};
    std::size_t post_steps{};
    double weight{};
    std::string reason;
    std::size_t bytes{};

    bool cycle(
        std::size_t level_index,
        const std::vector<double>& right,
        std::vector<double>& result) const {
        const auto& level = levels[level_index];
        const auto size = level.inverse_diagonal.size();
        if (right.size() != size) return false;
        auto& workspace = workspaces[level_index];
        if (level_index + 1 == levels.size()) {
            return cholesky_solve(
                coarse_cholesky, right, result, workspace.intermediate);
        }
        result.resize(size);
        for (std::size_t index = 0; index < size; ++index) {
            result[index] = weight * level.inverse_diagonal[index] * right[index];
        }
        for (std::size_t step = 1; step < pre_steps; ++step) {
            if (!multiply(level, result, workspace.product)) return false;
            for (std::size_t index = 0; index < size; ++index) {
                result[index] += weight * level.inverse_diagonal[index] *
                    (right[index] - workspace.product[index]);
            }
        }
        if (!multiply(level, result, workspace.product)) return false;
        workspace.coarse_right.assign(level.coarse_size, 0.0);
        for (std::size_t index = 0; index < size; ++index) {
            workspace.coarse_right[level.aggregate[index]] +=
                right[index] - workspace.product[index];
        }
        auto& coarse_correction = workspaces[level_index + 1].correction;
        if (!cycle(level_index + 1, workspace.coarse_right, coarse_correction)) {
            return false;
        }
        for (std::size_t index = 0; index < size; ++index) {
            result[index] += coarse_correction[level.aggregate[index]];
        }
        for (std::size_t step = 0; step < post_steps; ++step) {
            if (!multiply(level, result, workspace.product)) return false;
            for (std::size_t index = 0; index < size; ++index) {
                result[index] += weight * level.inverse_diagonal[index] *
                    (right[index] - workspace.product[index]);
            }
        }
        return finite(result);
    }
};

AggregationMultigrid2D::AggregationMultigrid2D(
    const LinearSystem& system,
    std::size_t grid_width,
    std::size_t maximum_coarse_unknowns,
    std::size_t pre_smoothing_steps,
    std::size_t post_smoothing_steps,
    double smoothing_weight)
    : impl_(std::make_unique<Impl>()) {
    const auto size = system.size();
    if (grid_width * grid_width != size || size == 0 ||
        maximum_coarse_unknowns == 0 || pre_smoothing_steps == 0 ||
        post_smoothing_steps == 0 || !(smoothing_weight > 0.0) ||
        !(smoothing_weight < 1.0) || !std::isfinite(smoothing_weight)) {
        impl_->reason = "invalid aggregation multigrid shape or parameters";
        return;
    }
    impl_->pre_steps = pre_smoothing_steps;
    impl_->post_steps = post_smoothing_steps;
    impl_->weight = smoothing_weight;
    auto rows = rows_from_system(system);
    if (rows.size() != size) {
        impl_->reason = "multigrid requires a valid dense or sparse matrix";
        return;
    }
    auto width = grid_width;
    while (true) {
        Level level;
        if (!make_level(width, rows, level)) {
            impl_->reason = "multigrid level construction failed";
            impl_->levels.clear();
            return;
        }
        const auto level_size = width * width;
        impl_->bytes += level.row_offsets.size() * sizeof(std::size_t) +
            level.columns.size() * sizeof(std::size_t) +
            level.values.size() * sizeof(double) +
            level.inverse_diagonal.size() * sizeof(double);
        if (level_size <= maximum_coarse_unknowns || width == 1) {
            impl_->levels.push_back(std::move(level));
            break;
        }
        const auto coarse_width = (width + 1) / 2;
        level.aggregate.resize(level_size);
        level.coarse_size = coarse_width * coarse_width;
        for (std::size_t row = 0; row < width; ++row) {
            for (std::size_t column = 0; column < width; ++column) {
                level.aggregate[row * width + column] =
                    (row / 2) * coarse_width + column / 2;
            }
        }
        impl_->bytes += level.aggregate.size() * sizeof(std::size_t);
        rows = aggregate_rows(level, coarse_width);
        impl_->levels.push_back(std::move(level));
        width = coarse_width;
    }
    if (!cholesky_factor(impl_->levels.back(), impl_->coarse_cholesky)) {
        impl_->reason = "coarsest Galerkin operator is not SPD";
        impl_->levels.clear();
        return;
    }
    impl_->workspaces.resize(impl_->levels.size());
    for (std::size_t index = 0; index < impl_->levels.size(); ++index) {
        const auto level_size = impl_->levels[index].inverse_diagonal.size();
        impl_->workspaces[index].product.resize(level_size);
        impl_->workspaces[index].correction.resize(level_size);
        impl_->workspaces[index].intermediate.resize(level_size);
        impl_->workspaces[index].coarse_right.resize(
            impl_->levels[index].coarse_size);
        impl_->bytes += (3 * level_size + impl_->levels[index].coarse_size) *
            sizeof(double);
    }
    impl_->bytes += impl_->coarse_cholesky.size() * sizeof(double);
    impl_->reason = "ready";
}

AggregationMultigrid2D::~AggregationMultigrid2D() = default;
AggregationMultigrid2D::AggregationMultigrid2D(
    AggregationMultigrid2D&&) noexcept = default;
AggregationMultigrid2D& AggregationMultigrid2D::operator=(
    AggregationMultigrid2D&&) noexcept = default;

bool AggregationMultigrid2D::valid() const {
    return impl_ != nullptr && !impl_->levels.empty() &&
        !impl_->coarse_cholesky.empty();
}

const std::string& AggregationMultigrid2D::reason() const {
    return impl_->reason;
}

std::size_t AggregationMultigrid2D::levels() const {
    return impl_->levels.size();
}

std::size_t AggregationMultigrid2D::storage_bytes() const {
    return impl_->bytes;
}

bool AggregationMultigrid2D::apply(
    const std::vector<double>& residual,
    std::vector<double>& correction) const {
    return valid() && impl_->cycle(0, residual, correction);
}

struct GeometricFivePointMultigrid2D::Impl {
    struct StencilLevel {
        std::size_t width{};
        std::vector<double> west;
        std::vector<double> east;
        std::vector<double> south;
        std::vector<double> north;
        std::vector<double> diagonal;
        std::vector<double> inverse_diagonal;
    };
    struct Workspace {
        std::vector<double> product;
        std::vector<double> residual;
        std::vector<double> coarse_right;
        std::vector<double> correction;
        std::vector<double> intermediate;
    };

    std::vector<StencilLevel> levels;
    mutable std::vector<Workspace> workspaces;
    std::vector<double> coarse_cholesky;
    std::size_t pre_steps{};
    std::size_t post_steps{};
    double weight{};
    std::size_t bytes{};
    std::string reason;

    static bool multiply(
        const StencilLevel& level,
        const std::vector<double>& input,
        std::vector<double>& output) {
        const auto size = level.width * level.width;
        if (input.size() != size) return false;
        output.resize(size);
        for (std::size_t row = 0; row < level.width; ++row) {
            for (std::size_t column = 0; column < level.width; ++column) {
                const auto index = row * level.width + column;
                auto value = level.diagonal[index] * input[index];
                if (column > 0) value -= level.west[index] * input[index - 1];
                if (column + 1 < level.width) {
                    value -= level.east[index] * input[index + 1];
                }
                if (row > 0) {
                    value -= level.south[index] * input[index - level.width];
                }
                if (row + 1 < level.width) {
                    value -= level.north[index] * input[index + level.width];
                }
                output[index] = value;
            }
        }
        return finite(output);
    }

    bool cycle(
        std::size_t level_index,
        const std::vector<double>& right,
        std::vector<double>& result) const {
        const auto& level = levels[level_index];
        const auto size = level.width * level.width;
        if (right.size() != size) return false;
        auto& workspace = workspaces[level_index];
        if (level_index + 1 == levels.size()) {
            return cholesky_solve(
                coarse_cholesky, right, result, workspace.intermediate);
        }
        result.assign(size, 0.0);
        for (std::size_t step = 0; step < pre_steps; ++step) {
            for (std::size_t row = 0; row < level.width; ++row) {
                for (std::size_t column = 0; column < level.width; ++column) {
                    const auto index = row * level.width + column;
                    auto value = right[index];
                    if (column > 0) value += level.west[index] * result[index - 1];
                    if (column + 1 < level.width) {
                        value += level.east[index] * result[index + 1];
                    }
                    if (row > 0) {
                        value += level.south[index] * result[index - level.width];
                    }
                    if (row + 1 < level.width) {
                        value += level.north[index] * result[index + level.width];
                    }
                    result[index] = (1.0 - weight) * result[index] +
                        weight * level.inverse_diagonal[index] * value;
                }
            }
        }
        if (!multiply(level, result, workspace.product)) return false;
        for (std::size_t index = 0; index < size; ++index) {
            workspace.residual[index] = right[index] - workspace.product[index];
        }
        const auto& coarse = levels[level_index + 1];
        workspace.coarse_right.assign(coarse.width * coarse.width, 0.0);
        for (std::size_t row = 0; row < level.width; ++row) {
            for (std::size_t column = 0; column < level.width; ++column) {
                workspace.coarse_right[(row / 2) * coarse.width + column / 2] +=
                    workspace.residual[row * level.width + column];
            }
        }
        auto& coarse_correction = workspaces[level_index + 1].correction;
        if (!cycle(level_index + 1, workspace.coarse_right, coarse_correction)) {
            return false;
        }
        for (std::size_t row = 0; row < level.width; ++row) {
            for (std::size_t column = 0; column < level.width; ++column) {
                result[row * level.width + column] +=
                    coarse_correction[(row / 2) * coarse.width + column / 2];
            }
        }
        for (std::size_t step = 0; step < post_steps; ++step) {
            for (std::size_t reverse_row = 0; reverse_row < level.width;
                 ++reverse_row) {
                const auto row = level.width - reverse_row - 1;
                for (std::size_t reverse_column = 0;
                     reverse_column < level.width; ++reverse_column) {
                    const auto column = level.width - reverse_column - 1;
                    const auto index = row * level.width + column;
                    auto value = right[index];
                    if (column > 0) value += level.west[index] * result[index - 1];
                    if (column + 1 < level.width) {
                        value += level.east[index] * result[index + 1];
                    }
                    if (row > 0) {
                        value += level.south[index] * result[index - level.width];
                    }
                    if (row + 1 < level.width) {
                        value += level.north[index] * result[index + level.width];
                    }
                    result[index] = (1.0 - weight) * result[index] +
                        weight * level.inverse_diagonal[index] * value;
                }
            }
        }
        return finite(result);
    }
};

GeometricFivePointMultigrid2D::GeometricFivePointMultigrid2D(
    std::size_t grid_width,
    const std::vector<double>& west,
    const std::vector<double>& east,
    const std::vector<double>& south,
    const std::vector<double>& north,
    const std::vector<double>& diagonal,
    std::size_t maximum_coarse_unknowns,
    std::size_t pre_smoothing_steps,
    std::size_t post_smoothing_steps,
    double smoothing_weight)
    : impl_(std::make_unique<Impl>()) {
    const auto size = grid_width * grid_width;
    if (grid_width == 0 || west.size() != size || east.size() != size ||
        south.size() != size || north.size() != size || diagonal.size() != size ||
        maximum_coarse_unknowns == 0 || pre_smoothing_steps == 0 ||
        post_smoothing_steps == 0 || !(smoothing_weight > 0.0) ||
        !(smoothing_weight < 2.0)) {
        impl_->reason = "invalid geometric five-point multigrid input";
        return;
    }
    impl_->pre_steps = pre_smoothing_steps;
    impl_->post_steps = post_smoothing_steps;
    impl_->weight = smoothing_weight;
    Impl::StencilLevel fine;
    fine.width = grid_width;
    fine.west = west;
    fine.east = east;
    fine.south = south;
    fine.north = north;
    fine.diagonal = diagonal;
    fine.inverse_diagonal.resize(size);
    for (std::size_t index = 0; index < size; ++index) {
        if (!(fine.diagonal[index] > 0.0) ||
            !std::isfinite(fine.diagonal[index])) {
            impl_->reason = "geometric multigrid requires positive finite diagonal";
            return;
        }
        fine.inverse_diagonal[index] = 1.0 / fine.diagonal[index];
    }
    impl_->levels.push_back(std::move(fine));
    while (impl_->levels.back().width * impl_->levels.back().width >
               maximum_coarse_unknowns &&
           impl_->levels.back().width > 1) {
        const auto& current = impl_->levels.back();
        Impl::StencilLevel coarse;
        coarse.width = (current.width + 1) / 2;
        const auto coarse_size = coarse.width * coarse.width;
        coarse.west.assign(coarse_size, 0.0);
        coarse.east.assign(coarse_size, 0.0);
        coarse.south.assign(coarse_size, 0.0);
        coarse.north.assign(coarse_size, 0.0);
        coarse.diagonal.assign(coarse_size, 0.0);
        for (std::size_t row = 0; row < current.width; ++row) {
            for (std::size_t column = 0; column < current.width; ++column) {
                const auto fine_index = row * current.width + column;
                const auto coarse_row = row / 2;
                const auto coarse_column = column / 2;
                const auto coarse_index = coarse_row * coarse.width + coarse_column;
                if (column > 0 && (column - 1) / 2 != coarse_column) {
                    coarse.west[coarse_index] += current.west[fine_index];
                }
                if (column + 1 < current.width &&
                    (column + 1) / 2 != coarse_column) {
                    coarse.east[coarse_index] += current.east[fine_index];
                }
                if (row > 0 && (row - 1) / 2 != coarse_row) {
                    coarse.south[coarse_index] += current.south[fine_index];
                }
                if (row + 1 < current.width &&
                    (row + 1) / 2 != coarse_row) {
                    coarse.north[coarse_index] += current.north[fine_index];
                }
                auto diagonal_mass = current.diagonal[fine_index];
                if (column > 0) diagonal_mass -= current.west[fine_index];
                if (column + 1 < current.width) {
                    diagonal_mass -= current.east[fine_index];
                }
                if (row > 0) diagonal_mass -= current.south[fine_index];
                if (row + 1 < current.width) {
                    diagonal_mass -= current.north[fine_index];
                }
                coarse.diagonal[coarse_index] += diagonal_mass;
            }
        }
        coarse.inverse_diagonal.resize(coarse_size);
        for (std::size_t index = 0; index < coarse_size; ++index) {
            coarse.diagonal[index] += coarse.west[index] + coarse.east[index] +
                coarse.south[index] + coarse.north[index];
            if (!(coarse.diagonal[index] > 0.0) ||
                !std::isfinite(coarse.diagonal[index])) {
                impl_->reason = "coarse five-point operator lost positive diagonal";
                impl_->levels.clear();
                return;
            }
            coarse.inverse_diagonal[index] = 1.0 / coarse.diagonal[index];
        }
        impl_->levels.push_back(std::move(coarse));
    }
    const auto& coarsest = impl_->levels.back();
    const auto coarse_size = coarsest.width * coarsest.width;
    std::vector<std::map<std::size_t, double>> coarse_rows(coarse_size);
    for (std::size_t row = 0; row < coarsest.width; ++row) {
        for (std::size_t column = 0; column < coarsest.width; ++column) {
            const auto index = row * coarsest.width + column;
            coarse_rows[index][index] = coarsest.diagonal[index];
            if (column > 0) coarse_rows[index][index - 1] = -coarsest.west[index];
            if (column + 1 < coarsest.width) {
                coarse_rows[index][index + 1] = -coarsest.east[index];
            }
            if (row > 0) {
                coarse_rows[index][index - coarsest.width] = -coarsest.south[index];
            }
            if (row + 1 < coarsest.width) {
                coarse_rows[index][index + coarsest.width] = -coarsest.north[index];
            }
        }
    }
    Level coarse_level;
    if (!make_level(coarsest.width, coarse_rows, coarse_level) ||
        !cholesky_factor(coarse_level, impl_->coarse_cholesky)) {
        impl_->reason = "coarsest five-point operator is not SPD";
        impl_->levels.clear();
        return;
    }
    impl_->workspaces.resize(impl_->levels.size());
    for (std::size_t index = 0; index < impl_->levels.size(); ++index) {
        const auto level_size =
            impl_->levels[index].width * impl_->levels[index].width;
        auto& workspace = impl_->workspaces[index];
        workspace.product.resize(level_size);
        workspace.residual.resize(level_size);
        workspace.correction.resize(level_size);
        workspace.intermediate.resize(level_size);
        if (index + 1 < impl_->levels.size()) {
            const auto coarse_width = impl_->levels[index + 1].width;
            workspace.coarse_right.resize(coarse_width * coarse_width);
        }
        impl_->bytes += 6 * level_size * sizeof(double) +
            (workspace.product.size() + workspace.residual.size() +
             workspace.correction.size() + workspace.intermediate.size() +
             workspace.coarse_right.size()) * sizeof(double);
    }
    impl_->bytes += impl_->coarse_cholesky.size() * sizeof(double);
    impl_->reason = "ready";
}

GeometricFivePointMultigrid2D::~GeometricFivePointMultigrid2D() = default;
GeometricFivePointMultigrid2D::GeometricFivePointMultigrid2D(
    GeometricFivePointMultigrid2D&&) noexcept = default;
GeometricFivePointMultigrid2D& GeometricFivePointMultigrid2D::operator=(
    GeometricFivePointMultigrid2D&&) noexcept = default;

bool GeometricFivePointMultigrid2D::valid() const {
    return impl_ != nullptr && !impl_->levels.empty() &&
        !impl_->coarse_cholesky.empty();
}

const std::string& GeometricFivePointMultigrid2D::reason() const {
    return impl_->reason;
}

std::size_t GeometricFivePointMultigrid2D::levels() const {
    return impl_->levels.size();
}

std::size_t GeometricFivePointMultigrid2D::storage_bytes() const {
    return impl_->bytes;
}

bool GeometricFivePointMultigrid2D::apply(
    const std::vector<double>& residual,
    std::vector<double>& correction) const {
    return valid() && impl_->cycle(0, residual, correction);
}

bool aggregation_amg_five_point_eligible(
    const LinearSystem& system,
    std::size_t* grid_width,
    std::string* reason) {
    const auto reject = [&](std::string message) {
        if (grid_width) *grid_width = 0;
        if (reason) *reason = std::move(message);
        return false;
    };
    const auto size = system.size();
    if (!system.has_sparse_matrix() || size < 16) {
        return reject("AMG requires a sparse system with at least 16 unknowns");
    }
    if (!system.symmetric || !system.positive_definite) {
        return reject("AMG requires a numerically symmetric positive-definite system");
    }
    const auto width = static_cast<std::size_t>(std::llround(std::sqrt(
        static_cast<double>(size))));
    if (width * width != size) {
        return reject("AMG requires a square two-dimensional grid cardinality");
    }
    for (std::size_t row = 0; row < size; ++row) {
        bool diagonal = false;
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            const auto column = system.sparsity.column_indices[offset];
            const bool west = row % width != 0 && column + 1 == row;
            const bool east = row % width + 1 < width && column == row + 1;
            const bool south = row >= width && column + width == row;
            const bool north = row + width < size && column == row + width;
            if (column == row) diagonal = true;
            else if (!west && !east && !south && !north) {
                return reject("AMG five-point topology probe found a non-grid coupling");
            }
        }
        if (!diagonal) return reject("AMG five-point topology probe found a missing diagonal");
    }
    if (grid_width) *grid_width = width;
    if (reason) *reason = "square five-point SPD topology admitted";
    return true;
}

AggregationAmgPcgResult aggregation_amg_pcg_solve(
    const LinearSystem& system,
    double absolute_tolerance,
    double relative_tolerance,
    int maximum_iterations) {
    AggregationAmgPcgResult result;
    result.eligible = aggregation_amg_five_point_eligible(
        system, &result.grid_width, &result.reason);
    if (!result.eligible) return result;
    if (!std::isfinite(absolute_tolerance) || !std::isfinite(relative_tolerance) ||
        absolute_tolerance < 0.0 || relative_tolerance < 0.0 ||
        maximum_iterations <= 0) {
        result.reason = "invalid AMG-PCG tolerance or iteration contract";
        return result;
    }
    AggregationMultigrid2D multigrid(system, result.grid_width);
    if (!multigrid.valid()) {
        result.reason = multigrid.reason();
        return result;
    }
    result.levels = multigrid.levels();
    result.storage_bytes = multigrid.storage_bytes();
    const auto preconditioner = [&](const std::vector<double>& residual,
                                    std::vector<double>& correction) {
        return multigrid.apply(residual, correction);
    };
    const auto krylov = preconditioned_conjugate_gradient(
        system,
        std::vector<double>(system.size()),
        preconditioner,
        absolute_tolerance,
        relative_tolerance,
        maximum_iterations);
    result.iterations = krylov.iterations;
    result.residual_inf = krylov.residual_history.empty()
        ? std::numeric_limits<double>::infinity()
        : krylov.residual_history.back();
    if (!krylov.converged) {
        result.reason = "AMG-preconditioned PCG failed: " + krylov.reason;
        return result;
    }
    result.solution = krylov.solution;
    result.solved = true;
    result.reason = "aggregation AMG-PCG converged; original residual gate required";
    return result;
}

}  // namespace smave
