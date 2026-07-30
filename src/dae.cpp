#include "smave/dae.hpp"

#include "smave/dae_learning.hpp"
#include "smave/expression.hpp"
#include "smave/linear.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace smave {
namespace {
std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read DAE source: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return first >= last ? std::string{} : std::string(first, last);
}
std::string strip_comments(std::string value) {
    value = std::regex_replace(value, std::regex(R"(/\*[\s\S]*?\*/)") , "");
    return std::regex_replace(value, std::regex(R"(//[^\n]*)"), "");
}
std::vector<std::string> split(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ';')) if (!trim(item).empty()) result.push_back(trim(item));
    return result;
}
std::map<std::string, std::string> attrs(std::string value) {
    std::map<std::string, std::string> result;
    value = trim(std::move(value));
    if (value.empty()) return result;
    if (value.front() != '(' || value.back() != ')') throw std::invalid_argument("invalid DAE attributes");
    value = value.substr(1, value.size() - 2);
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto equal = item.find('=');
        if (equal == std::string::npos) throw std::invalid_argument("invalid DAE attribute: " + item);
        result[trim(item.substr(0, equal))] = trim(item.substr(equal + 1));
    }
    return result;
}
double attr(const std::map<std::string, std::string>& values, const std::string& name, double fallback) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? fallback : std::stod(iterator->second);
}
std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : input) { hash ^= c; hash *= 1099511628211ULL; }
    std::ostringstream output; output << std::hex << std::setfill('0') << std::setw(16) << hash; return output.str();
}
void require_tag(std::istream& input, const std::string& expected) {
    std::string actual; input >> actual;
    if (!input || actual != expected) throw std::runtime_error("invalid index-1 DAE IR: expected " + expected);
}
std::vector<std::string> names(const std::string& expression) {
    const Expression parsed(expression);
    return {parsed.names().begin(), parsed.names().end()};
}
std::string replace_pre(std::string expression) {
    const std::regex pre_call(R"(\bpre\s*\(\s*([A-Za-z_]\w*)\s*\))");
    expression = std::regex_replace(expression, pre_call, "__smave_pre_$1");
    if (std::regex_search(expression, std::regex(R"(\bpre\s*\()"))) {
        throw std::invalid_argument("DAE reset supports only pre(state)");
    }
    return expression;
}
bool augment_matching(
    std::size_t equation,
    const std::vector<std::vector<std::size_t>>& incidence,
    std::vector<int>& variable_match,
    std::vector<bool>& visited) {
    for (const auto variable : incidence[equation]) {
        if (visited[variable]) continue;
        visited[variable] = true;
        if (variable_match[variable] < 0 || augment_matching(
                static_cast<std::size_t>(variable_match[variable]),
                incidence, variable_match, visited)) {
            variable_match[variable] = static_cast<int>(equation);
            return true;
        }
    }
    return false;
}
std::unordered_map<std::string, double> context(
    const IndexOneDaeIR& model, const std::vector<double>& state,
    const std::vector<double>& algebraic, double time,
    const std::vector<double>* pre_state = nullptr) {
    std::unordered_map<std::string, double> values(model.parameters.begin(), model.parameters.end());
    values["time"] = time;
    for (std::size_t i = 0; i < state.size(); ++i) {
        values[model.states[i].name] = state[i];
        if (pre_state != nullptr) {
            values["__smave_pre_" + model.states[i].name] = (*pre_state)[i];
        }
    }
    for (std::size_t i = 0; i < algebraic.size(); ++i) values[model.algebraics[i].name] = algebraic[i];
    return values;
}
bool guard_active(double value, int direction, double tolerance) {
    return direction > 0 ? value >= -tolerance : value <= tolerance;
}
bool guard_crossing(double left, double right, int direction, double tolerance) {
    return direction > 0
        ? left < -tolerance && right >= -tolerance
        : left > tolerance && right <= tolerance;
}
std::unordered_map<std::string, double> variable_map(
    const std::vector<DaeVariableIR>& variables,
    const std::vector<double>& values) {
    std::unordered_map<std::string, double> result;
    for (std::size_t index = 0; index < variables.size(); ++index) {
        result[variables[index].name] = values[index];
    }
    return result;
}
std::vector<double> residual(
    const IndexOneDaeIR& model, const std::vector<double>& previous,
    const std::vector<double>& candidate, double time, double step) {
    std::vector<double> state(candidate.begin(), candidate.begin() + model.states.size());
    std::vector<double> algebraic(candidate.begin() + model.states.size(), candidate.end());
    const auto values = context(model, state, algebraic, time);
    std::vector<double> result;
    result.reserve(candidate.size());
    for (std::size_t i = 0; i < model.states.size(); ++i) {
        result.push_back(state[i] - previous[i] - step * Expression(model.derivatives[i]).evaluate(values));
    }
    for (const auto& equation : model.constraints) result.push_back(Expression(equation.residual).evaluate(values));
    return result;
}

std::vector<double> equation_residuals(
    const std::vector<DaeEquationIR>& equations,
    const std::unordered_map<std::string, double>& values) {
    std::vector<double> result;
    result.reserve(equations.size());
    for (const auto& equation : equations) {
        result.push_back(Expression(equation.residual).evaluate(values));
    }
    return result;
}
double inf_norm(const std::vector<double>& values) {
    double result = 0.0; for (const double value : values) result = std::max(result, std::abs(value)); return result;
}
bool solve_dense(std::vector<std::vector<double>> matrix, std::vector<double> rhs, std::vector<double>& result) {
    const auto size = rhs.size();
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        if (std::abs(matrix[pivot][column]) < 1.0e-14) return false;
        std::swap(matrix[pivot], matrix[column]); std::swap(rhs[pivot], rhs[column]);
        for (std::size_t row = column + 1; row < size; ++row) {
            const double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t item = column; item < size; ++item) matrix[row][item] -= factor * matrix[column][item];
            rhs[row] -= factor * rhs[column];
        }
    }
    result.assign(size, 0.0);
    for (std::size_t reverse = 0; reverse < size; ++reverse) {
        const auto row = size - reverse - 1; double value = rhs[row];
        for (std::size_t column = row + 1; column < size; ++column) value -= matrix[row][column] * result[column];
        result[row] = value / matrix[row][row];
    }
    return std::all_of(result.begin(), result.end(), [](double value) { return std::isfinite(value); });
}

double sparse_algebraic_rank_margin(
    const IndexOneDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time);

double algebraic_rank_margin(
    const IndexOneDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time) {
    if (model.algebraics.empty()) return 1.0;
    if (model.algebraics.size() > 1024) {
        return sparse_algebraic_rank_margin(model, state, algebraic, time);
    }
    std::vector<std::vector<double>> jacobian(
        model.constraints.size(), std::vector<double>(model.algebraics.size()));
    double scale = 1.0;
    for (std::size_t column = 0; column < model.algebraics.size(); ++column) {
        auto lower = algebraic;
        auto upper = algebraic;
        const double variable_scale = std::max({
            1.0, model.algebraics[column].nominal, std::abs(algebraic[column])});
        const double perturbation = 1.0e-6 * variable_scale;
        lower[column] -= perturbation;
        upper[column] += perturbation;
        const auto lower_residual = equation_residuals(
            model.constraints, context(model, state, lower, time));
        const auto upper_residual = equation_residuals(
            model.constraints, context(model, state, upper, time));
        for (std::size_t row = 0; row < model.constraints.size(); ++row) {
            jacobian[row][column] =
                (upper_residual[row] - lower_residual[row]) /
                (2.0 * perturbation) * model.algebraics[column].nominal;
            scale = std::max(scale, std::abs(jacobian[row][column]));
        }
    }
    double minimum_pivot = scale;
    for (std::size_t column = 0; column < jacobian.size(); ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < jacobian.size(); ++row) {
            if (std::abs(jacobian[row][column]) >
                std::abs(jacobian[pivot][column])) {
                pivot = row;
            }
        }
        const double pivot_value = std::abs(jacobian[pivot][column]);
        minimum_pivot = std::min(minimum_pivot, pivot_value);
        if (pivot_value == 0.0) return 0.0;
        std::swap(jacobian[pivot], jacobian[column]);
        for (std::size_t row = column + 1; row < jacobian.size(); ++row) {
            const double factor = jacobian[row][column] / jacobian[column][column];
            for (std::size_t item = column; item < jacobian.size(); ++item) {
                jacobian[row][item] -= factor * jacobian[column][item];
            }
        }
    }
    return minimum_pivot / scale;
}

void enforce_algebraic_rank(
    const IndexOneDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time,
    const DaeTolerance& tolerance,
    std::size_t& checks,
    double& minimum_margin) {
    const double margin = algebraic_rank_margin(model, state, algebraic, time);
    ++checks;
    minimum_margin = std::min(minimum_margin, margin);
    if (!std::isfinite(margin) || margin <= tolerance.algebraic_rank) {
        throw std::runtime_error(
            "DAE algebraic Jacobian numerical rank gate failed at t=" +
            std::to_string(time));
    }
}

struct NonlinearResult {
    bool converged{false};
    std::vector<double> values;
    int iterations{0};
    double residual_inf{0.0};
    bool learned_attempted{false};
    bool learned_rejected{false};
    int krylov_iterations{0};
    bool sparse_newton{false};
    std::size_t jacobian_nonzeros{0};
    std::size_t jacobian_storage_bytes{0};
    std::size_t jacobian_colors{0};
    std::size_t jacobian_evaluation_batches{0};
    std::size_t jacobian_ad_batches{0};
    std::size_t jacobian_fd_fallback_batches{0};
    std::string inner_backend;
};

SparsityPattern dae_step_sparsity(const IndexOneDaeIR& model) {
    const auto state_count = model.states.size();
    const auto unknown_count = state_count + model.algebraics.size();
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < state_count; ++index) {
        positions.emplace(model.states[index].name, index);
    }
    for (std::size_t index = 0; index < model.algebraics.size(); ++index) {
        positions.emplace(model.algebraics[index].name, state_count + index);
    }
    std::vector<std::vector<std::size_t>> rows(unknown_count);
    for (std::size_t row = 0; row < state_count; ++row) {
        rows[row].push_back(row);
        for (const auto& name : names(model.derivatives[row])) {
            const auto position = positions.find(name);
            if (position != positions.end()) rows[row].push_back(position->second);
        }
    }
    for (std::size_t index = 0; index < model.constraints.size(); ++index) {
        for (const auto& name : model.constraints[index].variables) {
            const auto position = positions.find(name);
            if (position != positions.end()) rows[state_count + index].push_back(position->second);
        }
    }
    return SparsityPattern::from_rows(unknown_count, rows);
}

SparsityPattern equation_sparsity(
    const std::vector<DaeEquationIR>& equations,
    const std::vector<std::string>& unknowns) {
    std::unordered_map<std::string, std::size_t> positions;
    for (std::size_t index = 0; index < unknowns.size(); ++index) {
        positions.emplace(unknowns[index], index);
    }
    std::vector<std::vector<std::size_t>> rows(equations.size());
    for (std::size_t row = 0; row < equations.size(); ++row) {
        for (const auto& name : equations[row].variables) {
            const auto position = positions.find(name);
            if (position != positions.end()) rows[row].push_back(position->second);
        }
    }
    return SparsityPattern::from_rows(unknowns.size(), rows);
}

class EquationSystemEvaluator {
public:
    EquationSystemEvaluator(
        std::vector<std::string> unknowns,
        const std::vector<DaeEquationIR>& equations,
        std::unordered_map<std::string, double> fixed_values)
        : unknowns_(std::move(unknowns)), values_(std::move(fixed_values)) {
        expressions_.reserve(equations.size());
        for (const auto& equation : equations) {
            expressions_.emplace_back(equation.residual);
        }
    }

    std::vector<double> evaluate(const std::vector<double>& candidate) {
        update(candidate);
        std::vector<double> result;
        result.reserve(expressions_.size());
        for (const auto& expression : expressions_) {
            result.push_back(expression.evaluate(values_));
        }
        return result;
    }

    double evaluate_row(std::size_t row, const std::vector<double>& candidate) {
        update(candidate);
        return expressions_.at(row).evaluate(values_);
    }

    std::optional<std::vector<double>> directional_derivative(
        const std::vector<double>& candidate,
        const std::vector<double>& direction) {
        update(candidate);
        if (direction.size() != unknowns_.size()) return std::nullopt;
        std::unordered_map<std::string, double> directions;
        for (std::size_t index = 0; index < direction.size(); ++index) {
            if (direction[index] != 0.0) directions[unknowns_[index]] = direction[index];
        }
        std::vector<double> result;
        result.reserve(expressions_.size());
        for (const auto& expression : expressions_) {
            const auto derivative = expression.directional_derivative(values_, directions);
            if (!derivative.has_value()) return std::nullopt;
            result.push_back(*derivative);
        }
        return result;
    }

private:
    void update(const std::vector<double>& candidate) {
        if (candidate.size() != unknowns_.size()) {
            throw std::logic_error("DAE equation evaluator candidate size mismatch");
        }
        for (std::size_t index = 0; index < candidate.size(); ++index) {
            values_[unknowns_[index]] = candidate[index];
        }
    }

    std::vector<std::string> unknowns_;
    std::unordered_map<std::string, double> values_;
    std::vector<Expression> expressions_;
};

double sparse_algebraic_rank_margin(
    const IndexOneDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& algebraic,
    double time) {
    std::vector<std::string> unknowns;
    unknowns.reserve(model.algebraics.size());
    for (const auto& variable : model.algebraics) {
        unknowns.push_back(variable.name);
    }
    const auto sparsity = equation_sparsity(model.constraints, unknowns);
    EquationSystemEvaluator evaluator(
        unknowns, model.constraints, context(model, state, algebraic, time));
    const auto values = evaluator.evaluate(algebraic);
    LinearSystem system;
    system.sparsity = sparsity;
    system.sparse_values.resize(sparsity.nonzeros());
    system.right_hand_side.assign(algebraic.size(), 0.0);
    const auto colors = sparsity.greedy_column_coloring();
    const auto color_count = colors.empty()
        ? 0
        : *std::max_element(colors.begin(), colors.end()) + 1;
    std::vector<double> perturbations(algebraic.size());
    for (std::size_t column = 0; column < algebraic.size(); ++column) {
        const double variable_scale = std::max({
            1.0, model.algebraics[column].nominal,
            std::abs(algebraic[column])});
        perturbations[column] = 1.0e-6 * variable_scale;
    }
    for (std::size_t color = 0; color < color_count; ++color) {
        std::vector<double> direction(algebraic.size());
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) direction[column] = 1.0;
        }
        const auto derivatives = evaluator.directional_derivative(
            algebraic, direction);
        if (derivatives.has_value()) {
            for (std::size_t row = 0; row < sparsity.row_count; ++row) {
                for (std::size_t offset = sparsity.row_offsets[row];
                     offset < sparsity.row_offsets[row + 1]; ++offset) {
                    const auto column = sparsity.column_indices[offset];
                    if (colors[column] == color) {
                        system.sparse_values[offset] = (*derivatives)[row] *
                            model.algebraics[column].nominal;
                        break;
                    }
                }
            }
            continue;
        }
        auto perturbed = algebraic;
        for (std::size_t column = 0; column < colors.size(); ++column) {
            if (colors[column] == color) perturbed[column] += perturbations[column];
        }
        const auto shifted = evaluator.evaluate(perturbed);
        for (std::size_t row = 0; row < sparsity.row_count; ++row) {
            for (std::size_t offset = sparsity.row_offsets[row];
                 offset < sparsity.row_offsets[row + 1]; ++offset) {
                const auto column = sparsity.column_indices[offset];
                if (colors[column] == color) {
                    system.sparse_values[offset] =
                        (shifted[row] - values[row]) / perturbations[column] *
                        model.algebraics[column].nominal;
                    break;
                }
            }
        }
    }
    const auto factorization = sparse_ordered_threshold_pivot_solve(system);
    return factorization.solved ? factorization.minimum_scaled_pivot : 0.0;
}

class DaeStepEvaluator {
public:
    DaeStepEvaluator(
        const IndexOneDaeIR& model,
        const std::vector<double>& previous,
        double time,
        double step)
        : model_(model), previous_(previous), step_(step) {
        for (const auto& [name, value] : model.parameters) values_[name] = value;
        values_["time"] = time;
        for (const auto& expression : model.derivatives) {
            derivatives_.emplace_back(expression);
        }
        for (const auto& equation : model.constraints) {
            constraints_.emplace_back(equation.residual);
        }
    }

    std::vector<double> evaluate(const std::vector<double>& candidate) {
        update(candidate);
        std::vector<double> result(candidate.size());
        for (std::size_t row = 0; row < model_.states.size(); ++row) {
            result[row] = candidate[row] - previous_[row] -
                step_ * derivatives_[row].evaluate(values_);
        }
        for (std::size_t index = 0; index < constraints_.size(); ++index) {
            result[model_.states.size() + index] = constraints_[index].evaluate(values_);
        }
        return result;
    }

    double evaluate_row(std::size_t row, const std::vector<double>& candidate) {
        update(candidate);
        if (row < model_.states.size()) {
            return candidate[row] - previous_[row] -
                step_ * derivatives_[row].evaluate(values_);
        }
        return constraints_[row - model_.states.size()].evaluate(values_);
    }

    std::optional<std::vector<double>> directional_derivative(
        const std::vector<double>& candidate,
        const std::vector<double>& direction) {
        update(candidate);
        if (direction.size() != candidate.size()) return std::nullopt;
        std::unordered_map<std::string, double> directions;
        for (std::size_t index = 0; index < model_.states.size(); ++index) {
            if (direction[index] != 0.0) {
                directions[model_.states[index].name] = direction[index];
            }
        }
        for (std::size_t index = 0; index < model_.algebraics.size(); ++index) {
            const auto position = model_.states.size() + index;
            if (direction[position] != 0.0) {
                directions[model_.algebraics[index].name] = direction[position];
            }
        }
        std::vector<double> result(candidate.size());
        for (std::size_t row = 0; row < model_.states.size(); ++row) {
            const auto derivative = derivatives_[row].directional_derivative(
                values_, directions);
            if (!derivative.has_value()) return std::nullopt;
            result[row] = direction[row] - step_ * *derivative;
        }
        for (std::size_t index = 0; index < constraints_.size(); ++index) {
            const auto derivative = constraints_[index].directional_derivative(
                values_, directions);
            if (!derivative.has_value()) return std::nullopt;
            result[model_.states.size() + index] = *derivative;
        }
        return result;
    }

private:
    void update(const std::vector<double>& candidate) {
        for (std::size_t index = 0; index < model_.states.size(); ++index) {
            values_[model_.states[index].name] = candidate[index];
        }
        for (std::size_t index = 0; index < model_.algebraics.size(); ++index) {
            values_[model_.algebraics[index].name] =
                candidate[model_.states.size() + index];
        }
    }

    const IndexOneDaeIR& model_;
    const std::vector<double>& previous_;
    double step_{};
    std::unordered_map<std::string, double> values_;
    std::vector<Expression> derivatives_;
    std::vector<Expression> constraints_;
};

std::vector<std::vector<double>> finite_difference_jacobian(
    const std::vector<double>& candidate,
    const std::function<std::vector<double>(const std::vector<double>&)>& residual_function) {
    const auto values = residual_function(candidate);
    std::vector<std::vector<double>> jacobian(
        values.size(), std::vector<double>(candidate.size()));
    for (std::size_t column = 0; column < candidate.size(); ++column) {
        auto perturbed = candidate;
        const double perturbation = 1.0e-7 *
            std::max(1.0, std::abs(candidate[column]));
        perturbed[column] += perturbation;
        const auto shifted = residual_function(perturbed);
        if (shifted.size() != values.size()) {
            throw std::logic_error("DAE nonlinear system changed dimension");
        }
        for (std::size_t row = 0; row < values.size(); ++row) {
            jacobian[row][column] = (shifted[row] - values[row]) / perturbation;
        }
    }
    return jacobian;
}

bool symmetrize_spd(std::vector<std::vector<double>>& matrix) {
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        if (matrix[row].size() != matrix.size()) return false;
        for (std::size_t column = row + 1; column < matrix.size(); ++column) {
            const double scale = std::max({
                1.0, std::abs(matrix[row][column]), std::abs(matrix[column][row])});
            if (std::abs(matrix[row][column] - matrix[column][row]) > 1.0e-7 * scale) {
                return false;
            }
            const double value = 0.5 * (matrix[row][column] + matrix[column][row]);
            matrix[row][column] = value;
            matrix[column][row] = value;
        }
    }
    std::vector<std::vector<double>> factor(
        matrix.size(), std::vector<double>(matrix.size()));
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double value = matrix[row][column];
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= factor[row][inner] * factor[column][inner];
            }
            if (row == column) {
                if (!(value > 1.0e-12) || !std::isfinite(value)) return false;
                factor[row][column] = std::sqrt(value);
            } else {
                factor[row][column] = value / factor[column][column];
            }
        }
    }
    return true;
}
NonlinearResult damped_newton(
    std::vector<double> candidate,
    const std::function<std::vector<double>(const std::vector<double>&)>& residual_function,
    const DaeTolerance& tolerance) {
    NonlinearResult result;
    result.values = std::move(candidate);
    for (int iteration = 0; iteration < tolerance.maximum_newton_iterations; ++iteration) {
        const auto values = residual_function(result.values);
        if (values.size() != result.values.size()) {
            throw std::logic_error("DAE nonlinear system is not square");
        }
        const double norm = inf_norm(values);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, inf_norm(result.values));
        result.iterations = iteration;
        result.residual_inf = norm;
        if (norm <= threshold) {
            result.converged = true;
            return result;
        }
        const auto jacobian = finite_difference_jacobian(
            result.values, residual_function);
        std::vector<double> right(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            right[index] = -values[index];
        }
        std::vector<double> delta;
        if (!solve_dense(jacobian, right, delta)) return result;
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = result.values;
            for (std::size_t index = 0; index < trial.size(); ++index) {
                trial[index] += damping * delta[index];
            }
            const double trial_norm = inf_norm(residual_function(trial));
            if (std::isfinite(trial_norm) && trial_norm < norm) {
                result.values = std::move(trial);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) return result;
    }
    result.residual_inf = inf_norm(residual_function(result.values));
    result.iterations = tolerance.maximum_newton_iterations;
    return result;
}

NonlinearResult sparse_damped_newton(
    std::vector<double> candidate,
    const std::function<std::vector<double>(const std::vector<double>&)>&
        residual_function,
    const std::function<std::optional<std::vector<double>>(
        const std::vector<double>&, const std::vector<double>&)>&
        directional_residual_function,
    const SparsityPattern& sparsity,
    const DaeTolerance& tolerance) {
    NonlinearResult result;
    result.values = std::move(candidate);
    result.sparse_newton = true;
    const auto colors = sparsity.greedy_column_coloring();
    result.jacobian_colors = colors.empty()
        ? 0
        : *std::max_element(colors.begin(), colors.end()) + 1;
    for (int iteration = 0; iteration < tolerance.maximum_newton_iterations; ++iteration) {
        const auto values = residual_function(result.values);
        if (values.size() != result.values.size()) {
            throw std::logic_error("DAE sparse nonlinear system is not square");
        }
        const double norm = inf_norm(values);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, inf_norm(result.values));
        result.iterations = iteration;
        result.residual_inf = norm;
        if (norm <= threshold) {
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
        std::vector<double> perturbations(result.values.size());
        for (std::size_t column = 0; column < result.values.size(); ++column) {
            perturbations[column] = 1.0e-7 *
                std::max(1.0, std::abs(result.values[column]));
        }
        for (std::size_t color = 0; color < result.jacobian_colors; ++color) {
            std::vector<double> direction(result.values.size());
            for (std::size_t column = 0; column < colors.size(); ++column) {
                if (colors[column] == color) direction[column] = 1.0;
            }
            const auto derivatives = directional_residual_function
                ? directional_residual_function(result.values, direction)
                : std::nullopt;
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
            auto perturbed = result.values;
            for (std::size_t column = 0; column < colors.size(); ++column) {
                if (colors[column] == color) {
                    perturbed[column] += perturbations[column];
                }
            }
            const auto shifted = residual_function(perturbed);
            ++result.jacobian_evaluation_batches;
            ++result.jacobian_fd_fallback_batches;
            for (std::size_t row = 0; row < sparsity.row_count; ++row) {
                for (std::size_t offset = sparsity.row_offsets[row];
                     offset < sparsity.row_offsets[row + 1]; ++offset) {
                    const auto column = sparsity.column_indices[offset];
                    if (colors[column] == color) {
                        system.sparse_values[offset] =
                            (shifted[row] - values[row]) / perturbations[column];
                        break;
                    }
                }
            }
        }
        classify_linear_system(system, 1.0e-7);
        result.jacobian_nonzeros = system.nonzeros();
        result.jacobian_storage_bytes = system.sparse_storage_bytes();
        KrylovResult krylov;
        if (system.positive_definite) {
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
        std::vector<double> delta;
        if (krylov.converged) {
            delta = std::move(krylov.solution);
        } else {
            const auto direct = sparse_ordered_threshold_pivot_solve(system);
            if (!direct.solved) return result;
            result.inner_backend = "sparse-ordered-threshold-pivot-cpu-v2";
            delta = direct.solution;
        }
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = result.values;
            for (std::size_t index = 0; index < trial.size(); ++index) {
                trial[index] += damping * delta[index];
            }
            const double trial_norm = inf_norm(residual_function(trial));
            if (std::isfinite(trial_norm) && trial_norm < norm) {
                result.values = std::move(trial);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) return result;
    }
    result.residual_inf = inf_norm(residual_function(result.values));
    result.iterations = tolerance.maximum_newton_iterations;
    return result;
}

NonlinearResult learned_damped_newton(
    std::vector<double> candidate,
    const std::function<std::vector<double>(const std::vector<double>&)>& residual_function,
    const DaeTolerance& tolerance,
    const DaeMultigridArtifact& artifact,
    double step) {
    NonlinearResult result;
    result.values = std::move(candidate);
    result.learned_attempted = true;
    if (artifact.model_source_hash.empty() ||
        result.values.size() != artifact.unknown_count ||
        artifact.model_source_hash != artifact.hierarchy.model_source_hash ||
        step < artifact.minimum_step - 1.0e-14 ||
        step > artifact.maximum_step + 1.0e-14) {
        result.learned_rejected = true;
        return result;
    }
    for (int iteration = 0; iteration < tolerance.maximum_newton_iterations; ++iteration) {
        const auto values = residual_function(result.values);
        if (values.size() != result.values.size()) {
            throw std::logic_error("DAE nonlinear system is not square");
        }
        const double norm = inf_norm(values);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, inf_norm(result.values));
        result.iterations = iteration;
        result.residual_inf = norm;
        if (norm <= threshold) {
            result.converged = true;
            return result;
        }
        auto jacobian = finite_difference_jacobian(result.values, residual_function);
        if (!symmetrize_spd(jacobian)) {
            result.learned_rejected = true;
            return result;
        }
        LinearSystem system;
        system.matrix = std::move(jacobian);
        system.right_hand_side.resize(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            system.right_hand_side[index] = -values[index];
        }
        system.symmetric = true;
        system.positive_definite = true;
        const auto krylov = preconditioned_conjugate_gradient(
            system,
            std::vector<double>(values.size()),
            [&](const std::vector<double>& residual_values, std::vector<double>& applied) {
                return apply_learned_multigrid(
                    artifact.hierarchy, residual_values, applied);
            },
            std::max(1.0e-12, tolerance.absolute * 0.1),
            std::max(1.0e-10, tolerance.relative * 0.1),
            std::max(32, static_cast<int>(values.size() * 8)));
        result.krylov_iterations += krylov.iterations;
        if (!krylov.converged || krylov.breakdown || krylov.stagnated ||
            krylov.solution.size() != result.values.size()) {
            result.learned_rejected = true;
            return result;
        }
        double damping = 1.0;
        bool accepted = false;
        for (int attempt = 0; attempt < 12; ++attempt) {
            auto trial = result.values;
            for (std::size_t index = 0; index < trial.size(); ++index) {
                trial[index] += damping * krylov.solution[index];
            }
            const double trial_norm = inf_norm(residual_function(trial));
            if (std::isfinite(trial_norm) && trial_norm < norm) {
                result.values = std::move(trial);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) {
            result.learned_rejected = true;
            return result;
        }
    }
    result.residual_inf = inf_norm(residual_function(result.values));
    result.iterations = tolerance.maximum_newton_iterations;
    result.learned_rejected = true;
    return result;
}

NonlinearResult solve_dae_step(
    const IndexOneDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& initial_state,
    const std::vector<double>& initial_algebraic,
    double target_time,
    double step,
    const DaeTolerance& tolerance) {
    std::vector<double> candidate = initial_state;
    candidate.insert(candidate.end(), initial_algebraic.begin(), initial_algebraic.end());
    if (candidate.size() > 1024) {
        DaeStepEvaluator evaluator(model, previous_state, target_time, step);
        return sparse_damped_newton(
            std::move(candidate),
            [&](const std::vector<double>& values) {
                return evaluator.evaluate(values);
            },
            [&](const std::vector<double>& values,
                const std::vector<double>& direction) {
                return evaluator.directional_derivative(values, direction);
            },
            dae_step_sparsity(model), tolerance);
    }
    return damped_newton(
        std::move(candidate),
        [&](const std::vector<double>& values) {
            return residual(model, previous_state, values, target_time, step);
        },
        tolerance);
}

NonlinearResult solve_dae_candidate_step(
    const IndexOneDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& initial_state,
    const std::vector<double>& initial_algebraic,
    double target_time,
    double step,
    const DaeTolerance& tolerance,
    const DaeMultigridArtifact* artifact) {
    if (artifact == nullptr) {
        return solve_dae_step(
            model, previous_state, initial_state, initial_algebraic,
            target_time, step, tolerance);
    }
    try {
        artifact->validate();
    } catch (const std::exception&) {
        auto dense = solve_dae_step(
            model, previous_state, initial_state, initial_algebraic,
            target_time, step, tolerance);
        dense.learned_attempted = true;
        dense.learned_rejected = true;
        return dense;
    }
    if (artifact->model_source_hash != model.source_hash) {
        auto dense = solve_dae_step(
            model, previous_state, initial_state, initial_algebraic,
            target_time, step, tolerance);
        dense.learned_attempted = true;
        dense.learned_rejected = true;
        return dense;
    }
    std::vector<double> candidate = initial_state;
    candidate.insert(candidate.end(), initial_algebraic.begin(), initial_algebraic.end());
    const auto residual_function = [&](const std::vector<double>& values) {
        return residual(model, previous_state, values, target_time, step);
    };
    auto accelerated = learned_damped_newton(
        candidate, residual_function, tolerance, *artifact, step);
    if (accelerated.converged && accelerated.krylov_iterations > 0) return accelerated;
    NonlinearResult dense;
    if (candidate.size() > 1024) {
        DaeStepEvaluator evaluator(model, previous_state, target_time, step);
        dense = sparse_damped_newton(
            std::move(candidate),
            [&](const std::vector<double>& values) {
                return evaluator.evaluate(values);
            },
            [&](const std::vector<double>& values,
                const std::vector<double>& direction) {
                return evaluator.directional_derivative(values, direction);
            },
            dae_step_sparsity(model), tolerance);
    } else {
        dense = damped_newton(std::move(candidate), residual_function, tolerance);
    }
    dense.learned_attempted = true;
    dense.learned_rejected = true;
    dense.krylov_iterations = accelerated.krylov_iterations;
    return dense;
}

struct InitialEventTransaction {
    std::vector<double> state;
    std::vector<double> algebraic;
    std::vector<DaeEventRecord> records;
    int projection_iterations{0};
    double projection_residual_inf{0.0};
    std::size_t algebraic_rank_checks{0};
    double minimum_algebraic_rank_margin{1.0};
    std::size_t sparse_projections{0};
    std::size_t sparse_projection_iterations{0};
    std::size_t sparse_projection_krylov_iterations{0};
    std::size_t sparse_projection_jacobian_nonzeros{0};
    std::size_t sparse_projection_jacobian_storage_bytes{0};
    std::size_t sparse_projection_jacobian_colors{0};
    std::size_t sparse_projection_jacobian_evaluation_batches{0};
    std::size_t sparse_projection_jacobian_ad_batches{0};
    std::size_t sparse_projection_jacobian_fd_fallback_batches{0};
    std::string sparse_projection_inner_backend;
};

InitialEventTransaction execute_dae_event_transaction(
    const IndexOneDaeIR& model,
    double event_time,
    const std::vector<double>& event_state,
    const std::vector<double>& event_algebraic,
    std::vector<std::size_t> pending,
    const DaeTolerance& tolerance) {
    InitialEventTransaction transaction;
    transaction.state = event_state;
    transaction.algebraic = event_algebraic;
    const auto event_pre_state = event_state;
    std::vector<Expression> guards;
    std::vector<std::vector<Expression>> resets;
    for (const auto& event : model.events) {
        guards.emplace_back(event.guard);
        std::vector<Expression> event_resets;
        for (const auto& reset : event.resets) {
            event_resets.emplace_back(reset.expression);
        }
        resets.push_back(std::move(event_resets));
    }
    std::vector<bool> processed(model.events.size(), false);
    std::size_t iterations{};
    while (!pending.empty()) {
        if (++iterations > model.events.size()) {
            throw std::runtime_error("DAE initial event iteration did not reach a fixed point");
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
        const auto iteration_context = context(
            model, iteration_pre_state, iteration_pre_algebraic,
            event_time, &event_pre_state);
        std::vector<double> guards_before;
        for (const auto& guard : guards) {
            guards_before.push_back(guard.evaluate(iteration_context));
        }
        auto post_state = iteration_pre_state;
        std::unordered_set<std::string> reset_variables;
        for (const auto selected : pending) {
            processed[selected] = true;
            const auto& event = model.events[selected];
            for (std::size_t reset_index = 0;
                 reset_index < event.resets.size(); ++reset_index) {
                const auto& reset = event.resets[reset_index];
                if (!reset_variables.insert(reset.variable).second) {
                    throw std::runtime_error(
                        "simultaneous DAE initial events have conflicting reset for " +
                        reset.variable);
                }
                const auto position = std::find_if(
                    model.states.begin(), model.states.end(),
                    [&](const DaeVariableIR& state) {
                        return state.name == reset.variable;
                    });
                const auto state_index = static_cast<std::size_t>(
                    std::distance(model.states.begin(), position));
                const double candidate = resets[selected][reset_index]
                    .evaluate(iteration_context);
                if (!std::isfinite(candidate)) {
                    throw std::runtime_error(event.id + ": DAE reset produced NaN/Inf");
                }
                post_state[state_index] = candidate;
            }
        }
        NonlinearResult projection;
        if (iteration_pre_algebraic.size() > 1024) {
            std::vector<std::string> unknowns;
            unknowns.reserve(model.algebraics.size());
            for (const auto& variable : model.algebraics) {
                unknowns.push_back(variable.name);
            }
            EquationSystemEvaluator evaluator(
                unknowns, model.constraints,
                context(model, post_state, iteration_pre_algebraic, event_time));
            projection = sparse_damped_newton(
                iteration_pre_algebraic,
                [&](const std::vector<double>& candidate) {
                    return evaluator.evaluate(candidate);
                },
                [&](const std::vector<double>& candidate,
                    const std::vector<double>& direction) {
                    return evaluator.directional_derivative(candidate, direction);
                },
                equation_sparsity(model.constraints, unknowns), tolerance);
        } else {
            projection = damped_newton(
                iteration_pre_algebraic,
                [&](const std::vector<double>& candidate) {
                    return equation_residuals(
                        model.constraints,
                        context(model, post_state, candidate, event_time));
                },
                tolerance);
        }
        transaction.projection_iterations += projection.iterations;
        transaction.projection_residual_inf = std::max(
            transaction.projection_residual_inf, projection.residual_inf);
        if (!projection.converged) {
            throw std::runtime_error("DAE initial-event algebraic projection failed");
        }
        if (projection.sparse_newton) {
            ++transaction.sparse_projections;
            transaction.sparse_projection_iterations += projection.iterations;
            transaction.sparse_projection_krylov_iterations +=
                projection.krylov_iterations;
            transaction.sparse_projection_jacobian_nonzeros = std::max(
                transaction.sparse_projection_jacobian_nonzeros,
                projection.jacobian_nonzeros);
            transaction.sparse_projection_jacobian_storage_bytes = std::max(
                transaction.sparse_projection_jacobian_storage_bytes,
                projection.jacobian_storage_bytes);
            transaction.sparse_projection_jacobian_colors = std::max(
                transaction.sparse_projection_jacobian_colors,
                projection.jacobian_colors);
            transaction.sparse_projection_jacobian_evaluation_batches +=
                projection.jacobian_evaluation_batches;
            transaction.sparse_projection_jacobian_ad_batches +=
                projection.jacobian_ad_batches;
            transaction.sparse_projection_jacobian_fd_fallback_batches +=
                projection.jacobian_fd_fallback_batches;
            transaction.sparse_projection_inner_backend = projection.inner_backend;
        }
        enforce_algebraic_rank(
            model, post_state, projection.values, event_time, tolerance,
            transaction.algebraic_rank_checks,
            transaction.minimum_algebraic_rank_margin);
        const auto pre_state_map = variable_map(model.states, iteration_pre_state);
        const auto post_state_map = variable_map(model.states, post_state);
        const auto pre_algebraic_map = variable_map(
            model.algebraics, iteration_pre_algebraic);
        const auto post_algebraic_map = variable_map(
            model.algebraics, projection.values);
        for (const auto selected : pending) {
            transaction.records.push_back({
                model.events[selected].id,
                event_time,
                false,
                pre_state_map, post_state_map,
                pre_algebraic_map, post_algebraic_map});
        }
        const auto post_context = context(
            model, post_state, projection.values, event_time, &event_pre_state);
        pending.clear();
        for (std::size_t index = 0; index < model.events.size(); ++index) {
            if (processed[index]) continue;
            const double after = guards[index].evaluate(post_context);
            if (!guard_active(
                    guards_before[index], model.events[index].direction,
                    tolerance.absolute) &&
                guard_active(
                    after, model.events[index].direction,
                    tolerance.absolute)) {
                pending.push_back(index);
            }
        }
        transaction.state = std::move(post_state);
        transaction.algebraic = projection.values;
    }
    return transaction;
}

InitialEventTransaction execute_initial_events(
    const IndexOneDaeIR& model,
    const std::vector<double>& initial_state,
    const std::vector<double>& initial_algebraic,
    const DaeTolerance& tolerance) {
    const auto values = context(model, initial_state, initial_algebraic, 0.0);
    std::vector<std::size_t> pending;
    for (std::size_t index = 0; index < model.events.size(); ++index) {
        const double guard = Expression(model.events[index].guard).evaluate(values);
        if (guard_active(
                guard, model.events[index].direction, tolerance.guard)) {
            pending.push_back(index);
        }
    }
    return execute_dae_event_transaction(
        model, 0.0, initial_state, initial_algebraic,
        std::move(pending), tolerance);
}

struct LocatedDaeEvent {
    std::size_t index{0};
    double offset{0.0};
    std::vector<double> state;
    std::vector<double> algebraic;
    int newton_iterations{0};
    double residual_inf{0.0};
    double guard{0.0};
    bool grazing{false};
};

struct EventRootTelemetry {
    std::size_t solves{0};
    std::size_t common_solves{0};
    std::size_t sparse_solves{0};
    std::size_t sparse_newton_iterations{0};
    std::size_t sparse_krylov_iterations{0};
    std::size_t sparse_jacobian_nonzeros{0};
    std::size_t sparse_jacobian_storage_bytes{0};
    std::size_t sparse_jacobian_colors{0};
    std::size_t sparse_jacobian_evaluation_batches{0};
    std::size_t sparse_jacobian_ad_batches{0};
    std::size_t sparse_jacobian_fd_fallback_batches{0};
    std::string sparse_inner_backend;

    void record(const NonlinearResult& solution, bool common = false) {
        ++solves;
        if (common) ++common_solves;
        if (!solution.sparse_newton) return;
        ++sparse_solves;
        sparse_newton_iterations += solution.iterations;
        sparse_krylov_iterations += solution.krylov_iterations;
        sparse_jacobian_nonzeros = std::max(
            sparse_jacobian_nonzeros, solution.jacobian_nonzeros);
        sparse_jacobian_storage_bytes = std::max(
            sparse_jacobian_storage_bytes, solution.jacobian_storage_bytes);
        sparse_jacobian_colors = std::max(
            sparse_jacobian_colors, solution.jacobian_colors);
        sparse_jacobian_evaluation_batches += solution.jacobian_evaluation_batches;
        sparse_jacobian_ad_batches += solution.jacobian_ad_batches;
        sparse_jacobian_fd_fallback_batches +=
            solution.jacobian_fd_fallback_batches;
        sparse_inner_backend = solution.inner_backend;
    }
};

LocatedDaeEvent locate_dae_event(
    const IndexOneDaeIR& model,
    std::size_t event_index,
    double start_time,
    const std::vector<double>& start_state,
    const std::vector<double>& start_algebraic,
    double step,
    const DaeTolerance& tolerance,
    EventRootTelemetry& telemetry) {
    double left = 0.0;
    double right = step;
    auto right_solution = solve_dae_step(
        model, start_state, start_state, start_algebraic,
        start_time + right, right, tolerance);
    telemetry.record(right_solution);
    if (!right_solution.converged) {
        throw std::runtime_error("DAE event localization endpoint Newton failed");
    }
    while (right - left > tolerance.root_time) {
        const double middle = 0.5 * (left + right);
        const auto middle_solution = solve_dae_step(
            model, start_state, start_state, start_algebraic,
            start_time + middle, middle, tolerance);
        telemetry.record(middle_solution);
        if (!middle_solution.converged) {
            throw std::runtime_error("DAE event localization Newton failed");
        }
        std::vector<double> middle_state(
            middle_solution.values.begin(),
            middle_solution.values.begin() + model.states.size());
        std::vector<double> middle_algebraic(
            middle_solution.values.begin() + model.states.size(),
            middle_solution.values.end());
        const double middle_guard = Expression(model.events[event_index].guard).evaluate(
            context(model, middle_state, middle_algebraic, start_time + middle));
        const bool root_is_left = model.events[event_index].direction > 0
            ? middle_guard >= 0.0
            : middle_guard <= 0.0;
        if (root_is_left) {
            right = middle;
            right_solution = middle_solution;
        } else {
            left = middle;
        }
    }
    std::vector<double> state(
        right_solution.values.begin(),
        right_solution.values.begin() + model.states.size());
    std::vector<double> algebraic(
        right_solution.values.begin() + model.states.size(),
        right_solution.values.end());
    const double guard = Expression(model.events[event_index].guard).evaluate(
        context(model, state, algebraic, start_time + right));
    return {
        event_index, right, std::move(state), std::move(algebraic),
        right_solution.iterations, right_solution.residual_inf, guard, false};
}

LocatedDaeEvent locate_dae_grazing_event(
    const IndexOneDaeIR& model,
    std::size_t event_index,
    double start_time,
    const std::vector<double>& start_state,
    const std::vector<double>& start_algebraic,
    double step,
    const DaeTolerance& tolerance,
    EventRootTelemetry& telemetry) {
    const auto signed_guard = [&](
        double offset, NonlinearResult* located_solution = nullptr) {
        auto solution = solve_dae_step(
            model, start_state, start_state, start_algebraic,
            start_time + offset, offset, tolerance);
        telemetry.record(solution);
        if (!solution.converged) {
            throw std::runtime_error("DAE grazing localization Newton failed");
        }
        std::vector<double> state(
            solution.values.begin(), solution.values.begin() + model.states.size());
        std::vector<double> algebraic(
            solution.values.begin() + model.states.size(), solution.values.end());
        const double guard = Expression(model.events[event_index].guard).evaluate(
            context(model, state, algebraic, start_time + offset));
        if (located_solution != nullptr) *located_solution = std::move(solution);
        return model.events[event_index].direction > 0 ? guard : -guard;
    };
    constexpr double inverse_golden = 0.6180339887498948482;
    double left = 0.0;
    double right = step;
    double first = right - inverse_golden * (right - left);
    double second = left + inverse_golden * (right - left);
    double first_value = signed_guard(first);
    double second_value = signed_guard(second);
    while (right - left > tolerance.root_time) {
        if (first_value < second_value) {
            left = first;
            first = second;
            first_value = second_value;
            second = left + inverse_golden * (right - left);
            second_value = signed_guard(second);
        } else {
            right = second;
            second = first;
            second_value = first_value;
            first = right - inverse_golden * (right - left);
            first_value = signed_guard(first);
        }
    }
    const double offset = 0.5 * (left + right);
    NonlinearResult solution;
    const double peak = signed_guard(offset, &solution);
    const double probe = std::max(tolerance.root_time * 4.0, step / 64.0);
    if (offset <= probe || offset + probe >= step) return {};
    const double before = signed_guard(offset - probe);
    const double after = signed_guard(offset + probe);
    const double prominence = std::min(peak - before, peak - after);
    if (std::abs(peak) > tolerance.guard || prominence <= tolerance.guard) return {};
    std::vector<double> state(
        solution.values.begin(), solution.values.begin() + model.states.size());
    std::vector<double> algebraic(
        solution.values.begin() + model.states.size(), solution.values.end());
    const double guard = model.events[event_index].direction > 0 ? peak : -peak;
    return {
        event_index, offset, std::move(state), std::move(algebraic),
        solution.iterations, solution.residual_inf, guard, true};
}
}

std::vector<std::vector<double>> assemble_dae_step_jacobian(
    const IndexOneDaeIR& model,
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
        throw std::invalid_argument("invalid DAE Jacobian assembly context");
    }
    std::vector<double> candidate = candidate_state;
    candidate.insert(candidate.end(), candidate_algebraic.begin(), candidate_algebraic.end());
    return finite_difference_jacobian(candidate, [&](const std::vector<double>& values) {
        return residual(model, previous_state, values, target_time, step);
    });
}

std::vector<double> evaluate_dae_step_residual(
    const IndexOneDaeIR& model,
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
        throw std::invalid_argument("invalid DAE residual evaluation context");
    }
    std::vector<double> candidate = candidate_state;
    candidate.insert(candidate.end(), candidate_algebraic.begin(), candidate_algebraic.end());
    return residual(model, previous_state, candidate, target_time, step);
}

void IndexOneDaeIR::validate() const {
    if (schema_version != kIndexOneDaeSchemaVersion ||
        structural_class != "semi-explicit-index1-candidate" ||
        model_id.empty() || source_hash.empty()) {
        throw std::invalid_argument("invalid index-1 DAE identity/class");
    }
    std::unordered_set<std::string> known{"time"};
    for (const auto& [name, value] : parameters) if (!known.insert(name).second || !std::isfinite(value)) throw std::invalid_argument("invalid DAE parameter: " + name);
    std::unordered_set<std::string> state_names;
    for (const auto& variable : states) if (!state_names.insert(variable.name).second || !known.insert(variable.name).second || !std::isfinite(variable.start) || !(variable.nominal > 0.0)) throw std::invalid_argument("invalid DAE state: " + variable.name);
    std::unordered_set<std::string> algebraic_names;
    for (const auto& variable : algebraics) if (!algebraic_names.insert(variable.name).second || !known.insert(variable.name).second || !std::isfinite(variable.start) || !(variable.nominal > 0.0)) throw std::invalid_argument("invalid DAE algebraic: " + variable.name);
    if (states.empty() || derivatives.size() != states.size() || constraints.size() != algebraics.size()) throw std::invalid_argument("index-1 DAE must have matching derivative/constraint counts");
    for (const auto& expression : derivatives) for (const auto& name : names(expression)) if (!known.contains(name)) throw std::invalid_argument("DAE derivative references unknown name: " + name);
    for (const auto& equation : constraints) for (const auto& name : names(equation.residual)) if (!known.contains(name)) throw std::invalid_argument("DAE constraint references unknown name: " + name);
    for (const auto& equation : initial_constraints) for (const auto& name : names(equation.residual)) if (!known.contains(name)) throw std::invalid_argument("DAE initial equation references unknown name: " + name);
    std::unordered_set<std::string> event_ids;
    auto reset_known = known;
    for (const auto& state : states) reset_known.insert("__smave_pre_" + state.name);
    for (const auto& event : events) {
        if (!event_ids.insert(event.id).second ||
            (event.direction != -1 && event.direction != 1) || event.resets.empty()) {
            throw std::invalid_argument("invalid DAE initial event: " + event.id);
        }
        for (const auto& name : names(event.guard)) {
            if (!known.contains(name)) {
                throw std::invalid_argument("DAE initial event references unknown name: " + name);
            }
        }
        std::unordered_set<std::string> reset_names;
        for (const auto& reset : event.resets) {
            if (!state_names.contains(reset.variable) ||
                !reset_names.insert(reset.variable).second) {
                throw std::invalid_argument(
                    event.id + ": invalid DAE reset state " + reset.variable);
            }
            for (const auto& name : names(reset.expression)) {
                if (!reset_known.contains(name)) {
                    throw std::invalid_argument("DAE reset references unknown name: " + name);
                }
            }
        }
    }
    if (!initial_constraints.empty() && initial_constraints.size() != states.size()) {
        throw std::invalid_argument("DAE initial equation count must equal state count");
    }
    std::unordered_map<std::string, std::size_t> algebraic_index;
    for (std::size_t index = 0; index < algebraics.size(); ++index) {
        algebraic_index[algebraics[index].name] = index;
    }
    std::vector<std::vector<std::size_t>> incidence(constraints.size());
    for (std::size_t equation = 0; equation < constraints.size(); ++equation) {
        for (const auto& name : names(constraints[equation].residual)) {
            if (const auto item = algebraic_index.find(name); item != algebraic_index.end()) {
                incidence[equation].push_back(item->second);
            }
        }
    }
    if (!initial_constraints.empty()) {
        std::vector<std::string> initialization_variables;
        for (const auto& state : states) initialization_variables.push_back(state.name);
        for (const auto& algebraic : algebraics) {
            initialization_variables.push_back(algebraic.name);
        }
        std::unordered_map<std::string, std::size_t> initialization_index;
        for (std::size_t index = 0; index < initialization_variables.size(); ++index) {
            initialization_index[initialization_variables[index]] = index;
        }
        std::vector<DaeEquationIR> initialization_equations = initial_constraints;
        initialization_equations.insert(
            initialization_equations.end(), constraints.begin(), constraints.end());
        std::vector<std::vector<std::size_t>> initialization_incidence(
            initialization_equations.size());
        for (std::size_t equation = 0;
             equation < initialization_equations.size(); ++equation) {
            for (const auto& name : names(initialization_equations[equation].residual)) {
                if (const auto item = initialization_index.find(name);
                    item != initialization_index.end()) {
                    initialization_incidence[equation].push_back(item->second);
                }
            }
        }
        std::vector<int> initialization_match(initialization_variables.size(), -1);
        for (std::size_t equation = 0;
             equation < initialization_equations.size(); ++equation) {
            std::vector<bool> visited(initialization_variables.size(), false);
            if (!augment_matching(
                    equation, initialization_incidence,
                    initialization_match, visited)) {
                throw std::invalid_argument(
                    "DAE initialization system lacks a square structural matching");
            }
        }
    }
    std::vector<int> variable_match(algebraics.size(), -1);
    for (std::size_t equation = 0; equation < constraints.size(); ++equation) {
        std::vector<bool> visited(algebraics.size(), false);
        if (!augment_matching(equation, incidence, variable_match, visited)) {
            throw std::invalid_argument(
                "DAE algebraic incidence lacks a square structural matching; index-1 not established");
        }
    }
}
void IndexOneDaeIR::write(const std::filesystem::path& path) const {
    validate(); std::ofstream output(path); if (!output) throw std::runtime_error("cannot write DAE IR: " + path.string());
    output << "SMAVE_INDEX1_DAE 4\nMODEL " << std::quoted(model_id)
           << "\nSOURCE_HASH " << std::quoted(source_hash)
           << "\nSTRUCTURAL_CLASS " << std::quoted(structural_class)
           << "\nPARAMETERS " << parameters.size();
    for (const auto& [name, value] : parameters) output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << value;
    output << "\nSTATES " << states.size() << '\n'; for (const auto& item : states) output << "STATE " << std::quoted(item.name) << ' ' << item.start << ' ' << item.nominal << '\n';
    output << "ALGEBRAICS " << algebraics.size() << '\n'; for (const auto& item : algebraics) output << "ALGEBRAIC " << std::quoted(item.name) << ' ' << item.start << ' ' << item.nominal << '\n';
    output << "DERIVATIVES " << derivatives.size() << '\n'; for (const auto& expression : derivatives) output << "DERIVATIVE " << std::quoted(expression) << '\n';
    output << "CONSTRAINTS " << constraints.size() << '\n'; for (const auto& equation : constraints) output << "CONSTRAINT " << std::quoted(equation.id) << ' ' << std::quoted(equation.residual) << '\n';
    output << "INITIAL_CONSTRAINTS " << initial_constraints.size() << '\n'; for (const auto& equation : initial_constraints) output << "INITIAL_CONSTRAINT " << std::quoted(equation.id) << ' ' << std::quoted(equation.residual) << '\n';
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
IndexOneDaeIR IndexOneDaeIR::read(const std::filesystem::path& path) {
    std::ifstream input(path); if (!input) throw std::runtime_error("cannot read DAE IR: " + path.string());
    require_tag(input, "SMAVE_INDEX1_DAE"); int version{}; input >> version; if (version < 2 || version > 4) throw std::invalid_argument("unsupported index-1 DAE version");
    IndexOneDaeIR model; require_tag(input, "MODEL"); input >> std::quoted(model.model_id); require_tag(input, "SOURCE_HASH"); input >> std::quoted(model.source_hash); require_tag(input, "STRUCTURAL_CLASS"); input >> std::quoted(model.structural_class);
    std::size_t count{}; require_tag(input, "PARAMETERS"); input >> count; for (std::size_t i=0;i<count;++i) { std::string name; double value; input >> std::quoted(name) >> value; model.parameters[name]=value; }
    require_tag(input, "STATES"); input >> count; model.states.resize(count); for (auto& item:model.states) { require_tag(input,"STATE"); input >> std::quoted(item.name) >> item.start >> item.nominal; }
    require_tag(input, "ALGEBRAICS"); input >> count; model.algebraics.resize(count); for (auto& item:model.algebraics) { require_tag(input,"ALGEBRAIC"); input >> std::quoted(item.name) >> item.start >> item.nominal; }
    require_tag(input,"DERIVATIVES"); input >> count; model.derivatives.resize(count); for (auto& expression:model.derivatives) { require_tag(input,"DERIVATIVE"); input >> std::quoted(expression); }
    require_tag(input,"CONSTRAINTS"); input >> count; model.constraints.resize(count); for (auto& equation:model.constraints) { require_tag(input,"CONSTRAINT"); input >> std::quoted(equation.id) >> std::quoted(equation.residual); equation.variables=names(equation.residual); }
    require_tag(input,"INITIAL_CONSTRAINTS"); input >> count; model.initial_constraints.resize(count); for (auto& equation:model.initial_constraints) { require_tag(input,"INITIAL_CONSTRAINT"); input >> std::quoted(equation.id) >> std::quoted(equation.residual); equation.variables=names(equation.residual); }
    if (version >= 3) {
        require_tag(input, version == 3 ? "INITIAL_EVENTS" : "EVENTS");
        input >> count;
        model.events.resize(count);
        for (auto& event : model.events) {
            require_tag(input, version == 3 ? "INITIAL_EVENT" : "EVENT");
            std::size_t reset_count{};
            input >> std::quoted(event.id) >> event.direction >> event.priority
                  >> event.source_order >> std::quoted(event.guard) >> reset_count;
            event.resets.resize(reset_count);
            for (auto& reset : event.resets) {
                input >> std::quoted(reset.variable) >> std::quoted(reset.expression);
            }
        }
    }
    model.schema_version = kIndexOneDaeSchemaVersion;
    require_tag(input,"END"); model.validate(); return model;
}
IndexOneDaeIR compile_index_one_dae(const std::filesystem::path& source, const std::string& top) {
    const auto raw = read_file(source); auto text = strip_comments(raw); std::smatch match;
    if (!std::regex_search(text, match, std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) throw std::invalid_argument("DAE source has no model declaration");
    IndexOneDaeIR model; model.model_id=match[1].str(); model.source_hash=digest(raw); if (!top.empty() && top != model.model_id) throw std::invalid_argument("requested DAE top model does not match source");
    text=text.substr(match.position()+match.length());
    text=std::regex_replace(
        text, std::regex("end\\s+"+model.model_id+R"(\s*;\s*$)"), "");
    const auto initial_equation_pos = text.find("initial equation");
    const auto equation_search_start = initial_equation_pos == std::string::npos
        ? 0
        : initial_equation_pos + std::string("initial equation").size();
    const auto equation_pos = text.find("equation", equation_search_start);
    if (equation_pos==std::string::npos) throw std::invalid_argument("DAE model has no equation section");
    const auto declaration_end = initial_equation_pos == std::string::npos
        ? equation_pos
        : initial_equation_pos;
    const std::regex declaration(R"(^\s*(parameter\s+)?Real\s+([A-Za-z_]\w*)\s*(\([^;]*\))?\s*(?:=\s*(.+))?\s*$)");
    std::vector<DaeVariableIR> declared_variables;
    for (auto statement:split(text.substr(0,declaration_end))) {
        std::smatch item;
        if (!std::regex_match(statement,item,declaration)) throw std::invalid_argument("unsupported DAE declaration: "+statement);
        const auto attributes=attrs(item[3].str());
        for (const auto& [name, value] : attributes) {
            (void)value;
            if (name != "start" && name != "nominal") {
                throw std::invalid_argument("unsupported DAE variable attribute: " + name);
            }
        }
        DaeVariableIR variable; variable.name=item[2].str(); variable.start=attr(attributes,"start",0.0); variable.nominal=attr(attributes,"nominal",1.0);
        if (item[1].matched) {
            if (!attributes.empty()) throw std::invalid_argument("DAE parameters do not support attributes");
            if (!item[4].matched) throw std::invalid_argument("DAE parameter requires value");
            model.parameters[variable.name]=std::stod(trim(item[4].str()));
        } else if (item[4].matched) {
            throw std::invalid_argument("DAE variable binding equations are unsupported");
        } else {
            declared_variables.push_back(variable);
        }
    }
    if (initial_equation_pos != std::string::npos) {
        const auto initial_start = initial_equation_pos +
            std::string("initial equation").size();
        for (auto statement : split(text.substr(
                 initial_start, equation_pos - initial_start))) {
            if (statement.find("der(") != std::string::npos ||
                statement.find("when ") != std::string::npos) {
                throw std::invalid_argument(
                    "DAE initial equations cannot contain der/when in this subset");
            }
            const auto equal = statement.find('=');
            if (equal == std::string::npos) {
                throw std::invalid_argument("invalid DAE initial equation: " + statement);
            }
            DaeEquationIR equation;
            equation.id = "initial-" +
                std::to_string(model.initial_constraints.size() + 1);
            equation.residual = "(" + trim(statement.substr(0, equal)) +
                ") - (" + trim(statement.substr(equal + 1)) + ")";
            equation.variables = names(equation.residual);
            model.initial_constraints.push_back(std::move(equation));
        }
    }
    std::map<std::string, std::string> derivative_by_state;
    auto equation_section = text.substr(equation_pos + 8);
    const std::regex when_block(
        R"(\bwhen\s+([\s\S]*?)\s+then\s*([\s\S]*?)\s*end\s+when\s*;)");
    std::size_t event_order{};
    for (auto iterator = std::sregex_iterator(
             equation_section.begin(), equation_section.end(), when_block);
         iterator != std::sregex_iterator(); ++iterator) {
        if (std::regex_search((*iterator)[2].str(), std::regex(R"(\belsewhen\b)"))) {
            throw std::invalid_argument("DAE initial-event subset does not support elsewhen");
        }
        std::smatch guard_match;
        const auto guard_source = trim((*iterator)[1].str());
        if (!std::regex_match(
                guard_source, guard_match,
                std::regex(R"(^\s*(.+?)\s*(<=|>=|<|>)\s*(.+)\s*$)"))) {
            throw std::invalid_argument(
                "DAE initial-event guard must be one scalar comparison");
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
            event.resets.push_back({
                (*reset)[1].str(), replace_pre(trim((*reset)[2].str()))});
        }
        remaining = std::regex_replace(remaining, reset_expression, "");
        if (!trim(remaining).empty() || event.resets.empty()) {
            throw std::invalid_argument(
                "DAE initial-event body supports only reinit statements");
        }
        model.events.push_back(std::move(event));
    }
    equation_section = std::regex_replace(equation_section, when_block, "");
    for (auto statement:split(equation_section)) {
        std::smatch derivative; if (std::regex_match(statement,derivative,std::regex(R"(\s*der\s*\(\s*([A-Za-z_]\w*)\s*\)\s*=\s*(.+)\s*)"))) { const auto state=derivative[1].str(); if (!derivative_by_state.emplace(state, trim(derivative[2].str())).second) throw std::invalid_argument("duplicate DAE derivative: "+state); continue; }
        const auto equal=statement.find('='); if (equal==std::string::npos) throw std::invalid_argument("unsupported DAE equation: "+statement); const auto left=trim(statement.substr(0,equal)); const auto right=trim(statement.substr(equal+1)); if (left.empty()||right.empty()) throw std::invalid_argument("invalid DAE equation"); DaeEquationIR equation; equation.id="constraint-"+std::to_string(model.constraints.size()+1); equation.residual="("+left+") - ("+right+")"; equation.variables=names(equation.residual); model.constraints.push_back(std::move(equation));
    }
    for (const auto& variable : declared_variables) {
        const auto derivative = derivative_by_state.find(variable.name);
        if (derivative == derivative_by_state.end()) {
            model.algebraics.push_back(variable);
        } else {
            model.states.push_back(variable);
            model.derivatives.push_back(derivative->second);
            derivative_by_state.erase(derivative);
        }
    }
    if (!derivative_by_state.empty()) throw std::invalid_argument("derivative targets undeclared variable: "+derivative_by_state.begin()->first);
    if (model.states.empty()) throw std::invalid_argument("index-1 DAE has no states"); model.validate(); return model;
}
DaeRunResult simulate_index_one_dae(
    const IndexOneDaeIR& model,
    double end_time,
    double maximum_step,
    DaeTolerance tolerance,
    const DaeMultigridArtifact* artifact) {
    model.validate();
    DaeRunResult result;
    std::vector<double> state;
    std::vector<double> algebraic;
    std::vector<double> initialized_state;
    std::vector<double> initialized_algebraic;
    for (const auto& item : model.states) state.push_back(item.start);
    for (const auto& item : model.algebraics) algebraic.push_back(item.start);
    double time = 0.0;
    if (artifact != nullptr) {
        result.dae_preconditioner_version = artifact->hierarchy.expert_version;
    }
    try {
        if (!(end_time>0.0)||!std::isfinite(end_time)||!(maximum_step>0.0)||
            !std::isfinite(maximum_step)||!(tolerance.absolute>0.0)||
            !(tolerance.relative>0.0)||!(tolerance.root_time>0.0)||
            !(tolerance.guard>0.0)||!(tolerance.algebraic_rank>0.0)||
            tolerance.maximum_newton_iterations<=0) {
            throw std::invalid_argument("invalid DAE simulation interval/tolerance");
        }
        NonlinearResult initialization;
        if (model.initial_constraints.empty()) {
            if (algebraic.size() > 1024) {
                std::vector<std::string> unknowns;
                unknowns.reserve(model.algebraics.size());
                for (const auto& variable : model.algebraics) {
                    unknowns.push_back(variable.name);
                }
                EquationSystemEvaluator evaluator(
                    unknowns, model.constraints, context(model, state, algebraic, 0.0));
                initialization = sparse_damped_newton(
                    algebraic,
                    [&](const std::vector<double>& candidate) {
                        return evaluator.evaluate(candidate);
                    },
                    [&](const std::vector<double>& candidate,
                        const std::vector<double>& direction) {
                        return evaluator.directional_derivative(candidate, direction);
                    },
                    equation_sparsity(model.constraints, unknowns), tolerance);
            } else {
                initialization = damped_newton(
                    algebraic,
                    [&](const std::vector<double>& candidate) {
                        return equation_residuals(
                            model.constraints,
                            context(model, state, candidate, 0.0));
                    },
                    tolerance);
            }
            if (initialization.converged) algebraic = initialization.values;
        } else {
            std::vector<double> candidate = state;
            candidate.insert(candidate.end(), algebraic.begin(), algebraic.end());
            std::vector<DaeEquationIR> equations = model.initial_constraints;
            equations.insert(
                equations.end(), model.constraints.begin(), model.constraints.end());
            std::vector<std::string> unknowns;
            unknowns.reserve(candidate.size());
            for (const auto& variable : model.states) unknowns.push_back(variable.name);
            for (const auto& variable : model.algebraics) unknowns.push_back(variable.name);
            if (candidate.size() > 1024) {
                auto fixed_values = context(model, state, algebraic, 0.0);
                EquationSystemEvaluator evaluator(unknowns, equations, std::move(fixed_values));
                initialization = sparse_damped_newton(
                    std::move(candidate),
                    [&](const std::vector<double>& values) {
                        return evaluator.evaluate(values);
                    },
                    [&](const std::vector<double>& values,
                        const std::vector<double>& direction) {
                        return evaluator.directional_derivative(values, direction);
                    },
                    equation_sparsity(equations, unknowns), tolerance);
            } else {
                initialization = damped_newton(
                    std::move(candidate),
                    [&](const std::vector<double>& values) {
                        std::vector<double> candidate_state(
                            values.begin(), values.begin() + model.states.size());
                        std::vector<double> candidate_algebraic(
                            values.begin() + model.states.size(), values.end());
                        return equation_residuals(
                            equations,
                            context(model, candidate_state, candidate_algebraic, 0.0));
                    },
                    tolerance);
            }
            if (initialization.converged) {
                state.assign(
                    initialization.values.begin(),
                    initialization.values.begin() + model.states.size());
                algebraic.assign(
                    initialization.values.begin() + model.states.size(),
                    initialization.values.end());
            }
        }
        result.initialization_iterations = initialization.iterations;
        result.initialization_residual_inf = initialization.residual_inf;
        if (initialization.sparse_newton) {
            result.sparse_initialization = true;
            result.sparse_initialization_iterations = initialization.iterations;
            result.sparse_initialization_krylov_iterations =
                initialization.krylov_iterations;
            result.sparse_initialization_jacobian_nonzeros =
                initialization.jacobian_nonzeros;
            result.sparse_initialization_jacobian_storage_bytes =
                initialization.jacobian_storage_bytes;
            result.sparse_initialization_jacobian_colors =
                initialization.jacobian_colors;
            result.sparse_initialization_jacobian_evaluation_batches =
                initialization.jacobian_evaluation_batches;
            result.sparse_initialization_jacobian_ad_batches =
                initialization.jacobian_ad_batches;
            result.sparse_initialization_jacobian_fd_fallback_batches =
                initialization.jacobian_fd_fallback_batches;
            result.sparse_initialization_inner_backend = initialization.inner_backend;
        }
        if (!initialization.converged) {
            throw std::runtime_error("DAE consistent initialization Newton failed");
        }
        enforce_algebraic_rank(
            model, state, algebraic, 0.0, tolerance,
            result.algebraic_rank_checks,
            result.minimum_algebraic_rank_margin);
        if (!model.events.empty()) {
            const auto event_transaction = execute_initial_events(
                model, state, algebraic, tolerance);
            state = event_transaction.state;
            algebraic = event_transaction.algebraic;
            result.initial_events = event_transaction.records;
            result.initial_event_projection_iterations =
                event_transaction.projection_iterations;
            result.initial_event_projection_residual_inf =
                event_transaction.projection_residual_inf;
            result.algebraic_rank_checks += event_transaction.algebraic_rank_checks;
            result.minimum_algebraic_rank_margin = std::min(
                result.minimum_algebraic_rank_margin,
                event_transaction.minimum_algebraic_rank_margin);
            result.sparse_event_projections += event_transaction.sparse_projections;
            result.sparse_event_projection_iterations +=
                event_transaction.sparse_projection_iterations;
            result.sparse_event_projection_krylov_iterations +=
                event_transaction.sparse_projection_krylov_iterations;
            result.sparse_event_projection_jacobian_nonzeros = std::max(
                result.sparse_event_projection_jacobian_nonzeros,
                event_transaction.sparse_projection_jacobian_nonzeros);
            result.sparse_event_projection_jacobian_storage_bytes = std::max(
                result.sparse_event_projection_jacobian_storage_bytes,
                event_transaction.sparse_projection_jacobian_storage_bytes);
            result.sparse_event_projection_jacobian_colors = std::max(
                result.sparse_event_projection_jacobian_colors,
                event_transaction.sparse_projection_jacobian_colors);
            result.sparse_event_projection_jacobian_evaluation_batches +=
                event_transaction.sparse_projection_jacobian_evaluation_batches;
            result.sparse_event_projection_jacobian_ad_batches +=
                event_transaction.sparse_projection_jacobian_ad_batches;
            result.sparse_event_projection_jacobian_fd_fallback_batches +=
                event_transaction.sparse_projection_jacobian_fd_fallback_batches;
            if (!event_transaction.sparse_projection_inner_backend.empty()) {
                result.sparse_event_projection_inner_backend =
                    event_transaction.sparse_projection_inner_backend;
            }
        }
        initialized_state = state;
        initialized_algebraic = algebraic;
        std::vector<bool> event_armed;
        const auto initialized_context = context(model, state, algebraic, 0.0);
        for (const auto& event : model.events) {
            event_armed.push_back(!guard_active(
                Expression(event.guard).evaluate(initialized_context),
                event.direction, tolerance.guard));
        }
        std::size_t boundary_index{1};
        EventRootTelemetry event_root_telemetry;
        while (time < end_time) {
            const double target_time = std::min(
                end_time, static_cast<double>(boundary_index) * maximum_step);
            const double step = target_time - time;
            if (!(step > 0.0)) {
                ++boundary_index;
                continue;
            }
            const auto previous = state;
            const auto solved = solve_dae_candidate_step(
                model, previous, state, algebraic, target_time, step, tolerance, artifact);
            if (solved.learned_attempted) {
                result.learned_krylov_iterations += solved.krylov_iterations;
                if (solved.learned_rejected) {
                    ++result.learned_rejections;
                    ++result.dense_step_fallbacks;
                } else {
                    ++result.learned_preconditioned_steps;
                    result.learned_preconditioned_newton_iterations += solved.iterations;
                }
            }
            if (solved.sparse_newton) {
                ++result.sparse_newton_steps;
                result.sparse_newton_iterations += solved.iterations;
                result.sparse_krylov_iterations += solved.krylov_iterations;
                result.sparse_jacobian_nonzeros = solved.jacobian_nonzeros;
                result.sparse_jacobian_storage_bytes = solved.jacobian_storage_bytes;
                result.sparse_jacobian_colors = std::max(
                    result.sparse_jacobian_colors, solved.jacobian_colors);
                result.sparse_jacobian_evaluation_batches +=
                    solved.jacobian_evaluation_batches;
                result.sparse_jacobian_ad_batches += solved.jacobian_ad_batches;
                result.sparse_jacobian_fd_fallback_batches +=
                    solved.jacobian_fd_fallback_batches;
                result.sparse_inner_backend = solved.inner_backend;
            }
            if (!solved.converged) {
                ++result.rejected_steps;
                throw std::runtime_error(
                    "index-1 DAE Newton failed at t=" +
                    std::to_string(target_time));
            }
            std::vector<double> candidate_state(
                solved.values.begin(), solved.values.begin() + model.states.size());
            std::vector<double> candidate_algebraic(
                solved.values.begin() + model.states.size(), solved.values.end());
            enforce_algebraic_rank(
                model, candidate_state, candidate_algebraic, target_time, tolerance,
                result.algebraic_rank_checks,
                result.minimum_algebraic_rank_margin);
            const auto candidate_context = context(
                model, candidate_state, candidate_algebraic, target_time);
            const auto start_context = context(model, state, algebraic, time);
            std::vector<LocatedDaeEvent> located;
            for (std::size_t index = 0; index < model.events.size(); ++index) {
                const Expression guard(model.events[index].guard);
                const double left = guard.evaluate(start_context);
                const double right = guard.evaluate(candidate_context);
                const bool inactive = model.events[index].direction > 0
                    ? left < -tolerance.guard
                    : left > tolerance.guard;
                if (!event_armed[index] && inactive) event_armed[index] = true;
                if (event_armed[index] && guard_crossing(
                        left, right, model.events[index].direction, tolerance.guard)) {
                    located.push_back(locate_dae_event(
                        model, index, time, state, algebraic, step, tolerance,
                        event_root_telemetry));
                } else if (event_armed[index]) {
                    const double signed_left = model.events[index].direction > 0
                        ? left : -left;
                    const double signed_right = model.events[index].direction > 0
                        ? right : -right;
                    if (signed_left < -tolerance.guard &&
                        signed_right < -tolerance.guard) {
                        auto grazing = locate_dae_grazing_event(
                            model, index, time, state, algebraic, step, tolerance,
                            event_root_telemetry);
                        if (!grazing.state.empty()) located.push_back(std::move(grazing));
                    }
                }
            }
            if (!located.empty()) {
                const double earliest_offset = std::min_element(
                    located.begin(), located.end(),
                    [](const auto& left, const auto& right) {
                        return left.offset < right.offset;
                    })->offset;
                const auto root_solution = solve_dae_step(
                    model, state, state, algebraic,
                    time + earliest_offset, earliest_offset, tolerance);
                event_root_telemetry.record(root_solution, true);
                if (!root_solution.converged) {
                    throw std::runtime_error("DAE common event root Newton failed");
                }
                std::vector<double> root_state(
                    root_solution.values.begin(),
                    root_solution.values.begin() + model.states.size());
                std::vector<double> root_algebraic(
                    root_solution.values.begin() + model.states.size(),
                    root_solution.values.end());
                const double event_time = time + earliest_offset;
                const auto root_context = context(
                    model, root_state, root_algebraic, event_time);
                std::vector<std::size_t> pending;
                for (const auto& item : located) {
                    if (std::abs(item.offset - earliest_offset) <= tolerance.root_time) {
                        const double guard_residual = std::abs(
                            Expression(model.events[item.index].guard).evaluate(root_context));
                        if (guard_residual > tolerance.guard) {
                            throw std::runtime_error(
                                model.events[item.index].id +
                                ": DAE event root guard residual exceeds tolerance");
                        }
                        result.maximum_guard_residual = std::max(
                            result.maximum_guard_residual, guard_residual);
                        pending.push_back(item.index);
                        event_armed[item.index] = false;
                    }
                }
                const auto transaction = execute_dae_event_transaction(
                    model, event_time, root_state, root_algebraic,
                    std::move(pending), tolerance);
                if (result.events.size() + transaction.records.size() > 10000) {
                    throw std::runtime_error("DAE event chattering limit exceeded");
                }
                result.steps.push_back({
                    event_time, earliest_offset,
                    root_solution.iterations, root_solution.residual_inf});
                result.maximum_residual_inf = std::max(
                    result.maximum_residual_inf, root_solution.residual_inf);
                result.maximum_event_projection_residual_inf = std::max(
                    result.maximum_event_projection_residual_inf,
                    transaction.projection_residual_inf);
                result.algebraic_rank_checks += transaction.algebraic_rank_checks;
                result.minimum_algebraic_rank_margin = std::min(
                    result.minimum_algebraic_rank_margin,
                    transaction.minimum_algebraic_rank_margin);
                result.sparse_event_projections += transaction.sparse_projections;
                result.sparse_event_projection_iterations +=
                    transaction.sparse_projection_iterations;
                result.sparse_event_projection_krylov_iterations +=
                    transaction.sparse_projection_krylov_iterations;
                result.sparse_event_projection_jacobian_nonzeros = std::max(
                    result.sparse_event_projection_jacobian_nonzeros,
                    transaction.sparse_projection_jacobian_nonzeros);
                result.sparse_event_projection_jacobian_storage_bytes = std::max(
                    result.sparse_event_projection_jacobian_storage_bytes,
                    transaction.sparse_projection_jacobian_storage_bytes);
                result.sparse_event_projection_jacobian_colors = std::max(
                    result.sparse_event_projection_jacobian_colors,
                    transaction.sparse_projection_jacobian_colors);
                result.sparse_event_projection_jacobian_evaluation_batches +=
                    transaction.sparse_projection_jacobian_evaluation_batches;
                result.sparse_event_projection_jacobian_ad_batches +=
                    transaction.sparse_projection_jacobian_ad_batches;
                result.sparse_event_projection_jacobian_fd_fallback_batches +=
                    transaction.sparse_projection_jacobian_fd_fallback_batches;
                if (!transaction.sparse_projection_inner_backend.empty()) {
                    result.sparse_event_projection_inner_backend =
                        transaction.sparse_projection_inner_backend;
                }
                result.events.insert(
                    result.events.end(),
                    transaction.records.begin(), transaction.records.end());
                for (const auto& item : located) {
                    if (!item.grazing ||
                        std::abs(item.offset - earliest_offset) > tolerance.root_time) {
                        continue;
                    }
                    const auto record = std::find_if(
                        result.events.rbegin(), result.events.rend(),
                        [&](const DaeEventRecord& event) {
                            return event.id == model.events[item.index].id &&
                                std::abs(event.time - event_time) <= tolerance.root_time;
                        });
                    if (record != result.events.rend() && !record->grazing) {
                        record->grazing = true;
                        ++result.grazing_events;
                    }
                }
                state = transaction.state;
                algebraic = transaction.algebraic;
                time = event_time;
                continue;
            }
            state.assign(
                candidate_state.begin(), candidate_state.end());
            algebraic.assign(candidate_algebraic.begin(), candidate_algebraic.end());
            result.steps.push_back({
                target_time, step, solved.iterations, solved.residual_inf});
            result.maximum_residual_inf = std::max(
                result.maximum_residual_inf, solved.residual_inf);
            time = target_time;
            ++boundary_index;
        }
        result.event_root_solves = event_root_telemetry.solves;
        result.common_event_root_solves = event_root_telemetry.common_solves;
        result.sparse_event_root_solves = event_root_telemetry.sparse_solves;
        result.sparse_event_root_newton_iterations =
            event_root_telemetry.sparse_newton_iterations;
        result.sparse_event_root_krylov_iterations =
            event_root_telemetry.sparse_krylov_iterations;
        result.sparse_event_root_jacobian_nonzeros =
            event_root_telemetry.sparse_jacobian_nonzeros;
        result.sparse_event_root_jacobian_storage_bytes =
            event_root_telemetry.sparse_jacobian_storage_bytes;
        result.sparse_event_root_jacobian_colors =
            event_root_telemetry.sparse_jacobian_colors;
        result.sparse_event_root_jacobian_evaluation_batches =
            event_root_telemetry.sparse_jacobian_evaluation_batches;
        result.sparse_event_root_jacobian_ad_batches =
            event_root_telemetry.sparse_jacobian_ad_batches;
        result.sparse_event_root_jacobian_fd_fallback_batches =
            event_root_telemetry.sparse_jacobian_fd_fallback_batches;
        result.sparse_event_root_inner_backend =
            event_root_telemetry.sparse_inner_backend;
        result.success = true;
        result.final_time = time;
        result.message =
            "consistent initialization, DAE event localization, and backward Euler/Newton gates completed";
    } catch (const std::exception& error) {
        result.final_time = time;
        result.message = error.what();
    }
    if (initialized_state.empty()) initialized_state = state;
    if (initialized_algebraic.empty()) initialized_algebraic = algebraic;
    for (std::size_t index = 0; index < state.size(); ++index) {
        result.initial_state[model.states[index].name] = initialized_state[index];
        result.final_state[model.states[index].name] = state[index];
    }
    for (std::size_t index = 0; index < algebraic.size(); ++index) {
        result.initial_algebraics[model.algebraics[index].name] =
            initialized_algebraic[index];
        result.final_algebraics[model.algebraics[index].name] = algebraic[index];
    }
    return result;
}
void write_dae_report(
    const IndexOneDaeIR& model,
    const DaeRunResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write DAE report: "+path.string());
    output << "SMAVE_INDEX1_DAE_REPORT 6\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "FINAL_TIME " << std::setprecision(17) << result.final_time << '\n'
           << "INITIALIZATION_ITERATIONS " << result.initialization_iterations << '\n'
           << "INITIALIZATION_RESIDUAL " << result.initialization_residual_inf << '\n'
           << "INITIAL_EVENT_PROJECTION_ITERATIONS "
           << result.initial_event_projection_iterations << '\n'
           << "INITIAL_EVENT_PROJECTION_RESIDUAL "
           << result.initial_event_projection_residual_inf << '\n'
           << "MAX_EVENT_PROJECTION_RESIDUAL "
           << result.maximum_event_projection_residual_inf << '\n'
           << "MAX_GUARD_RESIDUAL " << result.maximum_guard_residual << '\n'
           << "MAX_RESIDUAL " << result.maximum_residual_inf << '\n'
           << "ALGEBRAIC_RANK_CHECKS " << result.algebraic_rank_checks << '\n'
           << "MIN_ALGEBRAIC_RANK_MARGIN "
           << result.minimum_algebraic_rank_margin << '\n'
           << "DAE_PRECONDITIONER_VERSION "
           << std::quoted(result.dae_preconditioner_version) << '\n'
           << "LEARNED_PRECONDITIONED_STEPS "
           << result.learned_preconditioned_steps << '\n'
           << "LEARNED_PRECONDITIONED_NEWTON_ITERATIONS "
           << result.learned_preconditioned_newton_iterations << '\n'
           << "LEARNED_KRYLOV_ITERATIONS "
           << result.learned_krylov_iterations << '\n'
           << "LEARNED_REJECTIONS " << result.learned_rejections << '\n'
           << "DENSE_STEP_FALLBACKS " << result.dense_step_fallbacks << '\n'
           << "SPARSE_NEWTON_STEPS " << result.sparse_newton_steps << '\n'
           << "SPARSE_NEWTON_ITERATIONS " << result.sparse_newton_iterations << '\n'
           << "SPARSE_KRYLOV_ITERATIONS " << result.sparse_krylov_iterations << '\n'
           << "SPARSE_JACOBIAN_NONZEROS " << result.sparse_jacobian_nonzeros << '\n'
           << "SPARSE_JACOBIAN_BYTES " << result.sparse_jacobian_storage_bytes << '\n'
           << "SPARSE_JACOBIAN_COLORS " << result.sparse_jacobian_colors << '\n'
           << "SPARSE_JACOBIAN_EVALUATION_BATCHES "
           << result.sparse_jacobian_evaluation_batches << '\n'
           << "SPARSE_JACOBIAN_AD_BATCHES "
           << result.sparse_jacobian_ad_batches << '\n'
           << "SPARSE_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.sparse_jacobian_fd_fallback_batches << '\n'
           << "SPARSE_INNER_BACKEND " << std::quoted(result.sparse_inner_backend) << '\n'
           << "SPARSE_INITIALIZATION " << result.sparse_initialization << '\n'
           << "SPARSE_INITIALIZATION_ITERATIONS "
           << result.sparse_initialization_iterations << '\n'
           << "SPARSE_INITIALIZATION_KRYLOV_ITERATIONS "
           << result.sparse_initialization_krylov_iterations << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_NONZEROS "
           << result.sparse_initialization_jacobian_nonzeros << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_BYTES "
           << result.sparse_initialization_jacobian_storage_bytes << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_COLORS "
           << result.sparse_initialization_jacobian_colors << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_EVALUATION_BATCHES "
           << result.sparse_initialization_jacobian_evaluation_batches << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_AD_BATCHES "
           << result.sparse_initialization_jacobian_ad_batches << '\n'
           << "SPARSE_INITIALIZATION_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.sparse_initialization_jacobian_fd_fallback_batches << '\n'
           << "SPARSE_INITIALIZATION_INNER_BACKEND "
           << std::quoted(result.sparse_initialization_inner_backend) << '\n'
           << "SPARSE_EVENT_PROJECTIONS " << result.sparse_event_projections << '\n'
           << "SPARSE_EVENT_PROJECTION_ITERATIONS "
           << result.sparse_event_projection_iterations << '\n'
           << "SPARSE_EVENT_PROJECTION_KRYLOV_ITERATIONS "
           << result.sparse_event_projection_krylov_iterations << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_NONZEROS "
           << result.sparse_event_projection_jacobian_nonzeros << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_BYTES "
           << result.sparse_event_projection_jacobian_storage_bytes << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_COLORS "
           << result.sparse_event_projection_jacobian_colors << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_EVALUATION_BATCHES "
           << result.sparse_event_projection_jacobian_evaluation_batches << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_AD_BATCHES "
           << result.sparse_event_projection_jacobian_ad_batches << '\n'
           << "SPARSE_EVENT_PROJECTION_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.sparse_event_projection_jacobian_fd_fallback_batches << '\n'
           << "SPARSE_EVENT_PROJECTION_INNER_BACKEND "
           << std::quoted(result.sparse_event_projection_inner_backend) << '\n'
           << "EVENT_ROOT_SOLVES " << result.event_root_solves << '\n'
           << "COMMON_EVENT_ROOT_SOLVES " << result.common_event_root_solves << '\n'
           << "SPARSE_EVENT_ROOT_SOLVES " << result.sparse_event_root_solves << '\n'
           << "SPARSE_EVENT_ROOT_NEWTON_ITERATIONS "
           << result.sparse_event_root_newton_iterations << '\n'
           << "SPARSE_EVENT_ROOT_KRYLOV_ITERATIONS "
           << result.sparse_event_root_krylov_iterations << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_NONZEROS "
           << result.sparse_event_root_jacobian_nonzeros << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_BYTES "
           << result.sparse_event_root_jacobian_storage_bytes << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_COLORS "
           << result.sparse_event_root_jacobian_colors << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_EVALUATION_BATCHES "
           << result.sparse_event_root_jacobian_evaluation_batches << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_AD_BATCHES "
           << result.sparse_event_root_jacobian_ad_batches << '\n'
           << "SPARSE_EVENT_ROOT_JACOBIAN_FD_FALLBACK_BATCHES "
           << result.sparse_event_root_jacobian_fd_fallback_batches << '\n'
           << "SPARSE_EVENT_ROOT_INNER_BACKEND "
           << std::quoted(result.sparse_event_root_inner_backend) << '\n'
           << "GRAZING_EVENTS " << result.grazing_events << '\n'
           << "STEPS " << result.steps.size() << '\n';
    output << "INITIAL_EVENTS " << result.initial_events.size() << '\n';
    for (const auto& event : result.initial_events) {
        output << "INITIAL_EVENT " << std::quoted(event.id) << ' '
               << std::setprecision(17) << event.time << '\n';
    }
    output << "EVENTS " << result.events.size() << '\n';
    for (const auto& event : result.events) {
        output << "EVENT " << std::quoted(event.id) << ' '
               << std::setprecision(17) << event.time << ' '
               << event.grazing << ' '
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
        output << "INITIAL_STATE " << std::quoted(item.name) << ' '
               << result.initial_state.at(item.name) << '\n';
    }
    for (const auto& item : model.algebraics) {
        output << "INITIAL_ALGEBRAIC " << std::quoted(item.name) << ' '
               << result.initial_algebraics.at(item.name) << '\n';
    }
    for (const auto& item : model.states) {
        output << "STATE " << std::quoted(item.name) << ' '
               << result.final_state.at(item.name) << '\n';
    }
    for (const auto& item : model.algebraics) {
        output << "ALGEBRAIC " << std::quoted(item.name) << ' '
               << result.final_algebraics.at(item.name) << '\n';
    }
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}
} // namespace smave
