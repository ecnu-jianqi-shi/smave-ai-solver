#include "smave/compiler.hpp"
#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::array<int, 4> budgets{0, 2, 3, 4};
constexpr std::size_t top_k = 3;

enum class Profile { low, middle, high };

struct FamilySpec {
    std::string name;
    int power{};
    double family_code{};
};

struct ActionSpec {
    std::string expert;
    int budget{};

    [[nodiscard]] auto key() const { return std::tie(expert, budget); }
};

struct Scenario {
    std::size_t index{};
    std::unordered_map<std::string, double> values;
};

struct ActionObservation {
    ActionSpec action;
    std::vector<double> features;
    double attempt_wall_us{};
    bool passed{false};
    bool execution_failure{false};
    bool gate_mismatch{false};
};

struct RequestObservation {
    FamilySpec family;
    Scenario scenario;
    std::vector<ActionObservation> actions;
    double terminal_wall_us{};
};

struct StaticProfile {
    double median_cost{};
    double pass_probability{};
    std::size_t attempts{};
    std::size_t passes{};
};

class ConditionedBudgetExpert final : public smave::Expert {
public:
    ConditionedBudgetExpert(std::string version, Profile profile, int power)
        : version_(std::move(version)), profile_(profile), power_(power) {}

    [[nodiscard]] std::string version() const override { return version_; }

    [[nodiscard]] smave::Capability match(const smave::BlockIR& block) const override {
        return smave::Capability{
            .nonlinear = !block.linear,
            .backend_roles = {smave::BackendRole::initializer},
            .evidence_level = smave::EvidenceLevel::e2,
            .maximum_permission = smave::Permission::warm_start,
        };
    }

    [[nodiscard]] smave::Estimate estimate(
        const smave::BlockIR&, const smave::BlockContext&) const override {
        return smave::Estimate{
            .pass_probability = 0.5,
            .expected_solve_time_us = 1.0,
            .failure_cost_us = 20.0,
            .risk_score = 0.0,
        };
    }

    [[nodiscard]] smave::ExpertResult solve(
        const smave::BlockIR& block,
        const smave::BlockContext& context,
        const smave::SolveBudget&) const override {
        const double root = context.values.at("root");
        const double regime = context.values.at("regime");
        const double center_distance = context.values.at("center_distance");
        const double family_code = context.values.at("family_code");
        const double score = profile_ == Profile::low
            ? regime
            : (profile_ == Profile::high ? 1.0 - regime : 2.0 * center_distance);
        const bool exact = score <= 0.095;
        const double family_offset = 0.008 + 0.012 * static_cast<double>(power_ - 2);
        const double offset = exact ? 0.0 : family_offset + 0.075 * score;

        const double profile_work = profile_ == Profile::middle ? 0.12 : 0.0;
        const int work_items = static_cast<int>(9000.0 * std::exp(
            profile_work + 0.75 * score + 0.15 * family_code));
        double work = 0.5;
        for (int index = 0; index < work_items; ++index) {
            work = std::fma(
                work, 1.0000000001,
                std::sin(0.0007 * static_cast<double>(index + 1) + root) * 1.0e-9);
        }

        std::unordered_map<std::string, double> candidate;
        for (const auto& unknown : block.unknowns) {
            candidate.emplace(unknown, root - offset);
        }
        return smave::ExpertResult{
            .candidate = std::move(candidate),
            .status = "candidate",
            .uncertainty = offset,
            .telemetry = {{"synthetic_work", work}, {"profile_score", score}},
        };
    }

private:
    std::string version_;
    Profile profile_{};
    int power_{};
};

double median(std::vector<double> values) {
    if (values.empty()) throw std::invalid_argument("median requires samples");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

double quantile(std::vector<double> values, double probability) {
    if (values.empty() || probability < 0.0 || probability > 1.0) {
        throw std::invalid_argument("invalid quantile request");
    }
    std::sort(values.begin(), values.end());
    const double position = probability * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

std::string expert_version(Profile profile) {
    switch (profile) {
        case Profile::low: return "conditioned-low-v1";
        case Profile::middle: return "conditioned-middle-v1";
        case Profile::high: return "conditioned-high-v1";
    }
    throw std::invalid_argument("unknown conditioned profile");
}

std::vector<ActionSpec> action_specs() {
    std::vector<ActionSpec> actions;
    for (const auto profile : {Profile::low, Profile::middle, Profile::high}) {
        for (const int budget : budgets) {
            actions.push_back(ActionSpec{.expert = expert_version(profile), .budget = budget});
        }
    }
    return actions;
}

std::vector<Scenario> scenarios(
    const FamilySpec& family,
    std::size_t count,
    double phase,
    double root_shift) {
    std::vector<Scenario> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double regime = (static_cast<double>(index) + phase) /
            static_cast<double>(count);
        const double root = 1.15 + root_shift +
            0.75 * static_cast<double>((index * 11 + family.power * 3) % count) /
                static_cast<double>(count - 1);
        result.push_back(Scenario{
            .index = index,
            .values = {
                {"root", root},
                {"regime", regime},
                {"center_distance", std::abs(regime - 0.5)},
                {"family_code", family.family_code},
            },
        });
    }
    return result;
}

smave::ModelIR build_model(
    const std::filesystem::path& directory,
    const FamilySpec& family) {
    std::filesystem::create_directories(directory);
    const auto source = directory / (family.name + ".mo");
    std::ofstream model(source);
    model << "model " << family.name << "\n"
          << "  parameter Real root = 1.5;\n"
          << "  parameter Real regime = 0.5;\n"
          << "  parameter Real center_distance = 0.0;\n"
          << "  parameter Real family_code = " << family.family_code << ";\n";
    constexpr int dimension = 12;
    for (int index = 0; index < dimension; ++index) {
        model << "  Real x" << index + 1 << "(start = 1.0);\n";
    }
    model << "equation\n";
    for (int index = 0; index < dimension; ++index) {
        const int next = (index + 1) % dimension;
        model << "  ";
        for (int factor = 0; factor < family.power; ++factor) {
            if (factor != 0) model << "*";
            model << "x" << index + 1;
        }
        model << " + 0.01*(x" << next + 1 << "-x" << index + 1 << ") = ";
        for (int factor = 0; factor < family.power; ++factor) {
            if (factor != 0) model << "*";
            model << "root";
        }
        model << " + 0*regime + 0*center_distance + 0*family_code;\n";
    }
    model << "end " << family.name << ";\n";
    model.close();
    return smave::compile_model(source);
}

struct FamilyRuntime {
    FamilySpec spec;
    smave::ModelIR model;
    smave::Registry registry;
    smave::RuntimeBundle bundle;
};

FamilyRuntime build_family_runtime(
    const std::filesystem::path& directory,
    const FamilySpec& family) {
    auto model = build_model(directory, family);
    auto registry = smave::make_default_registry(model);
    for (const auto profile : {Profile::low, Profile::middle, Profile::high}) {
        auto expert = std::make_shared<ConditionedBudgetExpert>(
            expert_version(profile), profile, family.power);
        registry.register_expert(
            expert,
            smave::ExpertGrant{
                .expert_version = expert->version(),
                .block_family = "*",
                .permission = smave::Permission::warm_start,
                .evidence_level = smave::EvidenceLevel::e2,
                .evidence_bundle = expert->version() + "-evidence",
                .artifact_hash = expert->version() + "-artifact",
            });
    }
    auto bundle = smave::make_default_bundle(model);
    for (const auto profile : {Profile::low, Profile::middle, Profile::high}) {
        const std::string version = expert_version(profile);
        bundle.add_expert(version, version + "-artifact", version + "-evidence");
    }
    registry.validate_bundle(bundle, model);
    return FamilyRuntime{
        .spec = family,
        .model = std::move(model),
        .registry = std::move(registry),
        .bundle = std::move(bundle),
    };
}

smave::RoutingConfig forced_action_routing(
    const smave::ModelIR& model,
    const ActionSpec& action) {
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.risk_weight = 0.0;
    routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    routing.calibration_winner = action.expert;
    routing.calibrated_terminal_fallback_cost_us = 1000.0;
    routing.expert_allowlist = {action.expert};
    routing.calibrations[action.expert].budget_options = {
        smave::RouteBudgetCalibration{
            .work_iterations = action.budget,
            .attempts = 1,
            .passes = 1,
            .pass_probability = 1.0,
            .median_attempt_wall_us = 1.0,
        },
    };
    return routing;
}

smave::RoutingConfig fallback_routing(const smave::ModelIR& model) {
    smave::RoutingConfig routing;
    routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    routing.calibration_winner = "original-damped-newton";
    return routing;
}

std::vector<double> features(
    const std::vector<std::string>& names,
    const smave::ModelIR& model,
    const Scenario& scenario) {
    smave::BlockContext context;
    context.values = scenario.values;
    return smave::extract_routing_features(names, model.blocks.front(), context);
}

ActionObservation observe_action(
    const FamilyRuntime& family,
    const ActionSpec& action,
    const Scenario& scenario,
    const std::vector<std::string>& feature_names,
    const std::filesystem::path& traces,
    int repetitions) {
    const smave::Runtime runtime(
        family.model, family.registry, family.bundle, {},
        forced_action_routing(family.model, action));
    std::vector<double> costs;
    std::vector<bool> passes;
    bool execution_failure = false;
    bool gate_mismatch = false;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto outcome = runtime.solve(
            scenario.values, traces / std::to_string(repetition));
        if (!outcome.success || outcome.blocks.empty() ||
            outcome.blocks.front().attempt_records.empty()) {
            execution_failure = true;
            continue;
        }
        const auto& block = outcome.blocks.front();
        const auto& attempt = block.attempt_records.front();
        if (attempt.expert_version != action.expert) {
            throw std::runtime_error("forced action trace selected the wrong expert");
        }
        const bool passed = attempt.outcome == "accepted";
        passes.push_back(passed);
        if (passed && block.gate.decision != smave::GateDecision::direct_accept) {
            gate_mismatch = true;
        }
        costs.push_back(std::max(
            1.0e-9, block.timing.total_us - block.timing.fallback_us));
    }
    if (execution_failure || costs.size() != static_cast<std::size_t>(repetitions) ||
        !std::all_of(passes.begin(), passes.end(), [&](bool value) {
            return value == passes.front();
        })) {
        throw std::runtime_error("forced action observation was not deterministic");
    }
    return ActionObservation{
        .action = action,
        .features = features(feature_names, family.model, scenario),
        .attempt_wall_us = median(std::move(costs)),
        .passed = passes.front(),
        .execution_failure = execution_failure,
        .gate_mismatch = gate_mismatch,
    };
}

double observe_terminal(
    const FamilyRuntime& family,
    const Scenario& scenario,
    const std::filesystem::path& traces,
    int repetitions) {
    const smave::Runtime runtime(
        family.model, family.registry, family.bundle, {},
        fallback_routing(family.model));
    std::vector<double> costs;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        const auto outcome = runtime.solve(
            scenario.values, traces / std::to_string(repetition));
        if (!outcome.success || outcome.blocks.empty() ||
            outcome.blocks.front().path != smave::SolvePath::full_fallback) {
            throw std::runtime_error("terminal fallback observation failed");
        }
        costs.push_back(outcome.blocks.front().timing.total_us);
    }
    return median(std::move(costs));
}

std::vector<RequestObservation> observe_split(
    const std::vector<FamilyRuntime>& families,
    const std::vector<ActionSpec>& actions,
    const std::vector<std::string>& feature_names,
    std::size_t scenario_count,
    double phase,
    double root_shift,
    const std::filesystem::path& traces,
    int repetitions) {
    std::vector<RequestObservation> requests;
    for (const auto& family : families) {
        for (const auto& scenario : scenarios(
                 family.spec, scenario_count, phase, root_shift)) {
            RequestObservation request{
                .family = family.spec,
                .scenario = scenario,
            };
            for (const auto& action : actions) {
                request.actions.push_back(observe_action(
                    family, action, scenario, feature_names,
                    traces / family.spec.name / std::to_string(scenario.index) /
                        action.expert / std::to_string(action.budget),
                    repetitions));
            }
            request.terminal_wall_us = observe_terminal(
                family, scenario,
                traces / family.spec.name / std::to_string(scenario.index) / "fallback",
                repetitions);
            requests.push_back(std::move(request));
        }
    }
    return requests;
}

std::vector<smave::RouteActionTrainingSample> training_samples(
    const std::vector<RequestObservation>& requests) {
    std::vector<smave::RouteActionTrainingSample> result;
    for (const auto& request : requests) {
        for (const auto& action : request.actions) {
            result.push_back(smave::RouteActionTrainingSample{
                .expert_version = action.action.expert,
                .work_iterations = action.action.budget,
                .independent_group = request.family.name + "-" +
                    std::to_string(request.scenario.index),
                .routing_family = request.family.name,
                .features = action.features,
                .attempt_wall_us = action.attempt_wall_us,
                .passed = action.passed,
            });
        }
    }
    return result;
}

void write_observations(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, const std::vector<RequestObservation>*>>&
        splits) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write conditioned action observations");
    output << std::setprecision(17)
           << "split\tfamily\tscenario\troot\tregime\tcenter_distance\tfamily_code"
              "\texpert\tbudget\tattempt_wall_us\taccepted\texecution_failure"
              "\tgate_mismatch\tterminal_wall_us\n";
    for (const auto& [split, requests] : splits) {
        for (const auto& request : *requests) {
            for (const auto& action : request.actions) {
                output << split << '\t' << request.family.name << '\t'
                       << request.scenario.index << '\t'
                       << request.scenario.values.at("root") << '\t'
                       << request.scenario.values.at("regime") << '\t'
                       << request.scenario.values.at("center_distance") << '\t'
                       << request.scenario.values.at("family_code") << '\t'
                       << action.action.expert << '\t' << action.action.budget << '\t'
                       << action.attempt_wall_us << '\t' << (action.passed ? 1 : 0) << '\t'
                       << (action.execution_failure ? 1 : 0) << '\t'
                       << (action.gate_mismatch ? 1 : 0) << '\t'
                       << request.terminal_wall_us << '\n';
            }
        }
    }
}

void write_model(
    const std::filesystem::path& path,
    const smave::RequestConditionedRoutingModel& model) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write conditioned routing model");
    output << std::setprecision(17)
           << "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 1\n";
    for (std::size_t index = 0; index < model.feature_names.size(); ++index) {
        output << "feature." << index << ".name=" << model.feature_names[index] << '\n'
               << "feature." << index << ".mean=" << model.feature_means[index] << '\n'
               << "feature." << index << ".scale=" << model.feature_scales[index] << '\n';
    }
    for (const auto& [expert, predictors] : model.actions) {
        for (const auto& predictor : predictors) {
            output << "action=" << expert << '@' << predictor.work_iterations << '\n'
                   << "training_samples=" << predictor.training_samples << '\n'
                   << "cost_calibration_error="
                   << predictor.cost_calibration_error << '\n'
                   << "pass_calibration_error="
                   << predictor.pass_calibration_error << '\n'
                   << "cost_calibration_upper_error="
                   << predictor.cost_calibration_upper_error << '\n'
                   << "pass_calibration_upper_error="
                   << predictor.pass_calibration_upper_error << '\n'
                   << "log_cost_coefficients=";
            for (std::size_t index = 0;
                 index < predictor.log_cost_coefficients.size(); ++index) {
                if (index != 0) output << ',';
                output << predictor.log_cost_coefficients[index];
            }
            output << "\npass_logit_coefficients=";
            for (std::size_t index = 0;
                 index < predictor.pass_logit_coefficients.size(); ++index) {
                if (index != 0) output << ',';
                output << predictor.pass_logit_coefficients[index];
            }
            output << '\n';
        }
    }
    output << "END\n";
}

std::map<std::pair<std::string, int>, StaticProfile> static_profiles(
    const std::vector<RequestObservation>& requests) {
    std::map<std::pair<std::string, int>, std::vector<double>> costs;
    std::map<std::pair<std::string, int>, std::size_t> passes;
    for (const auto& request : requests) {
        for (const auto& action : request.actions) {
            const auto key = std::make_pair(action.action.expert, action.action.budget);
            costs[key].push_back(action.attempt_wall_us);
            passes[key] += action.passed ? 1 : 0;
        }
    }
    std::map<std::pair<std::string, int>, StaticProfile> result;
    for (auto& [action, samples] : costs) {
        result.emplace(action, StaticProfile{
            .median_cost = median(std::move(samples)),
            .pass_probability = static_cast<double>(passes[action]) /
                static_cast<double>(requests.size()),
            .attempts = requests.size(),
            .passes = passes[action],
        });
    }
    return result;
}

double median_terminal(const std::vector<RequestObservation>& requests) {
    std::vector<double> costs;
    costs.reserve(requests.size());
    for (const auto& request : requests) costs.push_back(request.terminal_wall_us);
    return median(std::move(costs));
}

smave::RoutingConfig conditioned_routing(
    const smave::RequestConditionedRoutingModel& model,
    double terminal_cost) {
    smave::RoutingConfig routing;
    routing.top_k = top_k;
    routing.risk_weight = 0.0;
    routing.request_conditioned_model = model;
    routing.calibrated_terminal_fallback_cost_us = terminal_cost;
    for (const auto profile : {Profile::low, Profile::middle, Profile::high}) {
        routing.expert_allowlist.insert(expert_version(profile));
    }
    return routing;
}

smave::RoutingConfig static_routing(
    const smave::ModelIR& model,
    const std::map<std::pair<std::string, int>, StaticProfile>& profiles,
    double terminal_cost) {
    smave::RoutingConfig routing;
    routing.top_k = top_k;
    routing.risk_weight = 0.0;
    routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    routing.calibration_winner = expert_version(Profile::middle);
    routing.calibrated_terminal_fallback_cost_us = terminal_cost;
    for (const auto profile : {Profile::low, Profile::middle, Profile::high}) {
        routing.expert_allowlist.insert(expert_version(profile));
    }
    for (const auto& [action, profile] : profiles) {
        routing.calibrations[action.first].budget_options.push_back(
            smave::RouteBudgetCalibration{
                .work_iterations = action.second,
                .attempts = profile.attempts,
                .passes = profile.passes,
                .fallbacks = profile.attempts - profile.passes,
                .pass_probability = profile.pass_probability,
                .median_attempt_wall_us = profile.median_cost,
            });
    }
    return routing;
}

const ActionObservation& action_observation(
    const RequestObservation& request,
    const std::string& expert,
    int budget) {
    const auto found = std::find_if(
        request.actions.begin(), request.actions.end(), [&](const auto& action) {
            return action.action.expert == expert && action.action.budget == budget;
        });
    if (found == request.actions.end()) {
        throw std::invalid_argument("selected action lacks held-out observation");
    }
    return *found;
}

double realized_cost(
    const std::vector<smave::SolveStep>& steps,
    const RequestObservation& request) {
    double cost = 0.0;
    for (const auto& step : steps) {
        const auto& observation = action_observation(
            request, step.expert_version, step.budget.work_iterations);
        cost += observation.attempt_wall_us;
        if (observation.passed) return cost;
    }
    return cost + request.terminal_wall_us;
}

double realized_oracle(const RequestObservation& request) {
    double best = request.terminal_wall_us;
    const std::size_t subset_count = std::size_t{1} << request.actions.size();
    for (std::size_t mask = 1; mask < subset_count; ++mask) {
        std::vector<const ActionObservation*> selected;
        std::set<std::string> experts;
        bool valid = true;
        for (std::size_t index = 0; index < request.actions.size(); ++index) {
            if ((mask & (std::size_t{1} << index)) == 0) continue;
            if (!experts.insert(request.actions[index].action.expert).second) {
                valid = false;
                break;
            }
            selected.push_back(&request.actions[index]);
        }
        if (!valid || selected.size() > top_k) continue;
        std::sort(selected.begin(), selected.end(), [](const auto* left, const auto* right) {
            return left->action.key() < right->action.key();
        });
        do {
            double cost = 0.0;
            bool accepted = false;
            for (const auto* action : selected) {
                cost += action->attempt_wall_us;
                if (action->passed) {
                    accepted = true;
                    break;
                }
            }
            best = std::min(best, cost + (accepted ? 0.0 : request.terminal_wall_us));
        } while (std::next_permutation(
            selected.begin(), selected.end(), [](const auto* left, const auto* right) {
                return left->action.key() < right->action.key();
            }));
    }
    return best;
}

double exhaustive_expected_cost(
    std::vector<smave::SolveStep> alternatives,
    double terminal_cost) {
    std::map<std::string, std::vector<smave::SolveStep>> grouped;
    for (auto& alternative : alternatives) {
        grouped[alternative.expert_version].push_back(std::move(alternative));
    }
    std::vector<std::vector<smave::SolveStep>> groups;
    for (auto& [expert, actions] : grouped) {
        (void)expert;
        groups.push_back(std::move(actions));
    }
    double best = terminal_cost;
    std::vector<smave::SolveStep> selected;
    const auto enumerate = [&](const auto& self, std::size_t group) -> void {
        if (group == groups.size()) {
            if (selected.empty()) return;
            std::sort(
                selected.begin(), selected.end(), [](const auto& left, const auto& right) {
                    return std::tie(
                        left.expert_version, left.budget.work_iterations) <
                        std::tie(
                            right.expert_version, right.budget.work_iterations);
                });
            do {
                best = std::min(best, smave::expected_cascade_cost(selected, terminal_cost));
            } while (std::next_permutation(
                selected.begin(), selected.end(), [](const auto& left, const auto& right) {
                    return std::tie(
                        left.expert_version, left.budget.work_iterations) <
                        std::tie(
                            right.expert_version, right.budget.work_iterations);
                }));
            return;
        }
        self(self, group + 1);
        if (selected.size() == top_k) return;
        for (const auto& action : groups[group]) {
            selected.push_back(action);
            self(self, group + 1);
            selected.pop_back();
        }
    };
    enumerate(enumerate, 0);
    return best;
}

std::vector<smave::SolveStep> predicted_alternatives(
    const smave::RequestConditionedRoutingModel& model,
    const std::vector<double>& request_features) {
    std::vector<smave::SolveStep> result;
    for (const auto& [expert, predictors] : model.actions) {
        for (const auto& predictor : predictors) {
            const auto prediction = smave::predict_request_conditioned_action(
                model, expert, predictor.work_iterations, request_features);
            result.push_back(smave::SolveStep{
                .expert_version = expert,
                .budget = smave::SolveBudget{
                    .work_iterations = predictor.work_iterations},
                .estimated_cost_us = prediction.attempt_wall_us,
                .pass_probability = prediction.pass_probability,
            });
        }
    }
    return result;
}

std::string plan_signature(const std::vector<smave::SolveStep>& steps) {
    std::string result;
    for (const auto& step : steps) {
        if (!result.empty()) result += ",";
        result += step.expert_version + "@" +
            std::to_string(step.budget.work_iterations);
    }
    return result.empty() ? "terminal" : result;
}

struct EvaluationResult {
    double conditioned_regret{};
    double static_regret{};
    double fixed_regret{};
    double conditioned_median_request_regret{};
    double static_median_request_regret{};
    double cost_median_relative_error{};
    double cost_p95_relative_error{};
    double cost_maximum_relative_error{};
    double pass_brier_score{};
    double pass_ece{};
    double pass_maximum_action_calibration_error{};
    double plan_change_fraction{};
    std::size_t distinct_plans{};
    std::size_t dp_oracle_mismatches{};
    std::size_t production_successes{};
    std::size_t production_fallbacks{};
    std::size_t production_failures{};
    std::size_t production_gate_mismatches{};
    std::string fixed_action;
};

EvaluationResult evaluate(
    const std::vector<FamilyRuntime>& families,
    const std::vector<RequestObservation>& heldout,
    const smave::RequestConditionedRoutingModel& model,
    const std::map<std::pair<std::string, int>, StaticProfile>& profiles,
    double terminal_cost,
    const std::filesystem::path& production_traces) {
    EvaluationResult result;
    std::vector<double> cost_errors;
    double brier_sum = 0.0;
    std::array<double, 10> bin_probability{};
    std::array<double, 10> bin_observed{};
    std::array<std::size_t, 10> bin_count{};
    std::map<std::pair<std::string, int>, double> action_predicted;
    std::map<std::pair<std::string, int>, double> action_observed;
    std::map<std::pair<std::string, int>, std::size_t> action_count;
    for (const auto& request : heldout) {
        for (const auto& action : request.actions) {
            const auto prediction = smave::predict_request_conditioned_action(
                model, action.action.expert, action.action.budget, action.features);
            cost_errors.push_back(std::abs(
                prediction.attempt_wall_us - action.attempt_wall_us) /
                action.attempt_wall_us);
            const double observed = action.passed ? 1.0 : 0.0;
            const double error = prediction.pass_probability - observed;
            brier_sum += error * error;
            const std::size_t bin = std::min<std::size_t>(
                9, static_cast<std::size_t>(prediction.pass_probability * 10.0));
            bin_probability[bin] += prediction.pass_probability;
            bin_observed[bin] += observed;
            ++bin_count[bin];
            const auto key = std::make_pair(action.action.expert, action.action.budget);
            action_predicted[key] += prediction.pass_probability;
            action_observed[key] += observed;
            ++action_count[key];
        }
    }
    result.cost_median_relative_error = median(cost_errors);
    result.cost_p95_relative_error = quantile(cost_errors, 0.95);
    result.cost_maximum_relative_error = *std::max_element(
        cost_errors.begin(), cost_errors.end());
    const double observation_count = static_cast<double>(cost_errors.size());
    result.pass_brier_score = brier_sum / observation_count;
    for (std::size_t bin = 0; bin < bin_count.size(); ++bin) {
        if (bin_count[bin] == 0) continue;
        result.pass_ece += static_cast<double>(bin_count[bin]) / observation_count *
            std::abs(
                bin_probability[bin] / static_cast<double>(bin_count[bin]) -
                bin_observed[bin] / static_cast<double>(bin_count[bin]));
    }
    for (const auto& [action, count] : action_count) {
        result.pass_maximum_action_calibration_error = std::max(
            result.pass_maximum_action_calibration_error,
            std::abs(action_predicted[action] / static_cast<double>(count) -
                action_observed[action] / static_cast<double>(count)));
    }

    ActionSpec fixed;
    double fixed_training_cost = terminal_cost;
    for (const auto& [action, profile] : profiles) {
        const double expected = profile.median_cost +
            (1.0 - profile.pass_probability) * terminal_cost;
        if (expected < fixed_training_cost) {
            fixed_training_cost = expected;
            fixed = ActionSpec{.expert = action.first, .budget = action.second};
        }
    }
    result.fixed_action = fixed.expert.empty()
        ? "terminal" : fixed.expert + "@" + std::to_string(fixed.budget);

    double conditioned_total = 0.0;
    double static_total = 0.0;
    double fixed_total = 0.0;
    double oracle_total = 0.0;
    std::vector<double> conditioned_request_regrets;
    std::vector<double> static_request_regrets;
    std::map<std::string, std::size_t> plan_counts;
    const auto conditioned_config = conditioned_routing(model, terminal_cost);

    for (const auto& family : families) {
        const auto static_config = static_routing(family.model, profiles, terminal_cost);
        const auto candidates = smave::CompileRouter{}.lookup(
            family.model.blocks.front(), family.registry, family.bundle);
        const smave::Runtime conditioned_runtime(
            family.model, family.registry, family.bundle, {}, conditioned_config);
        for (const auto& request : heldout) {
            if (request.family.name != family.spec.name) continue;
            smave::BlockContext context;
            context.values = request.scenario.values;
            const auto conditioned_plan = smave::RuntimeRouter(conditioned_config).route(
                family.model.blocks.front(), context, candidates,
                family.registry, family.bundle);
            const auto static_plan = smave::RuntimeRouter(static_config).route(
                family.model.blocks.front(), context, candidates,
                family.registry, family.bundle);
            ++plan_counts[plan_signature(conditioned_plan.steps)];

            const auto alternatives = predicted_alternatives(
                model, request.actions.front().features);
            const auto dp = smave::optimize_joint_calibrated_cascade(
                alternatives, top_k, terminal_cost);
            const double dp_cost = smave::expected_cascade_cost(dp, terminal_cost);
            const double exhaustive_cost = exhaustive_expected_cost(
                alternatives, terminal_cost);
            if (std::abs(dp_cost - exhaustive_cost) > 1.0e-8 *
                    std::max(1.0, exhaustive_cost)) {
                ++result.dp_oracle_mismatches;
            }

            const double oracle = realized_oracle(request);
            const double conditioned = realized_cost(conditioned_plan.steps, request);
            const double static_value = realized_cost(static_plan.steps, request);
            double fixed_value = request.terminal_wall_us;
            if (!fixed.expert.empty()) {
                const auto& fixed_observation = action_observation(
                    request, fixed.expert, fixed.budget);
                fixed_value = fixed_observation.attempt_wall_us +
                    (fixed_observation.passed ? 0.0 : request.terminal_wall_us);
            }
            conditioned_total += conditioned;
            static_total += static_value;
            fixed_total += fixed_value;
            oracle_total += oracle;
            conditioned_request_regrets.push_back(conditioned / oracle);
            static_request_regrets.push_back(static_value / oracle);

            const auto outcome = conditioned_runtime.solve(
                request.scenario.values,
                production_traces / family.spec.name /
                    std::to_string(request.scenario.index));
            if (!outcome.success || outcome.blocks.empty()) {
                ++result.production_failures;
                continue;
            }
            ++result.production_successes;
            result.production_fallbacks += outcome.fallback_count;
            if (outcome.blocks.front().gate.decision != smave::GateDecision::direct_accept ||
                outcome.blocks.front().plan_id != conditioned_plan.plan_id) {
                ++result.production_gate_mismatches;
            }
        }
    }
    result.conditioned_regret = conditioned_total / oracle_total;
    result.static_regret = static_total / oracle_total;
    result.fixed_regret = fixed_total / oracle_total;
    result.conditioned_median_request_regret = median(
        std::move(conditioned_request_regrets));
    result.static_median_request_regret = median(std::move(static_request_regrets));
    result.distinct_plans = plan_counts.size();
    const auto modal = std::max_element(
        plan_counts.begin(), plan_counts.end(), [](const auto& left, const auto& right) {
            return left.second < right.second;
        });
    result.plan_change_fraction = 1.0 -
        static_cast<double>(modal->second) / static_cast<double>(heldout.size());
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::invalid_argument(
            "usage: request-conditioned-joint-route-evidence OUTPUT_DIRECTORY");
    }
    const std::filesystem::path output = argv[1];
    std::filesystem::create_directories(output);
    const std::vector<FamilySpec> family_specs{
        FamilySpec{.name = "ConditionedQuadratic", .power = 2, .family_code = 0.0},
        FamilySpec{.name = "ConditionedCubic", .power = 3, .family_code = 0.5},
        FamilySpec{.name = "ConditionedQuartic", .power = 4, .family_code = 1.0},
    };
    std::vector<FamilyRuntime> families;
    for (const auto& family : family_specs) {
        families.push_back(build_family_runtime(output / "models", family));
    }
    const auto actions = action_specs();
    const std::vector<std::string> feature_names{
        "context:root",
        "context:regime",
        "context:center_distance",
        "context:family_code",
    };
    const auto training = observe_split(
        families, actions, feature_names, 64, 0.25, 0.0,
        output / "training-traces", 3);
    const auto calibration = observe_split(
        families, actions, feature_names, 32, 0.75, 0.025,
        output / "calibration-traces", 3);
    const auto heldout = observe_split(
        families, actions, feature_names, 64, 0.75, 0.05,
        output / "heldout-traces", 3);

    const auto model = smave::train_request_conditioned_routing_model(
        feature_names,
        training_samples(training),
        training_samples(calibration),
        1.0e-3,
        5.0e-2,
        96);
    write_observations(
        output / "action-observations.tsv",
        {{"training", &training}, {"calibration", &calibration}, {"heldout", &heldout}});
    write_model(output / "request-conditioned-model.txt", model);
    const auto profiles = static_profiles(training);
    const double terminal_cost = median_terminal(training);
    const auto result = evaluate(
        families, heldout, model, profiles, terminal_cost,
        output / "production-heldout-traces");

    if (model.actions.size() != 3 || actions.size() != 12 || top_k != 3 ||
        training.size() != 192 || calibration.size() != 96 || heldout.size() != 192 ||
        result.conditioned_regret >= result.static_regret ||
        result.conditioned_regret >= result.fixed_regret ||
        result.conditioned_regret > 1.35 ||
        result.pass_brier_score > 0.16 || result.pass_ece > 0.16 ||
        result.pass_maximum_action_calibration_error > 0.2 ||
        result.cost_median_relative_error > 0.35 ||
        result.dp_oracle_mismatches != 0 || result.distinct_plans < 4 ||
        result.plan_change_fraction < 0.25 ||
        result.production_successes != heldout.size() ||
        result.production_failures != 0 || result.production_gate_mismatches != 0) {
        throw std::runtime_error("request-conditioned joint routing evidence failed");
    }

    std::ofstream evidence(output / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write conditioned routing evidence");
    evidence << std::setprecision(17)
             << "SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE 1\n"
             << "contract=production-trace-trained-request-conditioned-expert-budget-routing\n"
             << "families=quadratic,cubic,quartic\n"
             << "family_count=3\n"
             << "expert_count=3\n"
             << "budgets_per_expert=4\n"
             << "action_count=12\n"
             << "top_k=3\n"
             << "feature_count=" << feature_names.size() << '\n'
             << "features=root,regime,center_distance,family_code\n"
             << "training_requests=" << training.size() << '\n'
             << "calibration_requests=" << calibration.size() << '\n'
             << "heldout_requests=" << heldout.size() << '\n'
             << "training_action_observations=" << training.size() * actions.size() << '\n'
             << "calibration_action_observations="
             << calibration.size() * actions.size() << '\n'
             << "heldout_action_observations=" << heldout.size() * actions.size() << '\n'
             << "training_repetitions=3\n"
             << "calibration_repetitions=3\n"
             << "heldout_repetitions=3\n"
             << "action_observation_table=action-observations.tsv\n"
             << "frozen_model_parameters=request-conditioned-model.txt\n"
             << "cost_prediction_median_relative_error="
             << result.cost_median_relative_error << '\n'
             << "cost_prediction_p95_relative_error="
             << result.cost_p95_relative_error << '\n'
             << "cost_prediction_maximum_relative_error="
             << result.cost_maximum_relative_error << '\n'
             << "pass_prediction_brier_score=" << result.pass_brier_score << '\n'
             << "pass_prediction_ece=" << result.pass_ece << '\n'
             << "pass_prediction_maximum_action_calibration_error="
             << result.pass_maximum_action_calibration_error << '\n'
             << "conditioned_heldout_regret=" << result.conditioned_regret << '\n'
             << "static_profile_heldout_regret=" << result.static_regret << '\n'
             << "fixed_action_heldout_regret=" << result.fixed_regret << '\n'
             << "conditioned_median_request_regret="
             << result.conditioned_median_request_regret << '\n'
             << "static_profile_median_request_regret="
             << result.static_median_request_regret << '\n'
             << "fixed_action=" << result.fixed_action << '\n'
             << "distinct_conditioned_plans=" << result.distinct_plans << '\n'
             << "feature_changed_plan_fraction=" << result.plan_change_fraction << '\n'
             << "dp_exhaustive_mismatches=" << result.dp_oracle_mismatches << '\n'
             << "production_successes=" << result.production_successes << '\n'
             << "production_fallbacks=" << result.production_fallbacks << '\n'
             << "production_failures=" << result.production_failures << '\n'
             << "production_gate_mismatches="
             << result.production_gate_mismatches << '\n'
             << "original_equation_gate_preserved=1\n"
             << "terminal_fallback_preserved=1\n"
             << "heldout_not_used_for_training_or_calibration=1\n"
             << "realized_oracle_exhaustive=1\n"
             << "END\n";
    return 0;
}
