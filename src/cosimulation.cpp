#include "smave/cosimulation.hpp"

#include "smave/expression.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace smave {
namespace {

const HybridModeIR& find_mode(const HybridProgramIR& program, const std::string& id) {
    const auto iterator = std::find_if(
        program.modes.begin(), program.modes.end(),
        [&](const HybridModeIR& mode) { return mode.id == id; });
    if (iterator == program.modes.end()) throw std::logic_error("unknown active mode: " + id);
    return *iterator;
}

const EventCandidate* candidate_for(
    const std::vector<EventCandidate>& candidates,
    std::size_t tick,
    const std::string& transition_id) {
    const auto iterator = std::find_if(
        candidates.begin(), candidates.end(), [&](const EventCandidate& candidate) {
            return candidate.tick == tick && candidate.transition_id == transition_id;
        });
    return iterator == candidates.end() ? nullptr : &*iterator;
}

std::unordered_map<std::string, double> context(
    const std::unordered_map<std::string, double>& discrete,
    const std::unordered_map<std::string, double>& continuous,
    std::size_t tick,
    double time) {
    auto values = discrete;
    values.insert(continuous.begin(), continuous.end());
    values["tick"] = static_cast<double>(tick);
    values["time"] = time;
    return values;
}

void validate_coupling(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled) {
    continuous.validate();
    sampled.validate();
    std::unordered_set<std::string> continuous_states;
    for (const auto& state : continuous.states) continuous_states.insert(state.name);
    std::unordered_set<std::string> allowed{"tick", "time"};
    allowed.insert(continuous_states.begin(), continuous_states.end());
    for (const auto& [name, value] : sampled.initial_state) {
        (void)value;
        if (continuous_states.contains(name)) {
            throw std::invalid_argument(
                name + ": coupled continuous and discrete states must have distinct names");
        }
        if (!continuous.parameters.contains(name)) {
            throw std::invalid_argument(
                name + ": sampled state requires a same-name continuous parameter for hold coupling");
        }
        allowed.insert(name);
    }
    const auto validate_names = [&](const std::string& source, const std::string& purpose) {
        const Expression expression(source);
        for (const auto& name : expression.names()) {
            if (!allowed.contains(name)) {
                throw std::invalid_argument(purpose + " references uncoupled name: " + name);
            }
        }
    };
    for (const auto& mode : sampled.modes) {
        for (const auto& [variable, expression] : mode.updates) {
            validate_names(expression, mode.id + ": update " + variable);
        }
    }
    for (const auto& transition : sampled.transitions) {
        validate_names(transition.guard_expression, transition.id + ": guard");
        for (const auto& reset : transition.resets) {
            validate_names(reset.expression, transition.id + ": reset " + reset.variable);
        }
    }
}

void set_segment_state(
    ContinuousHybridIR& segment,
    const std::unordered_map<std::string, double>& state,
    const std::unordered_map<std::string, double>& discrete) {
    for (auto& item : segment.states) item.start = state.at(item.name);
    for (const auto& [name, value] : discrete) segment.parameters.at(name) = value;
}

struct BoundaryTransactionResult {
    std::unordered_map<std::string, double> continuous_state;
    std::unordered_map<std::string, double> discrete_state;
    std::string mode;
    std::vector<ContinuousEventRecord> continuous_events;
    std::vector<HybridEventRecord> sampled_events;
    std::vector<CoupledMicrostepRecord> superdense_steps;
    std::unordered_map<std::string, double> guards;
    std::size_t microsteps{0};
    std::size_t iterations{0};
    double maximum_guard_residual{0.0};
    double maximum_reset_error{0.0};
};

BoundaryTransactionResult execute_boundary_transaction(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled,
    std::size_t tick,
    double time,
    const std::unordered_map<std::string, double>& continuous_state,
    const std::unordered_map<std::string, double>& discrete_state,
    const std::string& mode,
    const std::unordered_map<std::string, double>& previous_guard,
    const std::vector<EventCandidate>& candidates,
    const ContinuousTolerance& tolerance,
    bool initialization,
    const std::vector<std::string>& prior_continuous_events = {},
    const std::unordered_map<std::string, double>* boundary_pre_state = nullptr) {
    BoundaryTransactionResult transaction;
    transaction.continuous_state = continuous_state;
    transaction.discrete_state = discrete_state;
    transaction.mode = mode;
    const auto physical_pre_state = boundary_pre_state == nullptr
        ? continuous_state
        : *boundary_pre_state;
    const auto update_context = context(discrete_state, continuous_state, tick, time);
    for (const auto& [variable, source] : find_mode(sampled, mode).updates) {
        transaction.discrete_state[variable] = Expression(source).evaluate(update_context);
    }
    for (const auto& [name, value] : transaction.discrete_state) {
        if (!std::isfinite(value)) {
            throw std::runtime_error(name + ": sampled update produced NaN/Inf");
        }
    }

    std::unordered_set<std::string> processed_transitions;
    std::vector<std::string> processed_continuous_events = prior_continuous_events;
    const auto select_transition = [&](const std::unordered_map<std::string, double>& before_state,
                                       bool first_iteration)
        -> const HybridTransitionIR* {
        const auto before = context(
            transaction.discrete_state, before_state, tick, time);
        const auto after = context(
            transaction.discrete_state, transaction.continuous_state, tick, time);
        std::vector<const HybridTransitionIR*> enabled;
        for (const auto& transition : sampled.transitions) {
            if (transition.source_mode != transaction.mode ||
                processed_transitions.contains(transition.id)) {
                continue;
            }
            const double after_value = Expression(transition.guard_expression).evaluate(after);
            bool activated{};
            if (first_iteration) {
                activated = initialization
                    ? after_value > 0.0
                    : previous_guard.at(transition.id) <= 0.0 && after_value > 0.0;
            } else {
                const double before_value =
                    Expression(transition.guard_expression).evaluate(before);
                activated = before_value <= 0.0 && after_value > 0.0;
            }
            if (activated) enabled.push_back(&transition);
        }
        std::sort(enabled.begin(), enabled.end(), [](const auto* left, const auto* right) {
            if (left->priority != right->priority) return left->priority > right->priority;
            return left->source_order < right->source_order;
        });
        return enabled.empty() ? nullptr : enabled.front();
    };

    const HybridTransitionIR* pending = select_transition(continuous_state, true);
    while (pending != nullptr) {
        if (++transaction.iterations > sampled.transitions.size() + continuous.events.size()) {
            throw std::runtime_error(
                "cross-domain superdense iteration did not reach a fixed point");
        }
        const auto before_continuous = transaction.continuous_state;
        const auto before_discrete = transaction.discrete_state;
        const auto reset_context = context(
            before_discrete, before_continuous, tick, time);
        auto reset_state = before_discrete;
        for (const auto& reset : pending->resets) {
            const double value = Expression(reset.expression).evaluate(reset_context);
            if (!std::isfinite(value)) {
                throw std::runtime_error(pending->id + ": sampled reset produced NaN/Inf");
            }
            reset_state[reset.variable] = value;
        }
        const auto* candidate = candidate_for(candidates, tick, pending->id);
        const bool accepted = candidate != nullptr &&
            candidate->source_mode == pending->source_mode;
        transaction.sampled_events.push_back({
            tick, time, pending->id, pending->source_mode, pending->target_mode,
            candidate != nullptr, accepted});
        transaction.superdense_steps.push_back({
            tick, time, transaction.superdense_steps.size(), "sampled", pending->id});
        processed_transitions.insert(pending->id);
        transaction.discrete_state = std::move(reset_state);
        transaction.mode = pending->target_mode;
        ++transaction.microsteps;

        ContinuousHybridIR parameterized = continuous;
        set_segment_state(
            parameterized, transaction.continuous_state, transaction.discrete_state);
        auto previous_parameters = parameterized.parameters;
        for (const auto& [name, value] : before_discrete) previous_parameters.at(name) = value;
        const auto boundary_events = process_continuous_parameter_events(
            parameterized, previous_parameters, time, transaction.continuous_state,
            physical_pre_state, processed_continuous_events, tolerance);
        if (!boundary_events.success) {
            throw std::runtime_error(
                "parameter-triggered continuous event failed: " + boundary_events.message);
        }
        transaction.continuous_state = boundary_events.final_state;
        transaction.continuous_events.insert(
            transaction.continuous_events.end(), boundary_events.events.begin(),
            boundary_events.events.end());
        for (const auto& event : boundary_events.events) {
            processed_continuous_events.push_back(event.id);
            transaction.superdense_steps.push_back({
                tick, time, transaction.superdense_steps.size(), "continuous", event.id});
            ++transaction.microsteps;
        }
        transaction.maximum_guard_residual = std::max(
            transaction.maximum_guard_residual,
            boundary_events.maximum_guard_residual);
        transaction.maximum_reset_error = std::max(
            transaction.maximum_reset_error,
            boundary_events.maximum_reset_error);
        pending = select_transition(before_continuous, false);
    }

    const auto final_context = context(
        transaction.discrete_state, transaction.continuous_state, tick, time);
    for (const auto& transition : sampled.transitions) {
        transaction.guards[transition.id] = transition.source_mode == transaction.mode
            ? Expression(transition.guard_expression).evaluate(final_context)
            : 0.0;
    }
    return transaction;
}

void write_values(
    std::ostream& output,
    std::string_view tag,
    const std::map<std::string, double>& order,
    const std::unordered_map<std::string, double>& values) {
    output << tag << ' ' << values.size();
    for (const auto& [name, unused] : order) {
        (void)unused;
        output << ' ' << std::quoted(name) << ' ' << std::setprecision(17) << values.at(name);
    }
    output << '\n';
}

}  // namespace

CoupledRunResult simulate_coupled(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled,
    double end_time,
    double maximum_step,
    ContinuousTolerance tolerance,
    const std::vector<EventCandidate>& candidates) {
    CoupledRunResult result;
    result.final_mode = sampled.initial_mode;
    result.final_discrete_state.insert(
        sampled.initial_state.begin(), sampled.initial_state.end());
    for (const auto& state : continuous.states) {
        result.final_continuous_state[state.name] = state.start;
    }
    double time = 0.0;
    try {
        validate_coupling(continuous, sampled);
        if (!(end_time > 0.0) || !std::isfinite(end_time)) {
            throw std::invalid_argument("coupled end time must be finite and positive");
        }
        if (!(maximum_step > 0.0) || !std::isfinite(maximum_step)) {
            throw std::invalid_argument("coupled maximum step must be finite and positive");
        }
        std::unordered_map<std::string, double> previous_guard;
        for (const auto& transition : sampled.transitions) previous_guard[transition.id] = 0.0;
        bool sampled_initialized = sampled.sample_offset == 0.0;
        {
            ContinuousHybridIR initialization = continuous;
            set_segment_state(
                initialization, result.final_continuous_state,
                result.final_discrete_state);
            const auto initialized = simulate_continuous(
                initialization, 0.0, 0.0, maximum_step,
                tolerance, true);
            if (!initialized.success) {
                throw std::runtime_error(
                    "continuous initialization failed: " + initialized.message);
            }
            if (sampled_initialized) {
                const auto pre_discrete = result.final_discrete_state;
                const auto pre_mode = result.final_mode;
                std::vector<std::string> initialized_event_ids;
                for (const auto& event : initialized.events) {
                    initialized_event_ids.push_back(event.id);
                }
                const auto boundary = execute_boundary_transaction(
                    continuous, sampled, 0, 0.0, initialized.final_state,
                    pre_discrete, pre_mode, previous_guard, candidates, tolerance,
                    true, initialized_event_ids, &result.final_continuous_state);
                result.final_continuous_state = boundary.continuous_state;
                result.continuous_events = initialized.events;
                result.continuous_events.insert(
                    result.continuous_events.end(), boundary.continuous_events.begin(),
                    boundary.continuous_events.end());
                result.sampled_events = boundary.sampled_events;
                result.superdense_steps = boundary.superdense_steps;
                result.maximum_guard_residual = std::max(
                    initialized.maximum_guard_residual, boundary.maximum_guard_residual);
                result.maximum_reset_error = std::max(
                    initialized.maximum_reset_error, boundary.maximum_reset_error);
                result.superdense_microsteps += boundary.microsteps;
                result.maximum_superdense_iterations = std::max(
                    result.maximum_superdense_iterations, boundary.iterations);
                result.samples.push_back({
                    0, 0.0, pre_mode, boundary.mode, boundary.continuous_state,
                    pre_discrete, boundary.discrete_state});
                result.final_discrete_state = boundary.discrete_state;
                result.final_mode = boundary.mode;
                previous_guard = boundary.guards;
            } else {
                result.continuous_events = initialized.events;
                result.final_continuous_state = initialized.final_state;
                result.maximum_guard_residual = initialized.maximum_guard_residual;
                result.maximum_reset_error = initialized.maximum_reset_error;
            }
        }
        std::size_t tick = sampled.sample_offset > 0.0 ? 0 : 1;
        while (time < end_time) {
            const double sample_boundary = sampled.sample_offset +
                static_cast<double>(tick) * sampled.sample_time;
            const double boundary = std::min(end_time, sample_boundary);
            ContinuousHybridIR segment = continuous;
            set_segment_state(
                segment, result.final_continuous_state, result.final_discrete_state);
            const auto continuous_result = simulate_continuous(
                segment, time, boundary, maximum_step, tolerance, false);
            result.accepted_steps += continuous_result.accepted_steps;
            result.rejected_steps += continuous_result.rejected_steps;
            result.maximum_scaled_local_error = std::max(
                result.maximum_scaled_local_error,
                continuous_result.maximum_scaled_local_error);
            result.maximum_guard_residual = std::max(
                result.maximum_guard_residual,
                continuous_result.maximum_guard_residual);
            result.maximum_reset_error = std::max(
                result.maximum_reset_error,
                continuous_result.maximum_reset_error);
            const auto event_count_before_segment = result.continuous_events.size();
            result.continuous_events.insert(
                result.continuous_events.end(),
                continuous_result.events.begin(), continuous_result.events.end());
            result.final_continuous_state = continuous_result.final_state;
            time = continuous_result.final_time;
            result.final_time = time;
            if (!continuous_result.success) {
                throw std::runtime_error(
                    "continuous segment failed: " + continuous_result.message);
            }
            if (boundary < sample_boundary) break;

            const auto pre_discrete = result.final_discrete_state;
            const auto pre_mode = result.final_mode;
            std::unordered_map<std::string, double> boundary_pre_state =
                result.final_continuous_state;
            std::size_t committed_segment_events = continuous_result.events.size();
            for (std::size_t index = 0; index < continuous_result.events.size(); ++index) {
                if (std::abs(continuous_result.events[index].time - boundary) <=
                    tolerance.root_time) {
                    boundary_pre_state = continuous_result.events[index].pre_state;
                    committed_segment_events = index;
                    break;
                }
            }
            BoundaryTransactionResult transaction;
            try {
                transaction = execute_boundary_transaction(
                    continuous, sampled, tick, boundary, result.final_continuous_state,
                    pre_discrete, pre_mode, previous_guard, candidates, tolerance,
                    !sampled_initialized,
                    {}, &boundary_pre_state);
            } catch (...) {
                result.continuous_events.resize(
                    event_count_before_segment + committed_segment_events);
                result.final_continuous_state = std::move(boundary_pre_state);
                throw;
            }
            result.continuous_events.insert(
                result.continuous_events.end(), transaction.continuous_events.begin(),
                transaction.continuous_events.end());
            result.sampled_events.insert(
                result.sampled_events.end(), transaction.sampled_events.begin(),
                transaction.sampled_events.end());
            result.superdense_steps.insert(
                result.superdense_steps.end(), transaction.superdense_steps.begin(),
                transaction.superdense_steps.end());
            result.maximum_guard_residual = std::max(
                result.maximum_guard_residual, transaction.maximum_guard_residual);
            result.maximum_reset_error = std::max(
                result.maximum_reset_error, transaction.maximum_reset_error);
            result.superdense_microsteps += transaction.microsteps;
            result.maximum_superdense_iterations = std::max(
                result.maximum_superdense_iterations, transaction.iterations);
            result.samples.push_back({
                tick, boundary, pre_mode, transaction.mode, transaction.continuous_state,
                pre_discrete, transaction.discrete_state});
            result.final_continuous_state = transaction.continuous_state;
            result.final_discrete_state = transaction.discrete_state;
            result.final_mode = transaction.mode;
            previous_guard = transaction.guards;
            sampled_initialized = true;
            ++tick;
        }
        for (const auto& candidate : candidates) {
            const bool accepted = std::any_of(
                result.sampled_events.begin(), result.sampled_events.end(),
                [&](const HybridEventRecord& event) {
                    return event.tick == candidate.tick &&
                        event.transition_id == candidate.transition_id &&
                        event.candidate_accepted;
                });
            if (!accepted) ++result.rejected_candidates;
        }
        result.success = true;
        result.message =
            "atomic cross-domain superdense fixed-point scheduling completed";
    } catch (const std::exception& error) {
        result.final_time = time;
        result.message = error.what();
    }
    return result;
}

void write_coupled_report(
    const ContinuousHybridIR& continuous,
    const HybridProgramIR& sampled,
    const CoupledRunResult& result,
    const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write coupled report: " + path.string());
    output << "SMAVE_COUPLED_REPORT 1\n"
           << "CONTINUOUS_MODEL " << std::quoted(continuous.model_id) << '\n'
           << "SAMPLED_MODEL " << std::quoted(sampled.model_id) << '\n'
           << "SAMPLE_TIME " << std::setprecision(17) << sampled.sample_time << '\n'
           << "SAMPLE_OFFSET " << sampled.sample_offset << '\n'
           << "SUCCESS " << result.success << '\n'
           << "FINAL_TIME " << std::setprecision(17) << result.final_time << '\n'
           << "FINAL_MODE " << std::quoted(result.final_mode) << '\n'
           << "ACCEPTED_STEPS " << result.accepted_steps << '\n'
           << "REJECTED_STEPS " << result.rejected_steps << '\n'
           << "REJECTED_CANDIDATES " << result.rejected_candidates << '\n'
           << "SUPERDENSE_MICROSTEPS " << result.superdense_microsteps << '\n'
           << "MAX_SUPERDENSE_ITERATIONS " << result.maximum_superdense_iterations << '\n'
           << "MAX_LOCAL_ERROR " << result.maximum_scaled_local_error << '\n'
           << "MAX_GUARD_RESIDUAL " << result.maximum_guard_residual << '\n'
           << "MAX_RESET_ERROR " << result.maximum_reset_error << '\n';
    std::map<std::string, double> continuous_order;
    for (const auto& state : continuous.states) continuous_order[state.name] = state.start;
    write_values(output, "FINAL_CONTINUOUS", continuous_order, result.final_continuous_state);
    write_values(output, "FINAL_DISCRETE", sampled.initial_state, result.final_discrete_state);
    output << "CONTINUOUS_EVENTS " << result.continuous_events.size() << '\n';
    for (const auto& event : result.continuous_events) {
        output << "CONTINUOUS_EVENT " << std::quoted(event.id) << ' '
               << std::setprecision(17) << event.time << ' ' << event.guard_residual << '\n';
    }
    output << "SAMPLED_EVENTS " << result.sampled_events.size() << '\n';
    for (const auto& event : result.sampled_events) {
        output << "SAMPLED_EVENT " << event.tick << ' ' << std::setprecision(17) << event.time
               << ' ' << std::quoted(event.transition_id) << ' '
               << std::quoted(event.source_mode) << ' ' << std::quoted(event.target_mode)
               << ' ' << event.candidate_present << ' ' << event.candidate_accepted << '\n';
    }
    output << "SAMPLES " << result.samples.size() << '\n';
    for (const auto& sample : result.samples) {
        output << "SAMPLE " << sample.tick << ' ' << std::setprecision(17) << sample.time
               << ' ' << std::quoted(sample.pre_mode) << ' ' << std::quoted(sample.post_mode);
        for (const auto& state : continuous.states) {
            output << ' ' << std::quoted(state.name) << ' '
                   << sample.continuous_state.at(state.name);
        }
        for (const auto& [name, value] : sampled.initial_state) {
            (void)value;
            output << ' ' << std::quoted(name) << ' '
                   << sample.post_discrete_state.at(name);
        }
        output << '\n';
    }
    output << "SUPERDENSE_STEPS " << result.superdense_steps.size() << '\n';
    for (const auto& step : result.superdense_steps) {
        output << "SUPERDENSE_STEP " << step.tick << ' ' << std::setprecision(17)
               << step.time << ' ' << step.ordinal << ' ' << std::quoted(step.domain)
               << ' ' << std::quoted(step.event_id) << '\n';
    }
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
