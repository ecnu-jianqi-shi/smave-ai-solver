if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE 1"
        "contract=production-trace-trained-request-conditioned-expert-budget-routing"
        "family_count=3"
        "expert_count=3"
        "budgets_per_expert=4"
        "action_count=12"
        "top_k=3"
        "training_requests=192"
        "calibration_requests=96"
        "heldout_requests=192"
        "dp_exhaustive_mismatches=0"
        "production_successes=192"
        "production_failures=0"
        "production_gate_mismatches=0"
        "original_equation_gate_preserved=1"
        "terminal_fallback_preserved=1"
        "heldout_not_used_for_training_or_calibration=1"
        "realized_oracle_exhaustive=1"
        "END")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "request-conditioned evidence missing marker: ${marker}")
    endif()
endforeach()
get_filename_component(EVIDENCE_DIRECTORY "${EVIDENCE}" DIRECTORY)
set(observations "${EVIDENCE_DIRECTORY}/action-observations.tsv")
set(model "${EVIDENCE_DIRECTORY}/request-conditioned-model.txt")
if(NOT EXISTS "${observations}" OR NOT EXISTS "${model}")
    message(FATAL_ERROR "request-conditioned evidence artifacts are missing")
endif()
file(STRINGS "${observations}" observation_lines)
list(LENGTH observation_lines observation_line_count)
if(NOT observation_line_count EQUAL 5761)
    message(FATAL_ERROR "request-conditioned observation table must have 5761 lines")
endif()
file(READ "${model}" model_report)
foreach(model_marker
        "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 1"
        "feature.0.name=context:root"
        "feature.3.name=context:family_code"
        "END")
    string(FIND "${model_report}" "${model_marker}" model_position)
    if(model_position EQUAL -1)
        message(FATAL_ERROR "request-conditioned model missing: ${model_marker}")
    endif()
endforeach()
string(REGEX MATCHALL "action=[^\n]+" model_actions "${model_report}")
list(LENGTH model_actions model_action_count)
if(NOT model_action_count EQUAL 12)
    message(FATAL_ERROR "request-conditioned model must contain 12 actions")
endif()
string(REGEX MATCH "conditioned_heldout_regret=([0-9.eE+-]+)" _ "${report}")
set(conditioned_regret "${CMAKE_MATCH_1}")
string(REGEX MATCH "static_profile_heldout_regret=([0-9.eE+-]+)" _ "${report}")
set(static_regret "${CMAKE_MATCH_1}")
string(REGEX MATCH "fixed_action_heldout_regret=([0-9.eE+-]+)" _ "${report}")
set(fixed_regret "${CMAKE_MATCH_1}")
if("${conditioned_regret}" STREQUAL "" OR
   "${static_regret}" STREQUAL "" OR
   "${fixed_regret}" STREQUAL "" OR
   conditioned_regret GREATER_EQUAL static_regret OR
   conditioned_regret GREATER_EQUAL fixed_regret OR
   conditioned_regret GREATER 1.35)
    message(FATAL_ERROR "request-conditioned regret contract failed")
endif()
string(REGEX MATCH "pass_prediction_brier_score=([0-9.eE+-]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 GREATER 0.16)
    message(FATAL_ERROR "request-conditioned Brier score exceeded 0.16")
endif()
string(REGEX MATCH "pass_prediction_ece=([0-9.eE+-]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 GREATER 0.16)
    message(FATAL_ERROR "request-conditioned ECE exceeded 0.16")
endif()
string(REGEX MATCH "cost_prediction_median_relative_error=([0-9.eE+-]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 GREATER 0.35)
    message(FATAL_ERROR "request-conditioned median cost error exceeded 0.35")
endif()
string(REGEX MATCH "feature_changed_plan_fraction=([0-9.eE+-]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 LESS 0.25)
    message(FATAL_ERROR "request-conditioned plans did not change across requests")
endif()
