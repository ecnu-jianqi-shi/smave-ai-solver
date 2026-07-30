#include "smave/runtime.hpp"

#include "smave/linear.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

const EquationIR& equation_by_id(const ModelIR& model, const std::string& id) {
    const auto iterator = std::find_if(
        model.equations.begin(), model.equations.end(),
        [&](const EquationIR& equation) { return equation.id == id; });
    if (iterator == model.equations.end()) throw std::logic_error("missing equation " + id);
    return *iterator;
}

double vector_two_norm(const std::vector<double>& values) {
    double sum{};
    for (const double value : values) sum += value * value;
    return std::sqrt(sum);
}

const VariableIR& variable_by_name(const ModelIR& model, const std::string& name) {
    const auto iterator = std::find_if(
        model.variables.begin(), model.variables.end(),
        [&](const VariableIR& variable) { return variable.name == name; });
    if (iterator == model.variables.end()) throw std::logic_error("missing variable " + name);
    return *iterator;
}

std::vector<double> residual_vector(
    const std::unordered_map<std::string, Expression>& residuals,
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values) {
    std::vector<double> result;
    result.reserve(block.equation_ids.size());
    for (const auto& equation_id : block.equation_ids) {
        result.push_back(residuals.at(equation_id).evaluate(values));
    }
    return result;
}

bool finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

bool solve_linear_system(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right,
    std::vector<double>& solution) {
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

bool symmetric_positive_definite(
    const std::vector<std::vector<double>>& matrix) {
    const std::size_t size = matrix.size();
    std::vector<std::vector<double>> lower(size, std::vector<double>(size));
    for (std::size_t row = 0; row < size; ++row) {
        if (matrix[row].size() != size) return false;
        for (std::size_t column = 0; column < size; ++column) {
            if (std::abs(matrix[row][column] - matrix[column][row]) >
                1.0e-8 * (1.0 + std::max(
                    std::abs(matrix[row][column]),
                    std::abs(matrix[column][row])))) return false;
        }
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

struct NewtonResult {
    NewtonResult() = default;
    NewtonResult(
        bool converged_value,
        std::unordered_map<std::string, double> result_values,
        int iteration_count)
        : converged(converged_value),
          values(std::move(result_values)),
          iterations(iteration_count) {}

    bool converged{false};
    std::unordered_map<std::string, double> values;
    int iterations{};
    int inner_krylov_iterations{};
    double inner_krylov_initial_residual{};
    double inner_krylov_final_residual{};
    bool inner_krylov_breakdown{false};
    bool inner_krylov_stagnated{false};
    std::string inner_linear_backend;
    std::size_t inner_jacobian_nonzeros{};
    std::size_t inner_jacobian_storage_bytes{};
    std::size_t inner_jacobian_colors{};
    std::size_t inner_jacobian_evaluation_batches{};
    std::size_t inner_jacobian_ad_batches{};
    std::size_t inner_jacobian_fd_fallback_batches{};
    bool inner_matrix_free{};
    std::size_t inner_operator_applications{};
    std::size_t inner_operator_ad_applications{};
    std::size_t inner_operator_fd_fallback_applications{};
    std::size_t inner_preconditioner_storage_bytes{};
    std::size_t inner_preconditioner_setup_entries{};
    std::size_t inner_preconditioner_ad_entries{};
    std::size_t inner_preconditioner_fd_fallback_entries{};
    std::size_t inner_preconditioner_identity_entries{};
};

struct SparseJacobianAssembly {
    LinearSystem system;
    std::size_t colors{};
    std::size_t evaluation_batches{};
    std::size_t ad_batches{};
    std::size_t fd_fallback_batches{};
};

SparseJacobianAssembly sparse_finite_difference_jacobian(
    const std::unordered_map<std::string, Expression>& residuals,
    const BlockIR& block,
    std::unordered_map<std::string, double>& values,
    const std::vector<double>& residual) {
    SparseJacobianAssembly assembly;
    auto& system = assembly.system;
    system.unknowns = block.unknowns;
    system.sparsity = block.jacobian_sparsity;
    system.sparse_values.resize(system.sparsity.nonzeros());
    system.right_hand_side = residual;
    for (double& value : system.right_hand_side) value = -value;
    const auto colors = system.sparsity.greedy_column_coloring();
    assembly.colors = colors.empty()
        ? 0
        : *std::max_element(colors.begin(), colors.end()) + 1;
    std::vector<double> steps(block.unknowns.size());
    for (std::size_t column = 0; column < block.unknowns.size(); ++column) {
        const double original = values.at(block.unknowns[column]);
        steps[column] = std::cbrt(std::numeric_limits<double>::epsilon()) *
            std::max(1.0, std::abs(original));
    }
    for (std::size_t color = 0; color < assembly.colors; ++color) {
        std::unordered_map<std::string, double> directions;
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) directions[block.unknowns[column]] = 1.0;
        }
        std::vector<double> derivatives(system.sparsity.row_count);
        bool automatic_differentiation = true;
        for (std::size_t row = 0; row < system.sparsity.row_count; ++row) {
            const auto derivative = residuals.at(block.equation_ids[row])
                .directional_derivative(values, directions);
            if (!derivative.has_value()) {
                automatic_differentiation = false;
                break;
            }
            derivatives[row] = *derivative;
        }
        if (automatic_differentiation) {
            ++assembly.evaluation_batches;
            ++assembly.ad_batches;
            for (std::size_t row = 0; row < system.sparsity.row_count; ++row) {
                for (std::size_t offset = system.sparsity.row_offsets[row];
                     offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                    const auto column = system.sparsity.column_indices[offset];
                    if (colors[column] == color) {
                        system.sparse_values[offset] = derivatives[row];
                        break;
                    }
                }
            }
            continue;
        }
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) values[block.unknowns[column]] += steps[column];
        }
        const auto plus = residual_vector(residuals, block, values);
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) values[block.unknowns[column]] -= 2.0 * steps[column];
        }
        const auto minus = residual_vector(residuals, block, values);
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) values[block.unknowns[column]] += steps[column];
        }
        assembly.evaluation_batches += 2;
        assembly.fd_fallback_batches += 2;
        for (std::size_t row = 0; row < system.sparsity.row_count; ++row) {
            for (std::size_t offset = system.sparsity.row_offsets[row];
                 offset < system.sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = system.sparsity.column_indices[offset];
                if (colors[column] == color) {
                    system.sparse_values[offset] =
                        (plus[row] - minus[row]) / (2.0 * steps[column]);
                    break;
                }
            }
        }
    }
    classify_linear_system(system, 1.0e-7);
    return assembly;
}

NewtonResult newton_solve(
    const ModelIR& model,
    const std::unordered_map<std::string, Expression>& residuals,
    const BlockIR& block,
    const std::unordered_map<std::string, double>& context,
    std::unordered_map<std::string, double> initial,
    const Tolerance& tolerance,
    int maximum_iterations,
    const Expert* jacobian_preconditioner = nullptr,
    const BlockContext* preconditioner_context = nullptr,
    bool sparse_krylov = false,
    bool matrix_free = false) {
    auto values = context;
    int accumulated_krylov_iterations{};
    double first_krylov_residual{};
    double last_krylov_residual{};
    std::string last_inner_backend;
    std::size_t last_jacobian_nonzeros{};
    std::size_t last_jacobian_storage_bytes{};
    std::size_t last_jacobian_colors{};
    std::size_t accumulated_jacobian_evaluation_batches{};
    std::size_t accumulated_jacobian_ad_batches{};
    std::size_t accumulated_jacobian_fd_fallback_batches{};
    std::size_t accumulated_operator_applications{};
    std::size_t accumulated_operator_ad_applications{};
    std::size_t accumulated_operator_fd_fallback_applications{};
    std::size_t last_preconditioner_storage_bytes{};
    std::size_t accumulated_preconditioner_setup_entries{};
    std::size_t accumulated_preconditioner_ad_entries{};
    std::size_t accumulated_preconditioner_fd_fallback_entries{};
    std::size_t accumulated_preconditioner_identity_entries{};
    for (const auto& unknown : block.unknowns) {
        values[unknown] = initial.contains(unknown)
            ? initial.at(unknown)
            : variable_by_name(model, unknown).start;
    }
    for (int iteration = 0; iteration <= maximum_iterations; ++iteration) {
        std::vector<double> residual;
        try {
            residual = residual_vector(residuals, block, values);
        } catch (const std::exception&) {
            return {false, values, iteration};
        }
        if (!finite(residual)) return {false, values, iteration};
        double scaled_inf = 0.0;
        for (std::size_t row = 0; row < residual.size(); ++row) {
            double scale = 1.0;
            for (const auto& name : equation_by_id(model, block.equation_ids[row]).variables) {
                scale = std::max(scale, std::abs(values.at(name)));
            }
            scaled_inf = std::max(
                scaled_inf,
                std::abs(residual[row]) / (tolerance.absolute + tolerance.relative * scale));
        }
        if (scaled_inf <= 1.0) {
            NewtonResult result{true, values, iteration};
            result.inner_krylov_iterations = accumulated_krylov_iterations;
            result.inner_krylov_initial_residual = first_krylov_residual;
            result.inner_krylov_final_residual = last_krylov_residual;
            result.inner_linear_backend = last_inner_backend;
            result.inner_jacobian_nonzeros = last_jacobian_nonzeros;
            result.inner_jacobian_storage_bytes = last_jacobian_storage_bytes;
            result.inner_jacobian_colors = last_jacobian_colors;
            result.inner_jacobian_evaluation_batches =
                accumulated_jacobian_evaluation_batches;
            result.inner_jacobian_ad_batches = accumulated_jacobian_ad_batches;
            result.inner_jacobian_fd_fallback_batches =
                accumulated_jacobian_fd_fallback_batches;
            result.inner_matrix_free = matrix_free;
            result.inner_operator_applications = accumulated_operator_applications;
            result.inner_operator_ad_applications =
                accumulated_operator_ad_applications;
            result.inner_operator_fd_fallback_applications =
                accumulated_operator_fd_fallback_applications;
            result.inner_preconditioner_storage_bytes =
                last_preconditioner_storage_bytes;
            result.inner_preconditioner_setup_entries =
                accumulated_preconditioner_setup_entries;
            result.inner_preconditioner_ad_entries =
                accumulated_preconditioner_ad_entries;
            result.inner_preconditioner_fd_fallback_entries =
                accumulated_preconditioner_fd_fallback_entries;
            result.inner_preconditioner_identity_entries =
                accumulated_preconditioner_identity_entries;
            return result;
        }
        if (iteration == maximum_iterations) break;
        std::vector<double> right = residual;
        for (double& item : right) item = -item;
        std::vector<double> delta;
        std::string current_inner_backend;
        if (matrix_free) {
            current_inner_backend = "jfnk-gmres-diagonal-cpu-v1";
            const auto linear_operator = [&](const std::vector<double>& input,
                                             std::vector<double>& output) {
                ++accumulated_operator_applications;
                std::unordered_map<std::string, double> directions;
                for (std::size_t index = 0; index < input.size(); ++index) {
                    if (input[index] != 0.0) {
                        directions[block.unknowns[index]] = input[index];
                    }
                }
                output.resize(block.equation_ids.size());
                bool automatic_differentiation = true;
                for (std::size_t row = 0; row < block.equation_ids.size(); ++row) {
                    const auto derivative = residuals.at(block.equation_ids[row])
                        .directional_derivative(values, directions);
                    if (!derivative.has_value()) {
                        automatic_differentiation = false;
                        break;
                    }
                    output[row] = *derivative;
                }
                if (automatic_differentiation) {
                    ++accumulated_operator_ad_applications;
                    return finite(output);
                }
                const double input_norm = std::max(1.0e-30, vector_two_norm(input));
                std::vector<double> current;
                current.reserve(block.unknowns.size());
                for (const auto& unknown : block.unknowns) current.push_back(values.at(unknown));
                const double step = std::sqrt(std::numeric_limits<double>::epsilon()) *
                    (1.0 + vector_two_norm(current)) / input_norm;
                auto shifted_values = values;
                for (std::size_t index = 0; index < input.size(); ++index) {
                    shifted_values[block.unknowns[index]] += step * input[index];
                }
                const auto shifted = residual_vector(residuals, block, shifted_values);
                for (std::size_t row = 0; row < output.size(); ++row) {
                    output[row] = (shifted[row] - residual[row]) / step;
                }
                ++accumulated_operator_fd_fallback_applications;
                return finite(output);
            };
            std::vector<double> inverse_diagonal(right.size(), 1.0);
            std::size_t identity_entries{};
            for (std::size_t row = 0; row < right.size(); ++row) {
                std::unordered_map<std::string, double> direction{
                    {block.unknowns[row], 1.0}};
                auto diagonal = residuals.at(block.equation_ids[row])
                    .directional_derivative(values, direction);
                ++accumulated_preconditioner_setup_entries;
                if (diagonal.has_value()) {
                    ++accumulated_preconditioner_ad_entries;
                } else {
                    const double current = values.at(block.unknowns[row]);
                    const double step = std::sqrt(std::numeric_limits<double>::epsilon()) *
                        (1.0 + std::abs(current));
                    auto shifted_values = values;
                    shifted_values[block.unknowns[row]] += step;
                    try {
                        diagonal =
                            (residuals.at(block.equation_ids[row]).evaluate(shifted_values) -
                             residual[row]) / step;
                    } catch (const std::exception&) {
                        diagonal.reset();
                    }
                    ++accumulated_preconditioner_fd_fallback_entries;
                }
                if (!diagonal.has_value() || !std::isfinite(*diagonal) ||
                    std::abs(*diagonal) <= std::sqrt(std::numeric_limits<double>::epsilon())) {
                    ++identity_entries;
                    continue;
                }
                inverse_diagonal[row] = 1.0 / *diagonal;
            }
            accumulated_preconditioner_identity_entries += identity_entries;
            last_preconditioner_storage_bytes =
                inverse_diagonal.size() * sizeof(double);
            if (identity_entries == right.size()) {
                current_inner_backend = "jfnk-gmres-identity-cpu-v1";
            }
            const Preconditioner diagonal_preconditioner =
                [inverse_diagonal = std::move(inverse_diagonal)](
                    const std::vector<double>& input, std::vector<double>& output) {
                    if (input.size() != inverse_diagonal.size()) return false;
                    output.resize(input.size());
                    for (std::size_t index = 0; index < input.size(); ++index) {
                        output[index] = inverse_diagonal[index] * input[index];
                    }
                    return finite(output);
                };
            const auto krylov = restarted_gmres(
                right.size(), linear_operator, right,
                std::vector<double>(right.size()), diagonal_preconditioner,
                tolerance.absolute, tolerance.relative,
                std::max(20, static_cast<int>(right.size()) * 4),
                std::min(40, static_cast<int>(right.size())));
            last_inner_backend = current_inner_backend;
            accumulated_krylov_iterations += krylov.iterations;
            if (first_krylov_residual == 0.0 && !krylov.residual_history.empty()) {
                first_krylov_residual = krylov.residual_history.front();
            }
            if (!krylov.residual_history.empty()) {
                last_krylov_residual = krylov.residual_history.back();
            }
            if (!krylov.converged) {
                NewtonResult failed{false, values, iteration};
                failed.inner_krylov_iterations = accumulated_krylov_iterations;
                failed.inner_krylov_initial_residual = first_krylov_residual;
                failed.inner_krylov_final_residual = last_krylov_residual;
                failed.inner_krylov_breakdown = krylov.breakdown;
                failed.inner_krylov_stagnated = krylov.stagnated;
                failed.inner_linear_backend = last_inner_backend;
                failed.inner_matrix_free = true;
                failed.inner_operator_applications = accumulated_operator_applications;
                failed.inner_operator_ad_applications =
                    accumulated_operator_ad_applications;
                failed.inner_operator_fd_fallback_applications =
                    accumulated_operator_fd_fallback_applications;
                failed.inner_preconditioner_storage_bytes =
                    last_preconditioner_storage_bytes;
                failed.inner_preconditioner_setup_entries =
                    accumulated_preconditioner_setup_entries;
                failed.inner_preconditioner_ad_entries =
                    accumulated_preconditioner_ad_entries;
                failed.inner_preconditioner_fd_fallback_entries =
                    accumulated_preconditioner_fd_fallback_entries;
                failed.inner_preconditioner_identity_entries =
                    accumulated_preconditioner_identity_entries;
                return failed;
            }
            delta = krylov.solution;
        } else if (sparse_krylov) {
            LinearSystem system;
            try {
                auto assembly = sparse_finite_difference_jacobian(
                    residuals, block, values, residual);
                last_jacobian_colors = assembly.colors;
                accumulated_jacobian_evaluation_batches +=
                    assembly.evaluation_batches;
                accumulated_jacobian_ad_batches += assembly.ad_batches;
                accumulated_jacobian_fd_fallback_batches +=
                    assembly.fd_fallback_batches;
                system = std::move(assembly.system);
            } catch (const std::exception&) {
                return {false, values, iteration};
            }
            last_jacobian_nonzeros = system.nonzeros();
            last_jacobian_storage_bytes = system.sparse_storage_bytes();
            KrylovResult krylov;
            if (system.positive_definite) {
                current_inner_backend = "pcg-ic0-cpu-v1";
                krylov = preconditioned_conjugate_gradient(
                    system, std::vector<double>(right.size()),
                    incomplete_cholesky_zero_preconditioner(
                        system, block.jacobian_sparsity),
                    tolerance.absolute, tolerance.relative,
                    std::max(20, static_cast<int>(right.size()) * 4));
            } else {
                current_inner_backend = "gmres-ilu0-cpu-v1";
                krylov = restarted_gmres(
                    system, std::vector<double>(right.size()),
                    incomplete_lu_zero_preconditioner(
                        system, block.jacobian_sparsity),
                    tolerance.absolute, tolerance.relative,
                    std::max(20, static_cast<int>(right.size()) * 4),
                    std::min(40, static_cast<int>(right.size())));
            }
            last_inner_backend = current_inner_backend;
            accumulated_krylov_iterations += krylov.iterations;
            if (!krylov.residual_history.empty()) {
                if (first_krylov_residual == 0.0) {
                    first_krylov_residual = krylov.residual_history.front();
                }
                last_krylov_residual = krylov.residual_history.back();
            }
            if (!krylov.converged) {
                NewtonResult failed{false, values, iteration};
                failed.inner_krylov_iterations = accumulated_krylov_iterations;
                failed.inner_krylov_breakdown = krylov.breakdown;
                failed.inner_krylov_stagnated = krylov.stagnated;
                failed.inner_krylov_initial_residual = first_krylov_residual;
                failed.inner_krylov_final_residual = last_krylov_residual;
                failed.inner_linear_backend = current_inner_backend;
                failed.inner_jacobian_nonzeros = last_jacobian_nonzeros;
                failed.inner_jacobian_storage_bytes = last_jacobian_storage_bytes;
                failed.inner_jacobian_colors = last_jacobian_colors;
                failed.inner_jacobian_evaluation_batches =
                    accumulated_jacobian_evaluation_batches;
                failed.inner_jacobian_ad_batches = accumulated_jacobian_ad_batches;
                failed.inner_jacobian_fd_fallback_batches =
                    accumulated_jacobian_fd_fallback_batches;
                return failed;
            }
            delta = std::move(krylov.solution);
        } else {
            std::vector<std::vector<double>> jacobian(
                residual.size(), std::vector<double>(block.unknowns.size()));
            for (std::size_t column = 0; column < block.unknowns.size(); ++column) {
                const std::string& unknown = block.unknowns[column];
                const double original = values.at(unknown);
                const double step = std::cbrt(std::numeric_limits<double>::epsilon()) *
                    std::max(1.0, std::abs(original));
                values[unknown] = original + step;
                std::vector<double> plus;
                try {
                    plus = residual_vector(residuals, block, values);
                } catch (const std::exception&) {
                    return {false, values, iteration};
                }
                values[unknown] = original - step;
                std::vector<double> minus;
                try {
                    minus = residual_vector(residuals, block, values);
                } catch (const std::exception&) {
                    return {false, values, iteration};
                }
                values[unknown] = original;
                for (std::size_t row = 0; row < residual.size(); ++row) {
                    jacobian[row][column] = (plus[row] - minus[row]) / (2.0 * step);
                }
            }
            if (jacobian_preconditioner != nullptr) {
            if (preconditioner_context == nullptr ||
                !symmetric_positive_definite(jacobian)) {
                return {false, values, iteration};
            }
            for (std::size_t row = 0; row < jacobian.size(); ++row) {
                for (std::size_t column = row + 1; column < jacobian.size(); ++column) {
                    const double value = 0.5 * (
                        jacobian[row][column] + jacobian[column][row]);
                    jacobian[row][column] = value;
                    jacobian[column][row] = value;
                }
            }
            LinearSystem system;
            system.unknowns = block.unknowns;
            system.matrix = jacobian;
            system.right_hand_side = right;
            system.symmetric = true;
            system.positive_definite = true;
            const Preconditioner action = [&](const std::vector<double>& residual,
                                              std::vector<double>& result) {
                return jacobian_preconditioner->apply_preconditioner(
                    block, *preconditioner_context, residual, result);
            };
            const auto krylov = preconditioned_conjugate_gradient(
                system, std::vector<double>(right.size()), action,
                tolerance.absolute, tolerance.relative,
                std::max(20, static_cast<int>(right.size()) * 4));
            accumulated_krylov_iterations += krylov.iterations;
            if (!krylov.residual_history.empty()) {
                if (first_krylov_residual == 0.0) {
                    first_krylov_residual = krylov.residual_history.front();
                }
                last_krylov_residual = krylov.residual_history.back();
            }
            if (!krylov.converged) {
                NewtonResult failed{false, values, iteration};
                failed.inner_krylov_iterations = accumulated_krylov_iterations;
                failed.inner_krylov_breakdown = krylov.breakdown;
                failed.inner_krylov_stagnated = krylov.stagnated;
                failed.inner_krylov_initial_residual = first_krylov_residual;
                failed.inner_krylov_final_residual = last_krylov_residual;
                return failed;
            }
            delta = krylov.solution;
            } else if (!solve_linear_system(std::move(jacobian), std::move(right), delta)) {
                return {false, values, iteration};
            }
        }
        const double current_norm = *std::max_element(
            residual.begin(), residual.end(), [](double left, double right) {
                return std::abs(left) < std::abs(right);
            });
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = values;
            for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                trial[block.unknowns[index]] += damping * delta[index];
            }
            try {
                const auto trial_residual = residual_vector(residuals, block, trial);
                const double trial_norm = std::abs(*std::max_element(
                    trial_residual.begin(), trial_residual.end(), [](double left, double right) {
                        return std::abs(left) < std::abs(right);
                    }));
                if (finite(trial_residual) && trial_norm < std::abs(current_norm)) {
                    values = std::move(trial);
                    accepted = true;
                    break;
                }
            } catch (const std::exception&) {
            }
            damping *= 0.5;
        }
        if (!accepted) return {false, values, iteration + 1};
        if (sparse_krylov) {
            last_krylov_residual = std::max(last_krylov_residual, 0.0);
        }
    }
    NewtonResult failed{false, values, maximum_iterations};
    failed.inner_krylov_iterations = accumulated_krylov_iterations;
    failed.inner_krylov_initial_residual = first_krylov_residual;
    failed.inner_krylov_final_residual = last_krylov_residual;
    failed.inner_linear_backend = last_inner_backend;
    failed.inner_jacobian_nonzeros = last_jacobian_nonzeros;
    failed.inner_jacobian_storage_bytes = last_jacobian_storage_bytes;
    failed.inner_jacobian_colors = last_jacobian_colors;
    failed.inner_jacobian_evaluation_batches =
        accumulated_jacobian_evaluation_batches;
    failed.inner_jacobian_ad_batches = accumulated_jacobian_ad_batches;
    failed.inner_jacobian_fd_fallback_batches =
        accumulated_jacobian_fd_fallback_batches;
    failed.inner_matrix_free = matrix_free;
    failed.inner_operator_applications = accumulated_operator_applications;
    failed.inner_operator_ad_applications = accumulated_operator_ad_applications;
    failed.inner_operator_fd_fallback_applications =
        accumulated_operator_fd_fallback_applications;
    failed.inner_preconditioner_storage_bytes = last_preconditioner_storage_bytes;
    failed.inner_preconditioner_setup_entries = accumulated_preconditioner_setup_entries;
    failed.inner_preconditioner_ad_entries = accumulated_preconditioner_ad_entries;
    failed.inner_preconditioner_fd_fallback_entries =
        accumulated_preconditioner_fd_fallback_entries;
    failed.inner_preconditioner_identity_entries =
        accumulated_preconditioner_identity_entries;
    return failed;
}

std::string make_trace_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 generator(static_cast<std::uint64_t>(now));
    std::ostringstream output;
    output << std::hex << now << '-' << generator();
    return output.str();
}

void write_trace(
    const ModelIR& model,
    const SolveOutcome& outcome,
    const std::unordered_map<std::string, double>& input,
    const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::ofstream output(directory / (outcome.trace_id + ".trace"));
    if (!output) throw std::runtime_error("cannot write trace");
    output << std::setprecision(17);
    output << "TRACE " << std::quoted(outcome.trace_id) << '\n';
    output << "MODEL " << std::quoted(model.model_id) << ' '
           << std::quoted(model.source_hash) << '\n';
    for (const auto& [name, value] : input) output << "INPUT " << name << ' ' << value << '\n';
    for (const auto& block : outcome.blocks) {
        output << "BLOCK " << block.block_id << ' ' << to_string(block.path) << ' '
               << std::quoted(block.plan_id) << ' ' << block.gate.residual_inf << ' '
               << block.expert_iterations << ' '
               << block.fallback_iterations << '\n';
        output << "TIMING routing_us=" << block.timing.routing_us
               << " expert_us=" << block.timing.expert_us
               << " gate_us=" << block.timing.gate_us
               << " correction_us=" << block.timing.correction_us
               << " fallback_us=" << block.timing.fallback_us
               << " total_us=" << block.timing.total_us << '\n';
        if (block.krylov_iterations > 0 || block.linear_spd) {
        output << "KRYLOV iterations=" << block.krylov_iterations
                   << " initial_residual=" << block.krylov_initial_residual
                   << " final_residual=" << block.krylov_final_residual
                   << " diagonal_condition=" << block.condition_estimate
                   << " spd=" << block.linear_spd
                   << " breakdown=" << block.krylov_breakdown
                   << " stagnated=" << block.krylov_stagnated
                   << " jacobian_preconditioner=" << block.jacobian_preconditioner
                   << " preconditioner=" << std::quoted(block.preconditioner_version)
                   << " inner_backend=" << std::quoted(block.inner_linear_backend)
                   << " jacobian_nnz=" << block.inner_jacobian_nonzeros
                   << " jacobian_bytes=" << block.inner_jacobian_storage_bytes
                   << " jacobian_colors=" << block.inner_jacobian_colors
                   << " jacobian_evaluation_batches="
                   << block.inner_jacobian_evaluation_batches
                   << " jacobian_ad_batches=" << block.inner_jacobian_ad_batches
                   << " jacobian_fd_fallback_batches="
                   << block.inner_jacobian_fd_fallback_batches
                   << " matrix_free=" << block.inner_matrix_free
                   << " operator_applications=" << block.inner_operator_applications
                   << " operator_ad_applications="
                   << block.inner_operator_ad_applications
                   << " operator_fd_fallback_applications="
                   << block.inner_operator_fd_fallback_applications
                   << " preconditioner_bytes="
                   << block.inner_preconditioner_storage_bytes
                   << " preconditioner_setup_entries="
                   << block.inner_preconditioner_setup_entries
                   << " preconditioner_ad_entries="
                   << block.inner_preconditioner_ad_entries
                   << " preconditioner_fd_fallback_entries="
                   << block.inner_preconditioner_fd_fallback_entries
                   << " preconditioner_identity_entries="
                   << block.inner_preconditioner_identity_entries
                   << '\n';
        }
        for (std::size_t index = 0; index < block.attempted_experts.size(); ++index) {
            output << "EXPERT " << block.attempted_experts[index] << ' '
                   << (index < block.estimated_costs_us.size()
                           ? block.estimated_costs_us[index]
                           : -1.0)
                   << '\n';
        }
        for (const auto& attempt : block.attempt_records) {
            output << "ATTEMPT " << std::quoted(attempt.expert_version) << ' '
                   << std::quoted(attempt.outcome) << ' '
                   << std::quoted(attempt.reason) << ' '
                   << attempt.estimated_cost_us << ' '
                   << attempt.iterations << ' '
                   << attempt.residual_inf << '\n';
        }
        for (const auto& residency : block.residency_records) {
            output << "RESIDENCY " << std::quoted(residency.expert_version) << ' '
                   << std::quoted(residency.device) << ' '
                   << std::quoted(residency.outcome) << ' '
                   << std::quoted(residency.reason) << ' '
                   << residency.expert_bytes << ' '
                   << residency.resident_bytes << ' '
                   << residency.invocation_heat;
            for (const auto& evicted : residency.evicted_experts) {
                output << ' ' << std::quoted(evicted);
            }
            output << '\n';
        }
    }
    for (const auto& [name, value] : outcome.values) output << "VALUE " << name << ' ' << value << '\n';
    output << "STATUS " << (outcome.success ? "success" : "failure") << ' '
           << std::quoted(outcome.message) << '\n';
    output << "SUMMARY direct=" << outcome.direct_count
           << " corrected=" << outcome.corrected_count
           << " warm_start=" << outcome.warm_start_count
           << " fallback=" << outcome.fallback_count
           << " residency_loads=" << outcome.residency_load_count
           << " residency_hits=" << outcome.residency_hit_count
           << " residency_rejections=" << outcome.residency_rejection_count
           << " residency_evictions=" << outcome.residency_eviction_count
           << " resident_expert_bytes=" << outcome.resident_expert_bytes
           << " total_us=" << outcome.timing.total_us << '\n';
}

}  // namespace

Runtime::Runtime(
    ModelIR model,
    Tolerance tolerance,
    RoutingConfig routing,
    ResidencyConfig residency,
    std::shared_ptr<SolveGatePolicy> solve_gate_policy)
    : Runtime(
          model,
          make_default_registry(model),
          make_default_bundle(model),
          tolerance,
          routing,
          residency,
          std::move(solve_gate_policy)) {}

Runtime::Runtime(
    ModelIR model,
    Registry registry,
    RuntimeBundle bundle,
    Tolerance tolerance,
    RoutingConfig routing,
    ResidencyConfig residency,
    std::shared_ptr<SolveGatePolicy> solve_gate_policy)
    : model_(std::move(model)),
      registry_(std::move(registry)),
      bundle_(std::move(bundle)),
      tolerance_(tolerance),
      runtime_router_(routing),
      residency_(std::make_shared<ExpertResidencyManager>(std::move(residency))),
      solve_gate_policy_(std::move(solve_gate_policy)),
      solve_gate_identity_(std::make_shared<const std::uint8_t>(0)) {
    model_.validate();
    registry_.validate_bundle(bundle_, model_);
    variable_indices_by_name_.reserve(model_.variables.size());
    for (std::size_t index = 0; index < model_.variables.size(); ++index) {
        variable_indices_by_name_.emplace(model_.variables[index].name, index);
    }
    equation_indices_by_id_.reserve(model_.equations.size());
    for (std::size_t index = 0; index < model_.equations.size(); ++index) {
        const auto& equation = model_.equations[index];
        equation_indices_by_id_.emplace(equation.id, index);
        residuals_.emplace(equation.id, Expression(equation.residual));
    }
    gate_plans_.reserve(model_.blocks.size());
    for (const auto& block : model_.blocks) {
        GateBlockPlan plan;
        plan.unknown_variable_indices.reserve(block.unknowns.size());
        for (const auto& unknown : block.unknowns) {
            plan.unknown_variable_indices.push_back(variable_indices_by_name_.at(unknown));
        }
        plan.equations.reserve(block.equation_ids.size());
        for (const auto& equation_id : block.equation_ids) {
            const auto& equation = model_.equations[equation_indices_by_id_.at(equation_id)];
            GateEquationPlan equation_plan;
            equation_plan.residual = &residuals_.at(equation_id);
            equation_plan.variable_indices.reserve(equation.variables.size());
            for (const auto& name : equation.variables) {
                equation_plan.variable_indices.push_back(variable_indices_by_name_.at(name));
            }
            plan.equations.push_back(std::move(equation_plan));
        }
        gate_plans_.emplace(block.id, std::move(plan));
    }
    std::unordered_map<std::string, double> defaults;
    for (const auto& variable : model_.variables) defaults[variable.name] = variable.start;
    for (const auto& block : model_.blocks) {
        if (!block.linear) continue;
        bool constant_matrix = true;
        for (std::size_t row = 0; row < block.equation_ids.size(); ++row) {
            std::vector<std::string> local_unknowns;
            local_unknowns.reserve(block.jacobian_sparsity.row(row).size());
            for (const auto column : block.jacobian_sparsity.row(row)) {
                local_unknowns.push_back(block.unknowns[column]);
            }
            if (!residuals_.at(block.equation_ids[row]).constant_linear_coefficients(
                    local_unknowns).has_value()) {
                constant_matrix = false;
                break;
            }
        }
        if (constant_matrix) {
            constant_linear_systems_.emplace(
                block.id,
                assemble_linear_system(model_, block, residuals_, defaults));
        }
    }
}

GateResult Runtime::evaluate_gate(
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    bool direct_permission) const {
    return evaluate_gate_fused(block, values, direct_permission);
}

GateResult Runtime::evaluate_solve_gate(
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    bool direct_permission) const {
    if (solve_gate_policy_) {
        return solve_gate_policy_->evaluate(*this, block, values, direct_permission);
    }
    return evaluate_gate(block, values, direct_permission);
}

std::shared_ptr<const void> Runtime::solve_gate_identity() const noexcept {
    return solve_gate_identity_;
}

GateResult Runtime::evaluate_gate_reference(
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    bool direct_permission) const {
    std::vector<double> residuals;
    try {
        residuals = residual_vector(residuals_, block, values);
    } catch (const std::exception& error) {
        GateResult gate;
        gate.reason = error.what();
        return gate;
    }
    return evaluate_gate_with_residuals(
        block, values, residuals, direct_permission);
}

GateResult Runtime::evaluate_gate_fused(
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    bool direct_permission) const {
    GateResult gate;
    const auto plan = gate_plans_.find(block.id);
    if (plan == gate_plans_.end() ||
        plan->second.equations.size() != block.equation_ids.size()) {
        gate.reason = "runtime gate plan missing or shape mismatch";
        return gate;
    }
    try {
        for (const std::size_t variable_index : plan->second.unknown_variable_indices) {
            const auto& variable = model_.variables[variable_index];
            const double value = values.at(variable.name);
            if (!std::isfinite(value)) {
                gate.reason = "NaN/Inf or missing candidate";
                return gate;
            }
            if (variable.minimum && value < *variable.minimum) {
                gate.reason = "lower-bound constraint violation";
                return gate;
            }
            if (variable.maximum && value > *variable.maximum) {
                gate.reason = "upper-bound constraint violation";
                return gate;
            }
        }
        gate.scaled_residuals.reserve(plan->second.equations.size());
        for (const auto& equation : plan->second.equations) {
            const double residual = equation.residual->evaluate(values);
            if (!std::isfinite(residual)) {
                gate.reason = "non-finite runtime residual";
                return gate;
            }
            double scale = 1.0;
            for (const std::size_t variable_index : equation.variable_indices) {
                const auto& variable = model_.variables[variable_index];
                scale = std::max(scale, std::abs(values.at(variable.name)));
                scale = std::max(scale, variable.nominal);
            }
            const double scaled = std::abs(residual) /
                (tolerance_.absolute + tolerance_.relative * scale);
            gate.scaled_residuals.push_back(scaled);
            gate.residual_inf = std::max(gate.residual_inf, scaled);
        }
    } catch (const std::exception& error) {
        gate.reason = error.what();
        return gate;
    }
    if (gate.residual_inf > 1.0) {
        gate.reason = "scaled residual exceeds tolerance";
        return gate;
    }
    if (!direct_permission) {
        gate.decision = GateDecision::need_correction;
        gate.reason = "Direct permission absent";
        return gate;
    }
    gate.decision = GateDecision::direct_accept;
    gate.reason = "runtime residual and constraints pass";
    return gate;
}

std::vector<GateResult> Runtime::evaluate_gate_batch(
    const BlockIR& block,
    const std::vector<std::unordered_map<std::string, double>>& values,
    bool direct_permission) const {
    std::vector<GateResult> results;
    results.reserve(values.size());
    const auto constant = constant_linear_systems_.find(block.id);
    if (block.linear && constant != constant_linear_systems_.end()) {
        auto system = constant->second;
        std::vector<double> candidate(block.unknowns.size());
        std::vector<double> residuals(block.equation_ids.size());
        for (const auto& item : values) {
            try {
                update_linear_right_hand_side(
                    system, model_, block, residuals_, item);
                for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                    candidate[index] = item.at(block.unknowns[index]);
                }
                const auto product = system.multiply(candidate);
                for (std::size_t row = 0; row < residuals.size(); ++row) {
                    residuals[row] = product[row] - system.right_hand_side[row];
                }
                results.push_back(evaluate_gate_with_residuals(
                    block, item, residuals, direct_permission));
            } catch (const std::exception& error) {
                results.push_back({
                    .decision = GateDecision::reject,
                    .reason = std::string("batched linear gate failed: ") + error.what(),
                });
            }
        }
        return results;
    }
    for (const auto& item : values) {
        results.push_back(evaluate_gate(block, item, direct_permission));
    }
    return results;
}

GateResult Runtime::evaluate_gate_with_residuals(
    const BlockIR& block,
    const std::unordered_map<std::string, double>& values,
    const std::vector<double>& residuals,
    bool direct_permission) const {
    GateResult gate;
    if (residuals.size() != block.equation_ids.size()) {
        gate.reason = "runtime residual shape mismatch";
        return gate;
    }
    for (const auto& unknown : block.unknowns) {
        if (!values.contains(unknown) || !std::isfinite(values.at(unknown))) {
            gate.reason = "NaN/Inf or missing candidate";
            return gate;
        }
        const auto& variable =
            model_.variables[variable_indices_by_name_.at(unknown)];
        if (variable.minimum.has_value() && values.at(unknown) < *variable.minimum) {
            gate.reason = "lower-bound constraint violation";
            return gate;
        }
        if (variable.maximum.has_value() && values.at(unknown) > *variable.maximum) {
            gate.reason = "upper-bound constraint violation";
            return gate;
        }
    }
    if (!finite(residuals)) {
        gate.reason = "non-finite runtime residual";
        return gate;
    }
    for (std::size_t index = 0; index < residuals.size(); ++index) {
        double scale = 1.0;
        const auto& equation =
            model_.equations[equation_indices_by_id_.at(block.equation_ids[index])];
        for (const auto& name : equation.variables) {
            scale = std::max(scale, std::abs(values.at(name)));
            scale = std::max(scale,
                model_.variables[variable_indices_by_name_.at(name)].nominal);
        }
        const double scaled = std::abs(residuals[index]) /
            (tolerance_.absolute + tolerance_.relative * scale);
        gate.scaled_residuals.push_back(scaled);
        gate.residual_inf = std::max(gate.residual_inf, scaled);
    }
    if (gate.residual_inf > 1.0) {
        gate.reason = "scaled residual exceeds tolerance";
        return gate;
    }
    if (!direct_permission) {
        gate.decision = GateDecision::need_correction;
        gate.reason = "Direct permission absent";
        return gate;
    }
    gate.decision = GateDecision::direct_accept;
    gate.reason = "runtime residual and constraints pass";
    return gate;
}

SolveOutcome Runtime::correct_candidate(
    const std::unordered_map<std::string, double>& context,
    const std::string& block_id,
    const std::unordered_map<std::string, double>& candidate,
    const std::string& expert_version,
    const std::filesystem::path& trace_directory,
    int maximum_iterations) const {
    if (maximum_iterations <= 0) {
        throw std::invalid_argument("candidate correction iterations must be positive");
    }
    const auto block = std::find_if(
        model_.blocks.begin(), model_.blocks.end(),
        [&](const BlockIR& item) { return item.id == block_id; });
    if (block == model_.blocks.end()) throw std::invalid_argument("unknown candidate block");
    if (model_.blocks.size() != 1) {
        throw std::invalid_argument("candidate correction MVP requires one model block");
    }
    std::set<std::string> allowed_context;
    for (const auto& variable : model_.variables) {
        allowed_context.insert(variable.name);
        if (variable.kind == "algebraic") {
            allowed_context.insert(variable.name + "_previous");
        }
    }
    for (const auto& [name, _] : context) {
        if (!allowed_context.contains(name)) {
            throw std::invalid_argument("unknown candidate context field: " + name);
        }
    }
    if (candidate.size() != block->unknowns.size() ||
        std::any_of(candidate.begin(), candidate.end(), [&](const auto& item) {
            return std::find(block->unknowns.begin(), block->unknowns.end(), item.first) ==
                block->unknowns.end();
        })) {
        return solve(context, trace_directory / "fallback-invalid-candidate-shape");
    }
    for (const auto& unknown : block->unknowns) {
        if (!candidate.contains(unknown) || !std::isfinite(candidate.at(unknown))) {
            return solve(context, trace_directory / "fallback-invalid-candidate");
        }
    }
    SolveOutcome outcome;
    const auto solve_started = std::chrono::steady_clock::now();
    outcome.trace_id = make_trace_id();
    outcome.resident_expert_bytes = residency_->snapshot().resident_bytes;
    outcome.values = context;
    for (const auto& variable : model_.variables) {
        if (!outcome.values.contains(variable.name)) outcome.values[variable.name] = variable.start;
    }
    BlockOutcome block_outcome;
    block_outcome.block_id = block->id;
    block_outcome.plan_id = "candidate-batch-" + expert_version;
    block_outcome.attempted_experts.push_back(expert_version);
    block_outcome.estimated_costs_us.push_back(0.0);
    const auto correction_started = std::chrono::steady_clock::now();
    const auto corrected = newton_solve(
        model_, residuals_, *block, outcome.values, candidate, tolerance_, maximum_iterations);
    block_outcome.timing.correction_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - correction_started).count();
    block_outcome.expert_iterations = corrected.iterations;
    if (corrected.converged) {
        const auto gate_started = std::chrono::steady_clock::now();
        block_outcome.gate = evaluate_solve_gate(*block, corrected.values, true);
        block_outcome.timing.gate_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - gate_started).count();
        if (block_outcome.gate.decision == GateDecision::direct_accept) {
            block_outcome.path = SolvePath::corrected_accept;
            block_outcome.solution = corrected.values;
            outcome.values = corrected.values;
            outcome.blocks.push_back(block_outcome);
            outcome.success = true;
            outcome.corrected_count = 1;
            outcome.message = "batched candidate passed correction and runtime gate";
            outcome.timing.total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - solve_started).count();
            outcome.blocks.front().timing.total_us = outcome.timing.total_us;
            write_trace(model_, outcome, context, trace_directory);
            return outcome;
        }
    }
    return solve(context, trace_directory / "fallback-rejected-candidate");
}

SolveOutcome Runtime::commit_corrected_candidate(
    const std::unordered_map<std::string, double>& context,
    const std::string& block_id,
    const std::unordered_map<std::string, double>& candidate,
    const std::string& expert_version,
    const std::filesystem::path& trace_directory) const {
    const auto block = std::find_if(
        model_.blocks.begin(), model_.blocks.end(),
        [&](const BlockIR& item) { return item.id == block_id; });
    if (block == model_.blocks.end()) {
        throw std::invalid_argument("unknown corrected candidate block");
    }
    if (model_.blocks.size() != 1) {
        throw std::invalid_argument("corrected candidate commit requires one model block");
    }
    std::set<std::string> allowed_context;
    for (const auto& variable : model_.variables) {
        allowed_context.insert(variable.name);
        if (variable.kind == "algebraic") {
            allowed_context.insert(variable.name + "_previous");
        }
    }
    for (const auto& [name, _] : context) {
        if (!allowed_context.contains(name)) {
            throw std::invalid_argument("unknown corrected candidate context field: " + name);
        }
    }
    if (candidate.size() != block->unknowns.size() ||
        std::any_of(candidate.begin(), candidate.end(), [&](const auto& item) {
            return std::find(block->unknowns.begin(), block->unknowns.end(), item.first) ==
                block->unknowns.end();
        })) {
        return solve(context, trace_directory / "fallback-invalid-candidate-shape");
    }
    for (const auto& unknown : block->unknowns) {
        if (!candidate.contains(unknown) || !std::isfinite(candidate.at(unknown))) {
            return solve(context, trace_directory / "fallback-invalid-candidate");
        }
    }
    SolveOutcome outcome;
    const auto solve_started = std::chrono::steady_clock::now();
    outcome.trace_id = make_trace_id();
    outcome.resident_expert_bytes = residency_->snapshot().resident_bytes;
    outcome.values = context;
    for (const auto& variable : model_.variables) {
        if (!outcome.values.contains(variable.name)) {
            outcome.values[variable.name] = variable.start;
        }
    }
    for (const auto& [name, value] : candidate) {
        outcome.values.insert_or_assign(name, value);
    }
    BlockOutcome block_outcome;
    block_outcome.block_id = block->id;
    block_outcome.plan_id = "corrected-candidate-" + expert_version;
    block_outcome.attempted_experts.push_back(expert_version);
    block_outcome.estimated_costs_us.push_back(0.0);
    const auto gate_started = std::chrono::steady_clock::now();
    block_outcome.gate = evaluate_solve_gate(*block, outcome.values, true);
    block_outcome.timing.gate_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - gate_started).count();
    if (block_outcome.gate.decision != GateDecision::direct_accept) {
        return solve(context, trace_directory / "fallback-rejected-candidate");
    }
    block_outcome.path = SolvePath::corrected_accept;
    block_outcome.solution = outcome.values;
    outcome.blocks.push_back(block_outcome);
    outcome.success = true;
    outcome.corrected_count = 1;
    outcome.message = "corrected candidate passed independent runtime gate";
    outcome.timing.total_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - solve_started).count();
    outcome.blocks.front().timing.total_us = outcome.timing.total_us;
    write_trace(model_, outcome, context, trace_directory);
    return outcome;
}

SolveOutcome Runtime::solve(
    const std::unordered_map<std::string, double>& context,
    const std::filesystem::path& trace_directory) const {
    SolveOutcome outcome;
    const auto solve_started = std::chrono::steady_clock::now();
    outcome.trace_id = make_trace_id();
    std::set<std::string> allowed_context;
    for (const auto& variable : model_.variables) {
        allowed_context.insert(variable.name);
        if (variable.kind == "algebraic") allowed_context.insert(variable.name + "_previous");
    }
    for (const auto& [name, _] : context) {
        if (!allowed_context.contains(name)) {
            outcome.message = "unknown scenario field: " + name;
            write_trace(model_, outcome, context, trace_directory);
            return outcome;
        }
    }
    outcome.values = context;
    for (const auto& variable : model_.variables) {
        if (variable.kind == "parameter" && !outcome.values.contains(variable.name)) {
            outcome.values[variable.name] = variable.start;
        }
    }
    try {
        for (const auto& block : model_.blocks) {
            const auto block_started = std::chrono::steady_clock::now();
            BlockOutcome block_outcome;
            block_outcome.block_id = block.id;
            BlockContext block_context;
            block_context.values = outcome.values;
            for (const auto& unknown : block.unknowns) {
                const auto previous = context.find(unknown + "_previous");
                if (previous != context.end()) {
                    block_context.previous_solution[unknown] = previous->second;
                }
            }
            const auto routing_started = std::chrono::steady_clock::now();
            std::optional<LinearSystem> linear_system;
            if (block.linear) {
                const auto cached = constant_linear_systems_.find(block.id);
                if (cached != constant_linear_systems_.end()) {
                    linear_system = cached->second;
                    update_linear_right_hand_side(
                        *linear_system, model_, block, residuals_, outcome.values);
                } else {
                    linear_system = assemble_linear_system(
                        model_, block, residuals_, outcome.values);
                }
                block_context.numeric_probe = BlockContext::NumericProbe{
                    .available = true,
                    .symmetric = linear_system->symmetric,
                    .positive_definite = linear_system->positive_definite,
                    .diagonal_condition_estimate =
                        linear_system->diagonal_condition_estimate,
                };
                block_outcome.linear_spd = linear_system->positive_definite;
                block_outcome.condition_estimate =
                    linear_system->diagonal_condition_estimate;
            }
            const auto eligible = compile_router_.lookup(
                block, registry_, bundle_);
            const auto plan = runtime_router_.route(
                block, block_context, eligible, registry_, bundle_);
            block_outcome.timing.routing_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - routing_started).count();
            block_outcome.plan_id = plan.plan_id;
            const auto record_attempt = [&](const std::string& version,
                                            const std::string& attempt_outcome,
                                            const std::string& reason,
                                            double estimated_cost,
                                            int iterations,
                                            double residual_inf) {
                block_outcome.attempt_records.push_back({
                    version, attempt_outcome, reason, estimated_cost,
                    iterations, residual_inf});
            };
            for (const auto& step : plan.steps) {
                block_outcome.attempted_experts.push_back(step.expert_version);
                block_outcome.estimated_costs_us.push_back(step.estimated_cost_us);
                if (step.builtin &&
                    step.expert_version == "structured-tridiagonal-direct-cpu-v1") {
                    const auto expert_started = std::chrono::steady_clock::now();
                    const auto structured =
                        structured_tridiagonal_direct_solve(*linear_system);
                    block_outcome.timing.expert_us +=
                        std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - expert_started).count();
                    if (!structured.eligible || !structured.solved) {
                        record_attempt(
                            step.expert_version,
                            structured.eligible ? "rejected" : "skipped",
                            structured.reason, step.estimated_cost_us, 0,
                            structured.residual_inf);
                        continue;
                    }
                    const auto values = linear_solution_values(
                        *linear_system, structured.solution, outcome.values);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, values, true);
                    block_outcome.timing.gate_us +=
                        std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        std::ostringstream metadata;
                        metadata << gate.reason
                                 << "; topology="
                                 << (structured.periodic ? "cyclic-tridiagonal"
                                                         : "tridiagonal")
                                 << " linear_residual=" << structured.residual_inf;
                        record_attempt(
                            step.expert_version, "accepted", metadata.str(),
                            step.estimated_cost_us, 1, gate.residual_inf);
                        block_outcome.path = SolvePath::direct_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, 1, gate.residual_inf);
                    continue;
                }
                if (step.builtin &&
                    (step.expert_version == "gmres-ilut-cpu-v1" ||
                     step.expert_version == "gmres-ilu0-cpu-v1")) {
                    if (linear_system->symmetric) {
                        record_attempt(
                            step.expert_version, "skipped",
                            "runtime matrix is symmetric; GMRES route is ineligible",
                            step.estimated_cost_us, 0, 0.0);
                        continue;
                    }
                    const auto expert_started = std::chrono::steady_clock::now();
                    std::vector<double> initial(block.unknowns.size());
                    for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                        const auto previous = block_context.previous_solution.find(
                            block.unknowns[index]);
                        initial[index] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, block.unknowns[index]).start;
                    }
                    const auto preconditioner = step.expert_version == "gmres-ilut-cpu-v1"
                        ? incomplete_lu_threshold_preconditioner(
                              *linear_system, 1.0e-4,
                              std::max<std::size_t>(
                                  4, std::min<std::size_t>(16, block.unknowns.size())))
                        : incomplete_lu_zero_preconditioner(
                              *linear_system, block.jacobian_sparsity);
                    const auto krylov = restarted_gmres(
                        *linear_system,
                        initial,
                        preconditioner,
                        tolerance_.absolute,
                        tolerance_.relative,
                        std::max(20, static_cast<int>(block.unknowns.size()) * 4),
                        std::min(20, static_cast<int>(block.unknowns.size())));
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.preconditioner_version = step.expert_version;
                    block_outcome.krylov_iterations = krylov.iterations;
                    block_outcome.expert_iterations += krylov.iterations;
                    block_outcome.krylov_breakdown = krylov.breakdown;
                    block_outcome.krylov_stagnated = krylov.stagnated;
                    if (!krylov.residual_history.empty()) {
                        block_outcome.krylov_initial_residual =
                            krylov.residual_history.front();
                        block_outcome.krylov_final_residual =
                            krylov.residual_history.back();
                    }
                    const double krylov_residual = krylov.residual_history.empty()
                        ? 0.0 : krylov.residual_history.back();
                    if (!krylov.converged) {
                        record_attempt(
                            step.expert_version, "rejected", krylov.reason,
                            step.estimated_cost_us, krylov.iterations,
                            krylov_residual);
                        continue;
                    }
                    const auto values = linear_solution_values(
                        *linear_system, krylov.solution, outcome.values);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, krylov.iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::direct_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, krylov.iterations,
                        gate.residual_inf);
                    continue;
                }
                if (step.builtin && step.expert_version == "pcg-jacobi-cpu-v1") {
                    if (!linear_system->positive_definite) {
                        record_attempt(
                            step.expert_version, "skipped",
                            "runtime matrix is not SPD; PCG route is ineligible",
                            step.estimated_cost_us, 0, 0.0);
                        continue;
                    }
                    const auto expert_started = std::chrono::steady_clock::now();
                    std::vector<double> initial(block.unknowns.size());
                    for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                        const auto previous = block_context.previous_solution.find(
                            block.unknowns[index]);
                        initial[index] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, block.unknowns[index]).start;
                    }
                    const auto krylov = preconditioned_conjugate_gradient(
                        *linear_system,
                        initial,
                        jacobi_preconditioner(*linear_system),
                        tolerance_.absolute,
                        tolerance_.relative,
                        std::max(20, static_cast<int>(block.unknowns.size()) * 4));
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.krylov_iterations = krylov.iterations;
                    block_outcome.expert_iterations += krylov.iterations;
                    if (!krylov.residual_history.empty()) {
                        block_outcome.krylov_initial_residual =
                            krylov.residual_history.front();
                        block_outcome.krylov_final_residual =
                            krylov.residual_history.back();
                    }
                    if (krylov.converged) {
                        const auto values = linear_solution_values(
                            *linear_system, krylov.solution, outcome.values);
                        const auto gate_started = std::chrono::steady_clock::now();
                        const auto gate = evaluate_solve_gate(block, values, true);
                        block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - gate_started).count();
                        if (gate.decision == GateDecision::direct_accept) {
                            record_attempt(
                                step.expert_version, "accepted", gate.reason,
                                step.estimated_cost_us, krylov.iterations,
                                gate.residual_inf);
                            block_outcome.path = SolvePath::direct_accept;
                            block_outcome.solution = values;
                            block_outcome.gate = gate;
                            break;
                        }
                        record_attempt(
                            step.expert_version, "rejected", gate.reason,
                            step.estimated_cost_us, krylov.iterations,
                            gate.residual_inf);
                    } else {
                        record_attempt(
                            step.expert_version, "rejected", krylov.reason,
                            step.estimated_cost_us, krylov.iterations,
                            krylov.residual_history.empty()
                                ? 0.0 : krylov.residual_history.back());
                    }
                    continue;
                }
                if (step.builtin &&
                    step.expert_version == "pcg-aggregation-amg-cpu-v1") {
                    const auto expert_started = std::chrono::steady_clock::now();
                    const auto amg = aggregation_amg_pcg_solve(
                        *linear_system,
                        tolerance_.absolute,
                        tolerance_.relative,
                        std::max(20, static_cast<int>(block.unknowns.size()) * 4));
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.preconditioner_version = step.expert_version;
                    block_outcome.krylov_iterations = amg.iterations;
                    block_outcome.expert_iterations += amg.iterations;
                    block_outcome.krylov_final_residual = amg.residual_inf;
                    if (!amg.solved) {
                        record_attempt(
                            step.expert_version,
                            amg.eligible ? "rejected" : "skipped",
                            amg.reason,
                            step.estimated_cost_us,
                            amg.iterations,
                            amg.residual_inf);
                        continue;
                    }
                    const auto values = linear_solution_values(
                        *linear_system, amg.solution, outcome.values);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        std::ostringstream metadata;
                        metadata << gate.reason << "; levels=" << amg.levels
                                 << "; storage_bytes=" << amg.storage_bytes
                                 << "; grid_width=" << amg.grid_width;
                        record_attempt(
                            step.expert_version,
                            "accepted",
                            metadata.str(),
                            step.estimated_cost_us,
                            amg.iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::direct_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version,
                        "rejected",
                        gate.reason,
                        step.estimated_cost_us,
                        amg.iterations,
                        gate.residual_inf);
                    continue;
                }
                if (step.builtin && step.expert_version == "pcg-ic0-cpu-v1") {
                    if (!linear_system->positive_definite) {
                        record_attempt(
                            step.expert_version, "skipped",
                            "runtime matrix is not SPD; PCG route is ineligible",
                            step.estimated_cost_us, 0, 0.0);
                        continue;
                    }
                    const auto expert_started = std::chrono::steady_clock::now();
                    std::vector<double> initial(block.unknowns.size());
                    for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                        const auto previous = block_context.previous_solution.find(
                            block.unknowns[index]);
                        initial[index] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, block.unknowns[index]).start;
                    }
                    const auto krylov = preconditioned_conjugate_gradient(
                        *linear_system,
                        initial,
                        incomplete_cholesky_zero_preconditioner(
                            *linear_system, block.jacobian_sparsity),
                        tolerance_.absolute,
                        tolerance_.relative,
                        std::max(20, static_cast<int>(block.unknowns.size()) * 4));
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.preconditioner_version = step.expert_version;
                    block_outcome.krylov_iterations = krylov.iterations;
                    block_outcome.expert_iterations += krylov.iterations;
                    block_outcome.krylov_breakdown = krylov.breakdown;
                    block_outcome.krylov_stagnated = krylov.stagnated;
                    if (!krylov.residual_history.empty()) {
                        block_outcome.krylov_initial_residual = krylov.residual_history.front();
                        block_outcome.krylov_final_residual = krylov.residual_history.back();
                    }
                    if (!krylov.converged) {
                        record_attempt(
                            step.expert_version, "rejected", krylov.reason,
                            step.estimated_cost_us, krylov.iterations,
                            krylov.residual_history.empty()
                                ? 0.0 : krylov.residual_history.back());
                        continue;
                    }
                    const auto values = linear_solution_values(
                        *linear_system, krylov.solution, outcome.values);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, krylov.iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::direct_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, krylov.iterations,
                        gate.residual_inf);
                    continue;
                }
                if (step.builtin &&
                    (step.expert_version == "accelerate-sparse-qr-cpu-v1" ||
                     step.expert_version == "superlu-dgssv-cpu-v1" ||
                     step.expert_version == "sparse-ordered-threshold-pivot-cpu-v2" ||
                     step.expert_version == "dense-direct-cpu-v1")) {
                    const auto expert_started = std::chrono::steady_clock::now();
                    std::vector<double> solution;
                    const bool sparse =
                        step.expert_version == "sparse-ordered-threshold-pivot-cpu-v2";
                    const bool industrial =
                        step.expert_version == "accelerate-sparse-qr-cpu-v1";
                    const bool superlu =
                        step.expert_version == "superlu-dgssv-cpu-v1";
                    SparseDirectResult sparse_result;
                    IndustrialSparseDirectResult industrial_result;
                    const bool solved = [&] {
                        if (industrial) {
                            industrial_result =
                                industrial_sparse_direct_solve(*linear_system);
                            solution = industrial_result.solution;
                            return industrial_result.solved;
                        }
                        if (superlu) {
                            industrial_result =
                                superlu_sparse_direct_solve(*linear_system);
                            solution = industrial_result.solution;
                            return industrial_result.solved;
                        }
                        if (sparse) {
                            sparse_result =
                                sparse_ordered_threshold_pivot_solve(*linear_system);
                            solution = sparse_result.solution;
                            return sparse_result.solved;
                        }
                        return dense_direct_solve(*linear_system, solution);
                    }();
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    if (solved) {
                        const auto values = linear_solution_values(
                            *linear_system, solution, outcome.values);
                        const auto gate_started = std::chrono::steady_clock::now();
                        const auto gate = evaluate_solve_gate(block, values, true);
                        block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - gate_started).count();
                        if (gate.decision == GateDecision::direct_accept) {
                            std::string reason = gate.reason;
                            if (industrial) {
                                std::ostringstream metadata;
                                metadata << reason
                                         << "; backend=" << industrial_result.backend
                                         << " matrix_nnz="
                                         << industrial_result.matrix_nonzeros
                                         << " linear_residual="
                                         << industrial_result.residual_inf
                                         << " rank_probe=passed";
                                reason = metadata.str();
                            } else if (sparse) {
                                std::ostringstream metadata;
                                metadata << reason << "; ordering=amd-greedy"
                                         << " row_swaps=" << sparse_result.row_swaps
                                         << " initial_nnz=" << sparse_result.initial_nonzeros
                                         << " upper_nnz=" << sparse_result.upper_nonzeros
                                         << " ordering_fill_edges="
                                         << sparse_result.ordering_fill_edges
                                         << " natural_fill_edges="
                                         << sparse_result.natural_fill_edges
                                         << " min_scaled_pivot="
                                         << sparse_result.minimum_scaled_pivot
                                         << " column_order=";
                                for (std::size_t index = 0;
                                     index < sparse_result.column_order.size(); ++index) {
                                    if (index != 0) metadata << ',';
                                    metadata << sparse_result.column_order[index];
                                }
                                reason = metadata.str();
                            }
                            record_attempt(
                                step.expert_version, "accepted", reason,
                                step.estimated_cost_us, 1, gate.residual_inf);
                            block_outcome.path = SolvePath::direct_accept;
                            block_outcome.solution = values;
                            block_outcome.gate = gate;
                            break;
                        }
                        record_attempt(
                            step.expert_version, "rejected", gate.reason,
                            step.estimated_cost_us, 1, gate.residual_inf);
                    } else {
                        record_attempt(
                            step.expert_version, "rejected",
                            industrial
                                ? industrial_result.reason
                                : (sparse
                                    ? "ordered scaled-threshold sparse factorization failed or produced non-finite values"
                                    : "dense factorization failed or produced non-finite values"),
                            step.estimated_cost_us, 0, 0.0);
                    }
                    continue;
                }
                if (step.builtin &&
                    (step.expert_version == "newton-krylov-csr-cpu-v1" ||
                     step.expert_version == "newton-krylov-jfnk-cpu-v1")) {
                    std::unordered_map<std::string, double> initial;
                    for (const auto& unknown : block.unknowns) {
                        const auto previous = block_context.previous_solution.find(unknown);
                        initial[unknown] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, unknown).start;
                    }
                    const auto expert_started = std::chrono::steady_clock::now();
                    const auto corrected = newton_solve(
                        model_, residuals_, block, outcome.values, initial,
                        tolerance_, std::max(12, step.budget.work_iterations),
                        nullptr, nullptr,
                        step.expert_version == "newton-krylov-csr-cpu-v1",
                        step.expert_version == "newton-krylov-jfnk-cpu-v1");
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.krylov_iterations = corrected.inner_krylov_iterations;
                    block_outcome.expert_iterations += corrected.iterations;
                    block_outcome.krylov_initial_residual =
                        corrected.inner_krylov_initial_residual;
                    block_outcome.krylov_final_residual =
                        corrected.inner_krylov_final_residual;
                    block_outcome.krylov_breakdown = corrected.inner_krylov_breakdown;
                    block_outcome.krylov_stagnated = corrected.inner_krylov_stagnated;
                    block_outcome.inner_linear_backend = corrected.inner_linear_backend;
                    block_outcome.inner_jacobian_nonzeros =
                        corrected.inner_jacobian_nonzeros;
                    block_outcome.inner_jacobian_storage_bytes =
                        corrected.inner_jacobian_storage_bytes;
                    block_outcome.inner_jacobian_colors =
                        corrected.inner_jacobian_colors;
                    block_outcome.inner_jacobian_evaluation_batches =
                        corrected.inner_jacobian_evaluation_batches;
                    block_outcome.inner_jacobian_ad_batches =
                        corrected.inner_jacobian_ad_batches;
                    block_outcome.inner_jacobian_fd_fallback_batches =
                        corrected.inner_jacobian_fd_fallback_batches;
                    block_outcome.inner_matrix_free = corrected.inner_matrix_free;
                    block_outcome.inner_operator_applications =
                        corrected.inner_operator_applications;
                    block_outcome.inner_operator_ad_applications =
                        corrected.inner_operator_ad_applications;
                    block_outcome.inner_operator_fd_fallback_applications =
                        corrected.inner_operator_fd_fallback_applications;
                    block_outcome.inner_preconditioner_storage_bytes =
                        corrected.inner_preconditioner_storage_bytes;
                    block_outcome.inner_preconditioner_setup_entries =
                        corrected.inner_preconditioner_setup_entries;
                    block_outcome.inner_preconditioner_ad_entries =
                        corrected.inner_preconditioner_ad_entries;
                    block_outcome.inner_preconditioner_fd_fallback_entries =
                        corrected.inner_preconditioner_fd_fallback_entries;
                    block_outcome.inner_preconditioner_identity_entries =
                        corrected.inner_preconditioner_identity_entries;
                    if (!corrected.converged) {
                        record_attempt(
                            step.expert_version, "rejected",
                            "Newton-Krylov failed or line search stalled",
                            step.estimated_cost_us, corrected.inner_krylov_iterations,
                            corrected.inner_krylov_final_residual);
                        continue;
                    }
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, corrected.values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, corrected.inner_krylov_iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::corrected_accept;
                        block_outcome.solution = corrected.values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, corrected.inner_krylov_iterations,
                        gate.residual_inf);
                    continue;
                }
                if (!registry_.compatible(
                        step.expert_version, block, bundle_, step.permission)) {
                    record_attempt(
                        step.expert_version, "skipped",
                        "registry grant is incompatible with block/bundle/permission",
                        step.estimated_cost_us, 0, 0.0);
                    continue;
                }
                const auto residency = residency_->request(
                    step.expert_version,
                    registry_.grant(step.expert_version).resident_bytes);
                block_outcome.residency_records.push_back(ExpertResidencyRecord{
                    .expert_version = residency.expert_version,
                    .device = residency.device,
                    .outcome = residency.admitted
                        ? (residency.cache_hit ? "hit" : "loaded")
                        : "rejected",
                    .reason = residency.reason,
                    .expert_bytes = residency.expert_bytes,
                    .resident_bytes = residency.resident_bytes,
                    .invocation_heat = residency.invocation_heat,
                    .evicted_experts = residency.evicted_experts,
                });
                outcome.resident_expert_bytes = residency.resident_bytes;
                outcome.residency_eviction_count += residency.evicted_experts.size();
                if (!residency.admitted) {
                    ++outcome.residency_rejection_count;
                    record_attempt(
                        step.expert_version, "skipped", residency.reason,
                        step.estimated_cost_us, 0, 0.0);
                    continue;
                }
                if (residency.cache_hit) {
                    ++outcome.residency_hit_count;
                } else {
                    ++outcome.residency_load_count;
                }
                const auto& registered_expert = registry_.expert(step.expert_version);
                const auto capability = registered_expert.match(block);
                if (capability.preconditioner && !block.linear) {
                    block_outcome.jacobian_preconditioner = true;
                    std::unordered_map<std::string, double> initial;
                    for (const auto& unknown : block.unknowns) {
                        const auto previous = block_context.previous_solution.find(unknown);
                        initial[unknown] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, unknown).start;
                    }
                    const auto expert_started = std::chrono::steady_clock::now();
                    const auto corrected = newton_solve(
                        model_, residuals_, block, outcome.values, initial,
                        tolerance_, std::max(8, step.budget.work_iterations),
                        &registered_expert, &block_context);
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.preconditioner_version = step.expert_version;
                    block_outcome.krylov_iterations = corrected.inner_krylov_iterations;
                    block_outcome.expert_iterations += corrected.iterations;
                    block_outcome.krylov_initial_residual =
                        corrected.inner_krylov_initial_residual;
                    block_outcome.krylov_final_residual =
                        corrected.inner_krylov_final_residual;
                    block_outcome.krylov_breakdown = corrected.inner_krylov_breakdown;
                    block_outcome.krylov_stagnated = corrected.inner_krylov_stagnated;
                    if (!corrected.converged) {
                        record_attempt(
                            step.expert_version, "rejected",
                            "Newton Jacobian PCG failed, became non-SPD, or correction stalled",
                            step.estimated_cost_us, corrected.inner_krylov_iterations,
                            corrected.inner_krylov_final_residual);
                        continue;
                    }
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, corrected.values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, corrected.inner_krylov_iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::corrected_accept;
                        block_outcome.solution = corrected.values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, corrected.inner_krylov_iterations,
                        gate.residual_inf);
                    continue;
                }
                if (capability.preconditioner) {
                    const auto expert_started = std::chrono::steady_clock::now();
                    std::vector<double> initial(block.unknowns.size());
                    for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
                        const auto previous = block_context.previous_solution.find(
                            block.unknowns[index]);
                        initial[index] = previous != block_context.previous_solution.end()
                            ? previous->second
                            : variable_by_name(model_, block.unknowns[index]).start;
                    }
                    const Preconditioner learned_preconditioner =
                        [&](const std::vector<double>& residual,
                            std::vector<double>& result) {
                            return registered_expert.apply_preconditioner(
                                block, block_context, residual, result);
                        };
                    const auto krylov = preconditioned_conjugate_gradient(
                        *linear_system,
                        initial,
                        learned_preconditioner,
                        tolerance_.absolute,
                        tolerance_.relative,
                        std::max(20, static_cast<int>(block.unknowns.size()) * 4));
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    block_outcome.preconditioner_version = step.expert_version;
                    block_outcome.krylov_iterations = krylov.iterations;
                    block_outcome.expert_iterations += krylov.iterations;
                    block_outcome.krylov_breakdown = krylov.breakdown;
                    block_outcome.krylov_stagnated = krylov.stagnated;
                    if (!krylov.residual_history.empty()) {
                        block_outcome.krylov_initial_residual =
                            krylov.residual_history.front();
                        block_outcome.krylov_final_residual =
                            krylov.residual_history.back();
                    }
                    if (!krylov.converged) {
                        record_attempt(
                            step.expert_version, "rejected", krylov.reason,
                            step.estimated_cost_us, krylov.iterations,
                            krylov.residual_history.empty()
                                ? 0.0 : krylov.residual_history.back());
                        continue;
                    }
                    const auto values = linear_solution_values(
                        *linear_system, krylov.solution, outcome.values);
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, krylov.iterations,
                            gate.residual_inf);
                        block_outcome.path = SolvePath::corrected_accept;
                        block_outcome.solution = values;
                        block_outcome.gate = gate;
                        break;
                    }
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, krylov.iterations,
                        gate.residual_inf);
                    continue;
                }
                ExpertResult result;
                const auto expert_started = std::chrono::steady_clock::now();
                try {
                    result = registered_expert.solve(
                        block, block_context, step.budget);
                } catch (const std::exception& error) {
                    block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - expert_started).count();
                    record_attempt(
                        step.expert_version, "rejected",
                        std::string("expert exception: ") + error.what(),
                        step.estimated_cost_us, 0, 0.0);
                    continue;
                }
                block_outcome.timing.expert_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - expert_started).count();
                if (result.status != "candidate" ||
                    !std::all_of(
                        block.unknowns.begin(), block.unknowns.end(),
                        [&](const std::string& name) { return result.candidate.contains(name); })) {
                    record_attempt(
                        step.expert_version, "rejected",
                        "expert returned non-candidate status or incomplete shape",
                        step.estimated_cost_us, 0, 0.0);
                    continue;
                }
                auto candidate_values = outcome.values;
                for (const auto& [name, value] : result.candidate) {
                    candidate_values[name] = value;
                }
                if (step.permission == Permission::direct) {
                    const auto gate_started = std::chrono::steady_clock::now();
                    const auto gate = evaluate_solve_gate(block, candidate_values, true);
                    block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - gate_started).count();
                    if (gate.decision == GateDecision::direct_accept) {
                        record_attempt(
                            step.expert_version, "accepted", gate.reason,
                            step.estimated_cost_us, 0, gate.residual_inf);
                        block_outcome.path = SolvePath::direct_accept;
                        block_outcome.solution = std::move(candidate_values);
                        block_outcome.gate = gate;
                        break;
                    }
                }
                const auto correction_started = std::chrono::steady_clock::now();
                const auto corrected = newton_solve(
                    model_, residuals_, block, outcome.values, result.candidate,
                    tolerance_, step.budget.work_iterations);
                block_outcome.timing.correction_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - correction_started).count();
                block_outcome.expert_iterations += corrected.iterations;
                if (!corrected.converged) {
                    record_attempt(
                        step.expert_version, "rejected",
                        "Newton correction failed to converge",
                        step.estimated_cost_us, corrected.iterations,
                        [&] {
                            try {
                                const auto residual = residual_vector(
                                    residuals_, block, corrected.values);
                                double maximum = 0.0;
                                for (const double value : residual) {
                                    maximum = std::max(maximum, std::abs(value));
                                }
                                return maximum;
                            } catch (const std::exception&) {
                                return std::numeric_limits<double>::infinity();
                            }
                        }());
                    continue;
                }
                const auto gate_started = std::chrono::steady_clock::now();
                const auto gate = evaluate_solve_gate(block, corrected.values, true);
                block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - gate_started).count();
                if (gate.decision != GateDecision::direct_accept) {
                    record_attempt(
                        step.expert_version, "rejected", gate.reason,
                        step.estimated_cost_us, corrected.iterations,
                        gate.residual_inf);
                    continue;
                }
                record_attempt(
                    step.expert_version, "accepted", gate.reason,
                    step.estimated_cost_us, corrected.iterations,
                    gate.residual_inf);
                block_outcome.path = step.permission == Permission::warm_start
                    ? SolvePath::warm_start_accept
                    : SolvePath::corrected_accept;
                block_outcome.solution = corrected.values;
                block_outcome.gate = gate;
                break;
            }
            if (block_outcome.solution.empty()) {
                block_outcome.attempted_experts.push_back(plan.terminal_fallback);
                block_outcome.estimated_costs_us.push_back(-1.0);
                std::unordered_map<std::string, double> original_initial;
                for (const auto& unknown : block.unknowns) {
                    original_initial[unknown] = variable_by_name(model_, unknown).start;
                }
                const auto fallback_started = std::chrono::steady_clock::now();
                const bool matrix_free_fallback =
                    block.unknowns.size() > 1024 && block.smooth &&
                    (plan.assessment.structural_density >= 0.05 ||
                     plan.assessment.estimated_sparse_bytes >= 64U * 1024U * 1024U);
                const auto fallback = newton_solve(
                    model_, residuals_, block, outcome.values, original_initial,
                    tolerance_, 50, nullptr, nullptr,
                    block.unknowns.size() > 1024 && block.smooth &&
                        !matrix_free_fallback,
                    matrix_free_fallback);
                block_outcome.timing.fallback_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - fallback_started).count();
                block_outcome.fallback_iterations = fallback.iterations;
                block_outcome.inner_linear_backend = fallback.inner_linear_backend;
                block_outcome.inner_jacobian_nonzeros = fallback.inner_jacobian_nonzeros;
                block_outcome.inner_jacobian_storage_bytes =
                    fallback.inner_jacobian_storage_bytes;
                block_outcome.inner_jacobian_colors = fallback.inner_jacobian_colors;
                block_outcome.inner_jacobian_evaluation_batches =
                    fallback.inner_jacobian_evaluation_batches;
                block_outcome.inner_jacobian_ad_batches =
                    fallback.inner_jacobian_ad_batches;
                block_outcome.inner_jacobian_fd_fallback_batches =
                    fallback.inner_jacobian_fd_fallback_batches;
                block_outcome.inner_matrix_free = fallback.inner_matrix_free;
                block_outcome.inner_operator_applications =
                    fallback.inner_operator_applications;
                block_outcome.inner_operator_ad_applications =
                    fallback.inner_operator_ad_applications;
                block_outcome.inner_operator_fd_fallback_applications =
                    fallback.inner_operator_fd_fallback_applications;
                block_outcome.inner_preconditioner_storage_bytes =
                    fallback.inner_preconditioner_storage_bytes;
                block_outcome.inner_preconditioner_setup_entries =
                    fallback.inner_preconditioner_setup_entries;
                block_outcome.inner_preconditioner_ad_entries =
                    fallback.inner_preconditioner_ad_entries;
                block_outcome.inner_preconditioner_fd_fallback_entries =
                    fallback.inner_preconditioner_fd_fallback_entries;
                block_outcome.inner_preconditioner_identity_entries =
                    fallback.inner_preconditioner_identity_entries;
                block_outcome.path = SolvePath::full_fallback;
                block_outcome.solution = fallback.values;
                const auto gate_started = std::chrono::steady_clock::now();
                block_outcome.gate = evaluate_solve_gate(block, fallback.values, true);
                block_outcome.timing.gate_us += std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - gate_started).count();
                if (!fallback.converged ||
                    block_outcome.gate.decision != GateDecision::direct_accept) {
                    record_attempt(
                        plan.terminal_fallback, "rejected",
                        !fallback.converged
                            ? "original Newton solver failed to converge"
                            : block_outcome.gate.reason,
                        -1.0, fallback.iterations,
                        block_outcome.gate.residual_inf);
                    outcome.blocks.push_back(std::move(block_outcome));
                    outcome.message = block.id + ": original solver failed";
                    outcome.timing.total_us = std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - solve_started).count();
                    write_trace(model_, outcome, context, trace_directory);
                    return outcome;
                }
                record_attempt(
                    plan.terminal_fallback, "fallback",
                    "all planned experts exhausted; original solver passed gate",
                    -1.0, fallback.iterations,
                    block_outcome.gate.residual_inf);
            }
            for (const auto& unknown : block.unknowns) {
                outcome.values[unknown] = block_outcome.solution.at(unknown);
            }
            block_outcome.timing.total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - block_started).count();
            outcome.timing.routing_us += block_outcome.timing.routing_us;
            outcome.timing.expert_us += block_outcome.timing.expert_us;
            outcome.timing.gate_us += block_outcome.timing.gate_us;
            outcome.timing.correction_us += block_outcome.timing.correction_us;
            outcome.timing.fallback_us += block_outcome.timing.fallback_us;
            switch (block_outcome.path) {
                case SolvePath::direct_accept: ++outcome.direct_count; break;
                case SolvePath::corrected_accept: ++outcome.corrected_count; break;
                case SolvePath::warm_start_accept: ++outcome.warm_start_count; break;
                case SolvePath::full_fallback: ++outcome.fallback_count; break;
            }
            outcome.blocks.push_back(std::move(block_outcome));
        }
        outcome.success = true;
        outcome.message = "all blocks passed the independent runtime gate";
    } catch (const std::exception& error) {
        outcome.message = error.what();
    }
    outcome.timing.total_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - solve_started).count();
    write_trace(model_, outcome, context, trace_directory);
    return outcome;
}

std::unordered_map<std::string, double> read_scenario(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read scenario: " + path.string());
    std::unordered_map<std::string, double> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            if (std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isspace(c); })) {
                continue;
            }
            throw std::invalid_argument("scenario line must be name=value: " + line);
        }
        const std::string name = line.substr(0, separator);
        const std::string raw = line.substr(separator + 1);
        auto trim = [](std::string value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            const auto last = value.find_last_not_of(" \t\r\n");
            return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
        };
        values[trim(name)] = std::stod(trim(raw));
    }
    return values;
}

std::string to_string(GateDecision decision) {
    switch (decision) {
        case GateDecision::reject: return "REJECT";
        case GateDecision::need_correction: return "NEED_CORRECTION";
        case GateDecision::direct_accept: return "DIRECT_ACCEPT";
    }
    return "UNKNOWN";
}

std::string to_string(SolvePath path) {
    switch (path) {
        case SolvePath::direct_accept: return "DIRECT_ACCEPT";
        case SolvePath::corrected_accept: return "CORRECTED_ACCEPT";
        case SolvePath::warm_start_accept: return "WARM_START_ACCEPT";
        case SolvePath::full_fallback: return "FULL_FALLBACK";
    }
    return "UNKNOWN";
}

}  // namespace smave
