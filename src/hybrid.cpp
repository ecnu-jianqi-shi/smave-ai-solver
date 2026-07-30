#include "smave/hybrid.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace smave {
namespace {

void require_tag(std::istream& input, std::string_view expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error(
            "invalid hybrid IR: expected " + std::string(expected) + ", got " + actual);
    }
}

std::unordered_map<std::string, double> evaluation_context(
    const std::unordered_map<std::string, double>& state, std::size_t tick,
    double sample_time, double sample_offset) {
    auto context = state;
    context["tick"] = static_cast<double>(tick);
    context["time"] = sample_offset + static_cast<double>(tick) * sample_time;
    return context;
}

const HybridModeIR& find_mode(const HybridProgramIR& program, const std::string& id) {
    const auto iterator = std::find_if(
        program.modes.begin(), program.modes.end(),
        [&](const HybridModeIR& mode) { return mode.id == id; });
    if (iterator == program.modes.end()) throw std::logic_error("unknown active mode: " + id);
    return *iterator;
}

const EventCandidate* candidate_for(
    const std::vector<EventCandidate>& candidates, std::size_t tick,
    const std::string& transition_id) {
    const auto iterator = std::find_if(
        candidates.begin(), candidates.end(), [&](const EventCandidate& candidate) {
            return candidate.tick == tick && candidate.transition_id == transition_id;
        });
    return iterator == candidates.end() ? nullptr : &*iterator;
}

}  // namespace

void HybridProgramIR::validate() const {
    if (schema_version != kHybridSchemaVersion) {
        throw std::invalid_argument("unsupported hybrid schema: " + schema_version);
    }
    if (!(sample_time > 0.0) || !std::isfinite(sample_time)) {
        throw std::invalid_argument("hybrid sample time must be finite and positive");
    }
    if (!(sample_offset >= 0.0) || !std::isfinite(sample_offset) ||
        sample_offset >= sample_time) {
        throw std::invalid_argument(
            "hybrid sample offset must be finite, non-negative, and less than sample time");
    }
    std::unordered_set<std::string> mode_ids;
    for (const auto& mode : modes) {
        if (!mode_ids.insert(mode.id).second) {
            throw std::invalid_argument("duplicate hybrid mode: " + mode.id);
        }
        for (const auto& [variable, expression] : mode.updates) {
            if (!initial_state.contains(variable)) {
                throw std::invalid_argument("mode update references unknown state: " + variable);
            }
            (void)Expression(expression);
        }
    }
    if (!mode_ids.contains(initial_mode)) {
        throw std::invalid_argument("unknown initial hybrid mode: " + initial_mode);
    }
    std::unordered_set<std::string> transition_ids;
    for (const auto& transition : transitions) {
        if (!transition_ids.insert(transition.id).second) {
            throw std::invalid_argument("duplicate hybrid transition: " + transition.id);
        }
        if (!mode_ids.contains(transition.source_mode) ||
            !mode_ids.contains(transition.target_mode)) {
            throw std::invalid_argument(transition.id + ": transition references unknown mode");
        }
        (void)Expression(transition.guard_expression);
        std::unordered_set<std::string> reset_variables;
        for (const auto& reset : transition.resets) {
            if (!initial_state.contains(reset.variable)) {
                throw std::invalid_argument(transition.id + ": reset references unknown state");
            }
            if (!reset_variables.insert(reset.variable).second) {
                throw std::invalid_argument(transition.id + ": duplicate reset variable");
            }
            (void)Expression(reset.expression);
        }
    }
}

HybridProgramIR HybridProgramIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read hybrid IR: " + path.string());
    require_tag(input, "SMAVE_HYBRID");
    int version{};
    input >> version;
    if (version != 1 && version != 2) {
        throw std::invalid_argument("unsupported hybrid IR version");
    }
    HybridProgramIR program;
    require_tag(input, "MODEL");
    input >> std::quoted(program.model_id);
    require_tag(input, "SAMPLE_TIME");
    input >> program.sample_time;
    if (version == 2) {
        require_tag(input, "SAMPLE_OFFSET");
        input >> program.sample_offset;
    }
    require_tag(input, "INITIAL_MODE");
    input >> std::quoted(program.initial_mode);
    require_tag(input, "STATE");
    std::size_t count{};
    input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name;
        double value{};
        input >> std::quoted(name) >> value;
        program.initial_state.emplace(std::move(name), value);
    }
    require_tag(input, "MODES");
    input >> count;
    program.modes.resize(count);
    for (auto& mode : program.modes) {
        require_tag(input, "MODE");
        std::size_t updates{};
        input >> std::quoted(mode.id) >> updates;
        for (std::size_t index = 0; index < updates; ++index) {
            std::string variable;
            std::string expression;
            input >> std::quoted(variable) >> std::quoted(expression);
            mode.updates.emplace(std::move(variable), std::move(expression));
        }
    }
    require_tag(input, "TRANSITIONS");
    input >> count;
    program.transitions.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto& transition = program.transitions[index];
        transition.source_order = index;
        std::size_t resets{};
        require_tag(input, "TRANSITION");
        input >> std::quoted(transition.id) >> std::quoted(transition.source_mode)
              >> std::quoted(transition.target_mode) >> transition.priority
              >> std::quoted(transition.guard_expression) >> resets;
        transition.resets.resize(resets);
        for (auto& reset : transition.resets) {
            input >> std::quoted(reset.variable) >> std::quoted(reset.expression);
        }
    }
    require_tag(input, "END");
    program.validate();
    return program;
}

std::vector<EventCandidate> read_event_candidates(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read event candidates: " + path.string());
    require_tag(input, "SMAVE_EVENT_CANDIDATES");
    int version{};
    input >> version;
    if (version != 1) throw std::invalid_argument("unsupported event candidate version");
    require_tag(input, "COUNT");
    std::size_t count{};
    input >> count;
    std::vector<EventCandidate> candidates(count);
    for (auto& candidate : candidates) {
        require_tag(input, "CANDIDATE");
        input >> candidate.tick >> std::quoted(candidate.transition_id)
              >> std::quoted(candidate.source_mode);
    }
    require_tag(input, "END");
    return candidates;
}

HybridRunResult run_hybrid(
    const HybridProgramIR& program, std::size_t ticks,
    const std::vector<EventCandidate>& candidates) {
    program.validate();
    HybridRunResult result;
    result.candidate_count = candidates.size();
    result.final_mode = program.initial_mode;
    result.final_state.insert(program.initial_state.begin(), program.initial_state.end());
    std::unordered_map<std::string, double> previous_guard;
    for (const auto& transition : program.transitions) previous_guard[transition.id] = 0.0;

    const std::size_t first_tick = program.sample_offset > 0.0 ? 0 : 1;
    const std::size_t final_tick = program.sample_offset > 0.0
        ? (ticks == 0 ? 0 : ticks - 1)
        : ticks;
    for (std::size_t tick = first_tick; ticks > 0 && tick <= final_tick; ++tick) {
        const auto pre_state = result.final_state;
        const auto pre_context = evaluation_context(
            pre_state, tick, program.sample_time, program.sample_offset);
        const auto& mode = find_mode(program, result.final_mode);
        std::unordered_map<std::string, double> updated = pre_state;
        for (const auto& [variable, source] : mode.updates) {
            updated[variable] = Expression(source).evaluate(pre_context);
        }
        result.final_state = updated;
        const auto guard_context = evaluation_context(
            result.final_state, tick, program.sample_time, program.sample_offset);
        std::vector<const HybridTransitionIR*> enabled;
        for (const auto& transition : program.transitions) {
            const double value = Expression(transition.guard_expression).evaluate(guard_context);
            const bool active = transition.source_mode == result.final_mode;
            const bool crossing = active &&
                previous_guard.at(transition.id) <= 0.0 && value > 0.0;
            previous_guard[transition.id] = active ? value : 0.0;
            if (crossing) enabled.push_back(&transition);
        }
        std::sort(enabled.begin(), enabled.end(), [](const auto* left, const auto* right) {
            if (left->priority != right->priority) return left->priority > right->priority;
            return left->source_order < right->source_order;
        });
        if (!enabled.empty()) {
            const auto& transition = *enabled.front();
            const auto event_pre_state = result.final_state;
            const auto reset_context = evaluation_context(
                event_pre_state, tick, program.sample_time, program.sample_offset);
            auto reset_state = event_pre_state;
            for (const auto& reset : transition.resets) {
                reset_state[reset.variable] = Expression(reset.expression).evaluate(reset_context);
            }
            const auto* candidate = candidate_for(candidates, tick, transition.id);
            const bool candidate_accepted = candidate != nullptr &&
                candidate->source_mode == transition.source_mode;
            result.events.push_back(HybridEventRecord{
                tick, program.sample_offset + tick * program.sample_time,
                transition.id, transition.source_mode,
                transition.target_mode, candidate != nullptr, candidate_accepted});
            if (candidate_accepted) ++result.accepted_candidates;
            result.final_state = std::move(reset_state);
            result.final_mode = transition.target_mode;
        }
    }
    for (const auto& candidate : candidates) {
        const bool accepted = std::any_of(
            result.events.begin(), result.events.end(), [&](const HybridEventRecord& event) {
                return event.tick == candidate.tick &&
                    event.transition_id == candidate.transition_id && event.candidate_accepted;
            });
        if (!accepted) ++result.rejected_candidates;
    }
    result.missed_events = std::count_if(
        result.events.begin(), result.events.end(),
        [](const HybridEventRecord& event) { return !event.candidate_accepted; });
    result.event_recall = result.events.empty()
        ? 1.0
        : static_cast<double>(result.events.size() - result.missed_events) /
            static_cast<double>(result.events.size());
    result.success = true;
    result.message = "authoritative sampled guards and atomic resets completed";
    return result;
}

void write_hybrid_report(
    const HybridProgramIR& program, const HybridRunResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write hybrid report: " + path.string());
    output << "SMAVE_HYBRID_REPORT 2\n"
           << "MODEL " << std::quoted(program.model_id) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "FINAL_MODE " << std::quoted(result.final_mode) << '\n'
           << "CANDIDATES " << result.candidate_count << '\n'
           << "ACCEPTED_CANDIDATES " << result.accepted_candidates << '\n'
           << "REJECTED_CANDIDATES " << result.rejected_candidates << '\n'
           << "MISSED_EVENTS " << result.missed_events << '\n'
           << "EVENT_RECALL " << std::setprecision(17) << result.event_recall << '\n'
           << "MAX_EVENT_TIME_ERROR " << std::setprecision(17)
           << result.maximum_event_time_error << '\n'
           << "MAX_RESET_ERROR " << result.maximum_reset_error << '\n'
           << "EVENTS " << result.events.size() << '\n';
    for (const auto& event : result.events) {
        output << "EVENT " << event.tick << ' ' << std::setprecision(17) << event.time << ' '
               << std::quoted(event.transition_id) << ' ' << std::quoted(event.source_mode) << ' '
               << std::quoted(event.target_mode) << ' ' << event.candidate_present << ' '
               << event.candidate_accepted << '\n';
    }
    output << "STATE " << result.final_state.size();
    for (const auto& [name, value] : result.final_state) {
        output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << value;
    }
    output << "\nMESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
