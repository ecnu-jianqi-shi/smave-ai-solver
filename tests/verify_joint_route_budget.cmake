if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_JOINT_ROUTE_BUDGET 1"
        "contract=exact-dp-joint-expert-budget-complete-cost"
        "experts=2"
        "actions=3"
        "top_k=2"
        "selected_steps=2"
        "step0.expert=joint-budget-probe-b-v1"
        "step0.budget=1"
        "step1.expert=joint-budget-probe-a-v1"
        "step1.budget=2"
        "selected_expected_cost_us=4.5"
        "exhaustive_oracle_cost_us=4.5"
        "oracle_gap_us=0"
        "best_single_action_cost_us=5"
        "joint_vs_best_single_ratio=0.90000000000000002"
        "same_expert_budget_exclusivity=1"
        "property_sweep_cases=256"
        "property_sweep_actions_per_case=6"
        "property_sweep_exhaustive_oracle_match=1"
        "property_sweep_maximum_oracle_gap_us=0"
        "property_sweep_expert_exclusivity=1"
        "property_sweep_ordering_invariant=1"
        "interaction_property_sweep_cases=256"
        "interaction_property_sweep_actions_per_case=6"
        "interaction_property_sweep_transitions_per_case=24"
        "interaction_property_sweep_exhaustive_oracle_match=1"
        "interaction_property_sweep_maximum_oracle_gap_us=0"
        "interaction_property_sweep_expert_exclusivity=1"
        "interaction_property_sweep_changed_from_independent=1"
        "interaction_property_sweep_changed_cases=126"
        "interaction_hardness_reduction=directed-hamiltonian-path"
        "interaction_hardness_vertices=4"
        "interaction_hardness_graphs=4096"
        "interaction_hardness_threshold_us=2.1875"
        "interaction_hardness_reduction_match=1"
        "interaction_hardness_full_length=1"
        "runtime.first_action_rejected=1"
        "runtime.second_action_accepted=1"
        "runtime.second_action_iterations=2"
        "runtime.original_equation_gate_accept=1"
        "runtime.terminal_fallback_used=0"
        "END")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "joint Router evidence missing marker: ${marker}")
    endif()
endforeach()
