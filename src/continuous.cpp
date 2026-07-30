#include "smave/continuous.hpp"

#include "smave/expression.hpp"

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
#include <unordered_set>

namespace smave {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read continuous source: " + path.string());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character);
    }).base();
    if (first >= last) return {};
    return {first, last};
}

std::string strip_comments(std::string value) {
    value = std::regex_replace(value, std::regex(R"(/\*[\s\S]*?\*/)"), "");
    value = std::regex_replace(value, std::regex(R"(//[^\n]*)"), "");
    return value;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, delimiter)) result.push_back(item);
    return result;
}

std::map<std::string, std::string> attributes(std::string source) {
    std::map<std::string, std::string> result;
    source = trim(std::move(source));
    if (source.empty()) return result;
    if (source.front() != '(' || source.back() != ')') {
        throw std::invalid_argument("invalid continuous declaration attributes: " + source);
    }
    source = source.substr(1, source.size() - 2);
    for (auto item : split(source, ',')) {
        const auto equal = item.find('=');
        if (equal == std::string::npos) {
            throw std::invalid_argument("unsupported continuous declaration attribute: " + item);
        }
        result[trim(item.substr(0, equal))] = trim(item.substr(equal + 1));
    }
    return result;
}

double attribute_number(
    const std::map<std::string, std::string>& values,
    const std::string& name,
    double fallback) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? fallback : std::stod(iterator->second);
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

void require_tag(std::istream& input, std::string_view expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error(
            "invalid continuous hybrid IR: expected " + std::string(expected));
    }
}

std::string replace_pre(std::string expression) {
    const std::regex pre_call(R"(\bpre\s*\(\s*([A-Za-z_]\w*)\s*\))");
    expression = std::regex_replace(expression, pre_call, "__smave_pre_$1");
    if (std::regex_search(expression, std::regex(R"(\bpre\s*\()"))) {
        throw std::invalid_argument("continuous reset supports only pre(state)");
    }
    return expression;
}

std::unordered_set<std::string> allowed_names(const ContinuousHybridIR& model) {
    std::unordered_set<std::string> result{"time"};
    for (const auto& [name, value] : model.parameters) {
        (void)value;
        result.insert(name);
    }
    for (const auto& state : model.states) result.insert(state.name);
    return result;
}

std::unordered_set<std::string> reset_allowed_names(const ContinuousHybridIR& model) {
    auto result = allowed_names(model);
    for (const auto& state : model.states) {
        result.insert("__smave_pre_" + state.name);
    }
    return result;
}

void validate_expression_names(
    const std::string& source,
    const std::unordered_set<std::string>& allowed,
    const std::string& purpose) {
    const Expression expression(source);
    for (const auto& name : expression.names()) {
        if (!allowed.contains(name)) {
            throw std::invalid_argument(purpose + " references undeclared name: " + name);
        }
    }
}

struct Evaluator {
    explicit Evaluator(const ContinuousHybridIR& model) : model(model) {
        for (const auto& state : model.states) derivatives.emplace_back(state.derivative);
        for (const auto& event : model.events) {
            guards.emplace_back(event.guard);
            std::vector<Expression> event_resets;
            for (const auto& reset : event.resets) event_resets.emplace_back(reset.expression);
            resets.push_back(std::move(event_resets));
        }
    }

    std::unordered_map<std::string, double> context(
        double time,
        const std::vector<double>& state,
        const std::vector<double>* pre_state = nullptr) const {
        std::unordered_map<std::string, double> values(model.parameters.begin(), model.parameters.end());
        values["time"] = time;
        for (std::size_t index = 0; index < model.states.size(); ++index) {
            values[model.states[index].name] = state[index];
            if (pre_state != nullptr) {
                values["__smave_pre_" + model.states[index].name] = (*pre_state)[index];
            }
        }
        return values;
    }

    std::vector<double> derivative(double time, const std::vector<double>& state) const {
        const auto values = context(time, state);
        std::vector<double> result;
        result.reserve(derivatives.size());
        for (const auto& expression : derivatives) {
            const double value = expression.evaluate(values);
            if (!std::isfinite(value)) throw std::runtime_error("continuous derivative is NaN/Inf");
            result.push_back(value);
        }
        return result;
    }

    const ContinuousHybridIR& model;
    std::vector<Expression> derivatives;
    std::vector<Expression> guards;
    std::vector<std::vector<Expression>> resets;
};

std::vector<double> combine(
    const std::vector<double>& base,
    const std::vector<double>& increment,
    double factor) {
    std::vector<double> result(base.size());
    for (std::size_t index = 0; index < base.size(); ++index) {
        result[index] = base[index] + factor * increment[index];
    }
    return result;
}

std::vector<double> rk4_step(
    const Evaluator& evaluator,
    double time,
    const std::vector<double>& state,
    double step) {
    const auto first = evaluator.derivative(time, state);
    const auto second = evaluator.derivative(
        time + step * 0.5, combine(state, first, step * 0.5));
    const auto third = evaluator.derivative(
        time + step * 0.5, combine(state, second, step * 0.5));
    const auto fourth = evaluator.derivative(
        time + step, combine(state, third, step));
    std::vector<double> result(state.size());
    for (std::size_t index = 0; index < state.size(); ++index) {
        result[index] = state[index] + step / 6.0 *
            (first[index] + 2.0 * second[index] + 2.0 * third[index] + fourth[index]);
        if (!std::isfinite(result[index])) {
            throw std::runtime_error("continuous integrator produced NaN/Inf");
        }
    }
    return result;
}

double scaled_error(
    const std::vector<double>& coarse,
    const std::vector<double>& fine,
    const ContinuousTolerance& tolerance) {
    double result{};
    for (std::size_t index = 0; index < coarse.size(); ++index) {
        const double scale = tolerance.absolute + tolerance.relative *
            std::max(std::abs(coarse[index]), std::abs(fine[index]));
        result = std::max(result, std::abs(fine[index] - coarse[index]) / (15.0 * scale));
    }
    return result;
}

bool crossing(double left, double right, int direction, double guard_tolerance) {
    if (direction > 0) return left < 0.0 && right >= -guard_tolerance;
    return left > 0.0 && right <= guard_tolerance;
}

bool guard_active(double value, int direction, double guard_tolerance) {
    return direction > 0 ? value >= -guard_tolerance : value <= guard_tolerance;
}

struct LocatedEvent {
    std::size_t index{};
    double offset{};
    std::vector<double> state;
    double guard{};
    bool grazing{false};
};

LocatedEvent locate_event(
    const Evaluator& evaluator,
    std::size_t event_index,
    double start_time,
    const std::vector<double>& start_state,
    double step,
    const ContinuousTolerance& tolerance) {
    const auto& event = evaluator.model.events[event_index];
    double left{};
    double right = step;
    auto right_state = rk4_step(evaluator, start_time, start_state, right);
    double right_guard = evaluator.guards[event_index].evaluate(
        evaluator.context(start_time + right, right_state));
    while (right - left > tolerance.root_time) {
        const double middle = 0.5 * (left + right);
        auto middle_state = rk4_step(evaluator, start_time, start_state, middle);
        const double middle_guard = evaluator.guards[event_index].evaluate(
            evaluator.context(start_time + middle, middle_state));
        const bool root_is_left = event.direction > 0
            ? middle_guard >= 0.0
            : middle_guard <= 0.0;
        if (root_is_left) {
            right = middle;
            right_state = std::move(middle_state);
            right_guard = middle_guard;
        } else {
            left = middle;
        }
    }
    return {event_index, right, std::move(right_state), right_guard, false};
}

LocatedEvent locate_grazing_event(
    const Evaluator& evaluator,
    std::size_t event_index,
    double start_time,
    const std::vector<double>& start_state,
    double step,
    const ContinuousTolerance& tolerance) {
    const auto& event = evaluator.model.events[event_index];
    const auto signed_guard = [&](double offset, std::vector<double>* located_state = nullptr) {
        auto candidate = rk4_step(evaluator, start_time, start_state, offset);
        const double guard = evaluator.guards[event_index].evaluate(
            evaluator.context(start_time + offset, candidate));
        if (located_state != nullptr) *located_state = std::move(candidate);
        return event.direction > 0 ? guard : -guard;
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
    std::vector<double> state;
    const double peak = signed_guard(offset, &state);
    const double probe = std::max(tolerance.root_time * 4.0, step / 64.0);
    if (offset <= probe || offset + probe >= step) return {};
    const double before = signed_guard(offset - probe);
    const double after = signed_guard(offset + probe);
    const double prominence = std::min(peak - before, peak - after);
    if (std::abs(peak) > tolerance.guard || prominence <= tolerance.guard) return {};
    const double guard = event.direction > 0 ? peak : -peak;
    return {event_index, offset, std::move(state), guard, true};
}

void enforce_state_contract(
    const ContinuousHybridIR& model,
    const std::vector<double>& state,
    double slack) {
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        if (!std::isfinite(state[index])) {
            throw std::runtime_error("continuous state is NaN/Inf");
        }
        const auto& contract = model.states[index];
        if (contract.has_minimum && state[index] < contract.minimum - slack) {
            throw std::runtime_error(contract.name + ": continuous lower-bound violation");
        }
        if (contract.has_maximum && state[index] > contract.maximum + slack) {
            throw std::runtime_error(contract.name + ": continuous upper-bound violation");
        }
    }
}

std::unordered_map<std::string, double> state_map(
    const ContinuousHybridIR& model,
    const std::vector<double>& state) {
    std::unordered_map<std::string, double> result;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        result[model.states[index].name] = state[index];
    }
    return result;
}

struct EventTransactionResult {
    std::vector<double> state;
    std::vector<ContinuousEventRecord> records;
    double maximum_guard_residual{0.0};
    double maximum_reset_error{0.0};
};

EventTransactionResult execute_event_transaction(
    const Evaluator& evaluator,
    double event_time,
    const std::vector<double>& initial_state,
    std::vector<std::size_t> pending,
    std::vector<bool>& event_armed,
    const ContinuousTolerance& tolerance,
    bool require_root_gate,
    const std::unordered_set<std::string>& already_processed = {},
    const std::vector<double>* physical_pre_state = nullptr) {
    const auto& model = evaluator.model;
    std::vector<bool> processed(model.events.size(), false);
    for (std::size_t index = 0; index < model.events.size(); ++index) {
        processed[index] = already_processed.contains(model.events[index].id);
    }
    EventTransactionResult transaction;
    transaction.state = initial_state;
    const auto& stable_pre_state = physical_pre_state == nullptr
        ? initial_state
        : *physical_pre_state;
    std::size_t event_iterations{};
    while (!pending.empty()) {
        if (++event_iterations > model.events.size()) {
            throw std::runtime_error("continuous event iteration did not reach a fixed point");
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
        const auto iteration_context = evaluator.context(
            event_time, iteration_pre_state, &stable_pre_state);
        std::vector<double> guards_before;
        guards_before.reserve(model.events.size());
        for (const auto& guard : evaluator.guards) {
            guards_before.push_back(guard.evaluate(iteration_context));
        }
        auto iteration_post_state = iteration_pre_state;
        std::unordered_set<std::string> reset_variables;
        struct PendingEventRecord {
            std::string id;
            double guard_residual{0.0};
        };
        std::vector<PendingEventRecord> pending_records;
        for (const auto selected : pending) {
            const auto& event = model.events[selected];
            processed[selected] = true;
            event_armed[selected] = false;
            const double guard_residual = std::abs(guards_before[selected]);
            if (require_root_gate && event_iterations == 1 &&
                guard_residual > tolerance.guard) {
                throw std::runtime_error(
                    event.id + ": simultaneous zero-crossing gate residual exceeds tolerance");
            }
            for (std::size_t reset_index = 0;
                 reset_index < event.resets.size(); ++reset_index) {
                const auto& reset = event.resets[reset_index];
                if (!reset_variables.insert(reset.variable).second) {
                    throw std::runtime_error(
                        "simultaneous continuous events have conflicting reset for " +
                        reset.variable);
                }
                const auto state_position = std::find_if(
                    model.states.begin(), model.states.end(),
                    [&](const ContinuousStateIR& item) {
                        return item.name == reset.variable;
                    });
                const auto index = static_cast<std::size_t>(
                    std::distance(model.states.begin(), state_position));
                const double candidate = evaluator.resets[selected][reset_index]
                    .evaluate(iteration_context);
                if (!std::isfinite(candidate)) {
                    throw std::runtime_error(event.id + ": reset produced NaN/Inf");
                }
                iteration_post_state[index] = candidate;
                const double independent = Expression(reset.expression)
                    .evaluate(iteration_context);
                transaction.maximum_reset_error = std::max(
                    transaction.maximum_reset_error, std::abs(candidate - independent));
            }
            if (require_root_gate && event_iterations == 1) {
                transaction.maximum_guard_residual = std::max(
                    transaction.maximum_guard_residual, guard_residual);
            }
            pending_records.push_back({event.id, guard_residual});
        }
        enforce_state_contract(model, iteration_post_state, tolerance.guard);
        const auto pre_state_values = state_map(model, iteration_pre_state);
        const auto post_state_values = state_map(model, iteration_post_state);
        for (const auto& record : pending_records) {
            transaction.records.push_back({
                record.id, event_time, record.guard_residual, false,
                pre_state_values, post_state_values});
        }
        const auto post_context = evaluator.context(
            event_time, iteration_post_state, &stable_pre_state);
        pending.clear();
        for (std::size_t index = 0; index < model.events.size(); ++index) {
            if (processed[index]) continue;
            const double after = evaluator.guards[index].evaluate(post_context);
            if (!guard_active(
                    guards_before[index], model.events[index].direction,
                    tolerance.guard) &&
                guard_active(after, model.events[index].direction, tolerance.guard)) {
                pending.push_back(index);
            }
        }
        transaction.state = std::move(iteration_post_state);
    }
    return transaction;
}

}  // namespace

void ContinuousHybridIR::validate() const {
    if (schema_version != kContinuousHybridSchemaVersion) {
        throw std::invalid_argument("unsupported continuous hybrid schema: " + schema_version);
    }
    if (model_id.empty() || source_hash.empty()) {
        throw std::invalid_argument("continuous hybrid model lacks identity");
    }
    std::unordered_set<std::string> names{"time"};
    for (const auto& [name, value] : parameters) {
        if (!names.insert(name).second || !std::isfinite(value)) {
            throw std::invalid_argument("invalid or duplicate continuous parameter: " + name);
        }
    }
    for (const auto& state : states) {
        if (!names.insert(state.name).second || !std::isfinite(state.start) ||
            !(state.nominal > 0.0) || state.derivative.empty()) {
            throw std::invalid_argument("invalid or duplicate continuous state: " + state.name);
        }
        if (state.has_minimum && state.has_maximum && state.minimum > state.maximum) {
            throw std::invalid_argument(state.name + ": invalid continuous bounds");
        }
    }
    if (states.empty()) throw std::invalid_argument("continuous model has no states");
    const auto allowed = allowed_names(*this);
    const auto reset_allowed = reset_allowed_names(*this);
    for (const auto& state : states) {
        validate_expression_names(state.derivative, allowed, state.name + " derivative");
    }
    std::unordered_set<std::string> event_ids;
    for (const auto& event : events) {
        if (!event_ids.insert(event.id).second ||
            (event.direction != -1 && event.direction != 1) || event.resets.empty()) {
            throw std::invalid_argument("invalid continuous event: " + event.id);
        }
        validate_expression_names(event.guard, allowed, event.id + " guard");
        std::unordered_set<std::string> reset_names;
        for (const auto& reset : event.resets) {
            if (!reset_names.insert(reset.variable).second || !names.contains(reset.variable) ||
                parameters.contains(reset.variable) || reset.variable == "time") {
                throw std::invalid_argument(event.id + ": invalid reset state " + reset.variable);
            }
            validate_expression_names(reset.expression, reset_allowed, event.id + " reset");
        }
    }
}

void ContinuousHybridIR::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write continuous hybrid IR: " + path.string());
    output << "SMAVE_CONTINUOUS_HYBRID 1\n"
           << "MODEL " << std::quoted(model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(source_hash) << '\n'
           << "PARAMETERS " << parameters.size();
    for (const auto& [name, value] : parameters) {
        output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << value;
    }
    output << "\nSTATES " << states.size() << '\n';
    for (const auto& state : states) {
        output << "STATE " << std::quoted(state.name) << ' ' << std::setprecision(17)
               << state.start << ' ' << state.nominal << ' ' << state.has_minimum << ' '
               << state.minimum << ' ' << state.has_maximum << ' ' << state.maximum << ' '
               << std::quoted(state.derivative) << '\n';
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

ContinuousHybridIR ContinuousHybridIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read continuous hybrid IR: " + path.string());
    require_tag(input, "SMAVE_CONTINUOUS_HYBRID");
    int version{};
    input >> version;
    if (version != 1) throw std::invalid_argument("unsupported continuous hybrid IR version");
    ContinuousHybridIR model;
    require_tag(input, "MODEL"); input >> std::quoted(model.model_id);
    require_tag(input, "SOURCE_HASH"); input >> std::quoted(model.source_hash);
    require_tag(input, "PARAMETERS");
    std::size_t count{};
    input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name;
        double value{};
        input >> std::quoted(name) >> value;
        model.parameters.emplace(std::move(name), value);
    }
    require_tag(input, "STATES"); input >> count;
    model.states.resize(count);
    for (auto& state : model.states) {
        require_tag(input, "STATE");
        input >> std::quoted(state.name) >> state.start >> state.nominal
              >> state.has_minimum >> state.minimum >> state.has_maximum
              >> state.maximum >> std::quoted(state.derivative);
    }
    require_tag(input, "EVENTS"); input >> count;
    model.events.resize(count);
    for (auto& event : model.events) {
        require_tag(input, "EVENT");
        std::size_t resets{};
        input >> std::quoted(event.id) >> event.direction >> event.priority
              >> event.source_order >> std::quoted(event.guard) >> resets;
        event.resets.resize(resets);
        for (auto& reset : event.resets) {
            input >> std::quoted(reset.variable) >> std::quoted(reset.expression);
        }
    }
    require_tag(input, "END");
    model.validate();
    return model;
}

ContinuousHybridIR compile_continuous_model(
    const std::filesystem::path& source,
    const std::string& top) {
    const auto raw = read_file(source);
    auto text = strip_comments(raw);
    std::smatch model_match;
    if (!std::regex_search(text, model_match, std::regex(R"(\bmodel\s+([A-Za-z_]\w*))"))) {
        throw std::invalid_argument("continuous source has no model declaration");
    }
    ContinuousHybridIR model;
    model.model_id = model_match[1].str();
    model.source_hash = digest(raw);
    if (!top.empty() && top != model.model_id) {
        throw std::invalid_argument("requested continuous top model does not match source");
    }
    text = text.substr(model_match.position() + model_match.length());
    text = std::regex_replace(
        text, std::regex("end\\s+" + model.model_id + R"(\s*;\s*$)"), "");
    const auto equation = text.find("equation");
    if (equation == std::string::npos) {
        throw std::invalid_argument("continuous model has no equation section");
    }
    const auto declaration_section = text.substr(0, equation);
    auto equation_section = text.substr(equation + std::string("equation").size());
    const std::regex declaration(
        R"(^\s*(parameter\s+)?Real\s+([A-Za-z_]\w*)\s*(\([^;]*\))?\s*(?:=\s*(.+))?\s*$)");
    for (auto statement : split(declaration_section, ';')) {
        statement = trim(std::move(statement));
        if (statement.empty()) continue;
        std::smatch match;
        if (!std::regex_match(statement, match, declaration)) {
            throw std::invalid_argument("unsupported continuous declaration: " + statement);
        }
        const auto declaration_attributes = attributes(match[3].str());
        const std::string name = match[2].str();
        if (match[1].matched) {
            if (!declaration_attributes.empty()) {
                throw std::invalid_argument(
                    "continuous parameters do not support declaration attributes: " + name);
            }
            if (!match[4].matched) {
                throw std::invalid_argument("continuous parameter requires a value: " + name);
            }
            const Expression value_expression(trim(match[4].str()));
            std::unordered_map<std::string, double> values(
                model.parameters.begin(), model.parameters.end());
            model.parameters[name] = value_expression.evaluate(values);
        } else {
            if (match[4].matched) {
                throw std::invalid_argument(
                    "continuous state binding equations are not start attributes: " + name);
            }
            static const std::set<std::string> supported_attributes{
                "start", "nominal", "min", "max"};
            for (const auto& [attribute, value] : declaration_attributes) {
                (void)value;
                if (!supported_attributes.contains(attribute)) {
                    throw std::invalid_argument(
                        "unsupported continuous state attribute: " + attribute);
                }
            }
            ContinuousStateIR state;
            state.name = name;
            state.start = attribute_number(declaration_attributes, "start", 0.0);
            state.nominal = attribute_number(declaration_attributes, "nominal", 1.0);
            if (const auto iterator = declaration_attributes.find("min");
                iterator != declaration_attributes.end()) {
                state.has_minimum = true;
                state.minimum = std::stod(iterator->second);
            }
            if (const auto iterator = declaration_attributes.find("max");
                iterator != declaration_attributes.end()) {
                state.has_maximum = true;
                state.maximum = std::stod(iterator->second);
            }
            model.states.push_back(std::move(state));
        }
    }
    std::unordered_map<std::string, std::size_t> state_index;
    for (std::size_t index = 0; index < model.states.size(); ++index) {
        if (!state_index.emplace(model.states[index].name, index).second ||
            model.parameters.contains(model.states[index].name)) {
            throw std::invalid_argument("duplicate continuous declaration: " + model.states[index].name);
        }
    }

    const std::regex when_block(R"(\bwhen\s+([\s\S]*?)\s+then\s+([\s\S]*?)\bend\s+when\s*;)");
    std::size_t event_order{};
    for (auto iterator = std::sregex_iterator(
             equation_section.begin(), equation_section.end(), when_block);
         iterator != std::sregex_iterator(); ++iterator) {
        if (std::regex_search((*iterator)[2].str(), std::regex(R"(\belsewhen\b)"))) {
            throw std::invalid_argument("continuous subset does not support elsewhen");
        }
        std::smatch guard_match;
        const auto guard_source = trim((*iterator)[1].str());
        if (!std::regex_match(
                guard_source, guard_match,
                std::regex(R"(^\s*(.+?)\s*(<=|>=|<|>)\s*(.+)\s*$)"))) {
            throw std::invalid_argument("continuous when guard must be one scalar comparison");
        }
        ContinuousEventIR event;
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
        for (auto reset = std::sregex_iterator(body.begin(), body.end(), reset_expression);
             reset != std::sregex_iterator(); ++reset) {
            event.resets.push_back({
                (*reset)[1].str(), replace_pre(trim((*reset)[2].str()))});
        }
        remaining = std::regex_replace(remaining, reset_expression, "");
        if (!trim(remaining).empty() || event.resets.empty()) {
            throw std::invalid_argument("continuous when body supports only reinit statements");
        }
        model.events.push_back(std::move(event));
    }
    equation_section = std::regex_replace(equation_section, when_block, "");
    const std::regex derivative(
        R"(\bder\s*\(\s*([A-Za-z_]\w*)\s*\)\s*=\s*([^;]+)\s*;)");
    for (auto iterator = std::sregex_iterator(
             equation_section.begin(), equation_section.end(), derivative);
         iterator != std::sregex_iterator(); ++iterator) {
        const auto state = state_index.find((*iterator)[1].str());
        if (state == state_index.end()) {
            throw std::invalid_argument("derivative references undeclared state: " + (*iterator)[1].str());
        }
        auto& target = model.states[state->second].derivative;
        if (!target.empty()) throw std::invalid_argument("duplicate derivative equation: " + (*iterator)[1].str());
        target = trim((*iterator)[2].str());
    }
    equation_section = std::regex_replace(equation_section, derivative, "");
    if (!trim(equation_section).empty()) {
        throw std::invalid_argument(
            "continuous subset accepts only explicit der(state)=expression and when/reinit");
    }
    for (const auto& state : model.states) {
        if (state.derivative.empty()) {
            throw std::invalid_argument("continuous state lacks derivative equation: " + state.name);
        }
    }
    model.validate();
    return model;
}

ContinuousRunResult simulate_continuous(
    const ContinuousHybridIR& model,
    double start_time,
    double end_time,
    double maximum_step,
    ContinuousTolerance tolerance,
    bool process_initial_events) {
    model.validate();
    if (!std::isfinite(start_time) || !std::isfinite(end_time) ||
        (process_initial_events ? end_time < start_time : end_time <= start_time) ||
        !(maximum_step > 0.0) ||
        !(tolerance.absolute > 0.0) || !(tolerance.relative > 0.0) ||
        !(tolerance.root_time > 0.0) || !(tolerance.guard > 0.0)) {
        throw std::invalid_argument("invalid continuous simulation interval/tolerance");
    }
    Evaluator evaluator(model);
    ContinuousRunResult result;
    std::vector<double> state;
    for (const auto& contract : model.states) state.push_back(contract.start);
    std::vector<bool> event_armed(model.events.size(), true);
    double time = start_time;
    double step = std::min(maximum_step, end_time - start_time);
    try {
        enforce_state_contract(model, state, tolerance.absolute);
        if (process_initial_events) {
            const auto initial_context = evaluator.context(time, state);
            std::vector<std::size_t> initial_events;
            for (std::size_t index = 0; index < model.events.size(); ++index) {
                const double guard = evaluator.guards[index].evaluate(initial_context);
                if (guard_active(guard, model.events[index].direction, tolerance.guard)) {
                    initial_events.push_back(index);
                }
            }
            if (!initial_events.empty()) {
                auto transaction = execute_event_transaction(
                    evaluator, time, state, std::move(initial_events), event_armed,
                    tolerance, false);
                if (transaction.records.size() > 10000) {
                    throw std::runtime_error("continuous event chattering limit exceeded");
                }
                state = std::move(transaction.state);
                result.events = std::move(transaction.records);
                result.maximum_reset_error = transaction.maximum_reset_error;
            }
        }
        while (time < end_time) {
            if (result.accepted_steps + result.rejected_steps > 1000000) {
                throw std::runtime_error("continuous simulation exceeded step limit");
            }
            step = std::min(step, end_time - time);
            const auto coarse = rk4_step(evaluator, time, state, step);
            const auto half = rk4_step(evaluator, time, state, step * 0.5);
            const auto fine = rk4_step(evaluator, time + step * 0.5, half, step * 0.5);
            const double error = scaled_error(coarse, fine, tolerance);
            if (error > 1.0) {
                ++result.rejected_steps;
                step *= std::max(0.2, 0.9 * std::pow(error, -0.2));
                if (step < std::numeric_limits<double>::epsilon() *
                        std::max(1.0, std::abs(time))) {
                    throw std::runtime_error("continuous integrator step underflow");
                }
                continue;
            }
            ++result.accepted_steps;
            result.maximum_scaled_local_error = std::max(
                result.maximum_scaled_local_error, error);
            const auto start_context = evaluator.context(time, state);
            const auto end_context = evaluator.context(time + step, fine);
            std::vector<LocatedEvent> located;
            for (std::size_t index = 0; index < model.events.size(); ++index) {
                const double left = evaluator.guards[index].evaluate(start_context);
                const double right = evaluator.guards[index].evaluate(end_context);
                const bool inactive = model.events[index].direction > 0
                    ? left < -tolerance.guard
                    : left > tolerance.guard;
                if (!event_armed[index] && inactive) event_armed[index] = true;
                if (event_armed[index] &&
                    crossing(left, right, model.events[index].direction, tolerance.guard)) {
                    located.push_back(locate_event(
                        evaluator, index, time, state, step, tolerance));
                } else if (event_armed[index]) {
                    const double signed_left = model.events[index].direction > 0 ? left : -left;
                    const double signed_right = model.events[index].direction > 0 ? right : -right;
                    if (signed_left < -tolerance.guard &&
                        signed_right < -tolerance.guard) {
                        auto grazing = locate_grazing_event(
                            evaluator, index, time, state, step, tolerance);
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
                std::vector<LocatedEvent> simultaneous;
                for (auto& candidate : located) {
                    if (std::abs(candidate.offset - earliest_offset) <= tolerance.root_time) {
                        simultaneous.push_back(std::move(candidate));
                    }
                }
                std::sort(simultaneous.begin(), simultaneous.end(), [&](const auto& left, const auto& right) {
                    const auto& left_event = model.events[left.index];
                    const auto& right_event = model.events[right.index];
                    if (left_event.priority != right_event.priority) {
                        return left_event.priority > right_event.priority;
                    }
                    return left_event.source_order < right_event.source_order;
                });
                const double event_time = time + earliest_offset;
                const auto event_pre_state = rk4_step(evaluator, time, state, earliest_offset);
                std::vector<std::size_t> pending;
                pending.reserve(simultaneous.size());
                for (const auto& selected : simultaneous) pending.push_back(selected.index);
                auto transaction = execute_event_transaction(
                    evaluator, event_time, event_pre_state, std::move(pending),
                    event_armed, tolerance, true);
                std::unordered_set<std::string> grazing_ids;
                for (const auto& selected : simultaneous) {
                    if (selected.grazing) {
                        grazing_ids.insert(model.events[selected.index].id);
                    }
                }
                for (auto& record : transaction.records) {
                    if (grazing_ids.contains(record.id)) {
                        record.grazing = true;
                        ++result.grazing_events;
                    }
                }
                if (result.events.size() + transaction.records.size() > 10000) {
                    throw std::runtime_error("continuous event chattering limit exceeded");
                }
                result.events.insert(
                    result.events.end(), transaction.records.begin(), transaction.records.end());
                result.maximum_guard_residual = std::max(
                    result.maximum_guard_residual, transaction.maximum_guard_residual);
                result.maximum_reset_error = std::max(
                    result.maximum_reset_error, transaction.maximum_reset_error);
                time = event_time;
                state = std::move(transaction.state);
                step = std::min(maximum_step, std::max(tolerance.root_time * 4.0, step * 0.5));
                continue;
            }
            enforce_state_contract(model, fine, tolerance.guard);
            time += step;
            state = fine;
            const double factor = error == 0.0
                ? 2.0
                : std::clamp(0.9 * std::pow(error, -0.2), 0.5, 2.0);
            step = std::min(maximum_step, step * factor);
        }
        result.success = true;
        result.final_time = time;
        result.final_state = state_map(model, state);
        result.message =
            "adaptive RK4 and atomic fixed-point event iteration completed";
    } catch (const std::exception& error) {
        result.final_time = time;
        result.final_state = state_map(model, state);
        result.message = error.what();
    }
    return result;
}

ContinuousBoundaryEventResult process_continuous_parameter_events(
    const ContinuousHybridIR& model,
    const std::map<std::string, double>& previous_parameters,
    double event_time,
    const std::unordered_map<std::string, double>& state,
    const std::unordered_map<std::string, double>& physical_pre_state,
    const std::vector<std::string>& already_processed,
    ContinuousTolerance tolerance) {
    model.validate();
    if (!std::isfinite(event_time) || !(tolerance.absolute > 0.0) ||
        !(tolerance.relative > 0.0) || !(tolerance.root_time > 0.0) ||
        !(tolerance.guard > 0.0)) {
        throw std::invalid_argument("invalid continuous boundary event time/tolerance");
    }
    if (previous_parameters.size() != model.parameters.size()) {
        throw std::invalid_argument("continuous parameter snapshot shape mismatch");
    }
    ContinuousBoundaryEventResult result;
    result.final_state = state;
    try {
        ContinuousHybridIR previous_model = model;
        previous_model.parameters = previous_parameters;
        previous_model.validate();
        Evaluator previous_evaluator(previous_model);
        Evaluator evaluator(model);
        std::vector<double> current_state;
        std::vector<double> event_pre_state;
        for (const auto& contract : model.states) {
            current_state.push_back(state.at(contract.name));
            event_pre_state.push_back(physical_pre_state.at(contract.name));
        }
        enforce_state_contract(model, current_state, tolerance.guard);
        const auto previous_context = previous_evaluator.context(event_time, current_state);
        const auto current_context = evaluator.context(event_time, current_state);
        std::unordered_set<std::string> processed(
            already_processed.begin(), already_processed.end());
        std::vector<std::size_t> pending;
        for (std::size_t index = 0; index < model.events.size(); ++index) {
            if (processed.contains(model.events[index].id)) continue;
            const double before = previous_evaluator.guards[index].evaluate(previous_context);
            const double after = evaluator.guards[index].evaluate(current_context);
            if (!guard_active(before, model.events[index].direction, tolerance.guard) &&
                guard_active(after, model.events[index].direction, tolerance.guard)) {
                pending.push_back(index);
            }
        }
        if (!pending.empty()) {
            std::vector<bool> event_armed(model.events.size(), true);
            auto transaction = execute_event_transaction(
                evaluator, event_time, current_state, std::move(pending), event_armed,
                tolerance, false, processed, &event_pre_state);
            result.final_state = state_map(model, transaction.state);
            result.events = std::move(transaction.records);
            result.maximum_guard_residual = transaction.maximum_guard_residual;
            result.maximum_reset_error = transaction.maximum_reset_error;
        }
        result.success = true;
        result.message = "parameter-triggered continuous event fixed point completed";
    } catch (const std::exception& error) {
        result.message = error.what();
    }
    return result;
}

void validate_continuous_reference(
    ContinuousRunResult& result,
    const std::filesystem::path& reference_path) {
    std::ifstream input(reference_path);
    if (!input) throw std::runtime_error("cannot read continuous reference: " + reference_path.string());
    require_tag(input, "SMAVE_CONTINUOUS_REFERENCE");
    int version{};
    input >> version;
    if (version != 1) throw std::invalid_argument("unsupported continuous reference schema");
    require_tag(input, "EVENTS");
    std::size_t count{};
    input >> count;
    result.reference_order_matched = result.events.size() == count;
    result.reference_time_matched = result.events.size() == count;
    for (std::size_t index = 0; index < count; ++index) {
        require_tag(input, "EVENT");
        std::string id;
        double expected{};
        double tolerance{};
        input >> std::quoted(id) >> expected >> tolerance;
        if (!(tolerance > 0.0)) throw std::invalid_argument("continuous reference tolerance must be positive");
        if (index >= result.events.size()) continue;
        if (result.events[index].id != id) result.reference_order_matched = false;
        const double error = std::abs(result.events[index].time - expected);
        result.maximum_event_time_error = std::max(result.maximum_event_time_error, error);
        if (error > tolerance) result.reference_time_matched = false;
    }
    require_tag(input, "END");
}

void write_continuous_report(
    const ContinuousHybridIR& model,
    const ContinuousRunResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write continuous report: " + path.string());
    output << "SMAVE_CONTINUOUS_REPORT 1\n"
           << "MODEL " << std::quoted(model.model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(model.source_hash) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "FINAL_TIME " << std::setprecision(17) << result.final_time << '\n'
           << "ACCEPTED_STEPS " << result.accepted_steps << '\n'
           << "REJECTED_STEPS " << result.rejected_steps << '\n'
           << "MAX_SCALED_LOCAL_ERROR " << result.maximum_scaled_local_error << '\n'
           << "MAX_GUARD_RESIDUAL " << result.maximum_guard_residual << '\n'
           << "MAX_RESET_ERROR " << result.maximum_reset_error << '\n'
           << "REFERENCE_ORDER_MATCHED " << result.reference_order_matched << '\n'
           << "REFERENCE_TIME_MATCHED " << result.reference_time_matched << '\n'
           << "MAX_EVENT_TIME_ERROR " << result.maximum_event_time_error << '\n'
           << "GRAZING_EVENTS " << result.grazing_events << '\n'
           << "EVENTS " << result.events.size() << '\n';
    for (const auto& event : result.events) {
        output << "EVENT " << std::quoted(event.id) << ' ' << std::setprecision(17)
               << event.time << ' ' << event.guard_residual << ' '
               << event.grazing << ' ' << event.post_state.size();
        std::vector<std::pair<std::string, double>> ordered(event.post_state.begin(), event.post_state.end());
        std::sort(ordered.begin(), ordered.end());
        for (const auto& [name, value] : ordered) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        output << '\n';
    }
    output << "FINAL_STATE " << result.final_state.size();
    std::vector<std::pair<std::string, double>> ordered(result.final_state.begin(), result.final_state.end());
    std::sort(ordered.begin(), ordered.end());
    for (const auto& [name, value] : ordered) {
        output << ' ' << std::quoted(name) << ' ' << value;
    }
    output << "\nMESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
