if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
  message(FATAL_ERROR "joint-route scaling evidence is missing")
endif()
if(NOT DEFINED SCALING OR NOT EXISTS "${SCALING}")
  message(FATAL_ERROR "joint-route scaling table is missing")
endif()
if(NOT DEFINED PREFLIGHT OR NOT EXISTS "${PREFLIGHT}")
  message(FATAL_ERROR "joint-route scaling preflight table is missing")
endif()
if(NOT DEFINED CONTRACT OR NOT EXISTS "${CONTRACT}")
  message(FATAL_ERROR "joint-route scaling contract is missing")
endif()

file(READ "${EVIDENCE}" text)
foreach(required
    "SMAVE_JOINT_ROUTE_SCALING_ROUND51 1"
    "scope=planning-only-no-numerical-solver-execution"
    "measured_profiles=9"
    "measured_uniform_profiles=8"
    "uniform_budgets_per_expert=2"
    "uniform_top_k_cap=6"
    "maximum_joint_states=1000000"
    "largest_measured_experts=16"
    "largest_measured_actions=32"
    "largest_measured_interaction_states=158209"
    "largest_measured_interaction_transitions=1412192"
    "state_estimator_matches_measured_recurrence=1"
    "exact_cap_acceptance_and_preflight_rejection=1"
    "production_shape_experts=3"
    "production_shape_budgets_per_expert=4"
    "production_shape_top_k=3"
    "production_shape_actions=12"
    "production_shape_interaction_states=49"
    "production_shape_interaction_transitions=204"
    "default_cap_largest_even_uniform_experts=20"
    "default_cap_largest_even_uniform_actions=40"
    "default_cap_largest_even_uniform_states=666561"
    "default_cap_first_rejected_even_uniform_experts=22"
    "default_cap_first_rejected_even_uniform_actions=44"
    "default_cap_first_rejected_even_uniform_states=1227425"
    "preflight_rejections_before_state_visit=3"
    "timing_claim=0"
    "immutable_v4_v5_v6_solver_execution=0"
    "cohort_search=0"
    "policy_tuning=0"
    "END")
  string(FIND "${text}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "joint-route scaling evidence missing ${required}")
  endif()
endforeach()

file(STRINGS "${SCALING}" scaling_lines)
list(LENGTH scaling_lines scaling_line_count)
if(NOT scaling_line_count EQUAL 10)
  message(FATAL_ERROR "joint-route scaling table must have 9 profiles")
endif()
file(STRINGS "${PREFLIGHT}" preflight_lines)
list(LENGTH preflight_lines preflight_line_count)
if(NOT preflight_line_count EQUAL 4)
  message(FATAL_ERROR "joint-route preflight table must have 3 profiles")
endif()

file(READ "${CONTRACT}" contract_text)
foreach(required_contract
    "SMAVE_JOINT_ROUTE_SCALING_ROUND51_PREFIRST_RUN_CONTRACT 1"
    "freeze_date=2026-07-29"
    "measured_uniform_expert_counts=2,4,6,8,10,12,14,16"
    "preflight_uniform_expert_counts=18,20,22"
    "immutable_v4_v5_v6_solver_execution=0"
    "cohort_search=0"
    "policy_tuning=0"
    "END")
  string(FIND "${contract_text}" "${required_contract}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "joint-route scaling contract missing ${required_contract}")
  endif()
endforeach()
