if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "SuiteSparse request-conditioned evidence is missing")
endif()

file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE 1"
        "contract=group-disjoint-final-heldout-v6-production-sparse-expert-budget-routing"
        "snapshot_date=2026-07-27"
        "matrix_id_disjoint=1"
        "collection_group_disjoint=1"
        "all_prior_development_heldout_excluded=1"
        "all_pre_v6_locked_groups_excluded=1"
        "final_heldout_frozen_before_action_timing=1"
        "training_matrix_count=6"
        "calibration_matrix_count=4"
        "heldout_matrix_count=3"
        "training_requests=48"
        "calibration_requests=32"
        "heldout_requests=24"
        "action_repetitions=5"
        "model_action_count=20"
        "candidate_model_action_count=19"
        "request_conditioned_terminal_cost_predictor=1"
        "calibration_offsets_applied=1"
        "cost_ridge_regularization=100"
        "maximum_cost_calibration_multiplier=4"
        "maximum_pass_logit_calibration_offset=1"
        "family_anchor_calibrated_abstention=1"
        "control_aware_family_anchor_gate=1"
        "control_aware_request_comparison=conservative-complete-cost-upper"
        "matrix_row_limit=10000"
        "built_in_direct_row_limit=512"
        "all_frozen_compatible_actions_executed=1"
        "dp_exhaustive_mismatches=0"
        "production_failures=0"
        "production_gate_mismatches=0"
        "production_plan_order_mismatches=0"
        "terminal_only_successes=24"
        "negative_results_retained=1"
        "original_equation_gate_recomputed=1"
        "terminal_numerical_fallback_preserved=1"
        "END")
    string(FIND "${report}" "${marker}" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR
            "SuiteSparse request-conditioned evidence missing marker: ${marker}")
    endif()
endforeach()

get_filename_component(EVIDENCE_DIRECTORY "${EVIDENCE}" DIRECTORY)
foreach(artifact
        action-observations.tsv
        request-summary.tsv
        matrix-split.txt
        request-conditioned-model.txt
        size-only-model.txt
        rhs-only-model.txt
        tolerance-only-model.txt
        production-attempt-traces.tsv
        terminal-attempt-traces.tsv
        family-anchor-calibration.tsv
        family-adaptation-calibration.tsv
        heldout-prediction-diagnostics.tsv
        conditional-cost-observations.tsv
        action-interactions.tsv)
    if(NOT EXISTS "${EVIDENCE_DIRECTORY}/${artifact}")
        message(FATAL_ERROR
            "SuiteSparse request-conditioned artifact is missing: ${artifact}")
    endif()
endforeach()

file(STRINGS "${EVIDENCE_DIRECTORY}/action-observations.tsv" observation_lines)
list(LENGTH observation_lines observation_line_count)
string(REGEX MATCH "raw_action_observations=([0-9]+)" _ "${report}")
set(raw_action_observations "${CMAKE_MATCH_1}")
if("${raw_action_observations}" STREQUAL "")
    message(FATAL_ERROR "SuiteSparse raw action observation count is missing")
endif()
math(EXPR expected_observation_lines "${raw_action_observations} + 1")
if(NOT observation_line_count EQUAL expected_observation_lines)
    message(FATAL_ERROR
        "SuiteSparse action observation table count does not match evidence")
endif()

file(STRINGS "${EVIDENCE_DIRECTORY}/request-summary.tsv" request_lines)
list(LENGTH request_lines request_line_count)
if(NOT request_line_count EQUAL 105)
    message(FATAL_ERROR "SuiteSparse request summary must have 105 lines")
endif()

file(STRINGS "${EVIDENCE_DIRECTORY}/matrix-split.txt" split_lines)
list(LENGTH split_lines split_line_count)
if(NOT split_line_count EQUAL 15)
    message(FATAL_ERROR "SuiteSparse matrix split must have 15 lines")
endif()

file(READ "${EVIDENCE_DIRECTORY}/request-conditioned-model.txt" model_report)
foreach(model_marker
        "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 2"
        "FEATURES 15"
        "ACTION_COUNT 20"
        "END")
    string(FIND "${model_report}" "${model_marker}" model_position)
    if(model_position EQUAL -1)
        message(FATAL_ERROR
            "SuiteSparse request-conditioned model missing: ${model_marker}")
    endif()
endforeach()
