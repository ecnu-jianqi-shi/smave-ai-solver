#!/usr/bin/env python3

import hashlib
import sys
from pathlib import Path


WORKERS = ((1, "One"), (2, "Two"), (4, "Four"), (8, "Eight"), (10, "Ten"))
PDE_WORKLOADS = (
    ("advection", "Advection"),
    ("darcy", "Darcy"),
    ("burgers", "Burgers"),
    ("diffusion_sorption", "DiffusionSorption"),
    ("shallow_water", "ShallowWater"),
    ("ns_incompressible", "NavierStokes"),
    ("cfd_1d", "CfdOneD"),
)


def parse_evidence(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generated_gate_tex(values: dict[str, str]) -> str:
    lines = ["% Generated from build/release/gate-parallel-scaling/evidence.txt."]
    for family, command_family in (("linear", "Linear"), ("nonlinear", "Nonlinear")):
        for worker, command_worker in WORKERS:
            speedup = float(values[f"{family}.worker_{worker}.paired_speedup"])
            lines.append(
                f"\\newcommand{{\\{command_family}Gate{command_worker}}}"
                f"{{{speedup:.3f}}}"
            )
        speedup = float(values[f"{family}.worker_10.paired_speedup"])
        lower = float(values[f"{family}.worker_10.bootstrap_95_lower"])
        upper = float(values[f"{family}.worker_10.bootstrap_95_upper"])
        lines.extend(
            [
                f"\\newcommand{{\\{command_family}GateTenShort}}{{{speedup:.2f}}}",
                f"\\newcommand{{\\{command_family}GateTenLower}}{{{lower:.3f}}}",
                f"\\newcommand{{\\{command_family}GateTenUpper}}{{{upper:.3f}}}",
            ]
        )
    return "\n".join(lines) + "\n"


def generated_joint_route_budget_tex(values: dict[str, str]) -> str:
    selected = float(values["selected_expected_cost_us"])
    best_single = float(values["best_single_action_cost_us"])
    ratio = float(values["joint_vs_best_single_ratio"])
    property_cases = int(values["property_sweep_cases"])
    property_actions = int(values["property_sweep_actions_per_case"])
    interaction_cases = int(values["interaction_property_sweep_cases"])
    interaction_transitions = int(
        values["interaction_property_sweep_transitions_per_case"]
    )
    interaction_changed = int(values["interaction_property_sweep_changed_cases"])
    hardness_graphs = int(values["interaction_hardness_graphs"])
    return "\n".join(
        [
            "% Generated from build/release/joint-route-budget/evidence.txt.",
            f"\\newcommand{{\\JointRouteSelectedCost}}{{{selected:.1f}}}",
            f"\\newcommand{{\\JointRouteBestSingleCost}}{{{best_single:.1f}}}",
            f"\\newcommand{{\\JointRouteVsBestSingle}}{{{ratio:.3f}}}",
            f"\\newcommand{{\\JointRoutePropertyCases}}{{{property_cases}}}",
            f"\\newcommand{{\\JointRoutePropertyActions}}{{{property_actions}}}",
            f"\\newcommand{{\\JointRouteInteractionCases}}{{{interaction_cases}}}",
            f"\\newcommand{{\\JointRouteInteractionTransitions}}{{{interaction_transitions}}}",
            f"\\newcommand{{\\JointRouteInteractionChanged}}{{{interaction_changed}}}",
            f"\\newcommand{{\\JointRouteHardnessGraphs}}{{{hardness_graphs}}}",
        ]
    ) + "\n"


def generated_joint_route_scaling_tex(values: dict[str, str]) -> str:
    return "\n".join(
        [
            "% Generated from build/release/joint-route-scaling-round51/evidence.txt.",
            f"\\newcommand{{\\JointScalingMeasuredExperts}}{{{int(values['largest_measured_experts'])}}}",
            f"\\newcommand{{\\JointScalingMeasuredActions}}{{{int(values['largest_measured_actions'])}}}",
            f"\\newcommand{{\\JointScalingMeasuredStates}}{{{int(values['largest_measured_interaction_states'])}}}",
            f"\\newcommand{{\\JointScalingMeasuredTransitions}}{{{int(values['largest_measured_interaction_transitions'])}}}",
            f"\\newcommand{{\\JointScalingProductionActions}}{{{int(values['production_shape_actions'])}}}",
            f"\\newcommand{{\\JointScalingProductionStates}}{{{int(values['production_shape_interaction_states'])}}}",
            f"\\newcommand{{\\JointScalingProductionTransitions}}{{{int(values['production_shape_interaction_transitions'])}}}",
            f"\\newcommand{{\\JointScalingCapAdmittedActions}}{{{int(values['default_cap_largest_even_uniform_actions'])}}}",
            f"\\newcommand{{\\JointScalingCapAdmittedStates}}{{{int(values['default_cap_largest_even_uniform_states'])}}}",
            f"\\newcommand{{\\JointScalingCapRejectedActions}}{{{int(values['default_cap_first_rejected_even_uniform_actions'])}}}",
            f"\\newcommand{{\\JointScalingCapRejectedStates}}{{{int(values['default_cap_first_rejected_even_uniform_states'])}}}",
        ]
    ) + "\n"


def generated_frozen_interaction_prevalence_tex(values: dict[str, str]) -> str:
    return "\n".join(
        [
            "% Generated from build/release/frozen-interaction-prevalence-round52/evidence.txt.",
            f"\\newcommand{{\\FrozenInteractionDevelopmentPairs}}{{{int(values['development_supported_pairs'])}}}",
            f"\\newcommand{{\\FrozenInteractionVFiveHeldoutPairs}}{{{int(values['v5.heldout_observed_pairs'])}}}",
            f"\\newcommand{{\\FrozenInteractionVSixHeldoutPairs}}{{{int(values['v6.heldout_observed_pairs'])}}}",
            f"\\newcommand{{\\FrozenInteractionHeldoutJaccard}}{{{float(values['v5_v6_heldout_pair_jaccard']):.3f}}}",
            f"\\newcommand{{\\FrozenInteractionDevelopmentHeldoutOverlap}}{{{int(values['development_to_both_heldout_overlap'])}}}",
            f"\\newcommand{{\\FrozenInteractionVFiveFailureRequests}}{{{int(values['v5.heldout_requests_with_two_expert_failures'])}}}",
            f"\\newcommand{{\\FrozenInteractionVSixFailureRequests}}{{{int(values['v6.heldout_requests_with_two_expert_failures'])}}}",
            f"\\newcommand{{\\FrozenInteractionPlanCandidates}}{{{int(values['v6.plan_gated_candidate_transitions'])}}}",
            f"\\newcommand{{\\FrozenInteractionConditionalRows}}{{{int(values['v6.conditional_timing_rows'])}}}",
        ]
    ) + "\n"


def generated_frozen_transition_attrition_tex(values: dict[str, str]) -> str:
    return "\n".join(
        [
            "% Generated from build/release/frozen-transition-attrition-round53/evidence.txt.",
            f"\\newcommand{{\\FrozenAttritionSupportedPairs}}{{{int(values['v6.development_supported_pairs'])}}}",
            f"\\newcommand{{\\FrozenAttritionTopThreeEliminated}}{{{int(values['v6.development_pairs_eliminated_at_unguarded_top3_selection'])}}}",
            f"\\newcommand{{\\FrozenAttritionUnguardedCandidates}}{{{int(values['v6.unguarded_candidate_transition_count'])}}}",
            f"\\newcommand{{\\FrozenAttritionUnguardedCandidateRequests}}{{{int(values['v6.unguarded_candidate_transition_requests'])}}}",
            f"\\newcommand{{\\FrozenAttritionSupportedUnguardedCandidates}}{{{int(values['v6.unguarded_candidate_transitions_development_supported'])}}}",
            f"\\newcommand{{\\FrozenAttritionFinalCandidates}}{{{int(values['v6.final_candidate_transition_count'])}}}",
            f"\\newcommand{{\\FrozenAttritionVFiveTerminalAbstentions}}{{{int(values['v5.final_terminal_abstention_requests'])}}}",
            f"\\newcommand{{\\FrozenAttritionVSixTerminalAbstentions}}{{{int(values['v6.final_terminal_abstention_requests'])}}}",
        ]
    ) + "\n"


def generated_joint_route_budget_shift_tex(values: dict[str, str]) -> str:
    quadratic_budget = int(values["JointQuadratic.selected_correction_budget"])
    cubic_budget = int(values["JointCubic.selected_correction_budget"])
    heldout_scenarios = (
        int(values["JointQuadratic.heldout_scenarios"])
        + int(values["JointCubic.heldout_scenarios"])
    )
    maximum_regret = float(values["maximum_heldout_regret"])
    maximum_calibration_error = float(values["maximum_action_calibration_error"])
    gate_mismatches = (
        int(values["JointQuadratic.heldout_gate_mismatches"])
        + int(values["JointCubic.heldout_gate_mismatches"])
    )
    first_rejections = (
        int(values["JointQuadratic.first_action_rejections"])
        + int(values["JointCubic.first_action_rejections"])
    )
    second_accepts = (
        int(values["JointQuadratic.second_action_accepts"])
        + int(values["JointCubic.second_action_accepts"])
    )
    return "\n".join(
        [
            "% Generated from build/release/joint-route-budget-shift/evidence.txt.",
            f"\\newcommand{{\\JointShiftQuadraticBudget}}{{{quadratic_budget}}}",
            f"\\newcommand{{\\JointShiftCubicBudget}}{{{cubic_budget}}}",
            f"\\newcommand{{\\JointShiftHeldoutScenarios}}{{{heldout_scenarios}}}",
            f"\\newcommand{{\\JointShiftMaximumRegret}}{{{maximum_regret:.3f}}}",
            f"\\newcommand{{\\JointShiftMaximumCalibration}}{{{maximum_calibration_error:.3f}}}",
            f"\\newcommand{{\\JointShiftGateMismatches}}{{{gate_mismatches}}}",
            f"\\newcommand{{\\JointShiftFirstRejections}}{{{first_rejections}}}",
            f"\\newcommand{{\\JointShiftSecondAccepts}}{{{second_accepts}}}",
        ]
    ) + "\n"


def generated_request_conditioned_joint_route_tex(values: dict[str, str]) -> str:
    return "\n".join(
        [
            "% Generated from build/release/request-conditioned-joint-route/evidence.txt.",
            f"\\newcommand{{\\ConditionedActionCount}}{{{int(values['action_count'])}}}",
            f"\\newcommand{{\\ConditionedHeldoutRequests}}{{{int(values['heldout_requests'])}}}",
            f"\\newcommand{{\\ConditionedCostMedianError}}{{{float(values['cost_prediction_median_relative_error']):.3f}}}",
            f"\\newcommand{{\\ConditionedBrier}}{{{float(values['pass_prediction_brier_score']):.4f}}}",
            f"\\newcommand{{\\ConditionedEce}}{{{float(values['pass_prediction_ece']):.4f}}}",
            f"\\newcommand{{\\ConditionedRegret}}{{{float(values['conditioned_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\StaticProfileRegret}}{{{float(values['static_profile_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\FixedActionRegret}}{{{float(values['fixed_action_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\ConditionedDistinctPlans}}{{{int(values['distinct_conditioned_plans'])}}}",
            f"\\newcommand{{\\ConditionedPlanChangePercent}}{{{100.0 * float(values['feature_changed_plan_fraction']):.1f}}}",
            f"\\newcommand{{\\ConditionedFallbacks}}{{{int(values['production_fallbacks'])}}}",
            f"\\newcommand{{\\ConditionedGateMismatches}}{{{int(values['production_gate_mismatches'])}}}",
        ]
    ) + "\n"


def generated_suitesparse_request_conditioned_route_tex(
    values: dict[str, str],
    v5_values: dict[str, str],
    v5_replay_values: dict[str, str],
) -> str:
    conditioned_regret = float(values["conditioned_heldout_regret"])
    fixed_regret = float(values["fixed_action_heldout_regret"])
    static_regret = float(values["static_profile_heldout_regret"])
    return "\n".join(
        [
            "% Generated from frozen v6 first-run evidence and the zero-execution v5 replay.",
            f"\\newcommand{{\\SuiteRouteActionCount}}{{{int(values['candidate_model_action_count'])}}}",
            f"\\newcommand{{\\SuiteRouteHeldoutMatrices}}{{{int(values['heldout_matrix_count'])}}}",
            f"\\newcommand{{\\SuiteRouteHeldoutRequests}}{{{int(values['heldout_requests'])}}}",
            f"\\newcommand{{\\SuiteRouteActionRepetitions}}{{{int(values['action_repetitions'])}}}",
            f"\\newcommand{{\\SuiteRouteCostMedianError}}{{{float(values['cost_prediction_median_relative_error']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteCostPNinetyFiveError}}{{{float(values['cost_prediction_p95_relative_error']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteCostMaxError}}{{{float(values['cost_prediction_maximum_relative_error']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteSelectedCostPNinetyFiveError}}{{{float(values['selected_cost_prediction_p95_relative_error']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteBrier}}{{{float(values['pass_prediction_brier_score']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteEce}}{{{float(values['pass_prediction_ece']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteMaxActionCalibration}}{{{float(values['pass_prediction_maximum_action_calibration_error']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteRawRegret}}{{{float(values['raw_conditioned_heldout_regret']):.6f}}}",
            f"\\newcommand{{\\SuiteRouteRegret}}{{{float(values['conditioned_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteRegretPrecise}}{{{conditioned_regret:.9f}}}",
            f"\\newcommand{{\\SuiteRouteStaticRegret}}{{{float(values['static_profile_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteFixedRegret}}{{{float(values['fixed_action_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteFamilyFixedRegret}}{{{float(values['family_fixed_action_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteFixedImprovementPercent}}{{{100.0 * (fixed_regret - conditioned_regret) / fixed_regret:.3f}}}",
            f"\\newcommand{{\\SuiteRouteStaticImprovementPercent}}{{{100.0 * (static_regret - conditioned_regret) / static_regret:.1f}}}",
            f"\\newcommand{{\\SuiteRouteSizeOnlyRegret}}{{{float(values['size_only_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteRhsOnlyRegret}}{{{float(values['rhs_only_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteToleranceOnlyRegret}}{{{float(values['tolerance_only_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteDistinctPlans}}{{{int(values['distinct_conditioned_plans'])}}}",
            f"\\newcommand{{\\SuiteRoutePlanChangePercent}}{{{100.0 * float(values['feature_changed_plan_fraction']):.1f}}}",
            f"\\newcommand{{\\SuiteRouteSuccesses}}{{{int(values['production_successes'])}}}",
            f"\\newcommand{{\\SuiteRouteFailures}}{{{int(values['production_failures'])}}}",
            f"\\newcommand{{\\SuiteRouteFallbacks}}{{{int(values['production_fallbacks'])}}}",
            f"\\newcommand{{\\SuiteRouteGateMismatches}}{{{int(values['production_gate_mismatches'])}}}",
            f"\\newcommand{{\\SuiteRoutePlanOrderMismatches}}{{{int(values['production_plan_order_mismatches'])}}}",
            f"\\newcommand{{\\SuiteRouteDpMismatches}}{{{int(values['dp_exhaustive_mismatches'])}}}",
            f"\\newcommand{{\\SuiteRouteInteractionDelta}}{{{float(values['maximum_action_interaction_delta']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteOrderDelta}}{{{float(values['maximum_action_order_delta']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteVFiveRequests}}{{{int(v5_replay_values['heldout_requests'])}}}",
            f"\\newcommand{{\\SuiteRouteVFiveReplayRegret}}{{{float(v5_replay_values['control_aware_replay_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteVFiveFixedRegret}}{{{float(v5_replay_values['official_fixed_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteVFiveStaticRegret}}{{{float(v5_values['static_profile_heldout_regret']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteVFiveReplayVsFixed}}{{{float(v5_replay_values['control_aware_vs_fixed_ratio']):.3f}}}",
            f"\\newcommand{{\\SuiteRouteVFiveSwitchedRequests}}{{{int(v5_replay_values['requests_switched_from_training_family'])}}}",
        ]
    ) + "\n"


def generated_pde_tex(values: dict[str, str]) -> str:
    lines = ["% Generated from build/release/pdebench-repeated-timing/evidence.txt."]
    medians = []
    for workload, command_workload in PDE_WORKLOADS:
        solves = int(values[f"{workload}.solves_per_run"])
        median = float(values[f"{workload}.median_speedup"])
        lower = float(values[f"{workload}.bootstrap_95_lower"])
        upper = float(values[f"{workload}.bootstrap_95_upper"])
        medians.append(median)
        lines.extend(
            [
                f"\\newcommand{{\\Pde{command_workload}Solves}}{{{solves}}}",
                f"\\newcommand{{\\Pde{command_workload}Median}}{{{median:.3f}}}",
                f"\\newcommand{{\\Pde{command_workload}Lower}}{{{lower:.3f}}}",
                f"\\newcommand{{\\Pde{command_workload}Upper}}{{{upper:.3f}}}",
            ]
        )
    lines.extend(
        [
            f"\\newcommand{{\\PdeMinimumShort}}{{{min(medians):.2f}}}",
            f"\\newcommand{{\\PdeMaximumShort}}{{{max(medians):.2f}}}",
            f"\\newcommand{{\\PdeNonCfdMaximumShort}}{{{max(medians[:-1]):.2f}}}",
            f"\\newcommand{{\\PdeMinimumRunWins}}{{{values['minimum_paired_run_wins']}}}",
        ]
    )
    return "\n".join(lines) + "\n"


def generated_pde_data(values: dict[str, str]) -> str:
    lines = [
        "% Generated by benchmark/analyze_pdebench_repeated_timing.py.",
        "workload speedup error_minus error_plus",
    ]
    for workload, _ in PDE_WORKLOADS:
        median = float(values[f"{workload}.median_speedup"])
        lower = float(values[f"{workload}.bootstrap_95_lower"])
        upper = float(values[f"{workload}.bootstrap_95_upper"])
        lines.append(
            f"{workload} {median:.10f} {median - lower:.10f} {upper - median:.10f}"
        )
    return "\n".join(lines) + "\n"


def generated_order_tex(values: dict[str, str]) -> str:
    diffusion_ratio = float(values["diffusion_sorption.paired_order_ratio_median"])
    diffusion_lower = float(
        values["diffusion_sorption.paired_order_ratio_bootstrap_95_lower"]
    )
    diffusion_upper = float(
        values["diffusion_sorption.paired_order_ratio_bootstrap_95_upper"]
    )
    cfd_ratio = float(values["cfd_1d.paired_order_ratio_median"])
    cfd_lower = float(values["cfd_1d.paired_order_ratio_bootstrap_95_lower"])
    cfd_upper = float(values["cfd_1d.paired_order_ratio_bootstrap_95_upper"])
    minimum_ratio = float(values["minimum_paired_order_ratio_median"])
    maximum_ratio = float(values["maximum_paired_order_ratio_median"])
    maximum_shift = 100.0 * float(values["maximum_absolute_median_order_shift"])
    lines = [
        "% Generated from build/release/pdebench-order-sensitivity/evidence.txt.",
        f"\\newcommand{{\\OrderSensitivityWorkloads}}{{{values['workloads']}}}",
        f"\\newcommand{{\\OrderSensitivityPairs}}{{{values['measured_pairs']}}}",
        f"\\newcommand{{\\OrderSensitivityMeasuredReports}}{{{values['measured_reports']}}}",
        f"\\newcommand{{\\OrderSensitivityWarmupReports}}{{{values['warmup_reports']}}}",
        f"\\newcommand{{\\OrderIntervalsContainingOne}}{{{values['bootstrap_95_intervals_containing_one']}}}",
        f"\\newcommand{{\\OrderIntervalsTotal}}{{{values['bootstrap_95_intervals_total']}}}",
        f"\\newcommand{{\\DiffusionOrderRatio}}{{{diffusion_ratio:.3f}}}",
        f"\\newcommand{{\\DiffusionOrderLower}}{{{diffusion_lower:.3f}}}",
        f"\\newcommand{{\\DiffusionOrderUpper}}{{{diffusion_upper:.3f}}}",
        f"\\newcommand{{\\CfdOrderRatio}}{{{cfd_ratio:.3f}}}",
        f"\\newcommand{{\\CfdOrderLower}}{{{cfd_lower:.3f}}}",
        f"\\newcommand{{\\CfdOrderUpper}}{{{cfd_upper:.3f}}}",
        f"\\newcommand{{\\MinimumOrderRatio}}{{{minimum_ratio:.3f}}}",
        f"\\newcommand{{\\MaximumOrderRatio}}{{{maximum_ratio:.3f}}}",
        f"\\newcommand{{\\MaximumOrderShiftPercent}}{{{maximum_shift:.1f}}}",
    ]
    return "\n".join(lines) + "\n"


def generated_operator_shared_tex(values: dict[str, str]) -> str:
    lines = [
        "% Generated from build/release/operator-shared-baseline/evidence.txt.",
        f"\\newcommand{{\\CalibratedRouterSpeedup}}{{{float(values['router.speedup']):.3f}}}",
        f"\\newcommand{{\\CalibratedRouterLower}}{{{float(values['router.lower']):.3f}}}",
        f"\\newcommand{{\\CalibratedRouterUpper}}{{{float(values['router.upper']):.3f}}}",
        f"\\newcommand{{\\CalibratedRouterWinPercent}}{{{float(values['router.win_percent']):.1f}}}",
        f"\\newcommand{{\\FusedLinearGateSpeedup}}{{{float(values['linear_gate.speedup']):.3f}}}",
        f"\\newcommand{{\\FusedLinearGateLower}}{{{float(values['linear_gate.lower']):.3f}}}",
        f"\\newcommand{{\\FusedNonlinearGateSpeedup}}{{{float(values['nonlinear_gate.speedup']):.3f}}}",
        f"\\newcommand{{\\FusedNonlinearGateLower}}{{{float(values['nonlinear_gate.lower']):.3f}}}",
    ]
    for family, command_family in (("linear", "Linear"), ("nonlinear", "Nonlinear")):
        lines.extend(
            [
                f"\\newcommand{{\\{command_family}OperatorSpeedup}}{{{float(values[f'{family}.verified_operator_speedup']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorLower}}{{{float(values[f'{family}.verified_operator_bootstrap_95_lower']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorUpper}}{{{float(values[f'{family}.verified_operator_bootstrap_95_upper']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorTrainingMs}}{{{float(values[f'{family}.training_wall_ms']):.2f}}}",
                f"\\newcommand{{\\{command_family}OperatorBreakEven}}{{{values[f'{family}.break_even_queries']}}}",
                f"\\newcommand{{\\{command_family}SharedBaselineSpeedup}}{{{float(values[f'{family}.shared_baseline_speedup']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorVsSharedBaseline}}{{{float(values[f'{family}.verified_operator_vs_shared_baseline_speedup']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorVsSharedBaselineLower}}{{{float(values[f'{family}.verified_operator_vs_shared_baseline_bootstrap_95_lower']):.3f}}}",
                f"\\newcommand{{\\{command_family}OperatorVsSharedBaselineUpper}}{{{float(values[f'{family}.verified_operator_vs_shared_baseline_bootstrap_95_upper']):.3f}}}",
                f"\\newcommand{{\\{command_family}SharedAccepted}}{{{values[f'{family}.accepted']}}}",
                f"\\newcommand{{\\{command_family}SharedFallbacks}}{{{values[f'{family}.fallbacks']}}}",
            ]
        )
    return "\n".join(lines) + "\n"


def generated_operator_shared_data(values: dict[str, str]) -> str:
    lines = [
        "% Generated by benchmark/analyze_operator_shared_baseline.py.",
        "workload speedup",
        f"LinearOperator {float(values['linear.verified_operator_speedup']):.10f}",
        f"LinearSharedBaseline {float(values['linear.shared_baseline_speedup']):.10f}",
        f"NonlinearOperator {float(values['nonlinear.verified_operator_speedup']):.10f}",
        f"NonlinearSharedBaseline {float(values['nonlinear.shared_baseline_speedup']):.10f}",
        f"CalibratedRouter {float(values['router.speedup']):.10f}",
        f"FusedLinearGate {float(values['linear_gate.speedup']):.10f}",
        f"FusedNonlinearGate {float(values['nonlinear_gate.speedup']):.10f}",
    ]
    return "\n".join(lines) + "\n"


def generated_hints_schedule_tex(values: dict[str, str]) -> str:
    average_iterations = float(values["average_iterations"])
    average_learned = float(values["average_learned_corrections"])
    maximum_gate_difference = float(values["maximum_gate_residual_difference"])
    return "\n".join(
        [
            "% Generated from build/release/hints-schedule-baseline/evidence.txt.",
            f"\\newcommand{{\\HintsScheduleAccepted}}{{{values['accepted']}}}",
            f"\\newcommand{{\\HintsScheduleFallbacks}}{{{values['fallbacks']}}}",
            f"\\newcommand{{\\HintsScheduleSpeedup}}{{{float(values['paired_median_speedup']):.3f}}}",
            f"\\newcommand{{\\HintsScheduleLower}}{{{float(values['bootstrap_95_lower']):.3f}}}",
            f"\\newcommand{{\\HintsScheduleUpper}}{{{float(values['bootstrap_95_upper']):.3f}}}",
            f"\\newcommand{{\\OperatorVsHintsSchedule}}{{{float(values['verified_operator_vs_hints_schedule_paired_median_speedup']):.3f}}}",
            f"\\newcommand{{\\OperatorVsHintsScheduleLower}}{{{float(values['verified_operator_vs_hints_schedule_bootstrap_95_lower']):.3f}}}",
            f"\\newcommand{{\\OperatorVsHintsScheduleUpper}}{{{float(values['verified_operator_vs_hints_schedule_bootstrap_95_upper']):.3f}}}",
            f"\\newcommand{{\\HintsAverageIterations}}{{{average_iterations:.2f}}}",
            f"\\newcommand{{\\HintsAverageLearnedCorrections}}{{{average_learned:.2f}}}",
            f"\\newcommand{{\\HintsGateResidualMismatches}}{{{values['gate_residual_mismatches']}}}",
            f"\\newcommand{{\\HintsMaximumGateResidualDifference}}{{\\num{{{maximum_gate_difference:.3e}}}}}",
        ]
    ) + "\n"


def generated_hints_native_tex(values: dict[str, str]) -> str:
    return "\n".join(
        [
            "% Generated from build/release/hints-native-baseline/evidence.txt.",
            f"\\newcommand{{\\HintsNativeCases}}{{{values['common_test_cases']}}}",
            f"\\newcommand{{\\HintsNativeSpeedup}}{{{float(values['paired_median_speedup']):.3f}}}",
            f"\\newcommand{{\\HintsNativeLower}}{{{float(values['bootstrap_95_lower']):.3f}}}",
            f"\\newcommand{{\\HintsNativeUpper}}{{{float(values['bootstrap_95_upper']):.3f}}}",
            f"\\newcommand{{\\HintsNativeAmortizedSpeedup}}{{{float(values['amortized_speedup']):.3f}}}",
            f"\\newcommand{{\\HintsNativeMaximumResidual}}{{\\num{{{float(values['maximum_common_gate_relative_inf']):.3e}}}}}",
        ]
    ) + "\n"


def generated_solver_analysis_tex(
    shift: dict[str, str],
    shift_matrix: dict[str, str],
    decomposition: dict[str, str],
) -> str:
    return "\n".join(
        [
            "% Generated from router-shift, router-shift-matrix, and complete-cost decomposition evidence.",
            f"\\newcommand{{\\ShiftGatePassingRankSpearman}}{{{float(shift['gate_passing_cost_rank_spearman']):.3f}}}",
            f"\\newcommand{{\\ShiftSelectedRegret}}{{{float(shift['source_selected_holdout_vs_fastest_gate_passing_median']):.3f}}}",
            f"\\newcommand{{\\ShiftGatePassingMaxCalibration}}{{{float(shift['heldout_gate_passing_max_calibration_error']):.3f}}}",
            f"\\newcommand{{\\ShiftAllMaxCalibration}}{{{float(shift['heldout_all_max_calibration_error']):.4f}}}",
            f"\\newcommand{{\\ShiftMatrixMinimumLower}}{{{float(shift_matrix['minimum_paired_speedup_ci95_lower']):.3f}}}",
            f"\\newcommand{{\\ShiftMatrixMaximumCalibration}}{{{float(shift_matrix['maximum_structurally_filtered_calibration_error']):.3f}}}",
            f"\\newcommand{{\\ShiftMatrixMaximumRegret}}{{{float(shift_matrix['maximum_selected_complete_cost_regret']):.3f}}}",
            f"\\newcommand{{\\TopologyGateStatusChanges}}{{{int(shift_matrix['topology.gate_status_changes'])}}}",
            f"\\newcommand{{\\LinearCandidateSharePercent}}{{{100.0 * float(decomposition['linear.candidate_share']):.2f}}}",
            f"\\newcommand{{\\LinearCorrectionGateSharePercent}}{{{100.0 * float(decomposition['linear.correction_runtime_gate_share']):.2f}}}",
            f"\\newcommand{{\\LinearGateSharePercent}}{{{100.0 * float(decomposition['linear.fused_gate_share']):.2f}}}",
            f"\\newcommand{{\\NonlinearCandidateSharePercent}}{{{100.0 * float(decomposition['nonlinear.candidate_share']):.2f}}}",
            f"\\newcommand{{\\NonlinearCorrectionGateSharePercent}}{{{100.0 * float(decomposition['nonlinear.correction_runtime_gate_share']):.2f}}}",
            f"\\newcommand{{\\NonlinearGateSharePercent}}{{{100.0 * float(decomposition['nonlinear.fused_gate_share']):.2f}}}",
            f"\\newcommand{{\\LinearMinimumCorrectionBudget}}{{{int(float(decomposition['linear.production_corrector_minimum_full_acceptance_budget']))}}}",
            f"\\newcommand{{\\NonlinearMinimumCorrectionBudget}}{{{int(float(decomposition['nonlinear.production_corrector_minimum_full_acceptance_budget']))}}}",
            f"\\newcommand{{\\NonlinearBudgetOneAcceptancePercent}}{{{100.0 * float(decomposition['nonlinear.production_corrector_budget1_acceptance_rate']):.2f}}}",
            f"\\newcommand{{\\NonlinearBudgetTwoVsZeroCostRatio}}{{{float(decomposition['nonlinear.production_corrector_budget2_vs_budget0_complete_ratio']):.3f}}}",
        ]
    ) + "\n"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> int:
    paper_directory = Path(__file__).resolve().parent
    repository_root = paper_directory.parent
    gate_evidence_path = (
        repository_root / "build/release/gate-parallel-scaling/evidence.txt"
    )
    gate_generated_path = paper_directory / "generated/gate_scaling_values.tex"
    gate_values = parse_evidence(gate_evidence_path)

    require(
        gate_values.get("strict_equivalence") == "1",
        "gate decision equivalence failed",
    )
    for family in ("linear", "nonlinear"):
        require(
            gate_values.get(f"{family}.repetitions") == "30",
            f"{family} repetitions changed",
        )
        require(
            gate_values.get(f"{family}.requests_per_repetition") == "2080",
            f"{family} request count changed",
        )
        require(
            gate_values.get(f"{family}.decision_mismatches") == "0",
            f"{family} decision mismatch",
        )
        require(
            gate_values.get(f"{family}.residual_mismatches") == "0",
            f"{family} residual mismatch",
        )

    expected_gate_tex = generated_gate_tex(gate_values)
    require(
        gate_generated_path.read_text() == expected_gate_tex,
        "generated gate macros are stale",
    )

    linear = float(gate_values["linear.worker_10.paired_speedup"])
    linear_lower = float(gate_values["linear.worker_10.bootstrap_95_lower"])
    linear_upper = float(gate_values["linear.worker_10.bootstrap_95_upper"])
    nonlinear = float(gate_values["nonlinear.worker_10.paired_speedup"])
    nonlinear_lower = float(gate_values["nonlinear.worker_10.bootstrap_95_lower"])
    nonlinear_upper = float(gate_values["nonlinear.worker_10.bootstrap_95_upper"])
    summary = (
        f"`{linear:.3f}× [{linear_lower:.3f}, {linear_upper:.3f}]` 与 "
        f"`{nonlinear:.3f}× [{nonlinear_lower:.3f}, {nonlinear_upper:.3f}]`"
    )
    ledger = (
        f"`{linear:.3f}× [{linear_lower:.3f}, {linear_upper:.3f}]` on a linear family "
        f"and `{nonlinear:.3f}× [{nonlinear_lower:.3f}, {nonlinear_upper:.3f}]`"
    )
    readme = (repository_root / "README.md").read_text()
    claim_ledger = (paper_directory / "CLAIM_EVIDENCE.md").read_text()
    require(summary in readme, "README gate summary is stale")
    require(ledger in claim_ledger, "claim ledger gate summary is stale")
    pde_evidence_path = (
        repository_root / "build/release/pdebench-repeated-timing/evidence.txt"
    )
    pde_generated_path = paper_directory / "generated/pde_timing_values.tex"
    pde_values = parse_evidence(pde_evidence_path)
    require(pde_values.get("repetitions") == "30", "PDE repetitions changed")
    require(pde_values.get("workloads") == "7", "PDE workload count changed")
    require(
        pde_values.get("measured_reports") == "210",
        "PDE measured-report count changed",
    )
    require(
        pde_values.get("warmup_reports") == "7",
        "PDE warm-up report count changed",
    )
    require(
        pde_values.get("all_bootstrap_95_lower_bounds_exceed_one") == "1",
        "PDE confidence lower bound failed",
    )
    for workload, _ in PDE_WORKLOADS:
        require(
            int(pde_values[f"{workload}.paired_run_wins"]) >= 29,
            f"{workload} paired-run wins fell below 29 of 30",
        )
    require(
        int(pde_values["minimum_paired_run_wins"]) >= 29,
        "minimum PDE paired-run wins fell below 29 of 30",
    )
    require(
        pde_generated_path.read_text() == generated_pde_tex(pde_values),
        "generated PDE macros are stale",
    )
    require(
        (paper_directory / "data/pdebench_repeated_timing.dat").read_text()
        == generated_pde_data(pde_values),
        "PDE plot data are stale",
    )
    pde_medians = [
        float(pde_values[f"{workload}.median_speedup"])
        for workload, _ in PDE_WORKLOADS
    ]
    pde_range = f"{min(pde_medians):.2f}×–{max(pde_medians):.2f}×"
    require(pde_range in readme, "README PDE range is stale")
    require(pde_range in claim_ledger, "claim ledger PDE range is stale")

    order_evidence_path = (
        repository_root / "build/release/pdebench-order-sensitivity/evidence.txt"
    )
    order_generated_path = paper_directory / "generated/order_sensitivity_values.tex"
    order_values = parse_evidence(order_evidence_path)
    require(order_values.get("repetitions") == "30", "order repetitions changed")
    require(order_values.get("workloads") == "7", "order workload count changed")
    require(
        order_values.get("measured_pairs") == "210",
        "order measured-pair count changed",
    )
    require(
        order_values.get("measured_reports") == "420",
        "order measured-report count changed",
    )
    require(
        order_values.get("warmup_reports") == "14",
        "order warm-up report count changed",
    )
    require(
        order_values.get("bootstrap_95_intervals_containing_one") == "6",
        "order interval coverage changed",
    )
    require(
        order_values.get("bootstrap_95_intervals_total") == "7",
        "order interval total changed",
    )
    require(
        order_values.get("all_measured_speedups_exceed_one") == "1",
        "an order-controlled measured speedup no longer exceeds one",
    )
    require(
        order_generated_path.read_text() == generated_order_tex(order_values),
        "generated order-sensitivity macros are stale",
    )

    shared_evidence_path = (
        repository_root / "build/release/operator-shared-baseline/evidence.txt"
    )
    shared_values = parse_evidence(shared_evidence_path)
    require(shared_values.get("families") == "2", "shared baseline family count changed")
    require(
        shared_values.get("total_attempted") == "12800",
        "shared baseline attempt count changed",
    )
    require(
        shared_values.get("total_failures") == "0",
        "shared baseline failure count changed",
    )
    require(
        shared_values.get("total_gate_mismatches") == "0",
        "shared baseline gate mismatch",
    )
    for family in ("linear", "nonlinear"):
        require(
            shared_values.get(f"{family}.same_accuracy") == "1",
            f"{family} shared baseline accuracy changed",
        )
        require(
            float(
                shared_values[
                    f"{family}.verified_operator_vs_shared_baseline_bootstrap_95_lower"
                ]
            )
            > 1.0,
            f"{family} verified operator no longer beats shared baseline",
        )
    require(shared_values.get("linear.accepted") == "6400", "linear baseline acceptance changed")
    require(shared_values.get("linear.fallbacks") == "0", "linear baseline fallback changed")
    require(shared_values.get("nonlinear.accepted") == "0", "nonlinear baseline acceptance changed")
    require(shared_values.get("nonlinear.fallbacks") == "6400", "nonlinear baseline fallback changed")
    require(
        (paper_directory / "generated/operator_shared_baseline_values.tex").read_text()
        == generated_operator_shared_tex(shared_values),
        "generated shared-baseline macros are stale",
    )
    require(
        (paper_directory / "data/operator_speedups.dat").read_text()
        == generated_operator_shared_data(shared_values),
        "operator speedup plot data are stale",
    )
    hints_values = parse_evidence(
        repository_root / "build/release/hints-schedule-baseline/evidence.txt"
    )
    for key, value in {
        "published_method": "HINTS",
        "published_paper_doi": "10.1038/s42256-024-00910-x",
        "algorithmic_schedule_reimplementation": "1",
        "official_public_code_executed": "0",
        "deep_onet_architecture_reproduced": "0",
        "official_pretrained_weights_used": "0",
        "shared_latent_operator_weights": "1",
        "evaluation_scenarios": "64",
        "repetitions": "100",
        "attempted": "6400",
        "accepted": "6400",
        "fallbacks": "0",
        "failures": "0",
        "gate_decision_mismatches": "0",
        "linear_matrix_assembly_in_timing": "0",
        "right_hand_side_update_in_timing": "1",
        "residual_kernel": "preassembled-linear-system-multiply",
        "same_accuracy": "1",
        "numerical_to_learned_ratio": "25",
        "maximum_iterations": "400",
        "published_full_implementation_claim": "0",
        "negative_result_retained": "1",
        "all_failures_retained": "1",
    }.items():
        require(hints_values.get(key) == value, f"HINTS schedule field changed: {key}")
    require(
        float(hints_values["bootstrap_95_upper"]) < 1.0,
        "HINTS schedule negative complete-cost result is no longer stable",
    )
    require(
        float(
            hints_values[
                "verified_operator_vs_hints_schedule_bootstrap_95_lower"
            ]
        ) > 1.0,
        "verified operator no longer beats the HINTS schedule control",
    )
    require(
        float(hints_values["maximum_mixed_qoi_error"]) <= 1.0,
        "HINTS schedule accuracy changed",
    )
    require(
        float(hints_values["maximum_gate_residual_difference"]) <= 1.0e-6,
        "HINTS schedule residual kernel diverged from the reference gate",
    )
    require(
        (paper_directory / "generated/hints_schedule_values.tex").read_text()
        == generated_hints_schedule_tex(hints_values),
        "generated HINTS schedule macros are stale",
    )
    native_hints_values = parse_evidence(
        repository_root / "build/release/hints-native-baseline/evidence.txt"
    )
    for key, value in {
        "published_method": "HINTS",
        "published_paper_doi": "10.1038/s42256-024-00910-x",
        "official_code_revision": "0c8b712f81ed08bdf27c3a215f8edb99910f5e2f",
        "official_public_code_executed": "1",
        "official_pretrained_weights_used": "1",
        "official_deeponet_architecture_executed": "1",
        "official_dataset_used": "1",
        "common_problem": "official-HINTS-1D-Poisson-test-set",
        "common_test_cases": "750",
        "common_original_equation_gate": "1",
        "online_timing_excludes_model-load-and-matrix-setup": "1",
        "setup_reported_separately": "1",
        "official_diagnostic_metric_loop_timed": "0",
        "smave_default_production_router": "1",
        "smave_selected_expert": "structured-tridiagonal-direct-cpu-v1",
        "official_failures": "0",
        "smave_failures": "0",
    }.items():
        require(
            native_hints_values.get(key) == value,
            f"native HINTS field changed: {key}",
        )
    require(
        float(native_hints_values["bootstrap_95_lower"]) > 1.0,
        "native HINTS paired lower bound no longer exceeds one",
    )
    require(
        float(native_hints_values["amortized_speedup"]) > 1.0,
        "native HINTS amortized speedup no longer exceeds one",
    )
    require(
        float(native_hints_values["maximum_common_gate_relative_inf"]) <= 1.0e-10,
        "native HINTS common residual gate regressed",
    )
    require(
        (paper_directory / "generated/hints_native_values.tex").read_text()
        == generated_hints_native_tex(native_hints_values),
        "generated native HINTS macros are stale",
    )
    require(
        "On all 750 cases from the official HINTS 1D Poisson test set" in claim_ledger,
        "claim ledger native HINTS statement is missing",
    )
    cascade_values = parse_evidence(
        repository_root / "build/release/cascade-ordering/evidence.txt"
    )
    for key, value in {
        "stages": "4",
        "permutations": "24",
        "terminal_cost_us": "10",
        "selected_order": "c,b,d,a",
        "exhaustive_optimum_match": "1",
    }.items():
        require(cascade_values.get(key) == value, f"cascade ordering field changed: {key}")
    require(
        abs(
            float(cascade_values["selected_expected_cost"])
            - float(cascade_values["exhaustive_minimum_cost"])
        )
        <= 1.0e-12,
        "cascade ordering no longer matches exhaustive minimum",
    )
    require(
        "exact minimum over all 24 permutations" in claim_ledger,
        "claim ledger cascade-ordering statement is missing",
    )
    joint_values = parse_evidence(
        repository_root / "build/release/joint-route-budget/evidence.txt"
    )
    for key, value in {
        "contract": "exact-dp-joint-expert-budget-complete-cost",
        "experts": "2",
        "actions": "3",
        "selected_steps": "2",
        "oracle_gap_us": "0",
        "same_expert_budget_exclusivity": "1",
        "property_sweep_cases": "256",
        "property_sweep_actions_per_case": "6",
        "property_sweep_exhaustive_oracle_match": "1",
        "property_sweep_maximum_oracle_gap_us": "0",
        "property_sweep_expert_exclusivity": "1",
        "property_sweep_ordering_invariant": "1",
        "interaction_property_sweep_cases": "256",
        "interaction_property_sweep_actions_per_case": "6",
        "interaction_property_sweep_transitions_per_case": "24",
        "interaction_property_sweep_exhaustive_oracle_match": "1",
        "interaction_property_sweep_maximum_oracle_gap_us": "0",
        "interaction_property_sweep_expert_exclusivity": "1",
        "interaction_property_sweep_changed_from_independent": "1",
        "interaction_property_sweep_changed_cases": "126",
        "interaction_hardness_reduction": "directed-hamiltonian-path",
        "interaction_hardness_vertices": "4",
        "interaction_hardness_graphs": "4096",
        "interaction_hardness_threshold_us": "2.1875",
        "interaction_hardness_reduction_match": "1",
        "interaction_hardness_full_length": "1",
        "runtime.first_action_rejected": "1",
        "runtime.second_action_accepted": "1",
        "runtime.original_equation_gate_accept": "1",
        "runtime.terminal_fallback_used": "0",
    }.items():
        require(joint_values.get(key) == value, f"joint routing field changed: {key}")
    require(
        float(joint_values["joint_vs_best_single_ratio"]) < 1.0,
        "joint routing no longer improves over the best single action",
    )
    require(
        (paper_directory / "generated/joint_route_budget_values.tex").read_text()
        == generated_joint_route_budget_tex(joint_values),
        "generated joint-route-budget macros are stale",
    )
    require(
        "exact finite-action recurrence is proved globally optimal" in claim_ledger,
        "claim ledger joint-route-budget statement is missing",
    )
    require(
        "interaction-aware finite cascade decision problem is proved NP-complete"
        in claim_ledger,
        "claim ledger interaction-aware hardness statement is missing",
    )
    scaling_values = parse_evidence(
        repository_root / "build/release/joint-route-scaling-round51/evidence.txt"
    )
    for key, value in {
        "contract": "deterministic-production-dp-tractability",
        "scope": "planning-only-no-numerical-solver-execution",
        "measured_profiles": "9",
        "measured_uniform_profiles": "8",
        "uniform_budgets_per_expert": "2",
        "uniform_top_k_cap": "6",
        "maximum_joint_states": "1000000",
        "largest_measured_experts": "16",
        "largest_measured_actions": "32",
        "largest_measured_interaction_states": "158209",
        "largest_measured_interaction_transitions": "1412192",
        "state_estimator_matches_measured_recurrence": "1",
        "exact_cap_acceptance_and_preflight_rejection": "1",
        "production_shape_actions": "12",
        "production_shape_interaction_states": "49",
        "production_shape_interaction_transitions": "204",
        "default_cap_largest_even_uniform_actions": "40",
        "default_cap_largest_even_uniform_states": "666561",
        "default_cap_first_rejected_even_uniform_actions": "44",
        "default_cap_first_rejected_even_uniform_states": "1227425",
        "preflight_rejections_before_state_visit": "3",
        "timing_claim": "0",
        "immutable_v4_v5_v6_solver_execution": "0",
        "cohort_search": "0",
        "policy_tuning": "0",
    }.items():
        require(
            scaling_values.get(key) == value,
            f"joint routing scaling field changed: {key}",
        )
    require(
        (paper_directory / "generated/joint_route_scaling_values.tex").read_text()
        == generated_joint_route_scaling_tex(scaling_values),
        "generated joint-route-scaling macros are stale",
    )
    require(
        "prefrozen planning-only tractability study" in claim_ledger,
        "claim ledger joint-route-scaling statement is missing",
    )
    interaction_prevalence_values = parse_evidence(
        repository_root
        / "build/release/frozen-interaction-prevalence-round52/evidence.txt"
    )
    for key, value in {
        "analysis_mode": "posthoc-frozen-observation-diagnostic",
        "stable_failure": "all-repetitions-failed-with-identical-status",
        "pair_definition": "ordered-distinct-expert-actions",
        "heldout_excluded_from_selection_and_calibration": "1",
        "v5.training_requests_with_two_expert_failures": "38",
        "v5.calibration_requests_with_two_expert_failures": "24",
        "v5.heldout_requests_with_two_expert_failures": "8",
        "v5.development_supported_pairs": "32",
        "v5.heldout_observed_pairs": "8",
        "v5.development_heldout_overlap": "0",
        "v5.plan_gated_candidate_transitions": "0",
        "v5.conditional_timing_rows": "0",
        "v5.calibrated_transitions": "0",
        "v6.training_requests_with_two_expert_failures": "38",
        "v6.calibration_requests_with_two_expert_failures": "24",
        "v6.heldout_requests_with_two_expert_failures": "16",
        "v6.development_supported_pairs": "32",
        "v6.heldout_observed_pairs": "56",
        "v6.development_heldout_overlap": "0",
        "v6.plan_gated_candidate_transitions": "0",
        "v6.conditional_timing_rows": "0",
        "v6.calibrated_transitions": "0",
        "development_supported_pairs": "32",
        "development_supported_expert_pair_classes": "1",
        "v5_v6_development_pair_set_equal": "1",
        "v5_v6_heldout_pair_intersection": "8",
        "v5_v6_heldout_pair_union": "56",
        "development_to_both_heldout_overlap": "0",
        "isolated_failure_support_is_not_conditional_calibration": "1",
        "heldout_diagnostic_only": "1",
        "conditional_timing_inference": "0",
        "policy_tuning": "0",
        "cohort_search": "0",
        "solver_execution": "0",
    }.items():
        require(
            interaction_prevalence_values.get(key) == value,
            f"frozen interaction prevalence field changed: {key}",
        )
    require(
        abs(
            float(interaction_prevalence_values["v5_v6_heldout_pair_jaccard"])
            - 1.0 / 7.0
        )
        < 1.0e-15,
        "frozen interaction held-out pair overlap changed",
    )
    require(
        (
            paper_directory
            / "generated/frozen_interaction_prevalence_values.tex"
        ).read_text()
        == generated_frozen_interaction_prevalence_tex(
            interaction_prevalence_values
        ),
        "generated frozen-interaction-prevalence macros are stale",
    )
    require(
        "post-hoc frozen-observation interaction audit" in claim_ledger,
        "claim ledger frozen-interaction-prevalence statement is missing",
    )
    transition_attrition_values = parse_evidence(
        repository_root
        / "build/release/frozen-transition-attrition-round53/evidence.txt"
    )
    for key, value in {
        "analysis_mode": "posthoc-frozen-observation-zero-execution-diagnostic",
        "candidate_rule": "adjacent-actions-in-control-aware-training-plan-with-first-action-failed",
        "heldout_excluded_from_all_attrition_counts": "1",
        "exact_control_aware_route_crosscheck": "1",
        "v5.model_byte_identical": "1",
        "v5.training_requests": "48",
        "v5.development_supported_pairs": "32",
        "v5.unguarded_multistep_requests": "48",
        "v5.final_terminal_abstention_requests": "0",
        "v5.final_single_action_requests": "48",
        "v5.final_multistep_requests": "0",
        "v5.control_aware_changed_requests": "48",
        "v5.unguarded_candidate_transition_count": "5",
        "v5.unguarded_candidate_transition_requests": "16",
        "v5.unguarded_candidate_transitions_development_supported": "0",
        "v5.final_candidate_transition_count": "0",
        "v5.development_pairs_eliminated_at_unguarded_top3_selection": "32",
        "v5.all_unguarded_candidates_removed_by_control_aware_plan": "5",
        "v6.model_byte_identical": "1",
        "v6.training_requests": "48",
        "v6.development_supported_pairs": "32",
        "v6.unguarded_multistep_requests": "48",
        "v6.final_terminal_abstention_requests": "16",
        "v6.final_single_action_requests": "32",
        "v6.final_multistep_requests": "0",
        "v6.control_aware_changed_requests": "48",
        "v6.unguarded_candidate_transition_count": "5",
        "v6.unguarded_candidate_transition_requests": "17",
        "v6.unguarded_candidate_transitions_development_supported": "0",
        "v6.final_candidate_transition_count": "0",
        "v6.development_pairs_eliminated_at_unguarded_top3_selection": "32",
        "v6.all_unguarded_candidates_removed_by_control_aware_plan": "5",
        "conditional_timing_inference": "0",
        "counterfactual_gain_inference": "0",
        "policy_tuning": "0",
        "cohort_search": "0",
        "solver_execution": "0",
    }.items():
        require(
            transition_attrition_values.get(key) == value,
            f"frozen transition attrition field changed: {key}",
        )
    require(
        (
            paper_directory
            / "generated/frozen_transition_attrition_values.tex"
        ).read_text()
        == generated_frozen_transition_attrition_tex(transition_attrition_values),
        "generated frozen-transition-attrition macros are stale",
    )
    require(
        "all 32 supported pairs are eliminated at unguarded top-3 selection"
        in claim_ledger
        and "five different, development-unsupported unguarded candidates"
        in claim_ledger,
        "claim ledger frozen-transition-attrition statement is missing",
    )
    joint_shift_values = parse_evidence(
        repository_root / "build/release/joint-route-budget-shift/evidence.txt"
    )
    for key, value in {
        "contract": "training-profile-to-heldout-production-joint-policy",
        "families": "quadratic,cubic",
        "training_repetitions": "3",
        "heldout_repetitions": "3",
        "JointQuadratic.training_scenarios": "32",
        "JointQuadratic.heldout_scenarios": "32",
        "JointQuadratic.actions": "3",
        "JointQuadratic.selected_steps": "2",
        "JointQuadratic.selected_correction_budget": "2",
        "JointQuadratic.heldout_failures": "0",
        "JointQuadratic.heldout_gate_mismatches": "0",
        "JointCubic.training_scenarios": "32",
        "JointCubic.heldout_scenarios": "32",
        "JointCubic.actions": "3",
        "JointCubic.selected_steps": "2",
        "JointCubic.selected_correction_budget": "4",
        "JointCubic.heldout_failures": "0",
        "JointCubic.heldout_gate_mismatches": "0",
        "budget_changes_across_families": "1",
        "all_heldout_success": "1",
        "all_heldout_zero_gate_mismatches": "1",
    }.items():
        require(
            joint_shift_values.get(key) == value,
            f"joint routing shift field changed: {key}",
        )
    require(
        float(joint_shift_values["maximum_heldout_regret"]) <= 1.25,
        "joint routing held-out regret exceeded 1.25",
    )
    require(
        float(joint_shift_values["maximum_action_calibration_error"]) <= 0.1,
        "joint routing action calibration error exceeded 0.1",
    )
    require(
        int(joint_shift_values["JointQuadratic.heldout_successes"])
        + int(joint_shift_values["JointCubic.heldout_successes"])
        == 64,
        "joint routing held-out success count changed",
    )
    require(
        int(joint_shift_values["JointQuadratic.first_action_rejections"])
        + int(joint_shift_values["JointCubic.first_action_rejections"])
        == 32,
        "joint routing first-stage rejection count changed",
    )
    require(
        int(joint_shift_values["JointQuadratic.second_action_accepts"])
        + int(joint_shift_values["JointCubic.second_action_accepts"])
        == 32,
        "joint routing second-stage acceptance count changed",
    )
    require(
        (paper_directory / "generated/joint_route_budget_shift_values.tex").read_text()
        == generated_joint_route_budget_shift_tex(joint_shift_values),
        "generated joint-route-budget shift macros are stale",
    )
    require(
        "Training profiles select correction budgets `2` and `4`" in claim_ledger,
        "claim ledger joint-route-budget shift statement is missing",
    )
    conditioned_values = parse_evidence(
        repository_root / "build/release/request-conditioned-joint-route/evidence.txt"
    )
    for key, value in {
        "contract": "production-trace-trained-request-conditioned-expert-budget-routing",
        "family_count": "3",
        "expert_count": "3",
        "budgets_per_expert": "4",
        "action_count": "12",
        "top_k": "3",
        "training_requests": "192",
        "calibration_requests": "96",
        "heldout_requests": "192",
        "training_action_observations": "2304",
        "calibration_action_observations": "1152",
        "heldout_action_observations": "2304",
        "action_observation_table": "action-observations.tsv",
        "frozen_model_parameters": "request-conditioned-model.txt",
        "dp_exhaustive_mismatches": "0",
        "production_successes": "192",
        "production_failures": "0",
        "production_gate_mismatches": "0",
        "original_equation_gate_preserved": "1",
        "terminal_fallback_preserved": "1",
        "heldout_not_used_for_training_or_calibration": "1",
        "realized_oracle_exhaustive": "1",
    }.items():
        require(
            conditioned_values.get(key) == value,
            f"request-conditioned routing field changed: {key}",
        )
    conditioned_regret = float(conditioned_values["conditioned_heldout_regret"])
    static_regret = float(conditioned_values["static_profile_heldout_regret"])
    fixed_regret = float(conditioned_values["fixed_action_heldout_regret"])
    require(
        conditioned_regret <= 1.35
        and conditioned_regret < static_regret
        and conditioned_regret < fixed_regret,
        "request-conditioned held-out regret contract failed",
    )
    require(
        float(conditioned_values["cost_prediction_median_relative_error"]) <= 0.35,
        "request-conditioned median cost error exceeded 0.35",
    )
    require(
        float(conditioned_values["pass_prediction_brier_score"]) <= 0.16
        and float(conditioned_values["pass_prediction_ece"]) <= 0.16
        and float(
            conditioned_values["pass_prediction_maximum_action_calibration_error"]
        )
        <= 0.2,
        "request-conditioned pass calibration contract failed",
    )
    require(
        int(conditioned_values["distinct_conditioned_plans"]) >= 4
        and float(conditioned_values["feature_changed_plan_fraction"]) >= 0.25,
        "request-conditioned plans did not adapt to request features",
    )
    observations = (
        repository_root
        / "build/release/request-conditioned-joint-route/action-observations.tsv"
    ).read_text().splitlines()
    require(
        len(observations) == 5761
        and observations[0].startswith("split\tfamily\tscenario\troot\tregime"),
        "request-conditioned action observation table is incomplete",
    )
    frozen_model = (
        repository_root
        / "build/release/request-conditioned-joint-route/request-conditioned-model.txt"
    ).read_text().splitlines()
    require(
        frozen_model[0] == "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 1"
        and frozen_model[-1] == "END"
        and sum(line.startswith("action=") for line in frozen_model) == 12,
        "request-conditioned frozen model contract failed",
    )
    require(
        (
            paper_directory / "generated/request_conditioned_joint_route_values.tex"
        ).read_text()
        == generated_request_conditioned_joint_route_tex(conditioned_values),
        "generated request-conditioned routing macros are stale",
    )
    require(
        "A request-conditioned model fitted from production action traces" in claim_ledger,
        "claim ledger request-conditioned routing statement is missing",
    )
    suitesparse_v6_directory = (
        repository_root
        / "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run"
    )
    suitesparse_conditioned_values = parse_evidence(
        suitesparse_v6_directory / "evidence.txt"
    )
    for key, value in {
        "contract": "group-disjoint-final-heldout-v6-production-sparse-expert-budget-routing",
        "snapshot_date": "2026-07-27",
        "matrix_id_disjoint": "1",
        "collection_group_disjoint": "1",
        "all_prior_development_heldout_excluded": "1",
        "all_pre_v6_locked_groups_excluded": "1",
        "training_matrix_count": "6",
        "calibration_matrix_count": "4",
        "heldout_matrix_count": "3",
        "training_requests": "48",
        "calibration_requests": "32",
        "heldout_requests": "24",
        "action_repetitions": "5",
        "model_action_count": "20",
        "candidate_model_action_count": "19",
        "matrix_row_limit": "10000",
        "built_in_direct_row_limit": "512",
        "all_frozen_compatible_actions_executed": "1",
        "unstable_action_observations": "0",
        "control_aware_family_anchor_gate": "1",
        "control_aware_heldout_excluded_from_selection_and_calibration": "1",
        "family_anchor_specialized_family_count": "0",
        "family_adaptation_enabled_family_count": "0",
        "conditional_cost_selected_transition_count": "0",
        "dp_exhaustive_mismatches": "0",
        "production_successes": "24",
        "production_failures": "0",
        "production_gate_mismatches": "0",
        "production_plan_order_mismatches": "0",
        "terminal_only_successes": "24",
        "conditioned_beats_static": "1",
        "conditioned_beats_fixed": "1",
        "conditioned_beats_family_fixed": "0",
        "negative_results_retained": "1",
        "original_equation_gate_recomputed": "1",
        "terminal_numerical_fallback_preserved": "1",
    }.items():
        require(
            suitesparse_conditioned_values.get(key) == value,
            f"SuiteSparse request-conditioned routing field changed: {key}",
        )
    require(
        float(suitesparse_conditioned_values["conditioned_heldout_regret"])
        < float(suitesparse_conditioned_values["fixed_action_heldout_regret"])
        < float(suitesparse_conditioned_values["static_profile_heldout_regret"])
        and abs(
            float(suitesparse_conditioned_values["conditioned_heldout_regret"])
            - float(
                suitesparse_conditioned_values["family_fixed_action_heldout_regret"]
            )
        )
        < 1.0e-12
        and float(suitesparse_conditioned_values["raw_conditioned_heldout_regret"])
        > float(suitesparse_conditioned_values["fixed_action_heldout_regret"]),
        "SuiteSparse routing control ordering changed",
    )
    suitesparse_observations = (
        suitesparse_v6_directory / "action-observations.tsv"
    ).read_text().splitlines()
    require(
        len(suitesparse_observations)
        == int(suitesparse_conditioned_values["raw_action_observations"]) + 1,
        "SuiteSparse action observation table is incomplete",
    )
    suitesparse_v5_directory = (
        repository_root
        / "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run"
    )
    suitesparse_v5_values = parse_evidence(suitesparse_v5_directory / "evidence.txt")
    for key, value in {
        "contract": "group-disjoint-final-heldout-v5-production-sparse-expert-budget-routing",
        "heldout_requests": "24",
        "production_successes": "24",
        "production_failures": "0",
        "production_gate_mismatches": "0",
        "production_plan_order_mismatches": "0",
        "dp_exhaustive_mismatches": "0",
        "conditioned_beats_static": "0",
        "conditioned_beats_fixed": "0",
        "conditioned_beats_family_fixed": "0",
        "negative_results_retained": "1",
    }.items():
        require(
            suitesparse_v5_values.get(key) == value,
            f"SuiteSparse v5 routing field changed: {key}",
        )
    suitesparse_v5_replay_directory = (
        repository_root / "build/release/suitesparse-control-aware-replay-v5"
    )
    suitesparse_v5_replay_values = parse_evidence(
        suitesparse_v5_replay_directory / "evidence.txt"
    )
    for key, value in {
        "contract": "frozen-observation-counterfactual-no-solver-reexecution",
        "heldout_requests": "24",
        "requests_switched_from_training_family": "0",
        "solver_reexecution": "0",
    }.items():
        require(
            suitesparse_v5_replay_values.get(key) == value,
            f"SuiteSparse v5 replay field changed: {key}",
        )
    require(
        abs(
            float(suitesparse_v5_replay_values["recomputed_fixed_regret"])
            - float(suitesparse_v5_replay_values["official_fixed_regret"])
        )
        < 1.0e-12
        and abs(
            float(suitesparse_v5_replay_values["control_aware_replay_regret"])
            - float(
                suitesparse_v5_replay_values[
                    "official_training_family_fixed_regret"
                ]
            )
        )
        < 1.0e-12
        and float(suitesparse_v5_replay_values["control_aware_vs_fixed_ratio"])
        > 1.7,
        "SuiteSparse v5 replay no-repair result changed",
    )
    for relative_path, evidence_key in {
        "benchmark/data-lock/suitesparse-final-heldout-v5-selection.tsv":
            "final_heldout_selection_sha256",
        "benchmark/data-lock/suitesparse-final-heldout-v5-payload.tsv":
            "final_heldout_payload_sha256",
        "benchmark/data-lock/suitesparse-final-heldout-v5.tsv":
            "final_heldout_freeze_sha256",
    }.items():
        require(
            sha256(repository_root / relative_path)
            == suitesparse_v5_values[evidence_key],
            f"SuiteSparse v5 frozen artifact changed: {relative_path}",
        )
    for relative_path, evidence_key in {
        "benchmark/data-lock/suitesparse-final-heldout-v6-selection.tsv":
            "final_heldout_selection_sha256",
        "benchmark/data-lock/suitesparse-final-heldout-v6-payload.tsv":
            "final_heldout_payload_sha256",
        "benchmark/data-lock/suitesparse-final-heldout-v6.tsv":
            "final_heldout_freeze_sha256",
    }.items():
        require(
            sha256(repository_root / relative_path)
            == suitesparse_conditioned_values[evidence_key],
            f"SuiteSparse v6 frozen artifact changed: {relative_path}",
        )
    require(
        sha256(suitesparse_v5_directory / "evidence.txt")
        == suitesparse_v5_replay_values["source_evidence_sha256"]
        and sha256(suitesparse_v5_directory / "request-conditioned-model.txt")
        == suitesparse_v5_replay_values["source_model_sha256"]
        and sha256(suitesparse_v5_directory / "action-observations.tsv")
        == suitesparse_v5_replay_values["source_observations_sha256"]
        and sha256(suitesparse_v5_directory / "request-summary.tsv")
        == suitesparse_v5_replay_values["source_request_summary_sha256"]
        and sha256(suitesparse_v5_replay_directory / "decisions.tsv")
        == suitesparse_v5_replay_values["decisions_sha256"],
        "SuiteSparse v5 replay source or decision hash changed",
    )
    require(
        (
            paper_directory
            / "generated/suitesparse_request_conditioned_route_values.tex"
        ).read_text()
        == generated_suitesparse_request_conditioned_route_tex(
            suitesparse_conditioned_values,
            suitesparse_v5_values,
            suitesparse_v5_replay_values,
        ),
        "generated SuiteSparse request-conditioned routing macros are stale",
    )
    require(
        "The frozen v5 control-aware replay switches zero requests" in claim_ledger
        and "The untouched v6 SuiteSparse cohort" in claim_ledger,
        "claim ledger SuiteSparse request-conditioned statement is missing",
    )
    shift_values = parse_evidence(
        repository_root / "build/release/router-shift/evidence.txt"
    )
    for key, value in {
        "contract": "source-5x5-to-heldout-6x6-complete-cost-shift",
        "winner_preserved": "1",
        "gate_status_mismatches": "0",
    }.items():
        require(shift_values.get(key) == value, f"router shift field changed: {key}")
    require(
        float(shift_values["gate_passing_cost_rank_spearman"]) >= 0.8,
        "gate-passing expert ordering is unstable under family shift",
    )
    require(
        float(shift_values["heldout_gate_passing_max_calibration_error"]) <= 0.02,
        "gate-passing expert calibration exceeded its observed bound",
    )
    shift_matrix_values = parse_evidence(
        repository_root / "build/release/router-shift-matrix/evidence.txt"
    )
    for key, value in {
        "contract": "source-calibrated-complete-cost-router-under-conditioning-and-topology-shift",
        "axes": "conditioning,topology",
        "all_axes_zero_gate_mismatches": "1",
        "all_axes_zero_dangerous_misroutes": "1",
        "all_axes_safe": "1",
    }.items():
        require(
            shift_matrix_values.get(key) == value,
            f"router shift matrix field changed: {key}",
        )
    require(
        int(shift_matrix_values["topology.gate_status_changes"]) >= 2,
        "topology shift no longer changes enough expert gate statuses",
    )
    require(
        float(shift_matrix_values["maximum_structurally_filtered_calibration_error"])
        <= 0.02,
        "shift-matrix calibration exceeded its observed bound",
    )
    require(
        float(shift_matrix_values["minimum_paired_speedup_ci95_lower"]) > 1.0,
        "shift-matrix paired speedup lower bound no longer exceeds one",
    )
    require(
        float(shift_matrix_values["maximum_selected_complete_cost_regret"]) <= 1.5,
        "shift-matrix selected complete-cost regret exceeded its bound",
    )
    decomposition_values = parse_evidence(
        repository_root / "build/release/complete-cost-decomposition/evidence.txt"
    )
    require(
        decomposition_values.get("families") == "linear,nonlinear",
        "complete-cost decomposition family set changed",
    )
    for family in ("linear", "nonlinear"):
        require(
            decomposition_values.get(f"{family}.correction_runtime_dominant") == "1",
            f"{family} correction/runtime gate is no longer dominant",
        )
        require(
            decomposition_values.get(f"{family}.full_acceptance_rate") == "1"
            and decomposition_values.get(f"{family}.fallbacks") == "0"
            and decomposition_values.get(f"{family}.failures") == "0",
            f"{family} complete-cost decomposition outcome changed",
        )
        require(
            decomposition_values.get(
                f"{family}.production_corrector_sweep_failures"
            ) == "0",
            f"{family} production corrector budget sweep has failures",
        )
    require(
        decomposition_values.get(
            "linear.production_corrector_minimum_full_acceptance_budget"
        ) == "0",
        "linear minimum correction budget changed",
    )
    require(
        decomposition_values.get(
            "nonlinear.production_corrector_minimum_full_acceptance_budget"
        ) == "2",
        "nonlinear minimum correction budget changed",
    )
    require(
        float(
            decomposition_values[
                "nonlinear.production_corrector_budget2_vs_budget0_complete_ratio"
            ]
        ) < 1.0,
        "nonlinear budget 2 no longer improves over zero correction",
    )
    require(
        (paper_directory / "generated/solver_analysis_values.tex").read_text()
        == generated_solver_analysis_tex(
            shift_values, shift_matrix_values, decomposition_values
        ),
        "generated solver-analysis macros are stale",
    )
    require(
        "gate-passing-expert cost-rank Spearman" in claim_ledger,
        "claim ledger router-shift statement is missing",
    )
    require(
        "conditioning and topology shift" in claim_ledger,
        "claim ledger router-shift-matrix statement is missing",
    )
    require(
        "correction plus runtime gate accounts for" in claim_ledger,
        "claim ledger complete-cost decomposition statement is missing",
    )
    require(
        "minimum full-acceptance correction budgets" in claim_ledger,
        "claim ledger correction-budget statement is missing",
    )
    calibrated_budget_values = parse_evidence(
        repository_root / "build/release/calibrated-correction-router/evidence.txt"
    )
    for key, value in {
        "contract": "production-router-propagates-profiled-correction-budget",
        "budget0.plan_budget": "0",
        "budget0.full_fallback": "1",
        "budget2.plan_budget": "2",
        "budget2.warm_start_accept": "1",
        "budget2.original_equation_gate_accept": "1",
        "zero_budget_raw_residual_check_preserved": "1",
        "numerical_fallback_preserved": "1",
    }.items():
        require(
            calibrated_budget_values.get(key) == value,
            f"calibrated correction Router field changed: {key}",
        )
    require(
        "profiled correction budget" in claim_ledger,
        "claim ledger calibrated correction Router statement is missing",
    )
    data_lock_values = parse_evidence(repository_root / "build/release/data-lock/evidence.txt")
    for key, value in {
        "contract": "full-local-consumed-data-lock",
        "pdebench_dataset_doi": "10.18419/DARUS-2986",
        "pdebench_dataset_version": "8.0",
        "pdebench_license": "CC-BY-4.0",
        "pdebench_files": "7",
        "pdebench_bytes": "50937093313",
        "suitesparse_collection_doi": "10.1145/2049662.2049663",
        "suitesparse_license": "CC-BY-4.0",
        "suitesparse_systems": "66",
        "suitesparse_matrix_files": "66",
        "suitesparse_rhs_files": "6",
        "suitesparse_bytes": "3911822320",
        "size_verified": "1",
        "official_md5_verified": "1",
        "sha256_verified": "1",
        "embedded_matrix_metadata_verified": "1",
        "upstream_network_verified": "1",
        "upstream_suitesparse_pages_verified": "66",
        "payloads_redistributed_in_core_bundle": "0",
        "public_immutable_mirror": "0",
        "independent_reproduction": "0",
    }.items():
        require(data_lock_values.get(key) == value, f"data-lock field changed: {key}")
    print("SMAVE_PAPER_EVIDENCE_CHECK 1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"paper evidence check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
