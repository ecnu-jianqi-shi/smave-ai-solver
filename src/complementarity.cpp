#include "smave/complementarity.hpp"

#include "smave/expression.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
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
    if (!input) throw std::runtime_error("cannot read complementarity source: " + path.string());
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
        throw std::runtime_error("invalid complementarity IR: expected " + expected);
    }
}

std::vector<double> multiply(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector,
    const std::vector<double>& offset) {
    std::vector<double> result = offset;
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        for (std::size_t column = 0; column < vector.size(); ++column) {
            result[row] += matrix[row][column] * vector[column];
        }
    }
    return result;
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) result = std::max(result, std::abs(value));
    return result;
}

bool positive_definite_symmetric_part(
    const std::vector<std::vector<double>>& matrix) {
    const auto size = matrix.size();
    std::vector<std::vector<double>> lower(size, std::vector<double>(size));
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double value = 0.5 * (matrix[row][column] + matrix[column][row]);
            for (std::size_t inner = 0; inner < column; ++inner) {
                value -= lower[row][inner] * lower[column][inner];
            }
            if (row == column) {
                const double scale = std::max(1.0, std::abs(matrix[row][row]));
                if (!(value > 1.0e-12 * scale) || !std::isfinite(value)) return false;
                lower[row][column] = std::sqrt(value);
            } else {
                lower[row][column] = value / lower[column][column];
            }
        }
    }
    return true;
}

ComplementarityAttempt gate_candidate(
    const ComplementarityIR& model,
    const std::string& backend,
    std::vector<double> solution,
    int iterations,
    const ComplementarityTolerance& tolerance,
    std::vector<double>* accepted_gap = nullptr) {
    ComplementarityAttempt attempt;
    attempt.backend = backend;
    attempt.iterations = iterations;
    if (solution.size() != model.variables.size() ||
        !std::all_of(solution.begin(), solution.end(), [](double value) {
            return std::isfinite(value);
        })) {
        attempt.outcome = "rejected";
        attempt.reason = "candidate is missing or non-finite";
        return attempt;
    }
    const auto gap = multiply(model.matrix, solution, model.offset);
    if (!std::all_of(gap.begin(), gap.end(), [](double value) {
            return std::isfinite(value);
        })) {
        attempt.outcome = "rejected";
        attempt.reason = "original gap equations produced NaN/Inf";
        return attempt;
    }
    for (std::size_t index = 0; index < solution.size(); ++index) {
        attempt.primal_violation = std::max(attempt.primal_violation, -solution[index]);
        attempt.dual_violation = std::max(attempt.dual_violation, -gap[index]);
        attempt.complementarity_violation = std::max(
            attempt.complementarity_violation, std::abs(solution[index] * gap[index]));
    }
    std::unordered_map<std::string, double> values;
    for (std::size_t index = 0; index < solution.size(); ++index) {
        values[model.variables[index].name] = solution[index];
    }
    for (std::size_t index = 0; index < solution.size(); ++index) {
        const double original = Expression(model.gap_expressions[index]).evaluate(values);
        attempt.equation_residual_inf = std::max(
            attempt.equation_residual_inf, std::abs(original - gap[index]));
    }
    const double scale = std::max({1.0, infinity_norm(solution), infinity_norm(gap)});
    const double threshold = tolerance.absolute + tolerance.relative * scale;
    if (attempt.primal_violation > threshold || attempt.dual_violation > threshold ||
        attempt.complementarity_violation > threshold ||
        attempt.equation_residual_inf > threshold) {
        attempt.outcome = "rejected";
        attempt.reason = "original complementarity residual or inequality gate failed";
        return attempt;
    }
    attempt.outcome = "accepted";
    attempt.reason = "original gap equations, inequalities, and complementarity gate passed";
    if (accepted_gap != nullptr) *accepted_gap = gap;
    return attempt;
}

std::pair<std::vector<double>, int> projected_gauss_seidel(
    const ComplementarityIR& model,
    const ComplementarityTolerance& tolerance) {
    std::vector<double> solution(model.variables.size());
    for (std::size_t index = 0; index < solution.size(); ++index) {
        solution[index] = std::max(0.0, model.variables[index].start);
    }
    for (int iteration = 1; iteration <= tolerance.maximum_pgs_iterations; ++iteration) {
        if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {{}, iteration};
        double maximum_change{};
        for (std::size_t row = 0; row < solution.size(); ++row) {
            if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {{}, iteration};
            double gap = model.offset[row];
            for (std::size_t column = 0; column < solution.size(); ++column) {
                gap += model.matrix[row][column] * solution[column];
            }
            const double updated = std::max(
                0.0, solution[row] - gap / model.matrix[row][row]);
            maximum_change = std::max(maximum_change, std::abs(updated - solution[row]));
            solution[row] = updated;
        }
        if (maximum_change <= tolerance.absolute +
                tolerance.relative * std::max(1.0, infinity_norm(solution))) {
            return {solution, iteration};
        }
    }
    return {solution, tolerance.maximum_pgs_iterations};
}

std::pair<std::vector<double>, int> fischer_burmeister_newton(
    const ComplementarityIR& model,
    const ComplementarityTolerance& tolerance) {
    const auto size = model.variables.size();
    std::vector<double> solution(size);
    for (std::size_t index = 0; index < size; ++index) {
        solution[index] = std::max(0.0, model.variables[index].start);
    }
    for (int iteration = 1; iteration <= tolerance.maximum_newton_iterations; ++iteration) {
        if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {{}, iteration};
        const auto gap = multiply(model.matrix, solution, model.offset);
        std::vector<double> residual(size);
        std::vector<std::vector<double>> jacobian(size, std::vector<double>(size));
        for (std::size_t row = 0; row < size; ++row) {
            const double radius = std::hypot(solution[row], gap[row]);
            const double inverse = radius > 1.0e-14 ? 1.0 / radius : 0.0;
            const double dz = radius > 1.0e-14
                ? solution[row] * inverse - 1.0
                : -std::sqrt(0.5);
            const double dw = radius > 1.0e-14
                ? gap[row] * inverse - 1.0
                : -std::sqrt(0.5);
            residual[row] = radius - solution[row] - gap[row];
            for (std::size_t column = 0; column < size; ++column) {
                jacobian[row][column] = dw * model.matrix[row][column];
            }
            jacobian[row][row] += dz;
        }
        if (infinity_norm(residual) <= tolerance.absolute) return {solution, iteration - 1};
        LinearSystem system;
        system.matrix = std::move(jacobian);
        system.right_hand_side.resize(size);
        for (std::size_t index = 0; index < size; ++index) {
            system.right_hand_side[index] = -residual[index];
        }
        std::vector<double> correction;
        if (!dense_direct_solve(system, correction)) return {{}, iteration};
        bool accepted{};
        for (int line_search = 0; line_search < 16; ++line_search) {
            if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {{}, iteration};
            const double factor = std::ldexp(1.0, -line_search);
            auto candidate = solution;
            for (std::size_t index = 0; index < size; ++index) {
                candidate[index] += factor * correction[index];
            }
            const auto candidate_gap = multiply(model.matrix, candidate, model.offset);
            std::vector<double> candidate_residual(size);
            for (std::size_t index = 0; index < size; ++index) {
                candidate_residual[index] = std::hypot(candidate[index], candidate_gap[index]) -
                    candidate[index] - candidate_gap[index];
            }
            if (infinity_norm(candidate_residual) < infinity_norm(residual)) {
                solution = std::move(candidate);
                accepted = true;
                break;
            }
        }
        if (!accepted) return {{}, iteration};
    }
    return {solution, tolerance.maximum_newton_iterations};
}

std::vector<double> active_set_enumeration(
    const ComplementarityIR& model,
    const ComplementarityTolerance& tolerance) {
    const auto size = model.variables.size();
    if (size > 20) return {};
    const std::uint64_t count = std::uint64_t{1} << size;
    for (std::uint64_t mask = 0; mask < count; ++mask) {
        if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {};
        std::vector<std::size_t> free;
        for (std::size_t index = 0; index < size; ++index) {
            if (tolerance.cancellation_requested && tolerance.cancellation_requested()) return {};
            if ((mask & (std::uint64_t{1} << index)) != 0) free.push_back(index);
        }
        std::vector<double> solution(size);
        if (!free.empty()) {
            LinearSystem system;
            system.matrix.assign(free.size(), std::vector<double>(free.size()));
            system.right_hand_side.resize(free.size());
            for (std::size_t row = 0; row < free.size(); ++row) {
                system.right_hand_side[row] = -model.offset[free[row]];
                for (std::size_t column = 0; column < free.size(); ++column) {
                    system.matrix[row][column] = model.matrix[free[row]][free[column]];
                }
            }
            std::vector<double> reduced;
            if (!dense_direct_solve(system, reduced)) continue;
            for (std::size_t index = 0; index < free.size(); ++index) {
                solution[free[index]] = reduced[index];
            }
        }
        const auto gap = multiply(model.matrix, solution, model.offset);
        bool valid = true;
        for (std::size_t index = 0; index < size; ++index) {
            if (solution[index] < -1.0e-12 || gap[index] < -1.0e-12 ||
                std::abs(solution[index] * gap[index]) > 1.0e-10) {
                valid = false;
                break;
            }
        }
        if (valid) return solution;
    }
    return {};
}

}  // namespace

void ComplementarityIR::validate() const {
    if (schema_version != kComplementaritySchemaVersion || model_id.empty() ||
        source_hash.empty() || variables.empty() ||
        gap_expressions.size() != variables.size() || matrix.size() != variables.size() ||
        offset.size() != variables.size()) {
        throw std::invalid_argument("invalid complementarity IR dimensions or identity");
    }
    for (const auto& row : matrix) {
        if (row.size() != variables.size() ||
            !std::all_of(row.begin(), row.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("invalid complementarity matrix");
        }
    }
    if (!std::all_of(offset.begin(), offset.end(), [](double value) {
            return std::isfinite(value);
        })) {
        throw std::invalid_argument("invalid complementarity offset");
    }
    if (!positive_definite_symmetric_part(matrix)) {
        throw std::invalid_argument(
            "complementarity subset requires a positive-definite symmetric part");
    }
    for (std::size_t index = 0; index < variables.size(); ++index) {
        if (variables[index].name.empty() || !std::isfinite(variables[index].start) ||
            matrix[index][index] <= 0.0) {
            throw std::invalid_argument("invalid complementarity variable or diagonal");
        }
    }
}

void ComplementarityIR::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write complementarity IR");
    output << schema_version << "\nMODEL " << std::quoted(model_id)
           << "\nSOURCE_HASH " << std::quoted(source_hash)
           << "\nSTRUCTURAL_CLASS " << std::quoted(structural_class)
           << "\nSIZE " << variables.size() << '\n';
    output << std::setprecision(17);
    for (std::size_t index = 0; index < variables.size(); ++index) {
        output << "PAIR " << std::quoted(variables[index].name) << ' '
               << variables[index].start << ' ' << std::quoted(gap_expressions[index]);
        for (const auto value : matrix[index]) output << ' ' << value;
        output << ' ' << offset[index] << '\n';
    }
    output << "END\n";
}

ComplementarityIR ComplementarityIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read complementarity IR");
    ComplementarityIR model;
    input >> model.schema_version;
    require_tag(input, "MODEL"); input >> std::quoted(model.model_id);
    require_tag(input, "SOURCE_HASH"); input >> std::quoted(model.source_hash);
    require_tag(input, "STRUCTURAL_CLASS"); input >> std::quoted(model.structural_class);
    std::size_t size{};
    require_tag(input, "SIZE"); input >> size;
    model.variables.resize(size);
    model.gap_expressions.resize(size);
    model.matrix.assign(size, std::vector<double>(size));
    model.offset.resize(size);
    for (std::size_t row = 0; row < size; ++row) {
        require_tag(input, "PAIR");
        input >> std::quoted(model.variables[row].name) >> model.variables[row].start
              >> std::quoted(model.gap_expressions[row]);
        for (auto& value : model.matrix[row]) input >> value;
        input >> model.offset[row];
    }
    require_tag(input, "END");
    model.validate();
    return model;
}

ComplementarityIR compile_complementarity(
    const std::filesystem::path& source,
    const std::string& top) {
    const auto raw = read_file(source);
    auto text = strip_comments(raw);
    std::smatch model_match;
    if (!std::regex_search(text, model_match,
            std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) {
        throw std::invalid_argument("complementarity source has no model declaration");
    }
    ComplementarityIR model;
    model.model_id = model_match[1].str();
    model.source_hash = digest(raw);
    if (!top.empty() && top != model.model_id) {
        throw std::invalid_argument("requested complementarity top model does not match source");
    }
    text = text.substr(model_match.position() + model_match.length());
    text = std::regex_replace(
        text, std::regex("end\\s+" + model.model_id + R"(\s*;\s*$)"), "");
    const auto equation_position = text.find("equation");
    if (equation_position == std::string::npos) {
        throw std::invalid_argument("complementarity model has no equation section");
    }
    const std::regex declaration(
        R"(^\s*Real\s+([A-Za-z_]\w*)\s*(?:\(\s*start\s*=\s*([^\)]+)\s*\))?\s*$)");
    for (const auto& statement : split(text.substr(0, equation_position))) {
        std::smatch match;
        if (!std::regex_match(statement, match, declaration)) {
            throw std::invalid_argument("unsupported complementarity declaration: " + statement);
        }
        model.variables.push_back({
            match[1].str(), match[2].matched ? std::stod(trim(match[2].str())) : 0.0});
    }
    std::unordered_map<std::string, std::size_t> variable_index;
    std::vector<std::string> variable_names;
    for (std::size_t index = 0; index < model.variables.size(); ++index) {
        if (!variable_index.emplace(model.variables[index].name, index).second) {
            throw std::invalid_argument("duplicate complementarity variable");
        }
        variable_names.push_back(model.variables[index].name);
    }
    model.gap_expressions.assign(model.variables.size(), {});
    const std::regex pair_expression(
        R"(^\s*complementarity\s*\(\s*([A-Za-z_]\w*)\s*,\s*(.+)\s*\)\s*$)");
    for (const auto& statement : split(text.substr(equation_position + 8))) {
        std::smatch match;
        if (!std::regex_match(statement, match, pair_expression)) {
            throw std::invalid_argument("unsupported complementarity equation: " + statement);
        }
        const auto variable = variable_index.find(match[1].str());
        if (variable == variable_index.end() ||
            !model.gap_expressions[variable->second].empty()) {
            throw std::invalid_argument("unknown or duplicate complementarity variable");
        }
        model.gap_expressions[variable->second] = trim(match[2].str());
    }
    model.matrix.assign(model.variables.size(),
                        std::vector<double>(model.variables.size()));
    model.offset.resize(model.variables.size());
    std::unordered_map<std::string, double> zero;
    for (const auto& variable : model.variables) zero[variable.name] = 0.0;
    for (std::size_t row = 0; row < model.variables.size(); ++row) {
        if (model.gap_expressions[row].empty()) {
            throw std::invalid_argument("every variable requires one complementarity pair");
        }
        const Expression expression(model.gap_expressions[row]);
        if (expression.names().size() > variable_names.size() ||
            !std::all_of(expression.names().begin(), expression.names().end(),
                [&](const std::string& name) { return variable_index.contains(name); })) {
            throw std::invalid_argument("complementarity gap references an unknown name");
        }
        const auto coefficients = expression.constant_linear_coefficients(variable_names);
        if (!coefficients.has_value()) {
            throw std::invalid_argument("complementarity gap must be constant affine");
        }
        model.matrix[row] = *coefficients;
        model.offset[row] = expression.evaluate(zero);
    }
    model.validate();
    return model;
}

ComplementarityResult solve_complementarity(
    const ComplementarityIR& model,
    const ComplementarityTolerance& tolerance) {
    model.validate();
    if (!(tolerance.absolute > 0.0) || !(tolerance.relative > 0.0) ||
        tolerance.maximum_pgs_iterations < 0 || tolerance.maximum_newton_iterations < 0) {
        throw std::invalid_argument("invalid complementarity tolerance or iteration budget");
    }
    ComplementarityResult result;
    result.plan_id = route_complementarity(model).plan_id;
    auto accept = [&](const std::string& backend,
                      const std::vector<double>& candidate,
                      int iterations) {
        std::vector<double> gap;
        auto attempt = gate_candidate(
            model, backend, candidate, iterations, tolerance, &gap);
        const bool accepted = attempt.outcome == "accepted";
        result.attempts.push_back(std::move(attempt));
        if (accepted) {
            result.success = true;
            result.accepted_backend = backend;
            result.solution = candidate;
            result.gap = std::move(gap);
            result.reason = result.attempts.back().reason;
        }
        return accepted;
    };
    const auto [pgs, pgs_iterations] = projected_gauss_seidel(model, tolerance);
    if (accept("projected-gauss-seidel-cpu-v1", pgs, pgs_iterations)) return result;
    const auto [newton, newton_iterations] =
        fischer_burmeister_newton(model, tolerance);
    if (accept("fischer-burmeister-newton-cpu-v1", newton, newton_iterations)) {
        return result;
    }
    result.terminal_fallback_used = true;
    const auto active_set = active_set_enumeration(model, tolerance);
    if (accept("enumerated-active-set-terminal-cpu-v1", active_set, 0)) return result;
    result.reason = model.variables.size() > 20
        ? "terminal active-set fallback is limited to 20 variables"
        : "all complementarity candidates failed the original gate";
    return result;
}

void write_complementarity_report(
    const ComplementarityIR& model,
    const ComplementarityResult& result,
    const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write complementarity report");
    output << std::setprecision(17)
           << "SMAVE_COMPLEMENTARITY_REPORT 1\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "PLAN " << std::quoted(result.plan_id) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "ACCEPTED_BACKEND " << std::quoted(result.accepted_backend) << '\n'
           << "TERMINAL_FALLBACK " << result.terminal_fallback_used << '\n';
    for (const auto& attempt : result.attempts) {
        output << "ATTEMPT " << std::quoted(attempt.backend) << ' '
               << std::quoted(attempt.outcome) << ' ' << std::quoted(attempt.reason)
               << " ITERATIONS " << attempt.iterations
               << " PRIMAL " << attempt.primal_violation
               << " DUAL " << attempt.dual_violation
               << " COMPLEMENTARITY " << attempt.complementarity_violation
               << " EQUATION_RESIDUAL " << attempt.equation_residual_inf << '\n';
    }
    for (std::size_t index = 0; index < result.solution.size(); ++index) {
        output << "PAIR " << std::quoted(model.variables[index].name)
               << " Z " << result.solution[index]
               << " W " << result.gap[index] << '\n';
    }
    output << "REASON " << std::quoted(result.reason) << '\n';
}

}  // namespace smave
