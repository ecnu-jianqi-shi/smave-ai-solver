#include "smave/compiler.hpp"

#include "smave/expression.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace smave {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read source: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    if (first >= last) return {};
    return {first, last};
}

std::string collapse_space(const std::string& value) {
    std::string result;
    bool pending_space = false;
    for (const unsigned char character : value) {
        if (std::isspace(character)) {
            pending_space = !result.empty();
        } else {
            if (pending_space) result.push_back(' ');
            result.push_back(static_cast<char>(character));
            pending_space = false;
        }
    }
    return result;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) result.push_back(item);
    return result;
}

std::string strip_comments(std::string value) {
    value = std::regex_replace(value, std::regex(R"(/\*[\s\S]*?\*/)"), "");
    value = std::regex_replace(value, std::regex(R"(//[^\n]*)"), "");
    return value;
}

std::string hash_text(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::map<std::string, std::string> parse_attributes(std::string source) {
    std::map<std::string, std::string> attributes;
    source = trim(std::move(source));
    if (source.empty()) return attributes;
    if (source.front() != '(' || source.back() != ')') {
        throw std::invalid_argument("invalid declaration attributes: " + source);
    }
    source = source.substr(1, source.size() - 2);
    for (auto item : split(source, ',')) {
        const auto equal = item.find('=');
        if (equal == std::string::npos) {
            throw std::invalid_argument("unsupported declaration attribute: " + item);
        }
        attributes[trim(item.substr(0, equal))] = trim(item.substr(equal + 1));
    }
    return attributes;
}

std::optional<double> optional_number(
    const std::map<std::string, std::string>& attributes,
    const std::string& key) {
    const auto iterator = attributes.find(key);
    if (iterator == attributes.end()) return std::nullopt;
    return std::stod(iterator->second);
}

std::vector<VariableIR> parse_declarations(const std::string& section) {
    const std::regex declaration(
        R"(^\s*(parameter\s+)?Real\s+([A-Za-z_]\w*)\s*(\([^;]*\))?\s*(?:=\s*(.+))?\s*$)");
    std::vector<VariableIR> variables;
    for (auto statement : split(section, ';')) {
        statement = collapse_space(statement);
        if (statement.empty()) continue;
        std::smatch match;
        if (!std::regex_match(statement, match, declaration)) {
            throw std::invalid_argument("unsupported declaration: " + statement);
        }
        const bool parameter = match[1].matched;
        const auto attributes = parse_attributes(match[3].str());
        VariableIR variable;
        variable.name = match[2].str();
        variable.kind = parameter ? "parameter" : "algebraic";
        variable.nominal = optional_number(attributes, "nominal").value_or(1.0);
        variable.start = parameter && match[4].matched
            ? std::stod(trim(match[4].str()))
            : optional_number(attributes, "start").value_or(0.0);
        variable.minimum = optional_number(attributes, "min");
        variable.maximum = optional_number(attributes, "max");
        if (const auto unit = attributes.find("unit"); unit != attributes.end()) {
            variable.unit = unit->second;
            if (variable.unit.size() >= 2 && variable.unit.front() == '"' &&
                variable.unit.back() == '"') {
                variable.unit = variable.unit.substr(1, variable.unit.size() - 2);
            }
        }
        variables.push_back(std::move(variable));
    }
    if (variables.empty()) throw std::invalid_argument("model has no Real variables");
    std::set<std::string> names;
    for (const auto& variable : variables) {
        if (!names.insert(variable.name).second) {
            throw std::invalid_argument("duplicate variable: " + variable.name);
        }
    }
    return variables;
}

std::vector<EquationIR> parse_equations(
    const std::string& source,
    const std::string& section,
    const std::vector<VariableIR>& variables) {
    std::set<std::string> known;
    for (const auto& variable : variables) known.insert(variable.name);
    std::vector<EquationIR> equations;
    std::size_t cursor = source.find("equation");
    for (auto statement : split(section, ';')) {
        statement = collapse_space(statement);
        if (statement.empty()) continue;
        const auto equal = statement.find('=');
        if (equal == std::string::npos || statement.find('=', equal + 1) != std::string::npos) {
            throw std::invalid_argument("equation must contain one equality: " + statement);
        }
        EquationIR equation;
        equation.id = "eq-" + std::to_string(equations.size() + 1);
        equation.residual = "(" + trim(statement.substr(0, equal)) + ")-(" +
            trim(statement.substr(equal + 1)) + ")";
        const Expression expression(equation.residual);
        equation.variables.assign(expression.names().begin(), expression.names().end());
        for (const auto& name : equation.variables) {
            if (!known.contains(name)) {
                throw std::invalid_argument("equation references undeclared name: " + name);
            }
        }
        const auto position = source.find(trim(statement), cursor);
        equation.source_line = position == std::string::npos
            ? 0
            : 1 + static_cast<std::size_t>(std::count(source.begin(), source.begin() + position, '\n'));
        if (position != std::string::npos) cursor = position + statement.size();
        equations.push_back(std::move(equation));
    }
    return equations;
}

std::unordered_map<std::string, std::string> structural_matching(
    const std::vector<EquationIR>& equations,
    const std::set<std::string>& unknowns) {
    std::unordered_map<std::string, std::string> variable_owner;
    std::unordered_map<std::string, std::vector<std::string>> incidence;
    for (const auto& equation : equations) {
        for (const auto& name : equation.variables) {
            if (unknowns.contains(name)) incidence[equation.id].push_back(name);
        }
    }
    std::function<bool(const std::string&, std::set<std::string>&)> assign;
    assign = [&](const std::string& equation_id, std::set<std::string>& visited) {
        for (const auto& variable : incidence[equation_id]) {
            if (!visited.insert(variable).second) continue;
            const auto owner = variable_owner.find(variable);
            if (owner == variable_owner.end() || assign(owner->second, visited)) {
                variable_owner[variable] = equation_id;
                return true;
            }
        }
        return false;
    };
    for (const auto& equation : equations) {
        std::set<std::string> visited;
        if (!assign(equation.id, visited)) {
            throw std::invalid_argument("incidence graph has no complete structural matching");
        }
    }
    std::unordered_map<std::string, std::string> matching;
    for (const auto& [variable, equation] : variable_owner) matching[equation] = variable;
    return matching;
}

std::vector<std::vector<std::string>> strongly_connected_components(
    const std::map<std::string, std::set<std::string>>& graph) {
    int next_index = 0;
    std::map<std::string, int> indices;
    std::map<std::string, int> lowlinks;
    std::vector<std::string> stack;
    std::set<std::string> on_stack;
    std::vector<std::vector<std::string>> components;
    std::function<void(const std::string&)> visit = [&](const std::string& node) {
        indices[node] = next_index;
        lowlinks[node] = next_index++;
        stack.push_back(node);
        on_stack.insert(node);
        for (const auto& dependency : graph.at(node)) {
            if (!indices.contains(dependency)) {
                visit(dependency);
                lowlinks[node] = std::min(lowlinks[node], lowlinks[dependency]);
            } else if (on_stack.contains(dependency)) {
                lowlinks[node] = std::min(lowlinks[node], indices[dependency]);
            }
        }
        if (lowlinks[node] == indices[node]) {
            std::vector<std::string> component;
            while (true) {
                const std::string member = stack.back();
                stack.pop_back();
                on_stack.erase(member);
                component.push_back(member);
                if (member == node) break;
            }
            components.push_back(std::move(component));
        }
    };
    for (const auto& [node, _] : graph) {
        if (!indices.contains(node)) visit(node);
    }
    return components;
}

bool is_linear(
    const std::vector<const EquationIR*>& equations,
    const std::vector<std::string>& unknowns,
    const std::vector<VariableIR>& variables) {
    std::unordered_set<std::string> block_unknowns(unknowns.begin(), unknowns.end());
    std::unordered_map<std::string, double> base;
    for (const auto& variable : variables) base[variable.name] = variable.start;
    for (const auto& unknown : unknowns) base[unknown] = 0.0;
    try {
        for (const auto* equation : equations) {
            const Expression expression(equation->residual);
            std::vector<std::string> local_unknowns;
            for (const auto& name : equation->variables) {
                if (block_unknowns.contains(name)) local_unknowns.push_back(name);
            }
            if (expression.constant_linear_coefficients(local_unknowns).has_value()) continue;
            auto combined = base;
            for (std::size_t index = 0; index < local_unknowns.size(); ++index) {
                combined[local_unknowns[index]] =
                    0.375 + 0.125 * static_cast<double>(index % 5);
            }
            for (const auto& unknown : local_unknowns) {
                const std::unordered_map<std::string, double> direction{{unknown, 1.0}};
                const auto reference = expression.directional_derivative(base, direction);
                if (!reference.has_value()) return false;
                for (const double probe_value : {0.5, 1.0, 2.0}) {
                    auto probe = base;
                    probe[unknown] = probe_value;
                    const auto derivative = expression.directional_derivative(probe, direction);
                    if (!derivative.has_value() ||
                        std::abs(*derivative - *reference) >
                            1.0e-9 * (1.0 + std::abs(*reference))) return false;
                }
                const auto combined_derivative =
                    expression.directional_derivative(combined, direction);
                if (!combined_derivative.has_value() ||
                    std::abs(*combined_derivative - *reference) >
                        1.0e-9 * (1.0 + std::abs(*reference))) return false;
            }
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

std::vector<BlockIR> build_blocks(
    const std::vector<VariableIR>& variables,
    const std::vector<EquationIR>& equations) {
    std::set<std::string> unknowns;
    for (const auto& variable : variables) {
        if (variable.kind == "algebraic") unknowns.insert(variable.name);
    }
    if (unknowns.size() != equations.size()) {
        throw std::invalid_argument(
            "prototype requires a square algebraic system: " +
            std::to_string(unknowns.size()) + " unknowns, " +
            std::to_string(equations.size()) + " equations");
    }
    const auto matching = structural_matching(equations, unknowns);
    std::map<std::string, std::set<std::string>> graph;
    for (const auto& unknown : unknowns) graph[unknown] = {};
    for (const auto& equation : equations) {
        const auto matched = matching.at(equation.id);
        for (const auto& name : equation.variables) {
            if (unknowns.contains(name) && name != matched) graph[matched].insert(name);
        }
    }
    auto components = strongly_connected_components(graph);
    std::map<std::string, std::size_t> component_by_variable;
    for (std::size_t index = 0; index < components.size(); ++index) {
        for (const auto& variable : components[index]) component_by_variable[variable] = index;
    }
    std::vector<std::vector<const EquationIR*>> component_equations(components.size());
    for (const auto& equation : equations) {
        component_equations[component_by_variable.at(matching.at(equation.id))].push_back(&equation);
    }
    std::vector<BlockIR> blocks;
    for (std::size_t index = 0; index < components.size(); ++index) {
        auto block_unknowns = components[index];
        std::stable_sort(
            block_unknowns.begin(), block_unknowns.end(),
            [&](const std::string& left, const std::string& right) {
                const auto left_position = std::find_if(
                    variables.begin(), variables.end(),
                    [&](const VariableIR& variable) { return variable.name == left; });
                const auto right_position = std::find_if(
                    variables.begin(), variables.end(),
                    [&](const VariableIR& variable) { return variable.name == right; });
                return left_position < right_position;
            });
        std::set<std::string> contexts;
        BlockIR block;
        block.unknowns = block_unknowns;
        std::unordered_map<std::string, std::size_t> unknown_positions;
        for (std::size_t column = 0; column < block_unknowns.size(); ++column) {
            unknown_positions.emplace(block_unknowns[column], column);
        }
        const auto& ordered_equations = component_equations[index];
        std::vector<std::vector<std::size_t>> sparsity_rows;
        sparsity_rows.reserve(ordered_equations.size());
        for (const auto* equation : ordered_equations) {
            block.equation_ids.push_back(equation->id);
            std::vector<std::size_t> row;
            for (const auto& name : equation->variables) {
                const auto position = unknown_positions.find(name);
                if (position != unknown_positions.end()) row.push_back(position->second);
            }
            sparsity_rows.push_back(std::move(row));
            for (const auto& name : equation->variables) {
                if (std::find(block_unknowns.begin(), block_unknowns.end(), name) ==
                    block_unknowns.end()) contexts.insert(name);
            }
        }
        block.jacobian_sparsity =
            SparsityPattern::from_rows(block_unknowns.size(), sparsity_rows);
        block.contexts.assign(contexts.begin(), contexts.end());
        block.linear = is_linear(ordered_equations, block_unknowns, variables);
        blocks.push_back(std::move(block));
    }
    std::vector<BlockIR> ordered;
    std::set<std::string> solved;
    while (!blocks.empty()) {
        auto ready = std::find_if(blocks.begin(), blocks.end(), [&](const BlockIR& block) {
            return std::none_of(block.contexts.begin(), block.contexts.end(), [&](const std::string& name) {
                return unknowns.contains(name) && !solved.contains(name);
            });
        });
        if (ready == blocks.end()) ready = blocks.begin();
        ready->id = "block-" + std::to_string(ordered.size() + 1);
        solved.insert(ready->unknowns.begin(), ready->unknowns.end());
        ordered.push_back(std::move(*ready));
        blocks.erase(ready);
    }
    return ordered;
}

}  // namespace

ModelIR compile_model(
    const std::filesystem::path& source,
    const std::optional<std::string>& top) {
    if (source.extension() != ".mo") {
        throw std::invalid_argument("the prototype frontend currently accepts .mo sources");
    }
    const std::string original = read_file(source);
    const std::string text = strip_comments(original);
    std::smatch model_match;
    if (!std::regex_search(text, model_match, std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) {
        throw std::invalid_argument("no Modelica model declaration found");
    }
    const std::string model_name = model_match[1].str();
    if (top.has_value() && *top != model_name) {
        throw std::invalid_argument("requested top does not match source model");
    }
    const std::regex end_pattern("\\bend\\s+" + model_name + R"(\s*;)");
    std::smatch end_match;
    const auto body_begin = static_cast<std::size_t>(model_match.position() + model_match.length());
    const std::string tail = text.substr(body_begin);
    if (!std::regex_search(tail, end_match, end_pattern)) {
        throw std::invalid_argument("missing end " + model_name + ";");
    }
    const std::string body = tail.substr(0, end_match.position());
    if (std::regex_search(body, std::regex(R"(\b(der|when|reinit|pre|edge|change)\s*\()"))) {
        throw std::invalid_argument(
            "derivatives and events are outside the Phase 0 subset; their semantics cannot be erased");
    }
    std::smatch equation_match;
    if (!std::regex_search(body, equation_match, std::regex(R"(\bequation\b)"))) {
        throw std::invalid_argument("model must contain an equation section");
    }
    const std::size_t equation_position = equation_match.position();
    ModelIR model;
    model.model_id = model_name;
    model.source_hash = hash_text(original);
    model.variables = parse_declarations(body.substr(0, equation_position));
    model.equations = parse_equations(
        text, body.substr(equation_position + equation_match.length()), model.variables);
    model.blocks = build_blocks(model.variables, model.equations);
    for (auto& block : model.blocks) block.fingerprint = block_fingerprint(block, model);
    model.validate();
    return model;
}

}  // namespace smave
