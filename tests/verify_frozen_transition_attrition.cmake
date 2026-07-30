foreach(required_path
    EVIDENCE TRANSITIONS CANDIDATES V5_PLANS V6_PLANS CONTRACT CONTRACT_COPY
    V5_ACTION_OBSERVATIONS V5_MODEL V6_ACTION_OBSERVATIONS V6_MODEL)
  if(NOT DEFINED ${required_path} OR NOT EXISTS "${${required_path}}")
    message(FATAL_ERROR "frozen transition attrition ${required_path} is missing")
  endif()
endforeach()

file(SHA256 "${V5_ACTION_OBSERVATIONS}" v5_action_sha256)
file(SHA256 "${V5_MODEL}" v5_model_sha256)
file(SHA256 "${V6_ACTION_OBSERVATIONS}" v6_action_sha256)
file(SHA256 "${V6_MODEL}" v6_model_sha256)
if(NOT v5_action_sha256 STREQUAL
    "38fc83ad2e1085b6141af71ec692de8e549dd59fe4148544e3f0f1a342662d90")
  message(FATAL_ERROR "frozen v5 action observations changed")
endif()
if(NOT v5_model_sha256 STREQUAL
    "247169f853c1b0f759bce171315aaae58d875635f90b26ce098276980b8962c6")
  message(FATAL_ERROR "frozen v5 model changed")
endif()
if(NOT v6_action_sha256 STREQUAL
    "16a229d396f61338c523b4275bfb5b10708faa956b9c97d6f641c35ed0dd2270")
  message(FATAL_ERROR "frozen v6 action observations changed")
endif()
if(NOT v6_model_sha256 STREQUAL
    "2292577a61273288902fb42416ebc1f3627839a083379d662b48d9bc3856de24")
  message(FATAL_ERROR "frozen v6 model changed")
endif()

file(SHA256 "${CONTRACT}" contract_sha256)
file(SHA256 "${CONTRACT_COPY}" contract_copy_sha256)
if(NOT contract_sha256 STREQUAL contract_copy_sha256)
  message(FATAL_ERROR "frozen transition attrition contract copy changed")
endif()
file(READ "${CONTRACT}" contract_text)
foreach(required
    "SMAVE_FROZEN_TRANSITION_ATTRITION_ROUND53_ANALYSIS_CONTRACT 1"
    "analysis_date=2026-07-29"
    "analysis_mode=posthoc-frozen-observation-zero-execution-diagnostic"
    "candidate_rule=adjacent-actions-in-control-aware-training-plan-with-first-action-failed"
    "heldout_role=excluded"
    "heldout_excluded_from_all_attrition_counts=1"
    "conditional_timing_inference=forbidden"
    "counterfactual_gain_inference=forbidden"
    "policy_tuning=0"
    "cohort_search=0"
    "solver_execution=0"
    "END")
  string(FIND "${contract_text}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "frozen transition attrition contract missing ${required}")
  endif()
endforeach()

file(READ "${EVIDENCE}" evidence_text)
foreach(required
    "SMAVE_FROZEN_TRANSITION_ATTRITION_ROUND53 1"
    "analysis_mode=posthoc-frozen-observation-zero-execution-diagnostic"
    "candidate_rule=adjacent-actions-in-control-aware-training-plan-with-first-action-failed"
    "heldout_excluded_from_all_attrition_counts=1"
    "exact_control_aware_route_crosscheck=1"
    "v5.model_byte_identical=1"
    "v5.training_requests=48"
    "v5.development_supported_pairs=32"
    "v5.unguarded_multistep_requests=48"
    "v5.final_terminal_abstention_requests=0"
    "v5.final_single_action_requests=48"
    "v5.final_multistep_requests=0"
    "v5.control_aware_changed_requests=48"
    "v5.unguarded_candidate_transition_count=5"
    "v5.unguarded_candidate_transition_requests=16"
    "v5.unguarded_candidate_transitions_development_supported=0"
    "v5.final_candidate_transition_count=0"
    "v5.development_pairs_with_unguarded_candidate=0"
    "v5.development_pairs_with_final_candidate=0"
    "v5.development_pairs_removed_by_anchor=0"
    "v5.development_pairs_never_unguarded_candidate=32"
    "v5.development_pairs_eliminated_at_unguarded_top3_selection=32"
    "v5.all_unguarded_candidates_removed_by_control_aware_plan=5"
    "v6.model_byte_identical=1"
    "v6.training_requests=48"
    "v6.development_supported_pairs=32"
    "v6.unguarded_multistep_requests=48"
    "v6.final_terminal_abstention_requests=16"
    "v6.final_single_action_requests=32"
    "v6.final_multistep_requests=0"
    "v6.control_aware_changed_requests=48"
    "v6.unguarded_candidate_transition_count=5"
    "v6.unguarded_candidate_transition_requests=17"
    "v6.unguarded_candidate_transitions_development_supported=0"
    "v6.final_candidate_transition_count=0"
    "v6.development_pairs_with_unguarded_candidate=0"
    "v6.development_pairs_with_final_candidate=0"
    "v6.development_pairs_removed_by_anchor=0"
    "v6.development_pairs_never_unguarded_candidate=32"
    "v6.development_pairs_eliminated_at_unguarded_top3_selection=32"
    "v6.all_unguarded_candidates_removed_by_control_aware_plan=5"
    "conditional_timing_inference=0"
    "counterfactual_gain_inference=0"
    "policy_tuning=0"
    "cohort_search=0"
    "solver_execution=0"
    "candidate_transitions=candidate-transitions.tsv"
    "END")
  string(FIND "${evidence_text}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "frozen transition attrition evidence missing ${required}")
  endif()
endforeach()

file(STRINGS "${TRANSITIONS}" transition_lines)
list(LENGTH transition_lines transition_line_count)
if(NOT transition_line_count EQUAL 65)
  message(FATAL_ERROR "transition attrition table must contain 64 data rows")
endif()
list(POP_FRONT transition_lines transition_header)
set(v5_transition_count 0)
set(v6_transition_count 0)
foreach(line IN LISTS transition_lines)
  string(REPLACE "\t" ";" fields "${line}")
  list(LENGTH fields field_count)
  if(NOT field_count EQUAL 16)
    message(FATAL_ERROR "transition attrition row must contain 16 columns")
  endif()
  list(GET fields 0 version)
  list(GET fields 2 isolated_training)
  list(GET fields 3 isolated_calibration)
  list(GET fields 4 modeled_requests)
  list(GET fields 5 unguarded_both)
  list(GET fields 6 unguarded_ordered)
  list(GET fields 7 unguarded_adjacent)
  list(GET fields 8 unguarded_candidate)
  list(GET fields 9 unguarded_candidate_matrices)
  list(GET fields 10 final_both)
  list(GET fields 11 final_ordered)
  list(GET fields 12 final_adjacent)
  list(GET fields 13 final_candidate)
  list(GET fields 14 final_candidate_matrices)
  list(GET fields 15 class)
  if(version STREQUAL "v5")
    math(EXPR v5_transition_count "${v5_transition_count} + 1")
  elseif(version STREQUAL "v6")
    math(EXPR v6_transition_count "${v6_transition_count} + 1")
  else()
    message(FATAL_ERROR "unknown transition attrition version ${version}")
  endif()
  if(NOT isolated_training STREQUAL "3" OR
     NOT isolated_calibration STREQUAL "2" OR
     NOT modeled_requests STREQUAL "16" OR
     NOT unguarded_both STREQUAL "0" OR
     NOT unguarded_ordered STREQUAL "0" OR
     NOT unguarded_adjacent STREQUAL "0" OR
     NOT unguarded_candidate STREQUAL "0" OR
     NOT unguarded_candidate_matrices STREQUAL "0" OR
     NOT final_both STREQUAL "0" OR
     NOT final_ordered STREQUAL "0" OR
     NOT final_adjacent STREQUAL "0" OR
     NOT final_candidate STREQUAL "0" OR
     NOT final_candidate_matrices STREQUAL "0" OR
     NOT class STREQUAL "eliminated-at-unguarded-top3-selection")
    message(FATAL_ERROR "unexpected transition attrition row: ${line}")
  endif()
endforeach()
if(NOT v5_transition_count EQUAL 32 OR NOT v6_transition_count EQUAL 32)
  message(FATAL_ERROR "each frozen version must contain 32 supported transitions")
endif()

file(STRINGS "${CANDIDATES}" candidate_lines)
list(LENGTH candidate_lines candidate_line_count)
if(NOT candidate_line_count EQUAL 11)
  message(FATAL_ERROR "candidate transition table must contain 10 data rows")
endif()
list(POP_FRONT candidate_lines candidate_header)
set(v5_candidate_count 0)
set(v6_candidate_count 0)
foreach(line IN LISTS candidate_lines)
  string(REPLACE "\t" ";" fields "${line}")
  list(LENGTH fields field_count)
  if(NOT field_count EQUAL 8)
    message(FATAL_ERROR "candidate transition row must contain 8 columns")
  endif()
  list(GET fields 0 version)
  list(GET fields 1 stage)
  list(GET fields 7 development_supported)
  if(NOT stage STREQUAL "unguarded" OR NOT development_supported STREQUAL "0")
    message(FATAL_ERROR "candidate transition must be unguarded and unsupported")
  endif()
  if(version STREQUAL "v5")
    math(EXPR v5_candidate_count "${v5_candidate_count} + 1")
  elseif(version STREQUAL "v6")
    math(EXPR v6_candidate_count "${v6_candidate_count} + 1")
  else()
    message(FATAL_ERROR "unknown candidate transition version ${version}")
  endif()
endforeach()
if(NOT v5_candidate_count EQUAL 5 OR NOT v6_candidate_count EQUAL 5)
  message(FATAL_ERROR "each frozen version must contain five unguarded candidates")
endif()

foreach(plan_path V5_PLANS V6_PLANS)
  file(STRINGS "${${plan_path}}" plan_lines)
  list(LENGTH plan_lines plan_line_count)
  if(NOT plan_line_count EQUAL 49)
    message(FATAL_ERROR "${plan_path} must contain 48 training requests")
  endif()
  list(POP_FRONT plan_lines plan_header)
  set(terminal_abstentions 0)
  set(single_action_plans 0)
  foreach(line IN LISTS plan_lines)
    string(REPLACE "\t" ";" fields "${line}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 9)
      message(FATAL_ERROR "request plan row must contain 9 columns")
    endif()
    list(GET fields 1 split)
    list(GET fields 6 unguarded_plan)
    list(GET fields 7 final_plan)
    list(GET fields 8 changed)
    string(FIND "${unguarded_plan}" "," unguarded_comma)
    string(FIND "${final_plan}" "," final_comma)
    if(final_plan STREQUAL "")
      math(EXPR terminal_abstentions "${terminal_abstentions} + 1")
    else()
      math(EXPR single_action_plans "${single_action_plans} + 1")
    endif()
    if(NOT split STREQUAL "training" OR
       unguarded_comma EQUAL -1 OR
       NOT final_comma EQUAL -1 OR
       NOT changed STREQUAL "1")
      message(FATAL_ERROR "unexpected request plan row: ${line}")
    endif()
  endforeach()
  if(plan_path STREQUAL "V5_PLANS")
    if(NOT terminal_abstentions EQUAL 0 OR NOT single_action_plans EQUAL 48)
      message(FATAL_ERROR "v5 final-plan distribution changed")
    endif()
  elseif(NOT terminal_abstentions EQUAL 16 OR NOT single_action_plans EQUAL 32)
    message(FATAL_ERROR "v6 final-plan distribution changed")
  endif()
endforeach()
