#include "smave/compiler.hpp"
#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

class FamilyBudgetExpert final : public smave::Expert {
public:
    FamilyBudgetExpert(
        std::string version, int power, bool selective, double offset)
        : version_(std::move(version)), power_(power), selective_(selective),
          offset_(offset) {}

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
        const double parameter = context.values.at("p");
        const auto selector = static_cast<long long>(context.values.at("selector"));
        const double root = std::pow(parameter, 1.0 / static_cast<double>(power_));
        const bool exact = selective_ && selector % 2 == 0;
        std::unordered_map<std::string, double> candidate;
        for (const auto& unknown : block.unknowns) {
            candidate.emplace(unknown, exact ? root : root - offset_);
        }
        return smave::ExpertResult{
            .candidate = std::move(candidate),
            .status = "candidate",
            .uncertainty = exact ? 0.0 : offset_,
        };
    }

private:
    std::string version_;
    int power_{};
    bool selective_{};
    double offset_{};
};

struct ActionSpec {
    std::string expert;
    int budget{};
};

struct ActionObservation {
    ActionSpec action;
    std::vector<double> attempt_costs;
    std::size_t attempts{};
    std::size_t passes{};
    std::size_t fallbacks{};
    std::size_t failures{};
    std::size_t erroneous_accepts{};
};

struct FamilyResult {
    std::string family;
    int selected_correction_budget{};
    std::size_t training_scenarios{};
    std::size_t heldout_scenarios{};
    std::size_t actions{};
    std::size_t selected_steps{};
    double training_expected_cost{};
    double heldout_source_expected_cost{};
    double heldout_oracle_expected_cost{};
    double heldout_regret{};
    double maximum_action_calibration_error{};
    std::size_t heldout_successes{};
    std::size_t heldout_gate_mismatches{};
    std::size_t heldout_failures{};
    std::size_t first_action_rejections{};
    std::size_t second_action_accepts{};
};

double median(std::vector<double> values) {
    if (values.empty()) throw std::invalid_argument("median requires samples");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

std::vector<std::unordered_map<std::string, double>> scenarios(
    int power, bool heldout) {
    std::vector<std::unordered_map<std::string, double>> result;
    for (int index = 0; index < 32; ++index) {
        const double root = heldout
            ? 1.3625 + 0.025 * static_cast<double>(index)
            : 1.20 + 0.025 * static_cast<double>(index);
        result.push_back({
            {"p", std::pow(root, static_cast<double>(power))},
            {"selector", static_cast<double>(index)},
        });
    }
    return result;
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

ActionObservation observe_action(
    const smave::ModelIR& model,
    const smave::Registry& registry,
    const smave::RuntimeBundle& bundle,
    const ActionSpec& action,
    const std::vector<std::unordered_map<std::string, double>>& inputs,
    const std::filesystem::path& traces,
    int repetitions) {
    ActionObservation observation{.action = action};
    const smave::Runtime runtime(
        model, registry, bundle, {}, forced_action_routing(model, action));
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const auto outcome = runtime.solve(
                inputs[index], traces / std::to_string(repetition) /
                    std::to_string(index));
            ++observation.attempts;
            if (!outcome.success || outcome.blocks.empty()) {
                ++observation.failures;
                continue;
            }
            const auto& block = outcome.blocks.front();
            if (block.attempt_records.empty()) {
                throw std::runtime_error("forced action was not attempted");
            }
            const auto& attempt = block.attempt_records.front();
            const bool accepted = attempt.outcome == "accepted";
            if (accepted) ++observation.passes;
            else ++observation.fallbacks;
            if (accepted && block.gate.decision != smave::GateDecision::direct_accept) {
                ++observation.erroneous_accepts;
            }
            const double attempt_cost = std::max(
                0.0, block.timing.total_us - block.timing.fallback_us);
            observation.attempt_costs.push_back(attempt_cost);
        }
    }
    return observation;
}

double observe_terminal_cost(
    const smave::ModelIR& model,
    const smave::Registry& registry,
    const smave::RuntimeBundle& bundle,
    const std::vector<std::unordered_map<std::string, double>>& inputs,
    const std::filesystem::path& traces,
    int repetitions) {
    const smave::Runtime runtime(model, registry, bundle, {}, fallback_routing(model));
    std::vector<double> costs;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const auto outcome = runtime.solve(
                inputs[index], traces / std::to_string(repetition) /
                    std::to_string(index));
            if (!outcome.success || outcome.blocks.empty() ||
                outcome.blocks.front().path != smave::SolvePath::full_fallback) {
                throw std::runtime_error("terminal fallback calibration failed");
            }
            costs.push_back(outcome.blocks.front().timing.total_us);
        }
    }
    return median(std::move(costs));
}

smave::RouteBudgetCalibration calibration(const ActionObservation& observation) {
    const double probability = static_cast<double>(observation.passes) /
        static_cast<double>(observation.attempts);
    return smave::RouteBudgetCalibration{
        .work_iterations = observation.action.budget,
        .attempts = observation.attempts,
        .passes = observation.passes,
        .fallbacks = observation.fallbacks,
        .failures = observation.failures,
        .erroneous_accepts = observation.erroneous_accepts,
        .pass_probability = probability,
        .calibration_error = 0.0,
        .median_attempt_wall_us = median(observation.attempt_costs),
    };
}

std::vector<smave::SolveStep> alternatives(
    const std::vector<ActionObservation>& observations) {
    std::vector<smave::SolveStep> result;
    for (const auto& observation : observations) {
        const auto profile = calibration(observation);
        if (profile.pass_probability <= 0.0) continue;
        result.push_back(smave::SolveStep{
            .expert_version = observation.action.expert,
            .budget = smave::SolveBudget{
                .work_iterations = observation.action.budget},
            .estimated_cost_us = profile.median_attempt_wall_us,
            .pass_probability = profile.pass_probability,
        });
    }
    return result;
}

std::vector<smave::SolveStep> reprice(
    const std::vector<smave::SolveStep>& plan,
    const std::vector<ActionObservation>& observations) {
    std::vector<smave::SolveStep> result;
    for (const auto& selected : plan) {
        const auto found = std::find_if(
            observations.begin(), observations.end(), [&](const auto& observation) {
                return observation.action.expert == selected.expert_version &&
                    observation.action.budget == selected.budget.work_iterations;
            });
        if (found == observations.end()) {
            throw std::runtime_error("heldout action missing from profile");
        }
        const auto profile = calibration(*found);
        auto repriced = selected;
        repriced.estimated_cost_us = profile.median_attempt_wall_us;
        repriced.pass_probability = profile.pass_probability;
        result.push_back(std::move(repriced));
    }
    return result;
}

FamilyResult evaluate_family(
    const std::filesystem::path& output,
    std::string family,
    int power,
    double correction_offset,
    int lower_budget,
    int full_budget) {
    std::filesystem::create_directories(output);
    const auto source = output / (family + ".mo");
    std::ofstream model_source(source);
    model_source << "model " << family << "\n"
                 << "  parameter Real p = 4.0;\n"
                 << "  parameter Real selector = 0.0;\n";
    constexpr int dimension = 12;
    for (int index = 0; index < dimension; ++index) {
        model_source << "  Real x" << index + 1 << "(start = 1.0);\n";
    }
    model_source << "equation\n";
    for (int index = 0; index < dimension; ++index) {
        const int next = (index + 1) % dimension;
        model_source << "  x" << index + 1;
        if (power == 2) model_source << "*x" << index + 1;
        else model_source << "*x" << index + 1 << "*x" << index + 1;
        model_source << " + 0.01*(x" << next + 1 << "-x" << index + 1
                     << ") = p + 0*selector;\n";
    }
    model_source << "end " << family << ";\n";
    model_source.close();
    const auto model = smave::compile_model(source);

    const std::string selective_version = family + "-selective-v1";
    const std::string corrective_version = family + "-corrective-v1";
    auto selective = std::make_shared<FamilyBudgetExpert>(
        selective_version, power, true, 0.25);
    auto corrective = std::make_shared<FamilyBudgetExpert>(
        corrective_version, power, false, correction_offset);
    auto registry = smave::make_default_registry(model);
    for (const auto& expert : {selective, corrective}) {
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
    for (const auto& expert : {selective, corrective}) {
        bundle.add_expert(
            expert->version(), expert->version() + "-artifact",
            expert->version() + "-evidence");
    }
    registry.validate_bundle(bundle, model);

    const std::vector<ActionSpec> action_specs{
        ActionSpec{.expert = selective_version, .budget = 0},
        ActionSpec{.expert = corrective_version, .budget = lower_budget},
        ActionSpec{.expert = corrective_version, .budget = full_budget},
    };
    const auto training = scenarios(power, false);
    const auto heldout = scenarios(power, true);
    std::vector<ActionObservation> training_observations;
    std::vector<ActionObservation> heldout_observations;
    for (const auto& action : action_specs) {
        training_observations.push_back(observe_action(
            model, registry, bundle, action, training,
            output / "training" / action.expert / std::to_string(action.budget), 3));
        heldout_observations.push_back(observe_action(
            model, registry, bundle, action, heldout,
            output / "heldout" / action.expert / std::to_string(action.budget), 3));
    }
    const double training_terminal = observe_terminal_cost(
        model, registry, bundle, training, output / "training-fallback", 3);
    const double heldout_terminal = observe_terminal_cost(
        model, registry, bundle, heldout, output / "heldout-fallback", 3);

    smave::RoutingConfig routing;
    routing.top_k = 2;
    routing.risk_weight = 0.0;
    routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    routing.calibration_winner = selective_version;
    routing.calibrated_terminal_fallback_cost_us = training_terminal;
    routing.expert_allowlist = {selective_version, corrective_version};
    for (const auto& observation : training_observations) {
        routing.calibrations[observation.action.expert].budget_options.push_back(
            calibration(observation));
    }

    smave::BlockContext context;
    context.values = heldout.front();
    const auto candidates = smave::CompileRouter{}.lookup(
        model.blocks.front(), registry, bundle);
    const auto source_plan = smave::RuntimeRouter(routing).route(
        model.blocks.front(), context, candidates, registry, bundle);
    const auto heldout_actions = alternatives(heldout_observations);
    const auto heldout_oracle = smave::optimize_joint_calibrated_cascade(
        heldout_actions, routing.top_k, heldout_terminal);
    const double training_expected = smave::expected_cascade_cost(
        source_plan.steps, training_terminal);
    const double heldout_source_expected = smave::expected_cascade_cost(
        reprice(source_plan.steps, heldout_observations), heldout_terminal);
    const double heldout_oracle_expected = smave::expected_cascade_cost(
        heldout_oracle, heldout_terminal);

    double maximum_calibration_error = 0.0;
    for (const auto& training_observation : training_observations) {
        const auto heldout_observation = std::find_if(
            heldout_observations.begin(), heldout_observations.end(),
            [&](const auto& candidate) {
                return candidate.action.expert == training_observation.action.expert &&
                    candidate.action.budget == training_observation.action.budget;
            });
        maximum_calibration_error = std::max(
            maximum_calibration_error,
            std::abs(
                calibration(training_observation).pass_probability -
                calibration(*heldout_observation).pass_probability));
    }

    FamilyResult result{
        .family = family,
        .training_scenarios = training.size(),
        .heldout_scenarios = heldout.size(),
        .actions = action_specs.size(),
        .selected_steps = source_plan.steps.size(),
        .training_expected_cost = training_expected,
        .heldout_source_expected_cost = heldout_source_expected,
        .heldout_oracle_expected_cost = heldout_oracle_expected,
        .heldout_regret = heldout_source_expected / heldout_oracle_expected,
        .maximum_action_calibration_error = maximum_calibration_error,
    };
    for (const auto& step : source_plan.steps) {
        if (step.expert_version == corrective_version) {
            result.selected_correction_budget = step.budget.work_iterations;
        }
    }

    const smave::Runtime production(model, registry, bundle, {}, routing);
    for (std::size_t index = 0; index < heldout.size(); ++index) {
        const auto outcome = production.solve(
            heldout[index], output / "production-heldout" / std::to_string(index));
        if (!outcome.success || outcome.blocks.empty()) {
            ++result.heldout_failures;
            continue;
        }
        ++result.heldout_successes;
        const auto& block = outcome.blocks.front();
        if (block.gate.decision != smave::GateDecision::direct_accept) {
            ++result.heldout_gate_mismatches;
        }
        if (block.attempt_records.size() >= 2 &&
            block.attempt_records[0].outcome == "rejected") {
            ++result.first_action_rejections;
            if (block.attempt_records[1].outcome == "accepted") {
                ++result.second_action_accepts;
            }
        }
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::invalid_argument(
            "usage: joint-route-budget-shift-evidence OUTPUT_DIRECTORY");
    }
    const std::filesystem::path output = argv[1];
    std::filesystem::create_directories(output);
    const auto quadratic = evaluate_family(
        output / "quadratic", "JointQuadratic", 2, 0.01, 1, 2);
    const auto cubic = evaluate_family(
        output / "cubic", "JointCubic", 3, 0.08, 2, 4);
    if (quadratic.selected_correction_budget == 0 ||
        cubic.selected_correction_budget == 0 ||
        quadratic.selected_correction_budget == cubic.selected_correction_budget) {
        throw std::runtime_error("joint policy did not adapt budget across equation families");
    }
    if (quadratic.heldout_regret > 1.25 || cubic.heldout_regret > 1.25 ||
        quadratic.maximum_action_calibration_error > 0.1 ||
        cubic.maximum_action_calibration_error > 0.1 ||
        quadratic.heldout_failures != 0 || cubic.heldout_failures != 0 ||
        quadratic.heldout_gate_mismatches != 0 || cubic.heldout_gate_mismatches != 0) {
        throw std::runtime_error("heldout joint policy contract failed");
    }

    std::ofstream evidence(output / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write joint shift evidence");
    evidence << std::setprecision(17)
             << "SMAVE_JOINT_ROUTE_BUDGET_SHIFT 1\n"
             << "contract=training-profile-to-heldout-production-joint-policy\n"
             << "families=quadratic,cubic\n"
             << "training_repetitions=3\n"
             << "heldout_repetitions=3\n";
    for (const auto& family : {quadratic, cubic}) {
        evidence << family.family << ".training_scenarios="
                 << family.training_scenarios << '\n'
                 << family.family << ".heldout_scenarios="
                 << family.heldout_scenarios << '\n'
                 << family.family << ".actions=" << family.actions << '\n'
                 << family.family << ".selected_steps=" << family.selected_steps << '\n'
                 << family.family << ".selected_correction_budget="
                 << family.selected_correction_budget << '\n'
                 << family.family << ".training_expected_cost_us="
                 << family.training_expected_cost << '\n'
                 << family.family << ".heldout_source_expected_cost_us="
                 << family.heldout_source_expected_cost << '\n'
                 << family.family << ".heldout_oracle_expected_cost_us="
                 << family.heldout_oracle_expected_cost << '\n'
                 << family.family << ".heldout_regret=" << family.heldout_regret << '\n'
                 << family.family << ".maximum_action_calibration_error="
                 << family.maximum_action_calibration_error << '\n'
                 << family.family << ".heldout_successes="
                 << family.heldout_successes << '\n'
                 << family.family << ".heldout_failures="
                 << family.heldout_failures << '\n'
                 << family.family << ".heldout_gate_mismatches="
                 << family.heldout_gate_mismatches << '\n'
                 << family.family << ".first_action_rejections="
                 << family.first_action_rejections << '\n'
                 << family.family << ".second_action_accepts="
                 << family.second_action_accepts << '\n';
    }
    evidence << "budget_changes_across_families=1\n"
             << "all_heldout_success=1\n"
             << "all_heldout_zero_gate_mismatches=1\n"
             << "maximum_heldout_regret="
             << std::max(quadratic.heldout_regret, cubic.heldout_regret) << '\n'
             << "maximum_action_calibration_error="
             << std::max(
                    quadratic.maximum_action_calibration_error,
                    cubic.maximum_action_calibration_error) << '\n'
             << "END\n";
    return 0;
}
