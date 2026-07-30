#include "smave/routing.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double terminal_cost_us = 50.0;
constexpr std::size_t production_state_limit = 1000000;

std::size_t choose(std::size_t count, std::size_t selected) {
    selected = std::min(selected, count - selected);
    std::size_t result = 1;
    for (std::size_t index = 1; index <= selected; ++index) {
        result = result * (count - selected + index) / index;
    }
    return result;
}

std::size_t expected_interaction_states(
    std::size_t experts, std::size_t budgets_per_expert, std::size_t top_k) {
    const std::size_t selected_limit = std::min(experts, top_k);
    std::size_t states = 1;
    for (std::size_t selected = 1; selected <= selected_limit; ++selected) {
        states += budgets_per_expert * selected * choose(experts, selected);
    }
    return states;
}

std::size_t expected_interaction_transitions(
    std::size_t experts, std::size_t budgets_per_expert, std::size_t top_k) {
    const std::size_t selected_limit = std::min(experts, top_k);
    std::size_t transitions = experts * budgets_per_expert;
    for (std::size_t selected = 1; selected < selected_limit; ++selected) {
        const std::size_t states =
            budgets_per_expert * selected * choose(experts, selected);
        transitions += states * (experts - selected) * budgets_per_expert;
    }
    return transitions;
}

std::size_t expected_terminal_states(
    std::size_t experts, std::size_t budgets_per_expert, std::size_t top_k) {
    const std::size_t selected = std::min(experts, top_k);
    return budgets_per_expert * selected * choose(experts, selected);
}

std::vector<smave::SolveStep> make_actions(
    std::size_t experts, std::size_t budgets_per_expert) {
    std::vector<smave::SolveStep> actions;
    for (std::size_t expert = 0; expert < experts; ++expert) {
        for (std::size_t budget = 0; budget < budgets_per_expert; ++budget) {
            const std::size_t sample = (expert * 37 + budget * 19 + 11) % 97;
            actions.push_back(smave::SolveStep{
                .expert_version = "scaling-expert-" + std::to_string(expert),
                .budget = smave::SolveBudget{
                    .work_iterations = static_cast<int>(budget)},
                .estimated_cost_us =
                    0.75 + 0.31 * static_cast<double>(expert + 1) +
                    0.17 * static_cast<double>(budget + 1),
                .pass_probability =
                    0.10 + 0.80 * static_cast<double>(sample) / 96.0,
            });
        }
    }
    return actions;
}

std::vector<smave::RouteConditionalCostCalibration> make_interactions(
    const std::vector<smave::SolveStep>& actions) {
    std::vector<smave::RouteConditionalCostCalibration> interactions;
    for (std::size_t previous_index = 0; previous_index < actions.size();
         ++previous_index) {
        const auto& previous = actions[previous_index];
        for (std::size_t next_index = 0; next_index < actions.size(); ++next_index) {
            const auto& next = actions[next_index];
            if (previous.expert_version == next.expert_version) continue;
            const std::size_t sample =
                (previous_index * 43 + next_index * 29 + 17) % 151;
            const double multiplier =
                0.50 + static_cast<double>(sample) / 100.0;
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
    return interactions;
}

struct ProfileResult {
    std::size_t experts{};
    std::size_t budgets_per_expert{};
    std::size_t top_k{};
    std::size_t actions{};
    std::size_t interactions{};
    smave::CascadeOptimizationDiagnostics independent;
    smave::CascadeOptimizationDiagnostics interaction;
    std::size_t independent_steps{};
    std::size_t interaction_steps{};
};

ProfileResult run_profile(
    std::size_t experts, std::size_t budgets_per_expert, std::size_t top_k) {
    const auto actions = make_actions(experts, budgets_per_expert);
    const auto interactions = make_interactions(actions);
    const std::size_t expected_states =
        expected_interaction_states(experts, budgets_per_expert, top_k);
    const std::size_t expected_transitions =
        expected_interaction_transitions(experts, budgets_per_expert, top_k);
    const std::size_t expected_terminals =
        expected_terminal_states(experts, budgets_per_expert, top_k);
    if (expected_states > production_state_limit) {
        throw std::runtime_error("measured scaling profile exceeds frozen state limit");
    }

    smave::CascadeOptimizationDiagnostics independent_diagnostics;
    const auto independent = smave::optimize_joint_calibrated_cascade(
        actions, top_k, terminal_cost_us, production_state_limit,
        &independent_diagnostics);
    smave::CascadeOptimizationDiagnostics interaction_diagnostics;
    const auto interaction = smave::optimize_interaction_aware_calibrated_cascade(
        actions, interactions, top_k, terminal_cost_us, expected_states,
        &interaction_diagnostics);

    if (interaction_diagnostics.estimated_states != expected_states ||
        interaction_diagnostics.visited_states != expected_states ||
        interaction_diagnostics.recursive_transitions != expected_transitions ||
        interaction_diagnostics.terminal_states != expected_terminals ||
        interaction_diagnostics.memo_hits + interaction_diagnostics.visited_states !=
            interaction_diagnostics.recursive_transitions + 1 ||
        interaction_diagnostics.state_limit_exceeded || independent.empty() ||
        interaction.empty() || independent.size() > top_k ||
        interaction.size() > top_k) {
        throw std::runtime_error("production cascade diagnostics disagree with oracle");
    }

    smave::CascadeOptimizationDiagnostics rejected_diagnostics;
    bool rejected = false;
    try {
        (void)smave::optimize_interaction_aware_calibrated_cascade(
            actions, interactions, top_k, terminal_cost_us, expected_states - 1,
            &rejected_diagnostics);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected || !rejected_diagnostics.state_limit_exceeded ||
        rejected_diagnostics.estimated_states != expected_states ||
        rejected_diagnostics.visited_states != 0) {
        throw std::runtime_error(
            "interaction state cap did not reject before recurrence traversal");
    }

    return {
        .experts = experts,
        .budgets_per_expert = budgets_per_expert,
        .top_k = top_k,
        .actions = actions.size(),
        .interactions = interactions.size(),
        .independent = independent_diagnostics,
        .interaction = interaction_diagnostics,
        .independent_steps = independent.size(),
        .interaction_steps = interaction.size(),
    };
}

struct PreflightResult {
    std::size_t experts{};
    std::size_t actions{};
    std::size_t top_k{};
    std::size_t state_limit{};
    std::size_t estimated_states{};
};

PreflightResult run_preflight(std::size_t experts) {
    constexpr std::size_t budgets_per_expert = 2;
    const std::size_t top_k = std::min<std::size_t>(6, experts);
    const auto actions = make_actions(experts, budgets_per_expert);
    const auto interactions = make_interactions(actions);
    const std::size_t expected_states =
        expected_interaction_states(experts, budgets_per_expert, top_k);
    const std::size_t state_limit =
        experts == 22 ? production_state_limit : expected_states - 1;
    smave::CascadeOptimizationDiagnostics diagnostics;
    bool rejected = false;
    try {
        (void)smave::optimize_interaction_aware_calibrated_cascade(
            actions, interactions, top_k, terminal_cost_us, state_limit,
            &diagnostics);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected || !diagnostics.state_limit_exceeded ||
        diagnostics.estimated_states != expected_states ||
        diagnostics.visited_states != 0) {
        throw std::runtime_error("scaling preflight did not reject deterministically");
    }
    return {
        .experts = experts,
        .actions = actions.size(),
        .top_k = top_k,
        .state_limit = state_limit,
        .estimated_states = expected_states,
    };
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        throw std::invalid_argument(
            "usage: joint-route-scaling-evidence OUTPUT_DIRECTORY CONTRACT");
    }
    const std::filesystem::path output_directory = argv[1];
    const std::filesystem::path contract = argv[2];
    std::filesystem::create_directories(output_directory);
    std::filesystem::copy_file(
        contract, output_directory / contract.filename(),
        std::filesystem::copy_options::overwrite_existing);

    const std::vector<std::size_t> measured_experts{2, 4, 6, 8, 10, 12, 14, 16};
    std::vector<ProfileResult> profiles;
    profiles.reserve(measured_experts.size() + 1);
    for (const std::size_t experts : measured_experts) {
        profiles.push_back(run_profile(experts, 2, std::min<std::size_t>(6, experts)));
    }
    const ProfileResult production_shape = run_profile(3, 4, 3);
    profiles.push_back(production_shape);

    const std::vector<PreflightResult> preflights{
        run_preflight(18), run_preflight(20), run_preflight(22)};

    std::ofstream scaling(output_directory / "scaling.tsv");
    scaling
        << "profile\texperts\tbudgets_per_expert\ttop_k\tactions\tinteractions"
           "\tindependent_states\tindependent_transitions\tinteraction_states"
           "\tinteraction_transitions\tinteraction_memo_hits"
           "\tinteraction_terminal_states\tindependent_steps\tinteraction_steps\n";
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        const auto& profile = profiles[index];
        scaling << (index + 1 == profiles.size() ? "production-shape" : "uniform")
                << '\t' << profile.experts << '\t' << profile.budgets_per_expert
                << '\t' << profile.top_k << '\t' << profile.actions << '\t'
                << profile.interactions << '\t'
                << profile.independent.visited_states << '\t'
                << profile.independent.recursive_transitions << '\t'
                << profile.interaction.visited_states << '\t'
                << profile.interaction.recursive_transitions << '\t'
                << profile.interaction.memo_hits << '\t'
                << profile.interaction.terminal_states << '\t'
                << profile.independent_steps << '\t' << profile.interaction_steps
                << '\n';
    }

    std::ofstream preflight(output_directory / "preflight.tsv");
    preflight << "experts\tactions\ttop_k\tstate_limit\testimated_states"
                 "\trejected_before_visit\n";
    for (const auto& result : preflights) {
        preflight << result.experts << '\t' << result.actions << '\t'
                  << result.top_k << '\t' << result.state_limit << '\t'
                  << result.estimated_states << "\t1\n";
    }

    const auto& largest_measured = profiles[measured_experts.size() - 1];
    const auto& frontier_admitted = preflights[1];
    const auto& frontier_rejected = preflights[2];
    std::ofstream evidence(output_directory / "evidence.txt");
    evidence << std::setprecision(std::numeric_limits<double>::max_digits10)
             << "SMAVE_JOINT_ROUTE_SCALING_ROUND51 1\n"
             << "contract=deterministic-production-dp-tractability\n"
             << "scope=planning-only-no-numerical-solver-execution\n"
             << "measured_profiles=" << profiles.size() << '\n'
             << "measured_uniform_profiles=" << measured_experts.size() << '\n'
             << "uniform_budgets_per_expert=2\n"
             << "uniform_top_k_cap=6\n"
             << "maximum_joint_states=" << production_state_limit << '\n'
             << "largest_measured_experts=" << largest_measured.experts << '\n'
             << "largest_measured_actions=" << largest_measured.actions << '\n'
             << "largest_measured_interaction_states="
             << largest_measured.interaction.visited_states << '\n'
             << "largest_measured_interaction_transitions="
             << largest_measured.interaction.recursive_transitions << '\n'
             << "state_estimator_matches_measured_recurrence=1\n"
             << "exact_cap_acceptance_and_preflight_rejection=1\n"
             << "production_shape_experts=" << production_shape.experts << '\n'
             << "production_shape_budgets_per_expert="
             << production_shape.budgets_per_expert << '\n'
             << "production_shape_top_k=" << production_shape.top_k << '\n'
             << "production_shape_actions=" << production_shape.actions << '\n'
             << "production_shape_interaction_states="
             << production_shape.interaction.visited_states << '\n'
             << "production_shape_interaction_transitions="
             << production_shape.interaction.recursive_transitions << '\n'
             << "default_cap_largest_even_uniform_experts="
             << frontier_admitted.experts << '\n'
             << "default_cap_largest_even_uniform_actions="
             << frontier_admitted.actions << '\n'
             << "default_cap_largest_even_uniform_states="
             << frontier_admitted.estimated_states << '\n'
             << "default_cap_first_rejected_even_uniform_experts="
             << frontier_rejected.experts << '\n'
             << "default_cap_first_rejected_even_uniform_actions="
             << frontier_rejected.actions << '\n'
             << "default_cap_first_rejected_even_uniform_states="
             << frontier_rejected.estimated_states << '\n'
             << "preflight_rejections_before_state_visit=" << preflights.size()
             << '\n'
             << "timing_claim=0\n"
             << "immutable_v4_v5_v6_solver_execution=0\n"
             << "cohort_search=0\n"
             << "policy_tuning=0\n"
             << "scaling_table=scaling.tsv\n"
             << "preflight_table=preflight.tsv\n"
             << "END\n";
}
