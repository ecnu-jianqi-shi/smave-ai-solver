if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
  message(FATAL_ERROR "frozen interaction prevalence evidence is missing")
endif()
if(NOT DEFINED REQUEST_COVERAGE OR NOT EXISTS "${REQUEST_COVERAGE}")
  message(FATAL_ERROR "frozen interaction request coverage is missing")
endif()
if(NOT DEFINED PAIR_SUPPORT OR NOT EXISTS "${PAIR_SUPPORT}")
  message(FATAL_ERROR "frozen interaction pair support is missing")
endif()
if(NOT DEFINED PAIR_CLASSES OR NOT EXISTS "${PAIR_CLASSES}")
  message(FATAL_ERROR "frozen interaction pair classes are missing")
endif()

file(READ "${EVIDENCE}" text)
foreach(required
    "SMAVE_FROZEN_INTERACTION_PREVALENCE_ROUND52 1"
    "analysis_mode=posthoc-frozen-observation-diagnostic"
    "stable_failure=all-repetitions-failed-with-identical-status"
    "pair_definition=ordered-distinct-expert-actions"
    "contract_sha256=ab4b40c5a2664549490b4995c1e7f527a12239fa781d3016ab8c6435ed156d3f"
    "v5.action_observations_sha256=38fc83ad2e1085b6141af71ec692de8e549dd59fe4148544e3f0f1a342662d90"
    "v5.frozen_evidence_sha256=65eb1f2cde08c3650ae5cb833fdb32f0bc8993a9f5e5edb55bbeec192a8c7806"
    "v5.action_rows=4920"
    "v5.request_actions=984"
    "v5.training_requests=48"
    "v5.calibration_requests=32"
    "v5.heldout_requests=24"
    "v5.training_requests_with_two_expert_failures=38"
    "v5.calibration_requests_with_two_expert_failures=24"
    "v5.heldout_requests_with_two_expert_failures=8"
    "v5.training_ordered_failure_pairs=64"
    "v5.training_pairs_two_matrix_support=40"
    "v5.calibration_ordered_failure_pairs=64"
    "v5.calibration_pairs_two_matrix_support=32"
    "v5.development_supported_pairs=32"
    "v5.heldout_observed_pairs=8"
    "v5.development_heldout_overlap=0"
    "v5.plan_gated_candidate_transitions=0"
    "v5.conditional_timing_rows=0"
    "v5.calibrated_transitions=0"
    "v6.action_observations_sha256=16a229d396f61338c523b4275bfb5b10708faa956b9c97d6f641c35ed0dd2270"
    "v6.frozen_evidence_sha256=c92e1b9f80327d3125d11b400ec6edcc8f90e0c3515d86cadd2b7c6fcc0d1d6f"
    "v6.action_rows=4920"
    "v6.request_actions=984"
    "v6.training_requests=48"
    "v6.calibration_requests=32"
    "v6.heldout_requests=24"
    "v6.training_requests_with_two_expert_failures=38"
    "v6.calibration_requests_with_two_expert_failures=24"
    "v6.heldout_requests_with_two_expert_failures=16"
    "v6.training_ordered_failure_pairs=64"
    "v6.training_pairs_two_matrix_support=40"
    "v6.calibration_ordered_failure_pairs=64"
    "v6.calibration_pairs_two_matrix_support=32"
    "v6.development_supported_pairs=32"
    "v6.heldout_observed_pairs=56"
    "v6.development_heldout_overlap=0"
    "v6.plan_gated_candidate_transitions=0"
    "v6.conditional_timing_rows=0"
    "v6.calibrated_transitions=0"
    "development_supported_pairs=32"
    "development_supported_expert_pair_classes=1"
    "development_supported_expert_pair=gmres-ilu0-cpu-v1<->gmres-ilut-cpu-v1"
    "v5_v6_development_pair_set_equal=1"
    "v5_v6_heldout_pair_intersection=8"
    "v5_v6_heldout_pair_union=56"
    "v5_v6_heldout_pair_jaccard=0.14285714285714285"
    "development_to_both_heldout_overlap=0"
    "isolated_failure_support_is_not_conditional_calibration=1"
    "heldout_diagnostic_only=1"
    "conditional_timing_inference=0"
    "policy_tuning=0"
    "cohort_search=0"
    "solver_execution=0"
    "END")
  string(FIND "${text}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "frozen interaction prevalence evidence missing ${required}")
  endif()
endforeach()

file(STRINGS "${REQUEST_COVERAGE}" request_lines)
list(LENGTH request_lines request_line_count)
if(NOT request_line_count EQUAL 209)
  message(FATAL_ERROR "frozen interaction request table must contain 208 rows")
endif()
file(STRINGS "${PAIR_SUPPORT}" pair_lines)
list(LENGTH pair_lines pair_line_count)
if(NOT pair_line_count EQUAL 161)
  message(FATAL_ERROR "frozen interaction pair table must contain 160 rows")
endif()
file(STRINGS "${PAIR_CLASSES}" class_lines)
list(LENGTH class_lines class_line_count)
if(NOT class_line_count EQUAL 8)
  message(FATAL_ERROR "frozen interaction pair-class table must contain 7 rows")
endif()
