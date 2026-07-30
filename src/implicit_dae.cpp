#include "smave/dae.hpp"

#include "smave/dae_learning.hpp"
#include "smave/expression.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace smave {
namespace {

std::string read_source(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read fully implicit DAE source: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character);
        });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(), [](unsigned char character) {
            return std::isspace(character);
        }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

std::string strip_comments(std::string value) {
    value = std::regex_replace(value, std::regex(R"(/\*[\s\S]*?\*/)"), "");
    return std::regex_replace(value, std::regex(R"(//[^\n]*)"), "");
}

std::vector<std::string> split_statements(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ';')) {
        item = trim(std::move(item));
        if (!item.empty()) result.push_back(std::move(item));
    }
    return result;
}

std::map<std::string, std::string> parse_attributes(std::string value) {
    std::map<std::string, std::string> result;
    value = trim(std::move(value));
    if (value.empty()) return result;
    if (value.front() != '(' || value.back() != ')') {
        throw std::invalid_argument("invalid fully implicit DAE attributes");
    }
    std::stringstream stream(value.substr(1, value.size() - 2));
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto equal = item.find('=');
        if (equal == std::string::npos) {
            throw std::invalid_argument("invalid fully implicit DAE attribute: " + item);
        }
        result[trim(item.substr(0, equal))] = trim(item.substr(equal + 1));
    }
    return result;
}

double attribute_value(
    const std::map<std::string, std::string>& attributes,
    const std::string& name,
    double fallback) {
    const auto iterator = attributes.find(name);
    return iterator == attributes.end() ? fallback : std::stod(iterator->second);
}

std::string source_digest(const std::string& source) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : source) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << hash;
    return output.str();
}

void require_tag(std::istream& input, const std::string& expected) {
    std::string actual;
    input >> actual;
    if (actual != expected) {
        throw std::invalid_argument(
            "expected fully implicit DAE tag " + expected + ", got " + actual);
    }
}

std::string derivative_name(const std::string& state) {
    return "__smave_der_" + state;
}

std::string rewrite_derivatives(
    const std::string& source,
    std::unordered_set<std::string>& states) {
    const std::regex derivative(R"(\bder\s*\(\s*([A-Za-z_]\w*)\s*\))");
    std::string result;
    std::size_t offset{};
    for (auto iterator = std::sregex_iterator(source.begin(), source.end(), derivative);
         iterator != std::sregex_iterator(); ++iterator) {
        const auto& match = *iterator;
        result.append(source, offset, static_cast<std::size_t>(match.position()) - offset);
        const auto state = match[1].str();
        states.insert(state);
        result += derivative_name(state);
        offset = static_cast<std::size_t>(match.position() + match.length());
    }
    result.append(source, offset, std::string::npos);
    return result;
}

std::vector<std::string> expression_names(const std::string& source) {
    const Expression expression(source);
    const auto& names = expression.names();
    return {names.begin(), names.end()};
}

std::string rewrite_pre(std::string expression) {
    expression = std::regex_replace(
        expression,
        std::regex(R"(\bpre\s*\(\s*([A-Za-z_]\w*)\s*\))"),
        "__smave_pre_$1");
    if (std::regex_search(expression, std::regex(R"(\bpre\s*\()"))) {
        throw std::invalid_argument("fully implicit DAE reset supports only pre(state)");
    }
    return expression;
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const double value : values) {
        if (!std::isfinite(value)) return std::numeric_limits<double>::infinity();
        result = std::max(result, std::abs(value));
    }
    return result;
}

bool finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

std::unordered_map<std::string, double> event_context(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time,
    const std::vector<double>* pre_state = nullptr) {
    std::unordered_map<std::string, double> values(
        model.parameters.begin(), model.parameters.end());
    values["time"] = time;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        values[model.states[index].name] = state[index];
        if (pre_state != nullptr) {
            values["__smave_pre_" + model.states[index].name] = (*pre_state)[index];
        }
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        values[model.algebraics[index].name] = algebraic[index];
    }
    return values;
}

bool event_guard_active(double value, int direction, double tolerance) {
    return direction > 0 ? value >= -tolerance : value <= tolerance;
}

bool event_guard_crossing(
    double left, double right, int direction, double tolerance) {
    return direction > 0
        ? left < -tolerance && right >= -tolerance
        : left > tolerance && right <= tolerance;
}

std::unordered_map<std::string, double> named_values(
    const std::vector<DaeVariableIR>& variables,
    const std::vector<double>& values) {
    std::unordered_map<std::string, double> result;
    for (std::size_t index = 0; index < variables.size(); ++index) {
        result[variables[index].name] = values[index];
    }
    return result;
}

std::unordered_map<std::string, double> step_context(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate,
    double time,
    double step) {
    std::unordered_map<std::string, double> values(model.parameters.begin(), model.parameters.end());
    values["time"] = time;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        values[model.states[index].name] = candidate[index];
        values[derivative_name(model.states[index].name)] =
            (candidate[index] - previous_state[index]) / step;
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        values[model.algebraics[index].name] = candidate[model.states.size() + index];
    }
    return values;
}

std::unordered_map<std::string, double> initialization_context(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& candidate,
    double time) {
    std::unordered_map<std::string, double> values(model.parameters.begin(), model.parameters.end());
    values["time"] = time;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        values[model.states[index].name] = state[index];
        values[derivative_name(model.states[index].name)] = candidate[index];
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        values[model.algebraics[index].name] = candidate[model.states.size() + index];
    }
    return values;
}

std::vector<double> initialization_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& candidate,
    double time) {
    const auto values = initialization_context(model, state, candidate, time);
    std::vector<double> result;
    result.reserve(model.equations.size());
    for (const auto& equation : model.equations) {
        result.push_back(Expression(equation.residual).evaluate(values));
    }
    return result;
}

std::optional<std::vector<double>> directional_initialization_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& candidate,
    const std::vector<double>& direction,
    double time) {
    if (candidate.size() != direction.size()) return std::nullopt;
    const auto values = initialization_context(model, state, candidate, time);
    std::unordered_map<std::string, double> directions;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        if (direction[index] != 0.0) {
            directions[derivative_name(model.states[index].name)] = direction[index];
        }
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        const auto position = model.states.size() + index;
        if (direction[position] != 0.0) {
            directions[model.algebraics[index].name] = direction[position];
        }
    }
    std::vector<double> result;
    result.reserve(model.equations.size());
    for (const auto& equation : model.equations) {
        const auto derivative = Expression(equation.residual)
            .directional_derivative(values, directions);
        if (!derivative.has_value()) return std::nullopt;
        result.push_back(*derivative);
    }
    return result;
}

std::vector<double> step_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate,
    double time,
    double step) {
    const auto values = step_context(model, previous_state, candidate, time, step);
    std::vector<double> result;
    result.reserve(model.equations.size());
    for (const auto& equation : model.equations) {
        result.push_back(Expression(equation.residual).evaluate(values));
    }
    return result;
}

std::optional<std::vector<double>> directional_step_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate,
    const std::vector<double>& direction,
    double time,
    double step) {
    if (candidate.size() != direction.size()) return std::nullopt;
    const auto values = step_context(model, previous_state, candidate, time, step);
    std::unordered_map<std::string, double> directions;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        if (direction[index] == 0.0) continue;
        directions[model.states[index].name] = direction[index];
        directions[derivative_name(model.states[index].name)] = direction[index] / step;
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        const auto position = model.states.size() + index;
        if (direction[position] != 0.0) {
            directions[model.algebraics[index].name] = direction[position];
        }
    }
    std::vector<double> result;
    result.reserve(model.equations.size());
    for (const auto& equation : model.equations) {
        const auto derivative = Expression(equation.residual)
            .directional_derivative(values, directions);
        if (!derivative.has_value()) return std::nullopt;
        result.push_back(*derivative);
    }
    return result;
}

SparsityPattern step_sparsity(const FullyImplicitDaeIR& model) {
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        positions[model.states[index].name] = index;
        positions[derivative_name(model.states[index].name)] = index;
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        positions[model.algebraics[index].name] = model.states.size() + index;
    }
    std::vector<std::vector<std::size_t>> rows(model.equations.size());
    for (std::size_t row = 0; row < model.equations.size(); ++row) {
        for (const auto& name : model.equations[row].variables) {
            const auto position = positions.find(name);
            if (position != positions.end()) rows[row].push_back(position->second);
        }
    }
    return SparsityPattern::from_rows(model.states.size() + model.algebraics.size(), rows);
}

SparsityPattern initialization_sparsity(const FullyImplicitDaeIR& model) {
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        positions[derivative_name(model.states[index].name)] = index;
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        positions[model.algebraics[index].name] = model.states.size() + index;
    }
    std::vector<std::vector<std::size_t>> rows(model.equations.size());
    for (std::size_t row = 0; row < model.equations.size(); ++row) {
        for (const auto& name : model.equations[row].variables) {
            const auto position = positions.find(name);
            if (position != positions.end()) rows[row].push_back(position->second);
        }
    }
    return SparsityPattern::from_rows(model.states.size() + model.algebraics.size(), rows);
}

bool augment_structural_matching(
    std::size_t row,
    const SparsityPattern& sparsity,
    std::vector<int>& column_match,
    std::vector<bool>& visited) {
    for (std::size_t offset = sparsity.row_offsets[row];
         offset < sparsity.row_offsets[row + 1]; ++offset) {
        const auto column = sparsity.column_indices[offset];
        if (visited[column]) continue;
        visited[column] = true;
        if (column_match[column] < 0 ||
            augment_structural_matching(
                static_cast<std::size_t>(column_match[column]),
                sparsity, column_match, visited)) {
            column_match[column] = static_cast<int>(row);
            return true;
        }
    }
    return false;
}

struct StepSolveResult {
    bool converged{false};
    std::vector<double> values;
    int iterations{};
    int krylov_iterations{};
    double residual_inf{};
    std::size_t jacobian_nonzeros{};
    std::size_t jacobian_storage_bytes{};
    std::size_t jacobian_colors{};
    std::size_t jacobian_evaluation_batches{};
    std::size_t jacobian_ad_batches{};
    std::size_t jacobian_fd_fallback_batches{};
    std::string inner_backend;
    bool learned_attempted{false};
    bool learned_rejected{false};
    std::size_t learned_krylov_iterations{0};
};

bool dense_solve(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right,
    std::vector<double>& solution) {
    const auto size = right.size();
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
    solution.assign(size, 0.0);
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

StepSolveResult dense_newton(
    std::vector<double> initial,
    const std::function<std::vector<double>(const std::vector<double>&)>& residual,
    const DaeTolerance& tolerance) {
    StepSolveResult result;
    result.values = std::move(initial);
    for (int iteration = 0; iteration < tolerance.maximum_newton_iterations; ++iteration) {
        const auto values = residual(result.values);
        result.iterations = iteration;
        result.residual_inf = infinity_norm(values);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, infinity_norm(result.values));
        if (result.residual_inf <= threshold) {
            result.converged = true;
            return result;
        }
        std::vector<std::vector<double>> jacobian(
            values.size(), std::vector<double>(values.size()));
        for (std::size_t column = 0; column < values.size(); ++column) {
            auto shifted = result.values;
            const double step = 1.0e-7 * std::max(1.0, std::abs(shifted[column]));
            shifted[column] += step;
            const auto shifted_values = residual(shifted);
            for (std::size_t row = 0; row < values.size(); ++row) {
                jacobian[row][column] = (shifted_values[row] - values[row]) / step;
            }
        }
        std::vector<double> right(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) right[index] = -values[index];
        std::vector<double> delta;
        if (!dense_solve(std::move(jacobian), std::move(right), delta)) return result;
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = result.values;
            for (std::size_t index = 0; index < trial.size(); ++index) {
                trial[index] += damping * delta[index];
            }
            if (infinity_norm(residual(trial)) < result.residual_inf) {
                result.values = std::move(trial);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) return result;
    }
    result.iterations = tolerance.maximum_newton_iterations;
    result.residual_inf = infinity_norm(residual(result.values));
    return result;
}

StepSolveResult sparse_newton(
    std::vector<double> initial,
    const std::function<std::vector<double>(const std::vector<double>&)>& residual,
    const std::function<std::optional<std::vector<double>>(
        const std::vector<double>&, const std::vector<double>&)>& directional_residual,
    const SparsityPattern& sparsity,
    const DaeTolerance& tolerance,
    const DaeMultigridArtifact* artifact = nullptr,
    double step = 0.0) {
    StepSolveResult result;
    result.values = std::move(initial);
    if (artifact != nullptr) {
        result.learned_attempted = true;
        try {
            artifact->validate();
        } catch (const std::exception&) {
            result.learned_rejected = true;
            return result;
        }
        if (artifact->residual_family != "fully-implicit-first-order-step" ||
            artifact->unknown_count != result.values.size() ||
            step < artifact->minimum_step - 1.0e-14 ||
            step > artifact->maximum_step + 1.0e-14) {
            result.learned_rejected = true;
            return result;
        }
    }
    const auto colors = sparsity.greedy_column_coloring();
    result.jacobian_colors = colors.empty()
        ? 0
        : *std::max_element(colors.begin(), colors.end()) + 1;
    for (int iteration = 0; iteration < tolerance.maximum_newton_iterations; ++iteration) {
        const auto values = residual(result.values);
        result.iterations = iteration;
        result.residual_inf = infinity_norm(values);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, infinity_norm(result.values));
        if (result.residual_inf <= threshold) {
            result.converged = true;
            return result;
        }
        LinearSystem system;
        system.sparsity = sparsity;
        system.sparse_values.resize(sparsity.nonzeros());
        system.right_hand_side.resize(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            system.right_hand_side[index] = -values[index];
        }
        for (std::size_t color = 0; color < result.jacobian_colors; ++color) {
            std::vector<double> direction(result.values.size());
            for (std::size_t column = 0; column < colors.size(); ++column) {
                if (colors[column] == color) direction[column] = 1.0;
            }
            const auto derivatives = directional_residual(result.values, direction);
            if (derivatives.has_value()) {
                ++result.jacobian_evaluation_batches;
                ++result.jacobian_ad_batches;
                for (std::size_t row = 0; row < sparsity.row_count; ++row) {
                    for (std::size_t offset = sparsity.row_offsets[row];
                         offset < sparsity.row_offsets[row + 1]; ++offset) {
                        const auto column = sparsity.column_indices[offset];
                        if (colors[column] == color) {
                            system.sparse_values[offset] = (*derivatives)[row];
                            break;
                        }
                    }
                }
                continue;
            }
            auto shifted = result.values;
            std::vector<double> steps(result.values.size());
            for (std::size_t column = 0; column < colors.size(); ++column) {
                if (colors[column] != color) continue;
                steps[column] = 1.0e-7 * std::max(1.0, std::abs(shifted[column]));
                shifted[column] += steps[column];
            }
            const auto shifted_values = residual(shifted);
            ++result.jacobian_evaluation_batches;
            ++result.jacobian_fd_fallback_batches;
            for (std::size_t row = 0; row < sparsity.row_count; ++row) {
                for (std::size_t offset = sparsity.row_offsets[row];
                     offset < sparsity.row_offsets[row + 1]; ++offset) {
                    const auto column = sparsity.column_indices[offset];
                    if (colors[column] == color) {
                        system.sparse_values[offset] =
                            (shifted_values[row] - values[row]) / steps[column];
                        break;
                    }
                }
            }
        }
        classify_linear_system(system, 1.0e-7);
        result.jacobian_nonzeros = system.nonzeros();
        result.jacobian_storage_bytes = system.sparse_storage_bytes();
        KrylovResult krylov;
        if (artifact != nullptr) {
            if (!system.positive_definite) {
                result.learned_rejected = true;
                return result;
            }
            double maximum_drift{};
            for (std::size_t row = 0; row < sparsity.row_count; ++row) {
                for (std::size_t offset = sparsity.row_offsets[row];
                     offset < sparsity.row_offsets[row + 1]; ++offset) {
                    const auto column = sparsity.column_indices[offset];
                    const double reference = artifact->hierarchy.fine_operator[row][column];
                    maximum_drift = std::max(
                        maximum_drift,
                        std::abs(system.sparse_values[offset] - reference) /
                            std::max(1.0, std::abs(reference)));
                }
            }
            const double drift_limit = std::max(
                1.0e-6, artifact->hierarchy.maximum_matrix_drift * 1.25 + 1.0e-12);
            if (!std::isfinite(maximum_drift) || maximum_drift > drift_limit) {
                result.learned_rejected = true;
                return result;
            }
            result.inner_backend = "pcg-learned-multigrid-cpu-v1";
            krylov = preconditioned_conjugate_gradient(
                system, std::vector<double>(values.size()),
                [&](const std::vector<double>& residual_values,
                    std::vector<double>& applied) {
                    return apply_learned_multigrid(
                        artifact->hierarchy, residual_values, applied);
                },
                tolerance.absolute, tolerance.relative,
                std::max(20, static_cast<int>(values.size()) * 4));
        } else if (system.positive_definite) {
            result.inner_backend = "pcg-ic0-cpu-v1";
            krylov = preconditioned_conjugate_gradient(
                system, std::vector<double>(values.size()),
                incomplete_cholesky_zero_preconditioner(system, sparsity),
                tolerance.absolute, tolerance.relative,
                std::max(20, static_cast<int>(values.size()) * 4));
        } else {
            result.inner_backend = "gmres-ilu0-cpu-v1";
            krylov = restarted_gmres(
                system, std::vector<double>(values.size()),
                incomplete_lu_zero_preconditioner(system, sparsity),
                tolerance.absolute, tolerance.relative,
                std::max(20, static_cast<int>(values.size()) * 4),
                std::min(40, static_cast<int>(values.size())));
        }
        result.krylov_iterations += krylov.iterations;
        if (artifact != nullptr) {
            result.learned_krylov_iterations += krylov.iterations;
        }
        if (!krylov.converged) {
            if (artifact != nullptr) result.learned_rejected = true;
            return result;
        }
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = result.values;
            for (std::size_t index = 0; index < trial.size(); ++index) {
                trial[index] += damping * krylov.solution[index];
            }
            if (infinity_norm(residual(trial)) < result.residual_inf) {
                result.values = std::move(trial);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) {
            if (artifact != nullptr) result.learned_rejected = true;
            return result;
        }
    }
    result.iterations = tolerance.maximum_newton_iterations;
    result.residual_inf = infinity_norm(residual(result.values));
    return result;
}

struct FullyImplicitCandidateSolve {
    StepSolveResult solved;
    bool dense_fallback{false};
};

FullyImplicitCandidateSolve solve_fully_implicit_step(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    std::vector<double> initial,
    double target_time,
    double step,
    const SparsityPattern& sparsity,
    const DaeTolerance& tolerance,
    const DaeMultigridArtifact* artifact = nullptr) {
    const auto residual = [&](const std::vector<double>& candidate) {
        return step_residual(model, previous_state, candidate, target_time, step);
    };
    const auto directional = [&] (
        const std::vector<double>& candidate,
        const std::vector<double>& direction) {
        return directional_step_residual(
            model, previous_state, candidate, direction, target_time, step);
    };
    FullyImplicitCandidateSolve result;
    if (artifact != nullptr && artifact->model_source_hash != model.source_hash) {
        result.solved = sparse_newton(
            initial, residual, directional, sparsity, tolerance);
        result.solved.learned_attempted = true;
        result.solved.learned_rejected = true;
    } else if (artifact != nullptr) {
        result.solved = sparse_newton(
            initial, residual, directional, sparsity, tolerance, artifact, step);
        if (result.solved.converged) return result;
        const auto learned_iterations = result.solved.learned_krylov_iterations;
        result.solved = sparse_newton(
            initial, residual, directional, sparsity, tolerance);
        result.solved.learned_attempted = true;
        result.solved.learned_rejected = true;
        result.solved.learned_krylov_iterations = learned_iterations;
        result.solved.krylov_iterations += learned_iterations;
    } else {
        result.solved = sparse_newton(
            initial, residual, directional, sparsity, tolerance);
    }
    if (!result.solved.converged) {
        result.solved = dense_newton(std::move(initial), residual, tolerance);
        result.dense_fallback = true;
    }
    const double threshold = tolerance.absolute + tolerance.relative *
        std::max(1.0, infinity_norm(result.solved.values));
    if (!result.solved.converged || !std::isfinite(result.solved.residual_inf) ||
        result.solved.residual_inf > threshold) {
        result.solved.converged = false;
    }
    return result;
}

FullyImplicitCandidateSolve project_fully_implicit_event(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time,
    const SparsityPattern& sparsity,
    const DaeTolerance& tolerance) {
    std::vector<double> initial(model.states.size(), 0.0);
    initial.insert(initial.end(), algebraic.begin(), algebraic.end());
    const auto residual = [&](const std::vector<double>& candidate) {
        return initialization_residual(model, state, candidate, time);
    };
    const auto directional = [&] (
        const std::vector<double>& candidate,
        const std::vector<double>& direction) {
        return directional_initialization_residual(
            model, state, candidate, direction, time);
    };
    FullyImplicitCandidateSolve result;
    result.solved = sparse_newton(
        initial, residual, directional, sparsity, tolerance);
    if (!result.solved.converged) {
        result.solved = dense_newton(std::move(initial), residual, tolerance);
        result.dense_fallback = true;
    }
    const double threshold = tolerance.absolute + tolerance.relative *
        std::max(1.0, infinity_norm(result.solved.values));
    if (!result.solved.converged || !std::isfinite(result.solved.residual_inf) ||
        result.solved.residual_inf > threshold) {
        result.solved.converged = false;
    }
    return result;
}

struct FullyImplicitEventTransaction {
    std::vector<double> state;
    std::vector<double> algebraic;
    std::vector<double> derivatives;
    std::vector<DaeEventRecord> records;
    std::size_t projection_solves{0};
    std::size_t projection_krylov_iterations{0};
    std::size_t dense_projection_fallbacks{0};
    double maximum_projection_residual_inf{0.0};
};

FullyImplicitEventTransaction execute_fully_implicit_event_transaction(
    const FullyImplicitDaeIR& model,
    double event_time,
    const std::vector<double>& event_state,
    const std::vector<double>& event_algebraic,
    std::vector<std::size_t> pending,
    const SparsityPattern& initialization_pattern,
    const DaeTolerance& tolerance) {
    FullyImplicitEventTransaction transaction;
    transaction.state = event_state;
    transaction.algebraic = event_algebraic;
    const auto event_pre_state = event_state;
    std::vector<bool> processed(model.events.size(), false);
    std::size_t iterations{};
    while (!pending.empty()) {
        if (++iterations > model.events.size()) {
            throw std::runtime_error(
                "fully implicit DAE event iteration did not reach a fixed point");
        }
        std::sort(pending.begin(), pending.end(), [&](const auto left, const auto right) {
            const auto& left_event = model.events[left];
            const auto& right_event = model.events[right];
            if (left_event.priority != right_event.priority) {
                return left_event.priority > right_event.priority;
            }
            return left_event.source_order < right_event.source_order;
        });
        const auto iteration_pre_state = transaction.state;
        const auto iteration_pre_algebraic = transaction.algebraic;
        const auto values = event_context(
            model, iteration_pre_state, iteration_pre_algebraic,
            event_time, &event_pre_state);
        std::vector<double> guards_before;
        guards_before.reserve(model.events.size());
        for (const auto& event : model.events) {
            guards_before.push_back(Expression(event.guard).evaluate(values));
        }
        auto post_state = iteration_pre_state;
        std::unordered_set<std::string> reset_variables;
        for (const auto selected : pending) {
            processed[selected] = true;
            const auto& event = model.events[selected];
            for (const auto& reset : event.resets) {
                if (!reset_variables.insert(reset.variable).second) {
                    throw std::runtime_error(
                        "fully implicit DAE simultaneous reset conflict: " + reset.variable);
                }
                const auto position = std::find_if(
                    model.states.begin(), model.states.end(), [&](const auto& state) {
                        return state.name == reset.variable;
                    });
                const auto index = static_cast<std::size_t>(
                    std::distance(model.states.begin(), position));
                const double reset_value = Expression(reset.expression).evaluate(values);
                if (!std::isfinite(reset_value)) {
                    throw std::runtime_error(event.id + ": reset produced NaN/Inf");
                }
                post_state[index] = reset_value;
            }
        }
        const auto projection = project_fully_implicit_event(
            model, post_state, iteration_pre_algebraic, event_time,
            initialization_pattern, tolerance);
        ++transaction.projection_solves;
        transaction.projection_krylov_iterations += projection.solved.krylov_iterations;
        transaction.dense_projection_fallbacks += projection.dense_fallback ? 1U : 0U;
        transaction.maximum_projection_residual_inf = std::max(
            transaction.maximum_projection_residual_inf,
            projection.solved.residual_inf);
        if (!projection.solved.converged) {
            throw std::runtime_error(
                "fully implicit DAE reset consistency projection failed");
        }
        transaction.state = std::move(post_state);
        transaction.derivatives.assign(
            projection.solved.values.begin(),
            projection.solved.values.begin() + model.states.size());
        transaction.algebraic.assign(
            projection.solved.values.begin() + model.states.size(),
            projection.solved.values.end());
        for (const auto selected : pending) {
            DaeEventRecord record;
            record.id = model.events[selected].id;
            record.time = event_time;
            record.pre_state = named_values(model.states, iteration_pre_state);
            record.post_state = named_values(model.states, transaction.state);
            record.pre_algebraics = named_values(
                model.algebraics, iteration_pre_algebraic);
            record.post_algebraics = named_values(
                model.algebraics, transaction.algebraic);
            transaction.records.push_back(std::move(record));
        }
        const auto post_values = event_context(
            model, transaction.state, transaction.algebraic,
            event_time, &event_pre_state);
        std::vector<std::size_t> activated;
        for (std::size_t index = 0; index < model.events.size(); ++index) {
            if (processed[index]) continue;
            const double after = Expression(model.events[index].guard).evaluate(post_values);
            if (!event_guard_active(
                    guards_before[index], model.events[index].direction,
                    tolerance.guard) &&
                event_guard_active(
                    after, model.events[index].direction, tolerance.guard)) {
                activated.push_back(index);
            }
        }
        pending = std::move(activated);
    }
    return transaction;
}

struct LocatedFullyImplicitEvent {
    std::size_t index{};
    double offset{};
    StepSolveResult solution;
    bool dense_fallback{false};
};

LocatedFullyImplicitEvent locate_fully_implicit_event(
    const FullyImplicitDaeIR& model,
    std::size_t event_index,
    const std::vector<double>& previous_state,
    const std::vector<double>& previous_algebraic,
    double start_time,
    double step,
    const StepSolveResult& endpoint,
    const SparsityPattern& sparsity,
    const DaeTolerance& tolerance) {
    double left{};
    double right = step;
    auto right_solution = endpoint;
    bool dense_fallback{};
    while (right - left > tolerance.root_time) {
        const double middle = left + 0.5 * (right - left);
        std::vector<double> initial = previous_state;
        initial.insert(initial.end(), previous_algebraic.begin(), previous_algebraic.end());
        const auto solved = solve_fully_implicit_step(
            model, previous_state, std::move(initial), start_time + middle,
            middle, sparsity, tolerance);
        if (!solved.solved.converged) {
            throw std::runtime_error("fully implicit DAE event root substep failed");
        }
        std::vector<double> middle_state(
            solved.solved.values.begin(),
            solved.solved.values.begin() + model.states.size());
        std::vector<double> middle_algebraic(
            solved.solved.values.begin() + model.states.size(),
            solved.solved.values.end());
        const double guard = Expression(model.events[event_index].guard).evaluate(
            event_context(model, middle_state, middle_algebraic, start_time + middle));
        const bool root_is_left = model.events[event_index].direction > 0
            ? guard >= 0.0
            : guard <= 0.0;
        if (root_is_left) {
            right = middle;
            right_solution = solved.solved;
            dense_fallback = dense_fallback || solved.dense_fallback;
        } else {
            left = middle;
        }
    }
    return {event_index, right, std::move(right_solution), dense_fallback};
}

}  // namespace

void FullyImplicitDaeIR::validate() const {
    if (schema_version != kFullyImplicitDaeSchemaVersion ||
        structural_class != "fully-implicit-first-order-candidate" ||
        model_id.empty() || source_hash.empty()) {
        throw std::invalid_argument("invalid fully implicit DAE identity/class");
    }
    std::unordered_set<std::string> known{"time"};
    for (const auto& [name, value] : parameters) {
        if (!known.insert(name).second || !std::isfinite(value)) {
            throw std::invalid_argument("invalid fully implicit DAE parameter: " + name);
        }
    }
    std::unordered_set<std::string> differentiated;
    for (const auto& variable : states) {
        if (!known.insert(variable.name).second || !std::isfinite(variable.start) ||
            !(variable.nominal > 0.0)) {
            throw std::invalid_argument("invalid fully implicit DAE state: " + variable.name);
        }
        known.insert(derivative_name(variable.name));
    }
    for (const auto& variable : algebraics) {
        if (!known.insert(variable.name).second || !std::isfinite(variable.start) ||
            !(variable.nominal > 0.0)) {
            throw std::invalid_argument("invalid fully implicit DAE algebraic: " + variable.name);
        }
    }
    if (states.empty() || equations.size() != states.size() + algebraics.size()) {
        throw std::invalid_argument("fully implicit DAE must be square and contain states");
    }
    for (const auto& equation : equations) {
        if (equation.id.empty()) throw std::invalid_argument("fully implicit DAE equation lacks id");
        for (const auto& name : expression_names(equation.residual)) {
            if (!known.contains(name)) {
                throw std::invalid_argument(
                    "fully implicit DAE equation references unknown name: " + name);
            }
            if (name.starts_with("__smave_der_")) differentiated.insert(name);
        }
    }
    for (const auto& state : states) {
        if (!differentiated.contains(derivative_name(state.name))) {
            throw std::invalid_argument(
                "fully implicit DAE state lacks derivative incidence: " + state.name);
        }
    }
    std::unordered_set<std::string> event_ids;
    std::unordered_set<std::string> state_names;
    for (const auto& state : states) state_names.insert(state.name);
    for (const auto& event : events) {
        if (!event_ids.insert(event.id).second || event.id.empty() ||
            (event.direction != -1 && event.direction != 1) || event.resets.empty()) {
            throw std::invalid_argument("invalid fully implicit DAE event: " + event.id);
        }
        for (const auto& name : expression_names(event.guard)) {
            if (!known.contains(name) || name.starts_with("__smave_der_")) {
                throw std::invalid_argument(
                    "fully implicit DAE event guard references invalid name: " + name);
            }
        }
        std::unordered_set<std::string> reset_states;
        for (const auto& reset : event.resets) {
            if (!state_names.contains(reset.variable) ||
                !reset_states.insert(reset.variable).second) {
                throw std::invalid_argument(
                    "fully implicit DAE event has invalid reset state: " + reset.variable);
            }
            for (const auto& name : expression_names(reset.expression)) {
                const bool pre_state = name.starts_with("__smave_pre_") &&
                    state_names.contains(name.substr(std::string("__smave_pre_").size()));
                if (!known.contains(name) && !pre_state) {
                    throw std::invalid_argument(
                        "fully implicit DAE reset references unknown name: " + name);
                }
            }
        }
    }
    for (const auto& [label, sparsity] : std::vector<std::pair<std::string, SparsityPattern>>{
             {"step", step_sparsity(*this)},
             {"initialization", initialization_sparsity(*this)}}) {
        if (sparsity.row_count != equations.size() ||
            sparsity.column_count != states.size() + algebraics.size()) {
            throw std::invalid_argument("invalid fully implicit DAE " + label + " sparsity");
        }
        std::vector<int> column_match(sparsity.column_count, -1);
        for (std::size_t row = 0; row < sparsity.row_count; ++row) {
            std::vector<bool> visited(sparsity.column_count, false);
            if (!augment_structural_matching(row, sparsity, column_match, visited)) {
                throw std::invalid_argument(
                    "fully implicit DAE " + label +
                    " incidence lacks a square structural matching");
            }
        }
    }
}

void FullyImplicitDaeIR::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write fully implicit DAE IR: " + path.string());
    output << "SMAVE_FULLY_IMPLICIT_DAE 2\nMODEL " << std::quoted(model_id)
           << "\nSOURCE_HASH " << std::quoted(source_hash)
           << "\nSTRUCTURAL_CLASS " << std::quoted(structural_class)
           << "\nPARAMETERS " << parameters.size();
    for (const auto& [name, value] : parameters) {
        output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << value;
    }
    output << "\nSTATES " << states.size() << '\n';
    for (const auto& item : states) {
        output << "STATE " << std::quoted(item.name) << ' ' << item.start << ' '
               << item.nominal << '\n';
    }
    output << "ALGEBRAICS " << algebraics.size() << '\n';
    for (const auto& item : algebraics) {
        output << "ALGEBRAIC " << std::quoted(item.name) << ' ' << item.start << ' '
               << item.nominal << '\n';
    }
    output << "EQUATIONS " << equations.size() << '\n';
    for (const auto& equation : equations) {
        output << "EQUATION " << std::quoted(equation.id) << ' '
               << std::quoted(equation.residual) << '\n';
    }
    output << "EVENTS " << events.size() << '\n';
    for (const auto& event : events) {
        output << "EVENT " << std::quoted(event.id) << ' ' << event.direction << ' '
               << event.priority << ' ' << event.source_order << ' '
               << std::quoted(event.guard) << ' ' << event.resets.size();
        for (const auto& reset : event.resets) {
            output << ' ' << std::quoted(reset.variable) << ' '
                   << std::quoted(reset.expression);
        }
        output << '\n';
    }
    output << "END\n";
}

FullyImplicitDaeIR FullyImplicitDaeIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read fully implicit DAE IR: " + path.string());
    require_tag(input, "SMAVE_FULLY_IMPLICIT_DAE");
    int version{};
    input >> version;
    if (version < 1 || version > 2) {
        throw std::invalid_argument("unsupported fully implicit DAE version");
    }
    FullyImplicitDaeIR model;
    require_tag(input, "MODEL");
    input >> std::quoted(model.model_id);
    require_tag(input, "SOURCE_HASH");
    input >> std::quoted(model.source_hash);
    require_tag(input, "STRUCTURAL_CLASS");
    input >> std::quoted(model.structural_class);
    std::size_t count{};
    require_tag(input, "PARAMETERS");
    input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name;
        double value{};
        input >> std::quoted(name) >> value;
        model.parameters[name] = value;
    }
    require_tag(input, "STATES");
    input >> count;
    model.states.resize(count);
    for (auto& item : model.states) {
        require_tag(input, "STATE");
        input >> std::quoted(item.name) >> item.start >> item.nominal;
    }
    require_tag(input, "ALGEBRAICS");
    input >> count;
    model.algebraics.resize(count);
    for (auto& item : model.algebraics) {
        require_tag(input, "ALGEBRAIC");
        input >> std::quoted(item.name) >> item.start >> item.nominal;
    }
    require_tag(input, "EQUATIONS");
    input >> count;
    model.equations.resize(count);
    for (auto& equation : model.equations) {
        require_tag(input, "EQUATION");
        input >> std::quoted(equation.id) >> std::quoted(equation.residual);
        equation.variables = expression_names(equation.residual);
    }
    if (version >= 2) {
        require_tag(input, "EVENTS");
        input >> count;
        model.events.resize(count);
        for (auto& event : model.events) {
            require_tag(input, "EVENT");
            std::size_t reset_count{};
            input >> std::quoted(event.id) >> event.direction >> event.priority
                  >> event.source_order >> std::quoted(event.guard) >> reset_count;
            event.resets.resize(reset_count);
            for (auto& reset : event.resets) {
                input >> std::quoted(reset.variable) >> std::quoted(reset.expression);
            }
        }
    }
    require_tag(input, "END");
    model.schema_version = kFullyImplicitDaeSchemaVersion;
    model.validate();
    return model;
}

FullyImplicitDaeIR compile_fully_implicit_dae(
    const std::filesystem::path& source,
    const std::string& top) {
    const auto raw = read_source(source);
    auto text = strip_comments(raw);
    std::smatch match;
    if (!std::regex_search(text, match, std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) {
        throw std::invalid_argument("fully implicit DAE source has no model declaration");
    }
    FullyImplicitDaeIR model;
    model.model_id = match[1].str();
    model.source_hash = source_digest(raw);
    if (!top.empty() && top != model.model_id) {
        throw std::invalid_argument("requested fully implicit DAE top does not match source");
    }
    text = text.substr(match.position() + match.length());
    text = std::regex_replace(
        text, std::regex("end\\s+" + model.model_id + R"(\s*;\s*$)"), "");
    if (std::regex_search(text, std::regex(R"(\binitial\s+equation\b)"))) {
        throw std::invalid_argument(
            "fully implicit DAE subset does not support initial equations");
    }
    const auto equation_position = text.find("equation");
    if (equation_position == std::string::npos) {
        throw std::invalid_argument("fully implicit DAE model has no equation section");
    }
    const std::regex declaration(
        R"(^\s*(parameter\s+)?Real\s+([A-Za-z_]\w*)\s*(\([^;]*\))?\s*(?:=\s*(.+))?\s*$)");
    std::vector<DaeVariableIR> declared_variables;
    for (auto statement : split_statements(text.substr(0, equation_position))) {
        std::smatch item;
        if (!std::regex_match(statement, item, declaration)) {
            throw std::invalid_argument(
                "unsupported fully implicit DAE declaration: " + statement);
        }
        const auto attributes = parse_attributes(item[3].str());
        for (const auto& [name, value] : attributes) {
            (void)value;
            if (name != "start" && name != "nominal") {
                throw std::invalid_argument("unsupported fully implicit DAE attribute: " + name);
            }
        }
        DaeVariableIR variable;
        variable.name = item[2].str();
        variable.start = attribute_value(attributes, "start", 0.0);
        variable.nominal = attribute_value(attributes, "nominal", 1.0);
        if (item[1].matched) {
            if (!attributes.empty() || !item[4].matched) {
                throw std::invalid_argument("fully implicit DAE parameter requires a plain value");
            }
            model.parameters[variable.name] = std::stod(trim(item[4].str()));
        } else if (item[4].matched) {
            throw std::invalid_argument(
                "fully implicit DAE variable binding equations are unsupported");
        } else {
            declared_variables.push_back(variable);
        }
    }
    std::unordered_set<std::string> state_names;
    auto equation_section = text.substr(equation_position + 8);
    const std::regex when_block(
        R"(\bwhen\s+([\s\S]*?)\s+then\s*([\s\S]*?)\s*end\s+when\s*;)");
    std::size_t event_order{};
    for (auto iterator = std::sregex_iterator(
             equation_section.begin(), equation_section.end(), when_block);
         iterator != std::sregex_iterator(); ++iterator) {
        if (std::regex_search((*iterator)[2].str(), std::regex(R"(\belsewhen\b)"))) {
            throw std::invalid_argument(
                "fully implicit DAE event subset does not support elsewhen");
        }
        std::smatch guard_match;
        const auto guard_source = trim((*iterator)[1].str());
        if (!std::regex_match(
                guard_source, guard_match,
                std::regex(R"(^\s*(.+?)\s*(<=|>=|<|>)\s*(.+)\s*$)"))) {
            throw std::invalid_argument(
                "fully implicit DAE event guard must be one scalar comparison");
        }
        if (guard_source.find("der(") != std::string::npos) {
            throw std::invalid_argument(
                "fully implicit DAE event guards cannot reference derivatives");
        }
        DaeEventIR event;
        event.id = "event-" + std::to_string(event_order + 1);
        event.source_order = event_order++;
        event.guard = "(" + trim(guard_match[1].str()) + ")-(" +
            trim(guard_match[3].str()) + ")";
        const auto comparison = guard_match[2].str();
        event.direction = comparison == ">" || comparison == ">=" ? 1 : -1;
        const auto body = (*iterator)[2].str();
        const std::regex reset_expression(
            R"(\breinit\s*\(\s*([A-Za-z_]\w*)\s*,\s*([\s\S]*?)\)\s*;)");
        std::string remaining = body;
        for (auto reset = std::sregex_iterator(
                 body.begin(), body.end(), reset_expression);
             reset != std::sregex_iterator(); ++reset) {
            const auto expression = trim((*reset)[2].str());
            if (expression.find("der(") != std::string::npos) {
                throw std::invalid_argument(
                    "fully implicit DAE reset cannot reference derivatives");
            }
            event.resets.push_back({
                (*reset)[1].str(), rewrite_pre(expression)});
        }
        remaining = std::regex_replace(remaining, reset_expression, "");
        if (!trim(remaining).empty() || event.resets.empty()) {
            throw std::invalid_argument(
                "fully implicit DAE event body supports only reinit statements");
        }
        model.events.push_back(std::move(event));
    }
    equation_section = std::regex_replace(equation_section, when_block, "");
    for (auto statement : split_statements(equation_section)) {
        const auto equal = statement.find('=');
        if (equal == std::string::npos) {
            throw std::invalid_argument("invalid fully implicit DAE equation: " + statement);
        }
        DaeEquationIR equation;
        equation.id = "implicit-equation-" + std::to_string(model.equations.size() + 1);
        equation.residual = rewrite_derivatives(
            "(" + trim(statement.substr(0, equal)) + ") - (" +
                trim(statement.substr(equal + 1)) + ")",
            state_names);
        equation.variables = expression_names(equation.residual);
        model.equations.push_back(std::move(equation));
    }
    std::unordered_set<std::string> declared_names;
    for (const auto& variable : declared_variables) declared_names.insert(variable.name);
    for (const auto& state : state_names) {
        if (!declared_names.contains(state)) {
            throw std::invalid_argument(
                "fully implicit DAE derivative references undeclared state: " + state);
        }
    }
    for (const auto& variable : declared_variables) {
        if (state_names.contains(variable.name)) model.states.push_back(variable);
        else model.algebraics.push_back(variable);
    }
    model.validate();
    return model;
}

std::vector<double> evaluate_fully_implicit_dae_step_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate_state,
    const std::vector<double>& candidate_algebraic,
    double target_time,
    double step) {
    model.validate();
    if (previous_state.size() != model.states.size() ||
        candidate_state.size() != model.states.size() ||
        candidate_algebraic.size() != model.algebraics.size() ||
        !(step > 0.0) || !std::isfinite(target_time) || !std::isfinite(step)) {
        throw std::invalid_argument("invalid fully implicit DAE residual context");
    }
    std::vector<double> candidate = candidate_state;
    candidate.insert(candidate.end(), candidate_algebraic.begin(), candidate_algebraic.end());
    return step_residual(model, previous_state, candidate, target_time, step);
}

std::vector<double> evaluate_fully_implicit_dae_initial_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& derivative,
    const std::vector<double>& algebraic,
    double time) {
    model.validate();
    if (state.size() != model.states.size() ||
        derivative.size() != model.states.size() ||
        algebraic.size() != model.algebraics.size() || !std::isfinite(time)) {
        throw std::invalid_argument("invalid fully implicit DAE initialization context");
    }
    std::vector<double> candidate = derivative;
    candidate.insert(candidate.end(), algebraic.begin(), algebraic.end());
    return initialization_residual(model, state, candidate, time);
}

FullyImplicitDaeRunResult simulate_fully_implicit_dae(
    const FullyImplicitDaeIR& model,
    double end_time,
    double maximum_step,
    DaeTolerance tolerance,
    const DaeMultigridArtifact* artifact) {
    model.validate();
    if (!(end_time >= 0.0) || !(maximum_step > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(maximum_step)) {
        throw std::invalid_argument("invalid fully implicit DAE simulation horizon");
    }
    FullyImplicitDaeRunResult result;
    const auto plan = route_fully_implicit_dae(model, {}, artifact);
    result.plan_id = plan.plan_id;
    result.terminal_fallback = plan.terminal_fallback;
    if (!plan.steps.empty()) {
        result.solver_backend = plan.steps.front().expert_version;
        result.backend_chain = plan.steps.front().backend_chain;
    } else {
        result.solver_backend = plan.terminal_fallback;
        result.backend_chain = {plan.terminal_fallback, "original-dae-residual-gate"};
    }
    std::vector<double> state;
    std::vector<double> algebraic;
    for (const auto& item : model.states) {
        state.push_back(item.start);
        result.initial_state[item.name] = item.start;
    }
    for (const auto& item : model.algebraics) {
        algebraic.push_back(item.start);
        result.initial_algebraics[item.name] = item.start;
    }
    const auto sparsity = step_sparsity(model);
    const auto initial_sparsity = initialization_sparsity(model);
    if (artifact != nullptr) {
        result.dae_preconditioner_version = artifact->hierarchy.expert_version;
    }
    double time{};
    try {
        std::vector<double> initial_candidate(model.states.size(), 0.0);
        initial_candidate.insert(
            initial_candidate.end(), algebraic.begin(), algebraic.end());
        const auto initial_residual = [&](const std::vector<double>& candidate) {
            return initialization_residual(model, state, candidate, 0.0);
        };
        const auto initial_directional = [&] (
            const std::vector<double>& candidate,
            const std::vector<double>& direction) {
            return directional_initialization_residual(
                model, state, candidate, direction, 0.0);
        };
        auto initialized = sparse_newton(
            initial_candidate, initial_residual, initial_directional,
            initial_sparsity, tolerance);
        if (initialized.converged) {
            result.sparse_initialization = true;
        } else {
            initialized = dense_newton(initial_candidate, initial_residual, tolerance);
            ++result.dense_initialization_fallbacks;
        }
        const double initialization_threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, infinity_norm(initialized.values));
        if (!initialized.converged || !std::isfinite(initialized.residual_inf) ||
            initialized.residual_inf > initialization_threshold) {
            throw std::runtime_error(
                "fully implicit DAE consistent initialization and residual gate failed");
        }
        result.initialization_iterations = initialized.iterations;
        result.initialization_krylov_iterations = initialized.krylov_iterations;
        result.initialization_residual_inf = initialized.residual_inf;
        result.initialization_jacobian_nonzeros = initialized.jacobian_nonzeros;
        result.initialization_jacobian_storage_bytes = initialized.jacobian_storage_bytes;
        result.initialization_jacobian_colors = initialized.jacobian_colors;
        result.initialization_jacobian_evaluation_batches =
            initialized.jacobian_evaluation_batches;
        result.initialization_jacobian_ad_batches = initialized.jacobian_ad_batches;
        result.initialization_jacobian_fd_fallback_batches =
            initialized.jacobian_fd_fallback_batches;
        result.initialization_inner_backend = initialized.inner_backend;
        for (std::size_t index = 0; index < model.states.size(); ++index) {
            result.initial_derivatives[model.states[index].name] =
                initialized.values[index];
        }
        algebraic.assign(
            initialized.values.begin() + model.states.size(), initialized.values.end());
        for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
            result.initial_algebraics[model.algebraics[index].name] = algebraic[index];
        }
        std::vector<std::size_t> initial_pending;
        const auto initialized_values = event_context(model, state, algebraic, 0.0);
        for (std::size_t index = 0; index < model.events.size(); ++index) {
            const double guard = Expression(model.events[index].guard).evaluate(
                initialized_values);
            if (event_guard_active(
                    guard, model.events[index].direction, tolerance.guard)) {
                initial_pending.push_back(index);
            }
        }
        if (!initial_pending.empty()) {
            const auto transaction = execute_fully_implicit_event_transaction(
                model, 0.0, state, algebraic, std::move(initial_pending),
                initial_sparsity, tolerance);
            state = transaction.state;
            algebraic = transaction.algebraic;
            result.initial_events = transaction.records;
            result.event_projection_solves += transaction.projection_solves;
            result.event_projection_krylov_iterations +=
                transaction.projection_krylov_iterations;
            result.dense_event_projection_fallbacks +=
                transaction.dense_projection_fallbacks;
            result.maximum_event_projection_residual_inf = std::max(
                result.maximum_event_projection_residual_inf,
                transaction.maximum_projection_residual_inf);
            for (std::size_t index = 0; index < model.states.size(); ++index) {
                result.initial_derivatives[model.states[index].name] =
                    transaction.derivatives[index];
            }
        }
        for (std::size_t index = 0; index < model.states.size(); ++index) {
            result.initial_state[model.states[index].name] = state[index];
        }
        for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
            result.initial_algebraics[model.algebraics[index].name] = algebraic[index];
        }
        std::vector<bool> event_armed;
        const auto post_initial_values = event_context(model, state, algebraic, 0.0);
        for (const auto& event : model.events) {
            event_armed.push_back(!event_guard_active(
                Expression(event.guard).evaluate(post_initial_values),
                event.direction, tolerance.guard));
        }
        std::size_t boundary_index{1};
        while (time + 1.0e-14 < end_time) {
            const double boundary_time = std::min(
                end_time, static_cast<double>(boundary_index) * maximum_step);
            if (boundary_time <= time + 1.0e-14) {
                ++boundary_index;
                continue;
            }
            const double step = boundary_time - time;
            std::vector<double> initial = state;
            initial.insert(initial.end(), algebraic.begin(), algebraic.end());
            const auto candidate = solve_fully_implicit_step(
                model, state, std::move(initial), boundary_time, step,
                sparsity, tolerance, artifact);
            auto solved = candidate.solved;
            if (solved.learned_attempted) {
                result.learned_krylov_iterations += solved.learned_krylov_iterations;
                if (solved.learned_rejected) {
                    ++result.learned_rejections;
                } else {
                    ++result.learned_preconditioned_steps;
                    result.learned_preconditioned_newton_iterations += solved.iterations;
                }
            }
            if (!candidate.dense_fallback && solved.converged) {
                ++result.sparse_newton_steps;
                result.sparse_newton_iterations += solved.iterations;
                result.sparse_krylov_iterations += solved.krylov_iterations;
                result.sparse_jacobian_nonzeros = solved.jacobian_nonzeros;
                result.sparse_jacobian_storage_bytes = solved.jacobian_storage_bytes;
                result.sparse_jacobian_colors = solved.jacobian_colors;
                result.sparse_jacobian_evaluation_batches += solved.jacobian_evaluation_batches;
                result.sparse_jacobian_ad_batches += solved.jacobian_ad_batches;
                result.sparse_jacobian_fd_fallback_batches +=
                    solved.jacobian_fd_fallback_batches;
                result.sparse_inner_backend = solved.inner_backend;
            } else if (candidate.dense_fallback) {
                ++result.rejected_steps;
                ++result.dense_step_fallbacks;
            }
            if (!solved.converged) {
                throw std::runtime_error(
                    "fully implicit DAE Newton and residual gate failed at t=" +
                    std::to_string(boundary_time));
            }
            std::vector<double> candidate_state(
                solved.values.begin(), solved.values.begin() + model.states.size());
            std::vector<double> candidate_algebraic(
                solved.values.begin() + model.states.size(), solved.values.end());
            const auto start_values = event_context(model, state, algebraic, time);
            const auto end_values = event_context(
                model, candidate_state, candidate_algebraic, boundary_time);
            std::vector<LocatedFullyImplicitEvent> located;
            for (std::size_t index = 0; index < model.events.size(); ++index) {
                const double left_guard = Expression(model.events[index].guard).evaluate(
                    start_values);
                const double right_guard = Expression(model.events[index].guard).evaluate(
                    end_values);
                if (event_armed[index] && event_guard_crossing(
                        left_guard, right_guard, model.events[index].direction,
                        tolerance.guard)) {
                    located.push_back(locate_fully_implicit_event(
                        model, index, state, algebraic, time, step,
                        solved, sparsity, tolerance));
                }
            }
            if (located.empty()) {
                state = std::move(candidate_state);
                algebraic = std::move(candidate_algebraic);
                result.steps.push_back({
                    boundary_time, step, solved.iterations, solved.residual_inf});
                result.maximum_residual_inf = std::max(
                    result.maximum_residual_inf, solved.residual_inf);
                time = boundary_time;
                ++boundary_index;
                const auto values = event_context(model, state, algebraic, time);
                for (std::size_t index = 0; index < model.events.size(); ++index) {
                    if (!event_guard_active(
                            Expression(model.events[index].guard).evaluate(values),
                            model.events[index].direction, tolerance.guard)) {
                        event_armed[index] = true;
                    }
                }
                continue;
            }
            const auto earliest = std::min_element(
                located.begin(), located.end(), [](const auto& left, const auto& right) {
                    return left.offset < right.offset;
                });
            const double event_offset = earliest->offset;
            const double event_time = time + event_offset;
            std::vector<double> root_state(
                earliest->solution.values.begin(),
                earliest->solution.values.begin() + model.states.size());
            std::vector<double> root_algebraic(
                earliest->solution.values.begin() + model.states.size(),
                earliest->solution.values.end());
            std::vector<std::size_t> pending;
            for (const auto& item : located) {
                if (std::abs(item.offset - event_offset) <= tolerance.root_time) {
                    const double guard = Expression(model.events[item.index].guard).evaluate(
                        event_context(model, root_state, root_algebraic, event_time));
                    result.maximum_guard_residual = std::max(
                        result.maximum_guard_residual, std::abs(guard));
                    pending.push_back(item.index);
                    event_armed[item.index] = false;
                }
            }
            ++result.event_root_solves;
            const auto transaction = execute_fully_implicit_event_transaction(
                model, event_time, root_state, root_algebraic, std::move(pending),
                initial_sparsity, tolerance);
            if (result.events.size() + transaction.records.size() > 10000) {
                throw std::runtime_error("fully implicit DAE event chattering limit exceeded");
            }
            result.steps.push_back({
                event_time, event_offset, earliest->solution.iterations,
                earliest->solution.residual_inf});
            result.maximum_residual_inf = std::max(
                result.maximum_residual_inf, earliest->solution.residual_inf);
            result.event_projection_solves += transaction.projection_solves;
            result.event_projection_krylov_iterations +=
                transaction.projection_krylov_iterations;
            result.dense_event_projection_fallbacks +=
                transaction.dense_projection_fallbacks;
            result.maximum_event_projection_residual_inf = std::max(
                result.maximum_event_projection_residual_inf,
                transaction.maximum_projection_residual_inf);
            result.events.insert(
                result.events.end(), transaction.records.begin(), transaction.records.end());
            state = transaction.state;
            algebraic = transaction.algebraic;
            time = event_time;
            const auto values = event_context(model, state, algebraic, time);
            for (std::size_t index = 0; index < model.events.size(); ++index) {
                event_armed[index] = !event_guard_active(
                    Expression(model.events[index].guard).evaluate(values),
                    model.events[index].direction, tolerance.guard);
            }
        }
        result.success = true;
        result.final_time = time;
        result.message =
            "fully implicit initialization, backward Euler/Newton, root, reset, and consistency gates completed";
    } catch (const std::exception& error) {
        result.final_time = time;
        result.message = error.what();
    }
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        result.final_state[model.states[index].name] = state[index];
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        result.final_algebraics[model.algebraics[index].name] = algebraic[index];
    }
    return result;
}

void write_fully_implicit_dae_report(
    const FullyImplicitDaeIR& model,
    const FullyImplicitDaeRunResult& result,
    const std::filesystem::path& path) {
    model.validate();
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write fully implicit DAE report: " + path.string());
    }
    output << "SMAVE_FULLY_IMPLICIT_DAE_REPORT 2\nMODEL " << std::quoted(model.model_id)
           << "\nSOURCE_HASH " << std::quoted(model.source_hash)
           << "\nSTRUCTURAL_CLASS " << std::quoted(model.structural_class)
           << "\nPLAN_ID " << std::quoted(result.plan_id)
           << "\nSOLVER_BACKEND " << std::quoted(result.solver_backend)
           << "\nTERMINAL_FALLBACK " << std::quoted(result.terminal_fallback)
           << "\nSUCCESS " << result.success
           << "\nFINAL_TIME " << std::setprecision(17) << result.final_time
           << "\nMAX_RESIDUAL " << result.maximum_residual_inf
           << "\nINITIALIZATION_ITERATIONS " << result.initialization_iterations
           << "\nINITIALIZATION_KRYLOV_ITERATIONS "
           << result.initialization_krylov_iterations
           << "\nINITIALIZATION_RESIDUAL " << result.initialization_residual_inf
           << "\nSPARSE_INITIALIZATION " << result.sparse_initialization
           << "\nINITIALIZATION_JACOBIAN_NONZEROS "
           << result.initialization_jacobian_nonzeros
           << "\nINITIALIZATION_JACOBIAN_BYTES "
           << result.initialization_jacobian_storage_bytes
           << "\nINITIALIZATION_JACOBIAN_COLORS "
           << result.initialization_jacobian_colors
           << "\nINITIALIZATION_JACOBIAN_EVALUATION_BATCHES "
           << result.initialization_jacobian_evaluation_batches
           << "\nINITIALIZATION_JACOBIAN_AD_BATCHES "
           << result.initialization_jacobian_ad_batches
           << "\nINITIALIZATION_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.initialization_jacobian_fd_fallback_batches
           << "\nINITIALIZATION_INNER_BACKEND "
           << std::quoted(result.initialization_inner_backend)
           << "\nDENSE_INITIALIZATION_FALLBACKS "
           << result.dense_initialization_fallbacks
           << "\nSTEPS " << result.steps.size()
           << "\nREJECTED_STEPS " << result.rejected_steps
           << "\nDENSE_STEP_FALLBACKS " << result.dense_step_fallbacks
           << "\nLEARNED_PRECONDITIONER_VERSION "
           << std::quoted(result.dae_preconditioner_version)
           << "\nLEARNED_PRECONDITIONED_STEPS "
           << result.learned_preconditioned_steps
           << "\nLEARNED_PRECONDITIONED_NEWTON_ITERATIONS "
           << result.learned_preconditioned_newton_iterations
           << "\nLEARNED_KRYLOV_ITERATIONS " << result.learned_krylov_iterations
           << "\nLEARNED_REJECTIONS " << result.learned_rejections
           << "\nINITIAL_EVENTS " << result.initial_events.size()
           << "\nEVENTS " << result.events.size()
           << "\nEVENT_ROOT_SOLVES " << result.event_root_solves
           << "\nEVENT_PROJECTION_SOLVES " << result.event_projection_solves
           << "\nEVENT_PROJECTION_KRYLOV_ITERATIONS "
           << result.event_projection_krylov_iterations
           << "\nDENSE_EVENT_PROJECTION_FALLBACKS "
           << result.dense_event_projection_fallbacks
           << "\nMAX_GUARD_RESIDUAL " << result.maximum_guard_residual
           << "\nMAX_EVENT_PROJECTION_RESIDUAL "
           << result.maximum_event_projection_residual_inf
           << "\nSPARSE_NEWTON_STEPS " << result.sparse_newton_steps
           << "\nSPARSE_NEWTON_ITERATIONS " << result.sparse_newton_iterations
           << "\nSPARSE_KRYLOV_ITERATIONS " << result.sparse_krylov_iterations
           << "\nSPARSE_JACOBIAN_NONZEROS " << result.sparse_jacobian_nonzeros
           << "\nSPARSE_JACOBIAN_BYTES " << result.sparse_jacobian_storage_bytes
           << "\nSPARSE_JACOBIAN_COLORS " << result.sparse_jacobian_colors
           << "\nSPARSE_JACOBIAN_EVALUATION_BATCHES "
           << result.sparse_jacobian_evaluation_batches
           << "\nSPARSE_JACOBIAN_AD_BATCHES " << result.sparse_jacobian_ad_batches
           << "\nSPARSE_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.sparse_jacobian_fd_fallback_batches
           << "\nSPARSE_INNER_BACKEND " << std::quoted(result.sparse_inner_backend) << '\n';
    for (std::size_t index = 0; index < result.backend_chain.size(); ++index) {
        output << "BACKEND_CHAIN " << index << ' '
               << std::quoted(result.backend_chain[index]) << '\n';
    }
    for (const auto& event : result.initial_events) {
        output << "INITIAL_EVENT " << std::quoted(event.id) << ' '
               << std::setprecision(17) << event.time << '\n';
    }
    for (const auto& event : result.events) {
        output << "EVENT " << std::quoted(event.id) << ' '
               << std::setprecision(17) << event.time << ' '
               << event.post_state.size();
        std::vector<std::pair<std::string, double>> states(
            event.post_state.begin(), event.post_state.end());
        std::sort(states.begin(), states.end());
        for (const auto& [name, value] : states) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        std::vector<std::pair<std::string, double>> algebraics(
            event.post_algebraics.begin(), event.post_algebraics.end());
        std::sort(algebraics.begin(), algebraics.end());
        output << ' ' << algebraics.size();
        for (const auto& [name, value] : algebraics) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        output << '\n';
    }
    for (const auto& item : model.states) {
        output << "INITIAL_DERIVATIVE " << std::quoted(item.name) << ' '
               << result.initial_derivatives.at(item.name) << '\n';
        output << "STATE " << std::quoted(item.name) << ' '
               << result.final_state.at(item.name) << '\n';
    }
    for (const auto& item : model.algebraics) {
        output << "ALGEBRAIC " << std::quoted(item.name) << ' '
               << result.final_algebraics.at(item.name) << '\n';
    }
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
