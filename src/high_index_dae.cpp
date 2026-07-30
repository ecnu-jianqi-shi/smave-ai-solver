#include "smave/high_index_dae.hpp"

#include "smave/expression.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace smave {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read index-2 DAE source: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string strip_comments(std::string value) {
    value = std::regex_replace(value, std::regex(R"(/\*[\s\S]*?\*/)"), "");
    return std::regex_replace(value, std::regex(R"(//[^\n]*)"), "");
}

std::vector<std::string> split(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ';')) {
        item = trim(std::move(item));
        if (!item.empty()) result.push_back(std::move(item));
    }
    return result;
}

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

void require_tag(std::istream& input, const std::string& expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error("invalid index-2 DAE IR: expected " + expected);
    }
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) result = std::max(result, std::abs(value));
    return result;
}

bool solve_dense(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right,
    std::vector<double>& solution,
    double* minimum_scaled_pivot = nullptr) {
    const auto size = right.size();
    if (matrix.size() != size) return false;
    double minimum = std::numeric_limits<double>::infinity();
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        double pivot_score{};
        for (std::size_t row = column; row < size; ++row) {
            double row_scale{};
            for (std::size_t item = column; item < size; ++item) {
                row_scale = std::max(row_scale, std::abs(matrix[row][item]));
            }
            const double score = row_scale > 0.0
                ? std::abs(matrix[row][column]) / row_scale
                : 0.0;
            if (score > pivot_score) {
                pivot = row;
                pivot_score = score;
            }
        }
        if (!(pivot_score > 1.0e-14) || !std::isfinite(pivot_score)) return false;
        minimum = std::min(minimum, pivot_score);
        std::swap(matrix[column], matrix[pivot]);
        std::swap(right[column], right[pivot]);
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
        const auto row = size - reverse - 1;
        double value = right[row];
        for (std::size_t column = row + 1; column < size; ++column) {
            value -= matrix[row][column] * solution[column];
        }
        solution[row] = value / matrix[row][row];
    }
    if (minimum_scaled_pivot != nullptr) *minimum_scaled_pivot = minimum;
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

std::unordered_map<std::string, double> context(
    const IndexTwoDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time) {
    std::unordered_map<std::string, double> values;
    values["time"] = time;
    for (std::size_t index = 0; index < state.size(); ++index) {
        values[model.states[index].name] = state[index];
    }
    for (std::size_t index = 0; index < multipliers.size(); ++index) {
        values[model.multipliers[index].name] = multipliers[index];
    }
    return values;
}

std::vector<double> evaluate_dynamics(
    const IndexTwoDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time) {
    const auto values = context(model, state, multipliers, time);
    std::vector<double> result;
    result.reserve(model.dynamics.size());
    for (const auto& source : model.dynamics) {
        result.push_back(Expression(source).evaluate(values));
    }
    return result;
}

std::vector<double> evaluate_constraints(
    const IndexTwoDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time) {
    const auto values = context(model, state, multipliers, time);
    std::vector<double> result;
    result.reserve(model.constraints.size());
    for (const auto& equation : model.constraints) {
        result.push_back(Expression(equation.residual).evaluate(values));
    }
    return result;
}

std::vector<std::vector<double>> constraint_jacobian(const IndexTwoDaeIR& model) {
    std::vector<std::string> states;
    for (const auto& state : model.states) states.push_back(state.name);
    std::vector<std::vector<double>> jacobian;
    for (const auto& constraint : model.constraints) {
        const auto coefficients =
            Expression(constraint.residual).constant_linear_coefficients(states);
        if (!coefficients.has_value()) {
            throw std::invalid_argument("index-2 constraints must be affine in states");
        }
        jacobian.push_back(*coefficients);
    }
    return jacobian;
}

std::vector<double> hidden_residual(
    const IndexTwoDaeIR& model,
    const std::vector<std::vector<double>>& constraint_gradient,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time) {
    const auto dynamics = evaluate_dynamics(model, state, multipliers, time);
    std::vector<double> result(constraint_gradient.size());
    for (std::size_t row = 0; row < constraint_gradient.size(); ++row) {
        for (std::size_t column = 0; column < dynamics.size(); ++column) {
            result[row] += constraint_gradient[row][column] * dynamics[column];
        }
    }
    return result;
}

std::optional<std::vector<double>> directional_dynamics(
    const IndexTwoDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time,
    std::size_t variable,
    bool multiplier_direction) {
    const auto values = context(model, state, multipliers, time);
    std::unordered_map<std::string, double> direction;
    direction[multiplier_direction
        ? model.multipliers[variable].name
        : model.states[variable].name] = 1.0;
    std::vector<double> result;
    result.reserve(model.dynamics.size());
    for (const auto& source : model.dynamics) {
        const auto derivative = Expression(source).directional_derivative(values, direction);
        if (!derivative.has_value()) return std::nullopt;
        result.push_back(*derivative);
    }
    return result;
}

std::vector<std::vector<double>> hidden_jacobian(
    const IndexTwoDaeIR& model,
    const std::vector<std::vector<double>>& constraint_gradient,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time,
    bool use_ad) {
    const auto size = model.multipliers.size();
    std::vector<std::vector<double>> jacobian(size, std::vector<double>(size));
    for (std::size_t column = 0; column < size; ++column) {
        std::vector<double> derivative;
        if (use_ad) {
            const auto ad = directional_dynamics(
                model, state, multipliers, time, column, true);
            if (!ad.has_value()) return {};
            derivative = *ad;
        } else {
            const double step = 1.0e-7 * std::max(1.0, std::abs(multipliers[column]));
            auto plus = multipliers;
            auto minus = multipliers;
            plus[column] += step;
            minus[column] -= step;
            const auto plus_value = evaluate_dynamics(model, state, plus, time);
            const auto minus_value = evaluate_dynamics(model, state, minus, time);
            derivative.resize(model.states.size());
            for (std::size_t row = 0; row < derivative.size(); ++row) {
                derivative[row] = (plus_value[row] - minus_value[row]) / (2.0 * step);
            }
        }
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t state_index = 0;
                 state_index < model.states.size(); ++state_index) {
                jacobian[row][column] += constraint_gradient[row][state_index] *
                    derivative[state_index];
            }
        }
    }
    return jacobian;
}

bool project_initial_state(
    const IndexTwoDaeIR& model,
    const std::vector<std::vector<double>>& gradient,
    std::vector<double>& state,
    double tolerance) {
    const std::vector<double> multipliers(model.multipliers.size());
    const auto residual = evaluate_constraints(model, state, multipliers, 0.0);
    if (infinity_norm(residual) <= tolerance) return true;
    std::vector<std::vector<double>> gram(
        gradient.size(), std::vector<double>(gradient.size()));
    for (std::size_t row = 0; row < gradient.size(); ++row) {
        for (std::size_t column = 0; column < gradient.size(); ++column) {
            for (std::size_t state_index = 0;
                 state_index < model.states.size(); ++state_index) {
                gram[row][column] += gradient[row][state_index] *
                    gradient[column][state_index];
            }
        }
    }
    std::vector<double> right(residual.size());
    for (std::size_t index = 0; index < residual.size(); ++index) {
        right[index] = -residual[index];
    }
    std::vector<double> correction;
    if (!solve_dense(gram, right, correction)) return false;
    for (std::size_t state_index = 0; state_index < state.size(); ++state_index) {
        for (std::size_t row = 0; row < gradient.size(); ++row) {
            state[state_index] += gradient[row][state_index] * correction[row];
        }
    }
    return infinity_norm(evaluate_constraints(model, state, multipliers, 0.0)) <= tolerance;
}

bool initialize_multipliers(
    const IndexTwoDaeIR& model,
    const std::vector<std::vector<double>>& gradient,
    const std::vector<double>& state,
    std::vector<double>& multipliers,
    const IndexTwoDaeTolerance& tolerance,
    bool use_ad,
    int& iterations,
    double& rank_margin) {
    for (int iteration = 0; iteration <= tolerance.maximum_newton_iterations; ++iteration) {
        const auto residual = hidden_residual(
            model, gradient, state, multipliers, 0.0);
        if (infinity_norm(residual) <= tolerance.absolute) {
            iterations = iteration;
            const auto jacobian = hidden_jacobian(
                model, gradient, state, multipliers, 0.0, use_ad);
            std::vector<double> probe;
            if (jacobian.empty() || !solve_dense(
                    jacobian, std::vector<double>(jacobian.size(), 1.0),
                    probe, &rank_margin) || rank_margin < tolerance.hidden_rank) {
                return false;
            }
            return true;
        }
        const auto jacobian = hidden_jacobian(
            model, gradient, state, multipliers, 0.0, use_ad);
        if (jacobian.empty()) return false;
        std::vector<double> right(residual.size());
        for (std::size_t index = 0; index < residual.size(); ++index) {
            right[index] = -residual[index];
        }
        std::vector<double> correction;
        if (!solve_dense(jacobian, right, correction, &rank_margin) ||
            rank_margin < tolerance.hidden_rank) return false;
        for (std::size_t index = 0; index < multipliers.size(); ++index) {
            multipliers[index] += correction[index];
        }
    }
    return false;
}

struct StepSolve {
    bool converged{false};
    std::vector<double> state;
    std::vector<double> multipliers;
    int iterations{};
    double dynamic_residual{};
    double constraint_residual{};
    double hidden_residual{};
    double rank_margin{};
};

std::vector<double> step_residual(
    const IndexTwoDaeIR& model,
    const std::vector<double>& previous,
    const std::vector<double>& state,
    const std::vector<double>& multipliers,
    double time,
    double step) {
    const auto dynamics = evaluate_dynamics(model, state, multipliers, time);
    auto result = evaluate_constraints(model, state, multipliers, time);
    result.insert(result.begin(), state.size(), 0.0);
    for (std::size_t index = 0; index < state.size(); ++index) {
        result[index] = state[index] - previous[index] - step * dynamics[index];
    }
    return result;
}

StepSolve solve_step(
    const IndexTwoDaeIR& model,
    const std::vector<std::vector<double>>& gradient,
    const std::vector<double>& previous,
    const std::vector<double>& initial_multipliers,
    double time,
    double step,
    const IndexTwoDaeTolerance& tolerance,
    bool use_ad) {
    StepSolve result;
    result.state = previous;
    result.multipliers = initial_multipliers;
    const auto unknowns = model.states.size() + model.multipliers.size();
    for (int iteration = 0; iteration <= tolerance.maximum_newton_iterations; ++iteration) {
        const auto residual = step_residual(
            model, previous, result.state, result.multipliers, time, step);
        const double threshold = tolerance.absolute + tolerance.relative *
            std::max(1.0, infinity_norm(result.state));
        if (infinity_norm(residual) <= threshold) {
            result.iterations = iteration;
            result.dynamic_residual = infinity_norm(std::vector<double>(
                residual.begin(), residual.begin() +
                    static_cast<std::ptrdiff_t>(model.states.size())));
            result.constraint_residual = infinity_norm(std::vector<double>(
                residual.begin() + static_cast<std::ptrdiff_t>(model.states.size()),
                residual.end()));
            result.hidden_residual = infinity_norm(hidden_residual(
                model, gradient, result.state, result.multipliers, time));
            const auto hidden = hidden_jacobian(
                model, gradient, result.state, result.multipliers, time, use_ad);
            std::vector<double> probe;
            if (hidden.empty() || !solve_dense(
                    hidden, std::vector<double>(hidden.size(), 1.0), probe,
                    &result.rank_margin) ||
                result.rank_margin < tolerance.hidden_rank ||
                result.hidden_residual > threshold) {
                return result;
            }
            result.converged = true;
            return result;
        }
        std::vector<std::vector<double>> jacobian(
            unknowns, std::vector<double>(unknowns));
        if (use_ad) {
            for (std::size_t column = 0; column < model.states.size(); ++column) {
                const auto derivative = directional_dynamics(
                    model, result.state, result.multipliers, time, column, false);
                if (!derivative.has_value()) return result;
                for (std::size_t row = 0; row < model.states.size(); ++row) {
                    jacobian[row][column] =
                        (row == column ? 1.0 : 0.0) - step * (*derivative)[row];
                }
                for (std::size_t row = 0; row < gradient.size(); ++row) {
                    jacobian[model.states.size() + row][column] =
                        gradient[row][column];
                }
            }
            for (std::size_t column = 0; column < model.multipliers.size(); ++column) {
                const auto derivative = directional_dynamics(
                    model, result.state, result.multipliers, time, column, true);
                if (!derivative.has_value()) return result;
                for (std::size_t row = 0; row < model.states.size(); ++row) {
                    jacobian[row][model.states.size() + column] =
                        -step * (*derivative)[row];
                }
            }
        } else {
            std::vector<double> candidate = result.state;
            candidate.insert(candidate.end(), result.multipliers.begin(),
                             result.multipliers.end());
            for (std::size_t column = 0; column < unknowns; ++column) {
                const double delta = 1.0e-7 * std::max(1.0, std::abs(candidate[column]));
                auto plus = candidate;
                auto minus = candidate;
                plus[column] += delta;
                minus[column] -= delta;
                const std::vector<double> plus_state(
                    plus.begin(), plus.begin() +
                        static_cast<std::ptrdiff_t>(model.states.size()));
                const std::vector<double> minus_state(
                    minus.begin(), minus.begin() +
                        static_cast<std::ptrdiff_t>(model.states.size()));
                const std::vector<double> plus_multipliers(
                    plus.begin() + static_cast<std::ptrdiff_t>(model.states.size()),
                    plus.end());
                const std::vector<double> minus_multipliers(
                    minus.begin() + static_cast<std::ptrdiff_t>(model.states.size()),
                    minus.end());
                const auto plus_residual = step_residual(
                    model, previous, plus_state, plus_multipliers, time, step);
                const auto minus_residual = step_residual(
                    model, previous, minus_state, minus_multipliers, time, step);
                for (std::size_t row = 0; row < unknowns; ++row) {
                    jacobian[row][column] =
                        (plus_residual[row] - minus_residual[row]) / (2.0 * delta);
                }
            }
        }
        std::vector<double> right(residual.size());
        for (std::size_t index = 0; index < residual.size(); ++index) {
            right[index] = -residual[index];
        }
        std::vector<double> correction;
        if (!solve_dense(jacobian, right, correction)) return result;
        bool accepted{};
        const double base = infinity_norm(residual);
        for (int line_search = 0; line_search < 16; ++line_search) {
            const double factor = std::ldexp(1.0, -line_search);
            auto candidate_state = result.state;
            auto candidate_multipliers = result.multipliers;
            for (std::size_t index = 0; index < candidate_state.size(); ++index) {
                candidate_state[index] += factor * correction[index];
            }
            for (std::size_t index = 0; index < candidate_multipliers.size(); ++index) {
                candidate_multipliers[index] +=
                    factor * correction[model.states.size() + index];
            }
            if (infinity_norm(step_residual(
                    model, previous, candidate_state, candidate_multipliers,
                    time, step)) < base) {
                result.state = std::move(candidate_state);
                result.multipliers = std::move(candidate_multipliers);
                accepted = true;
                break;
            }
        }
        if (!accepted) return result;
    }
    return result;
}

}  // namespace

void IndexTwoDaeIR::validate() const {
    if (schema_version != kIndexTwoDaeSchemaVersion || model_id.empty() ||
        source_hash.empty() || states.empty() || multipliers.empty() ||
        dynamics.size() != states.size() || constraints.size() != multipliers.size()) {
        throw std::invalid_argument("invalid index-2 DAE IR dimensions or identity");
    }
    std::set<std::string> names;
    for (const auto& variable : states) {
        if (variable.name.empty() || !std::isfinite(variable.start) ||
            !names.insert(variable.name).second) {
            throw std::invalid_argument("invalid index-2 state");
        }
    }
    for (const auto& variable : multipliers) {
        if (variable.name.empty() || !std::isfinite(variable.start) ||
            !names.insert(variable.name).second) {
            throw std::invalid_argument("invalid index-2 multiplier");
        }
    }
    const auto gradient = constraint_jacobian(*this);
    for (const auto& row : gradient) {
        if (std::none_of(row.begin(), row.end(), [](double value) {
                return value != 0.0;
            })) {
            throw std::invalid_argument("index-2 constraint has zero state gradient");
        }
    }
}

void IndexTwoDaeIR::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write index-2 DAE IR");
    output << schema_version << "\nMODEL " << std::quoted(model_id)
           << "\nSOURCE_HASH " << std::quoted(source_hash)
           << "\nSTRUCTURAL_CLASS " << std::quoted(structural_class)
           << "\nSTATES " << states.size() << '\n';
    output << std::setprecision(17);
    for (std::size_t index = 0; index < states.size(); ++index) {
        output << "STATE " << std::quoted(states[index].name) << ' '
               << states[index].start << ' ' << states[index].nominal << ' '
               << std::quoted(dynamics[index]) << '\n';
    }
    output << "MULTIPLIERS " << multipliers.size() << '\n';
    for (const auto& variable : multipliers) {
        output << "MULTIPLIER " << std::quoted(variable.name) << ' '
               << variable.start << ' ' << variable.nominal << '\n';
    }
    output << "CONSTRAINTS " << constraints.size() << '\n';
    for (const auto& constraint : constraints) {
        output << "CONSTRAINT " << std::quoted(constraint.id) << ' '
               << std::quoted(constraint.residual) << '\n';
    }
    output << "END\n";
}

IndexTwoDaeIR IndexTwoDaeIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read index-2 DAE IR");
    IndexTwoDaeIR model;
    input >> model.schema_version;
    require_tag(input, "MODEL"); input >> std::quoted(model.model_id);
    require_tag(input, "SOURCE_HASH"); input >> std::quoted(model.source_hash);
    require_tag(input, "STRUCTURAL_CLASS"); input >> std::quoted(model.structural_class);
    std::size_t count{};
    require_tag(input, "STATES"); input >> count;
    model.states.resize(count);
    model.dynamics.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        require_tag(input, "STATE");
        input >> std::quoted(model.states[index].name) >> model.states[index].start
              >> model.states[index].nominal >> std::quoted(model.dynamics[index]);
    }
    require_tag(input, "MULTIPLIERS"); input >> count;
    model.multipliers.resize(count);
    for (auto& variable : model.multipliers) {
        require_tag(input, "MULTIPLIER");
        input >> std::quoted(variable.name) >> variable.start >> variable.nominal;
    }
    require_tag(input, "CONSTRAINTS"); input >> count;
    model.constraints.resize(count);
    for (auto& constraint : model.constraints) {
        require_tag(input, "CONSTRAINT");
        input >> std::quoted(constraint.id) >> std::quoted(constraint.residual);
        const Expression expression(constraint.residual);
        constraint.variables.assign(expression.names().begin(), expression.names().end());
    }
    require_tag(input, "END");
    model.validate();
    return model;
}

IndexTwoDaeIR compile_index_two_dae(
    const std::filesystem::path& source,
    const std::string& top) {
    const auto raw = read_file(source);
    auto text = strip_comments(raw);
    std::smatch model_match;
    if (!std::regex_search(text, model_match,
            std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) {
        throw std::invalid_argument("index-2 DAE source has no model declaration");
    }
    IndexTwoDaeIR model;
    model.model_id = model_match[1].str();
    model.source_hash = digest(raw);
    if (!top.empty() && top != model.model_id) {
        throw std::invalid_argument("requested index-2 DAE top does not match source");
    }
    text = text.substr(model_match.position() + model_match.length());
    text = std::regex_replace(
        text, std::regex("end\\s+" + model.model_id + R"(\s*;\s*$)"), "");
    const auto equation_position = text.find("equation");
    if (equation_position == std::string::npos) {
        throw std::invalid_argument("index-2 DAE model has no equation section");
    }
    const std::regex declaration(
        R"(^\s*Real\s+([A-Za-z_]\w*)\s*(?:\(\s*start\s*=\s*([^\)]+)\s*\))?\s*$)");
    std::vector<DaeVariableIR> declared;
    for (const auto& statement : split(text.substr(0, equation_position))) {
        std::smatch match;
        if (!std::regex_match(statement, match, declaration)) {
            throw std::invalid_argument("unsupported index-2 declaration: " + statement);
        }
        declared.push_back({
            match[1].str(), match[2].matched ? std::stod(trim(match[2].str())) : 0.0,
            1.0});
    }
    std::unordered_map<std::string, std::string> dynamics;
    std::vector<DaeEquationIR> constraints;
    const std::regex derivative(
        R"(^\s*der\s*\(\s*([A-Za-z_]\w*)\s*\)\s*=\s*(.+)\s*$)");
    for (const auto& statement : split(text.substr(equation_position + 8))) {
        std::smatch match;
        if (std::regex_match(statement, match, derivative)) {
            if (!dynamics.emplace(match[1].str(), trim(match[2].str())).second) {
                throw std::invalid_argument("duplicate index-2 state derivative");
            }
            continue;
        }
        const auto equal = statement.find('=');
        if (equal == std::string::npos) {
            throw std::invalid_argument("invalid index-2 constraint: " + statement);
        }
        DaeEquationIR equation;
        equation.id = "constraint-" + std::to_string(constraints.size() + 1);
        equation.residual = "(" + trim(statement.substr(0, equal)) + ")-(" +
            trim(statement.substr(equal + 1)) + ")";
        const Expression expression(equation.residual);
        equation.variables.assign(expression.names().begin(), expression.names().end());
        constraints.push_back(std::move(equation));
    }
    std::set<std::string> declared_names;
    for (const auto& variable : declared) declared_names.insert(variable.name);
    for (const auto& [name, _] : dynamics) {
        if (!declared_names.contains(name)) {
            throw std::invalid_argument("derivative references undeclared state");
        }
    }
    for (const auto& variable : declared) {
        const auto dynamic = dynamics.find(variable.name);
        if (dynamic != dynamics.end()) {
            model.states.push_back(variable);
            model.dynamics.push_back(dynamic->second);
        } else {
            model.multipliers.push_back(variable);
        }
    }
    model.constraints = std::move(constraints);
    if (model.constraints.size() != model.multipliers.size()) {
        throw std::invalid_argument(
            "index-2 DAE requires one affine constraint per multiplier");
    }
    std::set<std::string> state_names;
    std::set<std::string> multiplier_names;
    for (const auto& state : model.states) state_names.insert(state.name);
    for (const auto& multiplier : model.multipliers) {
        multiplier_names.insert(multiplier.name);
    }
    for (const auto& constraint : model.constraints) {
        for (const auto& name : constraint.variables) {
            if (!state_names.contains(name)) {
                throw std::invalid_argument(
                    "index-2 affine constraints may reference only states");
            }
        }
    }
    for (const auto& source_expression : model.dynamics) {
        const Expression expression(source_expression);
        for (const auto& name : expression.names()) {
            if (name != "time" && !state_names.contains(name) &&
                !multiplier_names.contains(name)) {
                throw std::invalid_argument("index-2 dynamics reference an unknown name");
            }
        }
    }
    model.validate();
    const auto gradient = constraint_jacobian(model);
    std::vector<double> state;
    std::vector<double> multipliers;
    for (const auto& variable : model.states) state.push_back(variable.start);
    for (const auto& variable : model.multipliers) multipliers.push_back(variable.start);
    if (!project_initial_state(model, gradient, state, 1.0e-10)) {
        throw std::invalid_argument("index-2 initial constraint projection failed");
    }
    const auto hidden = hidden_jacobian(
        model, gradient, state, multipliers, 0.0, true);
    std::vector<double> probe;
    double margin{};
    if (hidden.empty() || !solve_dense(
            hidden, std::vector<double>(hidden.size(), 1.0), probe, &margin) ||
        margin < 1.0e-10) {
        throw std::invalid_argument(
            "index-2 hidden Jacobian g_x*f_lambda is rank deficient");
    }
    return model;
}

IndexTwoDaeResult simulate_index_two_dae(
    const IndexTwoDaeIR& model,
    double end_time,
    double maximum_step,
    const IndexTwoDaeTolerance& tolerance) {
    model.validate();
    if (!(end_time >= 0.0) || !(maximum_step > 0.0) ||
        !(tolerance.absolute > 0.0) || !(tolerance.relative > 0.0) ||
        !(tolerance.hidden_rank > 0.0) ||
        tolerance.maximum_newton_iterations <= 0) {
        throw std::invalid_argument("invalid index-2 simulation controls");
    }
    IndexTwoDaeResult result;
    result.plan_id = route_index_two_dae(model).plan_id;
    const auto gradient = constraint_jacobian(model);
    std::vector<double> state;
    std::vector<double> multipliers;
    for (const auto& variable : model.states) state.push_back(variable.start);
    for (const auto& variable : model.multipliers) multipliers.push_back(variable.start);
    const double gate = tolerance.absolute + tolerance.relative;
    if (!project_initial_state(model, gradient, state, gate)) {
        result.reason = "index-2 initial constraint projection failed";
        return result;
    }
    double rank_margin{};
    if (!initialize_multipliers(
            model, gradient, state, multipliers, tolerance, true,
            result.initialization_iterations, rank_margin)) {
        int fallback_iterations{};
        if (!initialize_multipliers(
                model, gradient, state, multipliers, tolerance, false,
                fallback_iterations, rank_margin)) {
            result.reason = "index-2 hidden-constraint initialization failed";
            return result;
        }
        result.terminal_fallback_used = true;
        result.initialization_iterations = fallback_iterations;
    }
    ++result.hidden_rank_checks;
    result.minimum_hidden_rank_margin = rank_margin;
    result.initialization_constraint_residual_inf = infinity_norm(
        evaluate_constraints(model, state, multipliers, 0.0));
    result.initialization_hidden_residual_inf = infinity_norm(
        hidden_residual(model, gradient, state, multipliers, 0.0));
    for (std::size_t index = 0; index < state.size(); ++index) {
        result.initial_state[model.states[index].name] = state[index];
    }
    for (std::size_t index = 0; index < multipliers.size(); ++index) {
        result.initial_multipliers[model.multipliers[index].name] = multipliers[index];
    }
    double time{};
    std::size_t boundary{1};
    while (time < end_time) {
        const double target = std::min(
            end_time, static_cast<double>(boundary) * maximum_step);
        const double step = target - time;
        if (!(step > 0.0)) {
            ++boundary;
            continue;
        }
        const auto previous = state;
        auto solved = solve_step(
            model, gradient, previous, multipliers, target, step, tolerance, true);
        std::string backend = "index2-differentiated-constraint-newton-cpu-v1";
        if (!solved.converged) {
            ++result.rejected_steps;
            solved = solve_step(
                model, gradient, previous, multipliers, target, step, tolerance, false);
            backend = "index2-dense-kkt-terminal-cpu-v1";
            result.terminal_fallback_used = true;
        }
        if (!solved.converged) {
            result.reason = "index-2 implicit step failed both original-state candidates";
            return result;
        }
        state = solved.state;
        multipliers = solved.multipliers;
        time = target;
        ++boundary;
        ++result.hidden_rank_checks;
        result.minimum_hidden_rank_margin = std::min(
            result.minimum_hidden_rank_margin, solved.rank_margin);
        result.steps.push_back({
            time, step, backend, solved.iterations, solved.dynamic_residual,
            solved.constraint_residual, solved.hidden_residual,
            solved.rank_margin});
        result.solver_backend = backend;
    }
    for (std::size_t index = 0; index < state.size(); ++index) {
        result.final_state[model.states[index].name] = state[index];
    }
    for (std::size_t index = 0; index < multipliers.size(); ++index) {
        result.final_multipliers[model.multipliers[index].name] = multipliers[index];
    }
    result.success = true;
    result.reason =
        "original dynamics, affine constraints, differentiated hidden constraints, and rank gates passed";
    return result;
}

void write_index_two_dae_report(
    const IndexTwoDaeIR& model,
    const IndexTwoDaeResult& result,
    const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write index-2 DAE report");
    output << std::setprecision(17)
           << "SMAVE_INDEX2_DAE_REPORT 1\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "PLAN " << std::quoted(result.plan_id) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "SOLVER_BACKEND " << std::quoted(result.solver_backend) << '\n'
           << "TERMINAL_FALLBACK " << result.terminal_fallback_used << '\n'
           << "INITIALIZATION_ITERATIONS " << result.initialization_iterations << '\n'
           << "INITIAL_CONSTRAINT_RESIDUAL "
           << result.initialization_constraint_residual_inf << '\n'
           << "INITIAL_HIDDEN_RESIDUAL "
           << result.initialization_hidden_residual_inf << '\n'
           << "HIDDEN_RANK_CHECKS " << result.hidden_rank_checks << '\n'
           << "MINIMUM_HIDDEN_RANK_MARGIN "
           << result.minimum_hidden_rank_margin << '\n';
    for (const auto& step : result.steps) {
        output << "STEP " << step.time << ' ' << step.step
               << " BACKEND " << std::quoted(step.solver_backend)
               << " NEWTON " << step.newton_iterations
               << " DYNAMIC " << step.dynamic_residual_inf
               << " CONSTRAINT " << step.constraint_residual_inf
               << " HIDDEN " << step.hidden_residual_inf
               << " RANK " << step.hidden_rank_margin << '\n';
    }
    for (const auto& [name, value] : result.final_state) {
        output << "STATE " << std::quoted(name) << ' ' << value << '\n';
    }
    for (const auto& [name, value] : result.final_multipliers) {
        output << "MULTIPLIER " << std::quoted(name) << ' ' << value << '\n';
    }
    output << "REASON " << std::quoted(result.reason) << '\n';
}

}  // namespace smave
