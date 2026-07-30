if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "paired oracle evidence is missing")
endif()
file(READ "${EVIDENCE}" evidence)
foreach(required
    "SMAVE_PAIRED_SCENARIO_ORACLE 1"
    "contract=hindsight-per-scenario-best-safe-forced-complete-runtime-reference"
    "post_hoc_scenario_reference=1"
    "strict_lower_bound=0"
    "search_cost_excluded=1"
    "deployed_family_calibration_compared=1"
    "feature_selector_compared=1"
    "feature_selector_contract=leave-one-scenario-out-one-nearest-neighbor"
    "feature_selector_query_label_excluded=1"
    "feature_selector_label_generation_cost_excluded=1"
    "feature_selector_lookup_cost_excluded=1"
    "feature_selector_exact_winner_accuracy_is_telemetry=1"
    "feature_selector_practical_equivalence_primary=1"
    "literature_cart_selector_compared=1"
    "literature_cart_selector_contract=leave-one-scenario-out-depth-3-gini-cart"
    "literature_cart_selector_reimplementation=1"
    "literature_cart_selector_public_code_used=0"
    "literature_cart_selector_training_and_inference_cost_excluded=1"
    "structural_cost_selector_compared=1"
    "structural_cost_selector_contract=leave-one-scenario-out-per-expert-log-runtime-ridge"
    "structural_cost_selector_query_label_excluded=1"
    "structural_cost_selector_query_timing_excluded=1"
    "structural_cost_selector_features=aggregate-count-mean-variance-range-rms-l1"
    "structural_cost_selector_fixed_ridge=0.01"
    "structural_cost_selector_training_and_inference_cost_excluded=1"
    "scenarios=64"
    "repetitions=100"
    "samples=6400"
    "online_failures=0"
    "online_gate_mismatches=0"
    "calibrated_failures=0"
    "calibrated_gate_mismatches=0"
    "selector_training_scenarios_per_query=63"
    "selector_failures=0"
    "selector_gate_mismatches=0"
    "cart_failures=0"
    "cart_gate_mismatches=0"
    "structural_failures=0"
    "structural_gate_mismatches=0"
    "oracle_selection_failures=0")
    string(FIND "${evidence}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "paired oracle evidence missing ${required}")
    endif()
endforeach()
foreach(field safe_candidates distinct_oracle_winners paired_median_online_over_oracle
              bootstrap_95_lower bootstrap_95_upper online_within_5_percent_rate
              online_win_rate paired_median_calibrated_over_oracle
              calibrated_bootstrap_95_lower calibrated_bootstrap_95_upper
              calibrated_within_5_percent_rate paired_median_online_over_calibrated
              calibration_gain_bootstrap_95_lower calibration_gain_bootstrap_95_upper
              selector_correct_scenarios selector_accuracy selector_distinct_selected_experts
              paired_median_selector_over_oracle selector_oracle_bootstrap_95_lower
              selector_oracle_bootstrap_95_upper selector_within_5_percent_rate
              paired_median_selector_over_calibrated selector_calibrated_bootstrap_95_lower
              selector_calibrated_bootstrap_95_upper maximum_selector_mixed_qoi_error
              cart_correct_scenarios cart_accuracy cart_distinct_selected_experts
              paired_median_cart_over_oracle cart_oracle_bootstrap_95_lower
              cart_oracle_bootstrap_95_upper cart_within_5_percent_rate
              paired_median_cart_over_calibrated cart_calibrated_bootstrap_95_lower
              cart_calibrated_bootstrap_95_upper maximum_cart_mixed_qoi_error
              structural_correct_scenarios structural_accuracy
              structural_distinct_selected_experts paired_median_structural_over_oracle
              structural_oracle_bootstrap_95_lower structural_oracle_bootstrap_95_upper
              structural_within_5_percent_rate paired_median_structural_over_calibrated
              structural_calibrated_bootstrap_95_lower
              structural_calibrated_bootstrap_95_upper maximum_structural_mixed_qoi_error
              maximum_online_mixed_qoi_error maximum_calibrated_mixed_qoi_error)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${evidence}")
    if(NOT field_match)
        message(FATAL_ERROR "paired oracle evidence lacks ${field}")
    endif()
    set(${field} "${CMAKE_MATCH_1}")
endforeach()
if(safe_candidates LESS 2 OR distinct_oracle_winners LESS 1 OR
   selector_distinct_selected_experts LESS 1 OR
   cart_distinct_selected_experts LESS 1 OR
   structural_distinct_selected_experts LESS 1 OR
   maximum_online_mixed_qoi_error GREATER 1.0 OR
   maximum_calibrated_mixed_qoi_error GREATER 1.0 OR
   maximum_selector_mixed_qoi_error GREATER 1.0 OR
   maximum_cart_mixed_qoi_error GREATER 1.0 OR
   maximum_structural_mixed_qoi_error GREATER 1.0)
    message(FATAL_ERROR "paired oracle safety or candidate coverage failed")
endif()
message(STATUS "paired per-scenario safe oracle comparison passed")
