#include "smave/compiler.hpp"
#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

class JointBudgetProbeExpert final : public smave::Expert {
public:
    JointBudgetProbeExpert(std::string version, double candidate)
        : version_(std::move(version)), candidate_(candidate) {}

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
            .pass_probability = 1.0,
            .expected_solve_time_us = 1.0,
            .failure_cost_us = 10.0,
            .risk_score = 0.0,
        };
    }

    [[nodiscard]] smave::ExpertResult solve(
        const smave::BlockIR&,
        const smave::BlockContext&,
        const smave::SolveBudget&) const override {
        return smave::ExpertResult{
            .candidate = {{"x", candidate_}},
            .status = "candidate",
            .uncertainty = 0.0,
        };
    }

private:
    std::string version_;
    double candidate_{};
};

smave::RouteBudgetCalibration option(
    int budget,
    std::size_t passes,
    std::size_t fallbacks,
    double probability,
    double attempt_cost_us) {
    return smave::RouteBudgetCalibration{
        .work_iterations = budget,
        .attempts = passes + fallbacks,
        .passes = passes,
        .fallbacks = fallbacks,
        .pass_probability = probability,
        .calibration_error = 0.0,
        .median_attempt_wall_us = attempt_cost_us,
    };
}

double exhaustive_oracle(
    const std::vector<smave::SolveStep>& alternatives,
    std::size_t top_k,
    double terminal_cost_us,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions = {}) {
    double best = terminal_cost_us;
    const std::size_t subset_count = std::size_t{1} << alternatives.size();
    for (std::size_t mask = 1; mask < subset_count; ++mask) {
        std::vector<smave::SolveStep> subset;
        std::vector<std::string> experts;
        bool unique = true;
        for (std::size_t index = 0; index < alternatives.size(); ++index) {
            if ((mask & (std::size_t{1} << index)) == 0) continue;
            if (std::find(
                    experts.begin(), experts.end(),
                    alternatives[index].expert_version) != experts.end()) {
                unique = false;
                break;
            }
            experts.push_back(alternatives[index].expert_version);
            subset.push_back(alternatives[index]);
        }
        if (!unique || subset.size() > top_k) continue;
        std::sort(
            subset.begin(), subset.end(), [](const auto& left, const auto& right) {
                return std::tie(
                    left.expert_version,
                    left.budget.work_iterations) <
                    std::tie(
                        right.expert_version,
                        right.budget.work_iterations);
            });
        do {
            best = std::min(
                best,
                interactions.empty()
                    ? smave::expected_cascade_cost(subset, terminal_cost_us)
                    : smave::expected_interaction_aware_cascade_cost(
                          subset, terminal_cost_us, interactions));
        } while (std::next_permutation(
            subset.begin(), subset.end(), [](const auto& left, const auto& right) {
                return std::tie(
                    left.expert_version,
                    left.budget.work_iterations) <
                    std::tie(
                        right.expert_version,
                        right.budget.work_iterations);
            }));
    }
    return best;
}

std::uint64_t next_state(std::uint64_t& state) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state;
}

double unit_sample(std::uint64_t& state) {
    return static_cast<double>(next_state(state) >> 11) /
        static_cast<double>(std::uint64_t{1} << 53);
}

struct PropertySweepResult {
    std::size_t cases{};
    std::size_t actions{};
    double maximum_oracle_gap{};
};

bool same_plan(
    const std::vector<smave::SolveStep>& left,
    const std::vector<smave::SolveStep>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].expert_version != right[index].expert_version ||
            left[index].budget.work_iterations !=
                right[index].budget.work_iterations) {
            return false;
        }
    }
    return true;
}

PropertySweepResult verify_optimizer_property_sweep() {
    constexpr std::size_t case_count = 256;
    constexpr std::size_t expert_count = 3;
    constexpr std::size_t budgets_per_expert = 2;
    constexpr std::size_t action_count = expert_count * budgets_per_expert;
    std::uint64_t state = 0x5a17c9e3d42b8f61ULL;
    double maximum_gap = 0.0;
    for (std::size_t case_index = 0; case_index < case_count; ++case_index) {
        std::vector<smave::SolveStep> alternatives;
        for (std::size_t expert = 0; expert < expert_count; ++expert) {
            for (std::size_t budget = 0; budget < budgets_per_expert; ++budget) {
                alternatives.push_back(smave::SolveStep{
                    .expert_version = "property-expert-" + std::to_string(expert),
                    .budget = smave::SolveBudget{
                        .work_iterations = static_cast<int>(budget)},
                    .estimated_cost_us = 0.05 + 9.95 * unit_sample(state),
                    .pass_probability = 0.01 + 0.98 * unit_sample(state),
                });
            }
        }
        const std::size_t top_k = 1 + case_index % expert_count;
        const double terminal_cost_us = 0.1 + 19.9 * unit_sample(state);
        const auto selected = smave::optimize_joint_calibrated_cascade(
            alternatives, top_k, terminal_cost_us);
        const double selected_cost = smave::expected_cascade_cost(
            selected, terminal_cost_us);
        const double oracle_cost = exhaustive_oracle(
            alternatives, top_k, terminal_cost_us);
        const double gap = std::abs(selected_cost - oracle_cost);
        maximum_gap = std::max(maximum_gap, gap);
        if (gap > 1.0e-10 * std::max(1.0, oracle_cost)) {
            throw std::runtime_error(
                "joint Router property sweep disagrees with exhaustive oracle");
        }
        std::vector<std::string> experts;
        double previous_index = -std::numeric_limits<double>::infinity();
        for (const auto& step : selected) {
            if (std::find(
                    experts.begin(), experts.end(), step.expert_version) !=
                experts.end()) {
                throw std::runtime_error(
                    "joint Router property sweep violated expert exclusivity");
            }
            experts.push_back(step.expert_version);
            const double index = smave::cascade_ordering_index(step);
            if (index < previous_index) {
                throw std::runtime_error(
                    "joint Router property sweep violated cascade ordering");
            }
            previous_index = index;
        }
    }
    return {
        .cases = case_count,
        .actions = action_count,
        .maximum_oracle_gap = maximum_gap,
    };
}

struct InteractionPropertySweepResult {
    std::size_t cases{};
    std::size_t actions{};
    std::size_t transitions{};
    std::size_t changed_cases{};
    double maximum_oracle_gap{};
};

InteractionPropertySweepResult verify_interaction_optimizer_property_sweep() {
    constexpr std::size_t case_count = 256;
    constexpr std::size_t expert_count = 3;
    constexpr std::size_t budgets_per_expert = 2;
    constexpr std::size_t action_count = expert_count * budgets_per_expert;
    constexpr std::size_t transition_count =
        action_count * (action_count - budgets_per_expert);
    std::uint64_t state = 0x4c9f2d7a135be680ULL;
    std::size_t changed_cases = 0;
    double maximum_gap = 0.0;
    for (std::size_t case_index = 0; case_index < case_count; ++case_index) {
        std::vector<smave::SolveStep> alternatives;
        for (std::size_t expert = 0; expert < expert_count; ++expert) {
            for (std::size_t budget = 0; budget < budgets_per_expert; ++budget) {
                alternatives.push_back(smave::SolveStep{
                    .expert_version = "interaction-property-expert-" +
                        std::to_string(expert),
                    .budget = smave::SolveBudget{
                        .work_iterations = static_cast<int>(budget)},
                    .estimated_cost_us = 0.25 + 4.75 * unit_sample(state),
                    .pass_probability = 0.10 + 0.80 * unit_sample(state),
                });
            }
        }
        std::vector<smave::RouteConditionalCostCalibration> interactions;
        for (const auto& previous : alternatives) {
            for (const auto& next : alternatives) {
                if (previous.expert_version == next.expert_version) continue;
                const double multiplier = 0.20 + 4.80 * unit_sample(state);
                interactions.push_back(smave::RouteConditionalCostCalibration{
                    .previous = smave::RouteActionReference{
                        .expert_version = previous.expert_version,
                        .work_iterations = previous.budget.work_iterations},
                    .next = smave::RouteActionReference{
                        .expert_version = next.expert_version,
                        .work_iterations = next.budget.work_iterations},
                    .independent_training_groups = 2,
                    .independent_calibration_groups = 2,
                    .conditional_cost_multiplier = multiplier,
                    .conditional_cost_multiplier_upper = multiplier,
                });
            }
        }
        if (interactions.size() != transition_count) {
            throw std::runtime_error(
                "interaction Router property sweep transition count changed");
        }
        const std::size_t top_k = 1 + case_index % expert_count;
        const double terminal_cost_us = 10.0 + 20.0 * unit_sample(state);
        const auto selected = smave::optimize_interaction_aware_calibrated_cascade(
            alternatives, interactions, top_k, terminal_cost_us);
        const double selected_cost =
            smave::expected_interaction_aware_cascade_cost(
                selected, terminal_cost_us, interactions);
        const double oracle_cost = exhaustive_oracle(
            alternatives, top_k, terminal_cost_us, interactions);
        const double gap = std::abs(selected_cost - oracle_cost);
        maximum_gap = std::max(maximum_gap, gap);
        if (gap > 1.0e-10 * std::max(1.0, oracle_cost)) {
            throw std::runtime_error(
                "interaction Router property sweep disagrees with exhaustive oracle");
        }
        std::vector<std::string> experts;
        for (const auto& step : selected) {
            if (std::find(
                    experts.begin(), experts.end(), step.expert_version) !=
                experts.end()) {
                throw std::runtime_error(
                    "interaction Router property sweep violated expert exclusivity");
            }
            experts.push_back(step.expert_version);
        }
        const auto independent = smave::optimize_joint_calibrated_cascade(
            alternatives, top_k, terminal_cost_us);
        if (!same_plan(selected, independent)) ++changed_cases;
    }
    if (changed_cases == 0) {
        throw std::runtime_error(
            "interaction Router property sweep never changed the independent plan");
    }
    return {
        .cases = case_count,
        .actions = action_count,
        .transitions = transition_count,
        .changed_cases = changed_cases,
        .maximum_oracle_gap = maximum_gap,
    };
}

struct HardnessReductionResult {
    std::size_t graphs{};
    std::size_t vertices{};
    double threshold{};
};

bool directed_edge(
    std::size_t graph_mask,
    std::size_t vertex_count,
    std::size_t from,
    std::size_t to) {
    if (from == to) return false;
    std::size_t bit = 0;
    for (std::size_t source = 0; source < vertex_count; ++source) {
        for (std::size_t target = 0; target < vertex_count; ++target) {
            if (source == target) continue;
            if (source == from && target == to) {
                return (graph_mask & (std::size_t{1} << bit)) != 0;
            }
            ++bit;
        }
    }
    throw std::runtime_error("directed graph edge index was not found");
}

bool has_directed_hamiltonian_path(
    std::size_t graph_mask, std::size_t vertex_count) {
    std::vector<std::size_t> permutation(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index) {
        permutation[index] = index;
    }
    do {
        bool path = true;
        for (std::size_t index = 1; index < permutation.size(); ++index) {
            if (!directed_edge(
                    graph_mask, vertex_count,
                    permutation[index - 1], permutation[index])) {
                path = false;
                break;
            }
        }
        if (path) return true;
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return false;
}

HardnessReductionResult verify_interaction_hardness_reduction() {
    constexpr std::size_t vertex_count = 4;
    constexpr std::size_t directed_edge_count =
        vertex_count * (vertex_count - 1);
    constexpr std::size_t graph_count = std::size_t{1} << directed_edge_count;
    constexpr double terminal_cost_us = 5.0;
    double threshold = terminal_cost_us;
    double reach_probability = 1.0;
    threshold = 0.0;
    for (std::size_t index = 0; index < vertex_count; ++index) {
        threshold += reach_probability;
        reach_probability *= 0.5;
    }
    threshold += reach_probability * terminal_cost_us;

    std::vector<smave::SolveStep> alternatives;
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
        alternatives.push_back(smave::SolveStep{
            .expert_version = "hardness-vertex-" + std::to_string(vertex),
            .budget = smave::SolveBudget{.work_iterations = 0},
            .estimated_cost_us = 1.0,
            .pass_probability = 0.5,
        });
    }
    for (std::size_t graph_mask = 0; graph_mask < graph_count; ++graph_mask) {
        std::vector<smave::RouteConditionalCostCalibration> interactions;
        for (std::size_t from = 0; from < vertex_count; ++from) {
            for (std::size_t to = 0; to < vertex_count; ++to) {
                if (from == to) continue;
                const double multiplier =
                    directed_edge(graph_mask, vertex_count, from, to) ? 1.0 : 2.0;
                interactions.push_back(smave::RouteConditionalCostCalibration{
                    .previous = smave::RouteActionReference{
                        .expert_version = alternatives[from].expert_version,
                        .work_iterations = 0},
                    .next = smave::RouteActionReference{
                        .expert_version = alternatives[to].expert_version,
                        .work_iterations = 0},
                    .independent_training_groups = 1,
                    .independent_calibration_groups = 1,
                    .conditional_cost_multiplier = multiplier,
                    .conditional_cost_multiplier_upper = multiplier,
                });
            }
        }
        const auto selected = smave::optimize_interaction_aware_calibrated_cascade(
            alternatives, interactions, vertex_count, terminal_cost_us);
        if (selected.size() != vertex_count) {
            throw std::runtime_error(
                "interaction hardness reduction did not select every vertex");
        }
        const double cost = smave::expected_interaction_aware_cascade_cost(
            selected, terminal_cost_us, interactions);
        const bool optimizer_has_path = cost <= threshold + 1.0e-12;
        const bool graph_has_path = has_directed_hamiltonian_path(
            graph_mask, vertex_count);
        if (optimizer_has_path != graph_has_path) {
            throw std::runtime_error(
                "interaction hardness reduction disagrees with Hamiltonian path");
        }
    }
    return {
        .graphs = graph_count,
        .vertices = vertex_count,
        .threshold = threshold,
    };
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::invalid_argument("usage: joint-route-budget-evidence OUTPUT_DIRECTORY");
    }
    const std::filesystem::path output_directory = argv[1];
    std::filesystem::create_directories(output_directory);
    const auto source = output_directory / "JointRouteBudgetProbe.mo";
    std::ofstream(source)
        << "model JointRouteBudgetProbe\n"
        << "  parameter Real p = 4.0;\n"
        << "  Real x(start = 1.0);\n"
        << "equation\n"
        << "  x*x = p;\n"
        << "end JointRouteBudgetProbe;\n";
    const auto model = smave::compile_model(source);

    auto registry = smave::make_default_registry(model);
    auto expert_a = std::make_shared<JointBudgetProbeExpert>(
        "joint-budget-probe-a-v1", 1.99);
    auto expert_b = std::make_shared<JointBudgetProbeExpert>(
        "joint-budget-probe-b-v1", 1.5);
    for (const auto& expert : {expert_a, expert_b}) {
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
    for (const auto& expert : {expert_a, expert_b}) {
        bundle.add_expert(
            expert->version(),
            expert->version() + "-artifact",
            expert->version() + "-evidence");
    }
    registry.validate_bundle(bundle, model);

    smave::RoutingConfig routing;
    routing.top_k = 2;
    routing.risk_weight = 0.0;
    routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    routing.calibration_winner = expert_a->version();
    routing.calibrated_terminal_fallback_cost_us = 10.0;
    routing.expert_allowlist = {expert_a->version(), expert_b->version()};
    routing.calibrations[expert_a->version()].budget_options = {
        option(0, 13, 51, 0.2, 1.0),
        option(2, 64, 0, 1.0, 5.0),
    };
    routing.calibrations[expert_b->version()].budget_options = {
        option(1, 32, 32, 0.5, 2.0),
    };

    const std::unordered_map<std::string, double> scenario{{"p", 4.0}};
    smave::BlockContext context;
    context.values = scenario;
    const auto candidates = smave::CompileRouter{}.lookup(
        model.blocks.front(), registry, bundle);
    const auto plan = smave::RuntimeRouter(routing).route(
        model.blocks.front(), context, candidates, registry, bundle);
    if (plan.steps.size() != 2 ||
        plan.steps[0].expert_version != expert_b->version() ||
        plan.steps[0].budget.work_iterations != 1 ||
        plan.steps[1].expert_version != expert_a->version() ||
        plan.steps[1].budget.work_iterations != 2) {
        throw std::runtime_error("joint Router did not select the optimal expert-budget cascade");
    }

    std::vector<smave::SolveStep> alternatives;
    for (const auto& [expert, calibration] : routing.calibrations) {
        for (const auto& budget : calibration.budget_options) {
            if (budget.pass_probability <= 0.0) continue;
            alternatives.push_back(smave::SolveStep{
                .expert_version = expert,
                .budget = smave::SolveBudget{
                    .work_iterations = budget.work_iterations},
                .estimated_cost_us = budget.median_attempt_wall_us,
                .pass_probability = budget.pass_probability,
            });
        }
    }
    const double selected_cost = smave::expected_cascade_cost(
        plan.steps, routing.calibrated_terminal_fallback_cost_us);
    const double oracle_cost = exhaustive_oracle(
        alternatives, routing.top_k, routing.calibrated_terminal_fallback_cost_us);
    double best_single_cost = routing.calibrated_terminal_fallback_cost_us;
    for (const auto& alternative : alternatives) {
        best_single_cost = std::min(
            best_single_cost,
            smave::expected_cascade_cost(
                std::vector<smave::SolveStep>{alternative},
                routing.calibrated_terminal_fallback_cost_us));
    }
    if (std::abs(selected_cost - oracle_cost) > 1.0e-12) {
        throw std::runtime_error("joint Router disagrees with exhaustive oracle");
    }
    const auto property_sweep = verify_optimizer_property_sweep();
    const auto interaction_property_sweep =
        verify_interaction_optimizer_property_sweep();
    const auto hardness_reduction = verify_interaction_hardness_reduction();

    const auto outcome = smave::Runtime(
        model, registry, bundle, {}, routing).solve(
            scenario, output_directory / "runtime-traces");
    if (!outcome.success || outcome.blocks.empty()) {
        throw std::runtime_error("joint Router production Runtime failed");
    }
    const auto& block = outcome.blocks.front();
    if (block.path != smave::SolvePath::warm_start_accept ||
        block.attempt_records.size() < 2 ||
        block.attempt_records[0].expert_version != expert_b->version() ||
        block.attempt_records[0].outcome != "rejected" ||
        block.attempt_records[1].expert_version != expert_a->version() ||
        block.attempt_records[1].outcome != "accepted" ||
        block.gate.decision != smave::GateDecision::direct_accept ||
        block.attempt_records[1].iterations != 2) {
        throw std::runtime_error("joint Router Runtime path did not preserve rejection and acceptance");
    }

    std::ofstream evidence(output_directory / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write joint Router evidence");
    evidence << std::setprecision(17)
             << "SMAVE_JOINT_ROUTE_BUDGET 1\n"
             << "contract=exact-dp-joint-expert-budget-complete-cost\n"
             << "experts=2\n"
             << "actions=3\n"
             << "top_k=2\n"
             << "terminal_cost_us=10\n"
             << "selected_steps=" << plan.steps.size() << '\n'
             << "step0.expert=" << plan.steps[0].expert_version << '\n'
             << "step0.budget=" << plan.steps[0].budget.work_iterations << '\n'
             << "step1.expert=" << plan.steps[1].expert_version << '\n'
             << "step1.budget=" << plan.steps[1].budget.work_iterations << '\n'
             << "selected_expected_cost_us=" << selected_cost << '\n'
             << "exhaustive_oracle_cost_us=" << oracle_cost << '\n'
             << "oracle_gap_us=" << std::abs(selected_cost - oracle_cost) << '\n'
             << "best_single_action_cost_us=" << best_single_cost << '\n'
             << "joint_vs_best_single_ratio=" << selected_cost / best_single_cost << '\n'
             << "same_expert_budget_exclusivity=1\n"
             << "property_sweep_cases=" << property_sweep.cases << '\n'
             << "property_sweep_actions_per_case=" << property_sweep.actions << '\n'
             << "property_sweep_exhaustive_oracle_match=1\n"
             << "property_sweep_maximum_oracle_gap_us="
             << property_sweep.maximum_oracle_gap << '\n'
             << "property_sweep_expert_exclusivity=1\n"
             << "property_sweep_ordering_invariant=1\n"
             << "interaction_property_sweep_cases="
             << interaction_property_sweep.cases << '\n'
             << "interaction_property_sweep_actions_per_case="
             << interaction_property_sweep.actions << '\n'
             << "interaction_property_sweep_transitions_per_case="
             << interaction_property_sweep.transitions << '\n'
             << "interaction_property_sweep_exhaustive_oracle_match=1\n"
             << "interaction_property_sweep_maximum_oracle_gap_us="
             << interaction_property_sweep.maximum_oracle_gap << '\n'
             << "interaction_property_sweep_expert_exclusivity=1\n"
             << "interaction_property_sweep_changed_from_independent=1\n"
             << "interaction_property_sweep_changed_cases="
             << interaction_property_sweep.changed_cases << '\n'
             << "interaction_hardness_reduction=directed-hamiltonian-path\n"
             << "interaction_hardness_vertices="
             << hardness_reduction.vertices << '\n'
             << "interaction_hardness_graphs="
             << hardness_reduction.graphs << '\n'
             << "interaction_hardness_threshold_us="
             << hardness_reduction.threshold << '\n'
             << "interaction_hardness_reduction_match=1\n"
             << "interaction_hardness_full_length=1\n"
             << "runtime.first_action_rejected=1\n"
             << "runtime.second_action_accepted=1\n"
             << "runtime.second_action_iterations="
             << block.attempt_records[1].iterations << '\n'
             << "runtime.original_equation_gate_accept=1\n"
             << "runtime.terminal_fallback_used=0\n"
             << "END\n";
    return 0;
}
