foreach(required PHASE1_COUPLED PHASE1_CUBIC NONLINEAR_CASCADE PHASE4_EVALUATION PHASE4_HELDOUT PHASE4_EXTERNAL PAIRED_ORACLE PHASE5_STATISTICS PHASE5_ABLATION PHASE5_EXTERNAL OPERATOR_REPLICATION NONLINEAR_OPERATOR AMG_BACKEND GATE_ARCHITECTURE RISK_ADAPTIVE_GATE COMPONENT_ABLATION)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "academic evidence input is missing: ${required}")
    endif()
endforeach()
if(NOT DEFINED OUTPUT)
    message(FATAL_ERROR "academic evidence OUTPUT is required")
endif()

file(READ "${PHASE4_EVALUATION}" router)
file(READ "${PHASE1_COUPLED}" phase1_coupled)
file(READ "${PHASE1_CUBIC}" phase1_cubic)
file(READ "${PHASE4_HELDOUT}" heldout)
file(READ "${PHASE4_EXTERNAL}" external)
file(READ "${PHASE5_STATISTICS}" statistics)
file(READ "${PHASE5_ABLATION}" ablation)
file(READ "${PHASE5_EXTERNAL}" operator_external)
file(READ "${GATE_ARCHITECTURE}" gate_architecture)
file(READ "${RISK_ADAPTIVE_GATE}" risk_adaptive_gate)
file(READ "${NONLINEAR_CASCADE}/summary.txt" nonlinear_cascade_summary)
file(READ "${OPERATOR_REPLICATION}" operator_replication)
file(READ "${NONLINEAR_OPERATOR}" nonlinear_operator)
file(READ "${AMG_BACKEND}" amg_backend)
file(READ "${PAIRED_ORACLE}" paired_oracle)
file(READ "${COMPONENT_ABLATION}" component_ablation)

function(read_field content field output)
    string(REGEX MATCH "(^|\n)${field}=([^\n]+)" match "${content}")
    if(NOT match)
        message(FATAL_ERROR "academic evidence missing field ${field}")
    endif()
    set(${output} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

foreach(family coupled cubic)
    foreach(field repetitions bootstrap_samples baseline_mean_iterations
                  accelerated_mean_iterations paired_median_speedup
                  paired_speedup_ci95_lower paired_speedup_ci95_upper
                  baseline_failures accelerated_failures gate_mismatches same_accuracy)
        read_field("${phase1_${family}}" "${field}" "phase1_${family}_${field}")
    endforeach()
endforeach()

foreach(family coupled cubic)
    foreach(comparison classic-vs-fixed classic-vs-online fixed-vs-online)
        string(REPLACE "-" "_" comparison_key "${comparison}")
        file(READ "${NONLINEAR_CASCADE}/${family}/${comparison}.txt"
            cascade_${family}_${comparison_key})
        foreach(field repetitions bootstrap_samples baseline_mean_iterations
                      accelerated_mean_iterations paired_median_speedup
                      paired_speedup_ci95_lower paired_speedup_ci95_upper
                      baseline_failures accelerated_failures gate_mismatches same_accuracy)
            read_field("${cascade_${family}_${comparison_key}}" "${field}"
                "cascade_${family}_${comparison_key}_${field}")
        endforeach()
    endforeach()
endforeach()

foreach(marker "FIXED_CASCADE_PLAN_VERIFIED 1"
               "ONLINE_EQUATION_MOE_PLAN_VERIFIED 1"
               "DYNAMIC_REJECTION_CASCADE_VERIFIED 1"
               "PAIRED_ACCURACY_VERIFIED 1")
    string(FIND "${nonlinear_cascade_summary}" "${marker}" marker_found)
    if(marker_found EQUAL -1)
        message(FATAL_ERROR "nonlinear cascade summary missing ${marker}")
    endif()
endforeach()

foreach(field
        fixed_expert calibrated_expert oracle_expert
        paired_median_speedup paired_speedup_ci95_lower paired_speedup_ci95_upper
        paired_win_rate maximum_mixed_qoi_error calibrated_dangerous_misroutes
        heldout_competition_hash)
    read_field("${router}" "${field}" "router_${field}")
endforeach()

read_field("${operator_external}" "entries" operator_external_entries)
if(NOT operator_external_entries GREATER_EQUAL 2)
    message(FATAL_ERROR "fewer than two Operator external baselines")
endif()
foreach(backend "superlu-dgssv-cpu-v1" "accelerate-sparse-qr-cpu-v1")
    string(REGEX MATCH
        "BASELINE \"${backend}\"[^\n]*verified_operator_paired_median_speedup=([0-9.eE+-]+)[^\n]*verified_operator_bootstrap_95_lower=([0-9.eE+-]+)[^\n]*verified_operator_bootstrap_95_upper=([0-9.eE+-]+)[^\n]*maximum_mixed_qoi_error=([0-9.eE+-]+)[^\n]*same_accuracy=1"
        operator_external_match "${operator_external}")
    if(NOT operator_external_match)
        message(FATAL_ERROR "Operator external baseline is invalid: ${backend}")
    endif()
    string(REPLACE "-" "_" backend_key "${backend}")
    set(operator_external_${backend_key}_speedup "${CMAKE_MATCH_1}")
    set(operator_external_${backend_key}_lower "${CMAKE_MATCH_2}")
    set(operator_external_${backend_key}_upper "${CMAKE_MATCH_3}")
endforeach()
foreach(field
        cold_baseline_us cold_operator_us hot_repetitions
        hot_baseline_median_us hot_operator_median_us
        runtime_setup_us operator_setup_us rss_before_bytes
        rss_after_setup_bytes peak_rss_bytes peak_rss_semantics
        energy_available energy_source paired_median_speedup
        bootstrap_95_lower bootstrap_95_upper stable_speedup)
    read_field("${statistics}" "${field}" "operator_${field}")
endforeach()
foreach(field
        raw_candidate_median_us independent_gate_median_us
        fused_original_gate_median_us fused_original_gate_speedup
        fused_original_gate_false_accepts fused_original_gate_false_rejects
        batched_original_gate_median_us batched_original_gate_speedup
        batched_original_gate_false_accepts batched_original_gate_false_rejects
        correction_and_runtime_gate_median_us external_corrector_median_us
        external_corrector_acceptance_rate external_corrector_total_iterations
        external_corrector_maximum_residual external_corrector_gate_mismatches
        full_verified_median_us
        full_verified_acceptance_rate fallbacks failures)
    read_field("${ablation}" "${field}" "ablation_${field}")
endforeach()
foreach(workload operator-linear-100 cubic-coupled-nonlinear)
    foreach(field speedup paired_speedup_ci95_lower decision_mismatches
                  residual_mismatches false_accepts false_rejects)
        string(REPLACE "-" "_" workload_key "${workload}")
        read_field("${gate_architecture}" "${workload}.${field}"
            "gate_${workload_key}_${field}")
    endforeach()
endforeach()
foreach(marker "contract=experimental-immutable-input-token-incremental-certificate"
               "authority_contract=strict-per-request-fp64-original-expression"
               "deployment_promoted=0"
               "library_issued_immutable_input=1"
               "explicit_revocation_recheck=1"
               "concurrent_serialized_recheck=1"
               "full_solve_concurrency_revocation_recheck=1"
               "reconstructed_process_local_recheck=1"
               "bundle_version_change_recheck=1"
               "periodic_full_interval=16"
               "offline_strict_equivalence=1"
               "full_solve_strict_equivalence=1")
    string(FIND "${risk_adaptive_gate}" "${marker}" marker_found)
    if(marker_found EQUAL -1)
        message(FATAL_ERROR "risk-adaptive gate summary missing ${marker}")
    endif()
endforeach()
foreach(workload full-solve-linear full-solve-nonlinear full-solve-scaled-nonlinear)
    string(REPLACE "-" "_" workload_key "${workload}")
    foreach(field repetitions solves_per_repetition total_speedup
                  total_speedup_ci95_lower gate_speedup gate_speedup_ci95_lower
                  result_mismatches periodic_mismatches certificate_reuses)
        read_field("${risk_adaptive_gate}" "${workload}.${field}"
            "risk_${workload_key}_${field}")
    endforeach()
endforeach()
foreach(field full_solve_total_significant_workloads
              full_solve_gate_significant_workloads)
    read_field("${risk_adaptive_gate}" "${field}" "risk_${field}")
endforeach()
foreach(workload operator-linear-100 cubic-coupled-nonlinear)
    string(REPLACE "-" "_" workload_key "${workload}")
    foreach(field requests_per_repetition repetitions strict_median_us
                  adaptive_median_us paired_speedup paired_speedup_ci95_lower
                  high_risk_full_verifications periodic_full_verifications
                  certificate_reuses low_cost_rejects periodic_mismatches
                  false_accepts false_rejects)
        read_field("${risk_adaptive_gate}" "${workload}.${field}"
            "risk_${workload_key}_${field}")
    endforeach()
endforeach()
foreach(field family unknowns structural_nonzeros training_scenarios
              evaluation_scenarios repetitions bootstrap_samples
              cold_baseline_us cold_operator_us hot_repetitions
              hot_baseline_median_us hot_operator_median_us
              runtime_setup_us operator_setup_us rss_before_bytes
              rss_after_setup_bytes peak_rss_bytes peak_rss_semantics
              energy_available energy_source
              paired_median_speedup bootstrap_95_lower bootstrap_95_upper
              stable_speedup break_even_met failures fallbacks
              gate_mismatches same_accuracy)
    read_field("${operator_replication}" "${field}" "replication_${field}")
endforeach()
foreach(field family unknowns training_scenarios evaluation_scenarios repetitions
              corrected_accepts fallbacks failures gate_mismatches same_accuracy
              baseline_median_us operator_median_us online_speedup
              maximum_full_state_error maximum_qoi_error
              raw_candidate_median_us raw_candidate_maximum_full_state_error
              raw_candidate_maximum_qoi_error correction_and_runtime_gate_median_us
              external_corrector_median_us external_corrector_acceptance_rate
              external_corrector_maximum_residual router_online_over_forced_median
              raw_candidate_rejected_by_gate external_corrector_negative_result_retained)
    read_field("${nonlinear_operator}" "${field}" "nonlinear_operator_${field}")
endforeach()
foreach(field largest_unknowns largest_nonzeros largest_levels
              largest_storage_bytes largest_dense_bytes largest_amg_median_us
              largest_ic0_median_us largest_amg_speedup
              largest_amg_mean_iterations largest_ic0_mean_iterations
              largest_amg_residual_inf rss_before_bytes rss_after_bytes
              router_admitted router_irregular_rejected non_spd_rejected
              irregular_topology_rejected verified_linear_service_backend
              verified_linear_service_success verified_linear_service_fallback)
    read_field("${amg_backend}" "${field}" "amg_${field}")
endforeach()
foreach(field scenarios repetitions samples safe_candidates distinct_oracle_winners
              paired_median_online_over_oracle bootstrap_95_lower bootstrap_95_upper
              online_within_5_percent_rate online_win_rate maximum_online_mixed_qoi_error
              paired_median_calibrated_over_oracle calibrated_bootstrap_95_lower
              calibrated_bootstrap_95_upper calibrated_within_5_percent_rate
              paired_median_online_over_calibrated calibration_gain_bootstrap_95_lower
              calibration_gain_bootstrap_95_upper maximum_calibrated_mixed_qoi_error
              selector_training_scenarios_per_query selector_correct_scenarios
              selector_accuracy selector_distinct_selected_experts
              paired_median_selector_over_oracle selector_oracle_bootstrap_95_lower
              selector_oracle_bootstrap_95_upper selector_within_5_percent_rate
              paired_median_selector_over_calibrated selector_calibrated_bootstrap_95_lower
              selector_calibrated_bootstrap_95_upper maximum_selector_mixed_qoi_error
              selector_failures selector_gate_mismatches
              cart_correct_scenarios cart_accuracy cart_distinct_selected_experts
              paired_median_cart_over_oracle cart_oracle_bootstrap_95_lower
              cart_oracle_bootstrap_95_upper cart_within_5_percent_rate
              paired_median_cart_over_calibrated cart_calibrated_bootstrap_95_lower
              cart_calibrated_bootstrap_95_upper maximum_cart_mixed_qoi_error
              cart_failures cart_gate_mismatches
              structural_correct_scenarios structural_accuracy
              structural_distinct_selected_experts paired_median_structural_over_oracle
              structural_oracle_bootstrap_95_lower structural_oracle_bootstrap_95_upper
              structural_within_5_percent_rate paired_median_structural_over_calibrated
              structural_calibrated_bootstrap_95_lower
              structural_calibrated_bootstrap_95_upper maximum_structural_mixed_qoi_error
              structural_failures structural_gate_mismatches
              online_failures online_gate_mismatches calibrated_failures
              calibrated_gate_mismatches oracle_selection_failures)
    read_field("${paired_oracle}" "${field}" "oracle_${field}")
endforeach()
foreach(marker "contract=hindsight-per-scenario-best-safe-forced-complete-runtime-reference"
               "post_hoc_scenario_reference=1" "strict_lower_bound=0"
               "search_cost_excluded=1" "deployed_family_calibration_compared=1"
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
               "structural_cost_selector_training_and_inference_cost_excluded=1")
    string(FIND "${paired_oracle}" "${marker}" oracle_marker)
    if(oracle_marker EQUAL -1)
        message(FATAL_ERROR "paired oracle contract missing ${marker}")
    endif()
endforeach()
foreach(marker
        "SMAVE_SYSTEM_COMPONENT_ABLATION 1"
        "cross_workload_component_matrix=0"
        "single_workload_complete_ablation=1"
        "fallback_dynamic_rejection_verified=1"
        "artifact_tamper_rejected=1"
        "certificate_tamper_rejected=1"
        "bundle_tamper_rejected=1"
        "all_components_present=1")
    string(FIND "${component_ablation}" "${marker}" component_marker)
    if(component_marker EQUAL -1)
        message(FATAL_ERROR "component ablation contract missing ${marker}")
    endif()
endforeach()

string(REGEX MATCH "report_hash=([^\n]+)" heldout_hash_match "${heldout}")
if(NOT heldout_hash_match OR NOT CMAKE_MATCH_1 STREQUAL router_heldout_competition_hash)
    message(FATAL_ERROR "heldout competition hash does not match Router evidence")
endif()

read_field("${external}" "entries" external_entries)
if(NOT external_entries GREATER_EQUAL 2)
    message(FATAL_ERROR "fewer than two paired external baselines")
endif()
foreach(backend "superlu-dgssv-cpu-v1" "accelerate-sparse-qr-cpu-v1")
    string(REGEX MATCH
        "BASELINE \"${backend}\"[^\n]*external_median_us=([0-9.eE+-]+)[^\n]*calibrated_paired_median_speedup=([0-9.eE+-]+)[^\n]*calibrated_bootstrap_95_lower=([0-9.eE+-]+)[^\n]*calibrated_bootstrap_95_upper=([0-9.eE+-]+)[^\n]*maximum_mixed_qoi_error=([0-9.eE+-]+)[^\n]*external_failures=0[^\n]*calibrated_failures=0[^\n]*gate_mismatches=0[^\n]*same_accuracy=1"
        external_match "${external}")
    if(NOT external_match)
        message(FATAL_ERROR "paired external baseline is invalid: ${backend}")
    endif()
    string(REPLACE "-" "_" backend_key "${backend}")
    set(external_${backend_key}_median "${CMAKE_MATCH_1}")
    set(external_${backend_key}_speedup "${CMAKE_MATCH_2}")
    set(external_${backend_key}_lower "${CMAKE_MATCH_3}")
    set(external_${backend_key}_upper "${CMAKE_MATCH_4}")
endforeach()

if(NOT router_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT phase1_coupled_repetitions EQUAL 100 OR
   NOT phase1_cubic_repetitions EQUAL 100 OR
   NOT phase1_coupled_bootstrap_samples EQUAL 10000 OR
   NOT phase1_cubic_bootstrap_samples EQUAL 10000 OR
   NOT phase1_coupled_accelerated_mean_iterations LESS phase1_coupled_baseline_mean_iterations OR
   NOT phase1_cubic_accelerated_mean_iterations LESS phase1_cubic_baseline_mean_iterations OR
   NOT phase1_cubic_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT phase1_coupled_baseline_failures EQUAL 0 OR
   NOT phase1_coupled_accelerated_failures EQUAL 0 OR
   NOT phase1_cubic_baseline_failures EQUAL 0 OR
   NOT phase1_cubic_accelerated_failures EQUAL 0 OR
   NOT phase1_coupled_gate_mismatches EQUAL 0 OR
   NOT phase1_cubic_gate_mismatches EQUAL 0 OR
   NOT phase1_coupled_same_accuracy EQUAL 1 OR
   NOT phase1_cubic_same_accuracy EQUAL 1 OR
   NOT cascade_coupled_classic_vs_fixed_repetitions EQUAL 100 OR
   NOT cascade_coupled_classic_vs_online_repetitions EQUAL 100 OR
   NOT cascade_coupled_fixed_vs_online_repetitions EQUAL 100 OR
   NOT cascade_cubic_classic_vs_fixed_repetitions EQUAL 100 OR
   NOT cascade_cubic_classic_vs_online_repetitions EQUAL 100 OR
   NOT cascade_cubic_fixed_vs_online_repetitions EQUAL 100 OR
   NOT cascade_coupled_classic_vs_fixed_bootstrap_samples EQUAL 10000 OR
   NOT cascade_coupled_classic_vs_online_bootstrap_samples EQUAL 10000 OR
   NOT cascade_coupled_fixed_vs_online_bootstrap_samples EQUAL 10000 OR
   NOT cascade_cubic_classic_vs_fixed_bootstrap_samples EQUAL 10000 OR
   NOT cascade_cubic_classic_vs_online_bootstrap_samples EQUAL 10000 OR
   NOT cascade_cubic_fixed_vs_online_bootstrap_samples EQUAL 10000 OR
   NOT cascade_coupled_classic_vs_fixed_same_accuracy EQUAL 1 OR
   NOT cascade_coupled_classic_vs_online_same_accuracy EQUAL 1 OR
   NOT cascade_coupled_fixed_vs_online_same_accuracy EQUAL 1 OR
   NOT cascade_cubic_classic_vs_fixed_same_accuracy EQUAL 1 OR
   NOT cascade_cubic_classic_vs_online_same_accuracy EQUAL 1 OR
   NOT cascade_cubic_fixed_vs_online_same_accuracy EQUAL 1 OR
   NOT cascade_cubic_classic_vs_fixed_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT cascade_cubic_classic_vs_online_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT operator_bootstrap_95_lower GREATER 1.0 OR
   NOT operator_stable_speedup EQUAL 1 OR
   NOT replication_unknowns EQUAL 144 OR
   NOT replication_structural_nonzeros EQUAL 720 OR
   NOT replication_evaluation_scenarios EQUAL 64 OR
   NOT replication_repetitions EQUAL 100 OR
   NOT replication_bootstrap_samples EQUAL 10000 OR
   NOT replication_stable_speedup EQUAL 0 OR
   NOT replication_failures EQUAL 0 OR
   NOT replication_fallbacks EQUAL 0 OR
   NOT replication_gate_mismatches EQUAL 0 OR
   NOT replication_same_accuracy EQUAL 1 OR
   NOT router_calibrated_dangerous_misroutes EQUAL 0 OR
   NOT oracle_scenarios EQUAL 64 OR
   NOT oracle_repetitions EQUAL 100 OR
   NOT oracle_samples EQUAL 6400 OR
   NOT oracle_safe_candidates GREATER_EQUAL 2 OR
   NOT oracle_distinct_oracle_winners GREATER_EQUAL 1 OR
   NOT oracle_maximum_online_mixed_qoi_error LESS_EQUAL 1.0 OR
   NOT oracle_online_failures EQUAL 0 OR
   NOT oracle_online_gate_mismatches EQUAL 0 OR
   NOT oracle_calibrated_failures EQUAL 0 OR
   NOT oracle_calibrated_gate_mismatches EQUAL 0 OR
   NOT oracle_maximum_calibrated_mixed_qoi_error LESS_EQUAL 1.0 OR
   NOT oracle_calibration_gain_bootstrap_95_lower GREATER 1.0 OR
   NOT oracle_oracle_selection_failures EQUAL 0 OR
   NOT ablation_batched_original_gate_false_accepts EQUAL 0 OR
   NOT ablation_batched_original_gate_false_rejects EQUAL 0 OR
   NOT ablation_fused_original_gate_false_accepts EQUAL 0 OR
   NOT ablation_fused_original_gate_false_rejects EQUAL 0 OR
   NOT gate_operator_linear_100_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT gate_cubic_coupled_nonlinear_paired_speedup_ci95_lower GREATER 1.0 OR
   NOT gate_operator_linear_100_decision_mismatches EQUAL 0 OR
   NOT gate_operator_linear_100_residual_mismatches EQUAL 0 OR
   NOT gate_cubic_coupled_nonlinear_decision_mismatches EQUAL 0 OR
   NOT gate_cubic_coupled_nonlinear_residual_mismatches EQUAL 0 OR
   NOT ablation_fallbacks EQUAL 0 OR NOT ablation_failures EQUAL 0)
    message(FATAL_ERROR "academic evidence gates failed")
endif()

file(WRITE "${OUTPUT}"
    "SMAVE_ACADEMIC_EVIDENCE 1\n"
    "claim_scope=verified-heterogeneous-learned-numerical-solving\n"
    "nonlinear_warm_start_contract=paired-complete-runtime-original-vs-equation-moe\n"
    "nonlinear_warm_start_families=2\n"
    "coupled_baseline_mean_iterations=${phase1_coupled_baseline_mean_iterations}\n"
    "coupled_accelerated_mean_iterations=${phase1_coupled_accelerated_mean_iterations}\n"
    "coupled_paired_median_speedup=${phase1_coupled_paired_median_speedup}\n"
    "coupled_bootstrap_95_lower=${phase1_coupled_paired_speedup_ci95_lower}\n"
    "coupled_bootstrap_95_upper=${phase1_coupled_paired_speedup_ci95_upper}\n"
    "coupled_wall_speedup_supported=0\n"
    "cubic_baseline_mean_iterations=${phase1_cubic_baseline_mean_iterations}\n"
    "cubic_accelerated_mean_iterations=${phase1_cubic_accelerated_mean_iterations}\n"
    "cubic_paired_median_speedup=${phase1_cubic_paired_median_speedup}\n"
    "cubic_bootstrap_95_lower=${phase1_cubic_paired_speedup_ci95_lower}\n"
    "cubic_bootstrap_95_upper=${phase1_cubic_paired_speedup_ci95_upper}\n"
    "cubic_wall_speedup_supported=1\n"
    "nonlinear_warm_start_failures=0\n"
    "nonlinear_warm_start_gate_mismatches=0\n"
    "nonlinear_cascade_contract=classic-vs-fixed-ai-vs-online-equation-moe\n"
    "nonlinear_cascade_families=2\n"
    "nonlinear_cascade_comparisons_per_family=3\n"
    "nonlinear_cascade_fixed_plan_verified=1\n"
    "nonlinear_cascade_online_plan_verified=1\n"
    "nonlinear_cascade_dynamic_rejection_verified=1\n"
    "cascade_coupled_classic_vs_fixed_speedup=${cascade_coupled_classic_vs_fixed_paired_median_speedup}\n"
    "cascade_coupled_classic_vs_fixed_bootstrap_95_lower=${cascade_coupled_classic_vs_fixed_paired_speedup_ci95_lower}\n"
    "cascade_coupled_classic_vs_fixed_bootstrap_95_upper=${cascade_coupled_classic_vs_fixed_paired_speedup_ci95_upper}\n"
    "cascade_coupled_classic_vs_online_speedup=${cascade_coupled_classic_vs_online_paired_median_speedup}\n"
    "cascade_coupled_classic_vs_online_bootstrap_95_lower=${cascade_coupled_classic_vs_online_paired_speedup_ci95_lower}\n"
    "cascade_coupled_classic_vs_online_bootstrap_95_upper=${cascade_coupled_classic_vs_online_paired_speedup_ci95_upper}\n"
    "cascade_coupled_fixed_vs_online_speedup=${cascade_coupled_fixed_vs_online_paired_median_speedup}\n"
    "cascade_coupled_fixed_vs_online_bootstrap_95_lower=${cascade_coupled_fixed_vs_online_paired_speedup_ci95_lower}\n"
    "cascade_coupled_fixed_vs_online_bootstrap_95_upper=${cascade_coupled_fixed_vs_online_paired_speedup_ci95_upper}\n"
    "cascade_cubic_classic_vs_fixed_speedup=${cascade_cubic_classic_vs_fixed_paired_median_speedup}\n"
    "cascade_cubic_classic_vs_fixed_bootstrap_95_lower=${cascade_cubic_classic_vs_fixed_paired_speedup_ci95_lower}\n"
    "cascade_cubic_classic_vs_fixed_bootstrap_95_upper=${cascade_cubic_classic_vs_fixed_paired_speedup_ci95_upper}\n"
    "cascade_cubic_classic_vs_online_speedup=${cascade_cubic_classic_vs_online_paired_median_speedup}\n"
    "cascade_cubic_classic_vs_online_bootstrap_95_lower=${cascade_cubic_classic_vs_online_paired_speedup_ci95_lower}\n"
    "cascade_cubic_classic_vs_online_bootstrap_95_upper=${cascade_cubic_classic_vs_online_paired_speedup_ci95_upper}\n"
    "cascade_cubic_fixed_vs_online_speedup=${cascade_cubic_fixed_vs_online_paired_median_speedup}\n"
    "cascade_cubic_fixed_vs_online_bootstrap_95_lower=${cascade_cubic_fixed_vs_online_paired_speedup_ci95_lower}\n"
    "cascade_cubic_fixed_vs_online_bootstrap_95_upper=${cascade_cubic_fixed_vs_online_paired_speedup_ci95_upper}\n"
    "nonlinear_cascade_failures=0\n"
    "nonlinear_cascade_gate_mismatches=0\n"
    "router_contract=paired-complete-runtime-fixed-vs-calibrated\n"
    "router_fixed_expert=${router_fixed_expert}\n"
    "router_calibrated_expert=${router_calibrated_expert}\n"
    "router_paired_median_speedup=${router_paired_median_speedup}\n"
    "router_bootstrap_95_lower=${router_paired_speedup_ci95_lower}\n"
    "router_bootstrap_95_upper=${router_paired_speedup_ci95_upper}\n"
    "router_paired_win_rate=${router_paired_win_rate}\n"
    "router_maximum_mixed_qoi_error=${router_maximum_mixed_qoi_error}\n"
    "router_dangerous_misroutes=${router_calibrated_dangerous_misroutes}\n"
    "forced_competition_contract=isolated-forced-expert-with-terminal-fallback\n"
    "forced_competition_winner=${router_oracle_expert}\n"
    "forced_competition_not_paired_oracle=1\n"
    "paired_oracle_contract=hindsight-per-scenario-best-safe-forced-complete-runtime-reference\n"
    "paired_oracle_post_hoc_scenario_reference=1\n"
    "paired_oracle_strict_lower_bound=0\n"
    "paired_oracle_search_cost_excluded=1\n"
    "paired_oracle_safe_candidates=${oracle_safe_candidates}\n"
    "paired_oracle_distinct_winners=${oracle_distinct_oracle_winners}\n"
    "online_over_paired_oracle_median=${oracle_paired_median_online_over_oracle}\n"
    "online_over_paired_oracle_bootstrap_95_lower=${oracle_bootstrap_95_lower}\n"
    "online_over_paired_oracle_bootstrap_95_upper=${oracle_bootstrap_95_upper}\n"
    "online_within_5_percent_of_oracle_rate=${oracle_online_within_5_percent_rate}\n"
    "online_oracle_win_rate=${oracle_online_win_rate}\n"
    "calibrated_over_paired_oracle_median=${oracle_paired_median_calibrated_over_oracle}\n"
    "calibrated_over_paired_oracle_bootstrap_95_lower=${oracle_calibrated_bootstrap_95_lower}\n"
    "calibrated_over_paired_oracle_bootstrap_95_upper=${oracle_calibrated_bootstrap_95_upper}\n"
    "calibrated_within_5_percent_of_oracle_rate=${oracle_calibrated_within_5_percent_rate}\n"
    "online_over_calibrated_median=${oracle_paired_median_online_over_calibrated}\n"
    "online_over_calibrated_bootstrap_95_lower=${oracle_calibration_gain_bootstrap_95_lower}\n"
    "online_over_calibrated_bootstrap_95_upper=${oracle_calibration_gain_bootstrap_95_upper}\n"
    "feature_selector_contract=leave-one-scenario-out-one-nearest-neighbor\n"
    "feature_selector_query_label_excluded=1\n"
    "feature_selector_label_generation_cost_excluded=1\n"
    "feature_selector_lookup_cost_excluded=1\n"
    "feature_selector_exact_winner_accuracy_is_telemetry=1\n"
    "feature_selector_practical_equivalence_primary=1\n"
    "feature_selector_training_scenarios_per_query=${oracle_selector_training_scenarios_per_query}\n"
    "feature_selector_correct_scenarios=${oracle_selector_correct_scenarios}\n"
    "feature_selector_accuracy=${oracle_selector_accuracy}\n"
    "feature_selector_distinct_selected_experts=${oracle_selector_distinct_selected_experts}\n"
    "feature_selector_over_oracle_median=${oracle_paired_median_selector_over_oracle}\n"
    "feature_selector_over_oracle_bootstrap_95_lower=${oracle_selector_oracle_bootstrap_95_lower}\n"
    "feature_selector_over_oracle_bootstrap_95_upper=${oracle_selector_oracle_bootstrap_95_upper}\n"
    "feature_selector_within_5_percent_rate=${oracle_selector_within_5_percent_rate}\n"
    "feature_selector_over_calibrated_median=${oracle_paired_median_selector_over_calibrated}\n"
    "feature_selector_over_calibrated_bootstrap_95_lower=${oracle_selector_calibrated_bootstrap_95_lower}\n"
    "feature_selector_over_calibrated_bootstrap_95_upper=${oracle_selector_calibrated_bootstrap_95_upper}\n"
    "feature_selector_maximum_mixed_qoi_error=${oracle_maximum_selector_mixed_qoi_error}\n"
    "feature_selector_failures=${oracle_selector_failures}\n"
    "feature_selector_gate_mismatches=${oracle_selector_gate_mismatches}\n"
    "cart_selector_contract=leave-one-scenario-out-depth-3-gini-cart\n"
    "cart_selector_reimplementation=1\n"
    "cart_selector_public_code_used=0\n"
    "cart_selector_training_and_inference_cost_excluded=1\n"
    "cart_selector_correct_scenarios=${oracle_cart_correct_scenarios}\n"
    "cart_selector_accuracy=${oracle_cart_accuracy}\n"
    "cart_selector_distinct_selected_experts=${oracle_cart_distinct_selected_experts}\n"
    "cart_selector_over_oracle_median=${oracle_paired_median_cart_over_oracle}\n"
    "cart_selector_over_oracle_bootstrap_95_lower=${oracle_cart_oracle_bootstrap_95_lower}\n"
    "cart_selector_over_oracle_bootstrap_95_upper=${oracle_cart_oracle_bootstrap_95_upper}\n"
    "cart_selector_within_5_percent_rate=${oracle_cart_within_5_percent_rate}\n"
    "cart_selector_over_calibrated_median=${oracle_paired_median_cart_over_calibrated}\n"
    "cart_selector_over_calibrated_bootstrap_95_lower=${oracle_cart_calibrated_bootstrap_95_lower}\n"
    "cart_selector_over_calibrated_bootstrap_95_upper=${oracle_cart_calibrated_bootstrap_95_upper}\n"
    "cart_selector_maximum_mixed_qoi_error=${oracle_maximum_cart_mixed_qoi_error}\n"
    "cart_selector_failures=${oracle_cart_failures}\n"
    "cart_selector_gate_mismatches=${oracle_cart_gate_mismatches}\n"
    "structural_cost_selector_contract=leave-one-scenario-out-per-expert-log-runtime-ridge\n"
    "structural_cost_selector_query_label_excluded=1\n"
    "structural_cost_selector_query_timing_excluded=1\n"
    "structural_cost_selector_features=aggregate-count-mean-variance-range-rms-l1\n"
    "structural_cost_selector_fixed_ridge=0.01\n"
    "structural_cost_selector_training_and_inference_cost_excluded=1\n"
    "structural_cost_selector_correct_scenarios=${oracle_structural_correct_scenarios}\n"
    "structural_cost_selector_accuracy=${oracle_structural_accuracy}\n"
    "structural_cost_selector_distinct_selected_experts=${oracle_structural_distinct_selected_experts}\n"
    "structural_cost_selector_over_oracle_median=${oracle_paired_median_structural_over_oracle}\n"
    "structural_cost_selector_over_oracle_bootstrap_95_lower=${oracle_structural_oracle_bootstrap_95_lower}\n"
    "structural_cost_selector_over_oracle_bootstrap_95_upper=${oracle_structural_oracle_bootstrap_95_upper}\n"
    "structural_cost_selector_within_5_percent_rate=${oracle_structural_within_5_percent_rate}\n"
    "structural_cost_selector_over_calibrated_median=${oracle_paired_median_structural_over_calibrated}\n"
    "structural_cost_selector_over_calibrated_bootstrap_95_lower=${oracle_structural_calibrated_bootstrap_95_lower}\n"
    "structural_cost_selector_over_calibrated_bootstrap_95_upper=${oracle_structural_calibrated_bootstrap_95_upper}\n"
    "structural_cost_selector_maximum_mixed_qoi_error=${oracle_maximum_structural_mixed_qoi_error}\n"
    "structural_cost_selector_failures=${oracle_structural_failures}\n"
    "structural_cost_selector_gate_mismatches=${oracle_structural_gate_mismatches}\n"
    "component_ablation_contract=single-workload-component-evidence-matrix\n"
    "component_ablation_cross_workload=0\n"
    "component_ablation_single_workload_complete=1\n"
    "component_ablation_router=1\n"
    "component_ablation_candidate=1\n"
    "component_ablation_correction=1\n"
    "component_ablation_gate=1\n"
    "component_ablation_fallback=1\n"
    "component_ablation_artifact_authority=1\n"
    "component_ablation_artifact_tamper_rejected=1\n"
    "component_ablation_certificate_tamper_rejected=1\n"
    "component_ablation_bundle_tamper_rejected=1\n"
    "paired_oracle_online_failures=${oracle_online_failures}\n"
    "paired_oracle_gate_mismatches=${oracle_online_gate_mismatches}\n"
    "paired_oracle_calibrated_failures=${oracle_calibrated_failures}\n"
    "paired_oracle_calibrated_gate_mismatches=${oracle_calibrated_gate_mismatches}\n"
    "paired_external_baselines=${external_entries}\n"
    "superlu_external_median_us=${external_superlu_dgssv_cpu_v1_median}\n"
    "superlu_calibrated_paired_speedup=${external_superlu_dgssv_cpu_v1_speedup}\n"
    "superlu_calibrated_bootstrap_95_lower=${external_superlu_dgssv_cpu_v1_lower}\n"
    "superlu_calibrated_bootstrap_95_upper=${external_superlu_dgssv_cpu_v1_upper}\n"
    "accelerate_external_median_us=${external_accelerate_sparse_qr_cpu_v1_median}\n"
    "accelerate_calibrated_paired_speedup=${external_accelerate_sparse_qr_cpu_v1_speedup}\n"
    "accelerate_calibrated_bootstrap_95_lower=${external_accelerate_sparse_qr_cpu_v1_lower}\n"
    "accelerate_calibrated_bootstrap_95_upper=${external_accelerate_sparse_qr_cpu_v1_upper}\n"
    "operator_contract=paired-complete-runtime-classic-vs-verified-operator\n"
    "operator_cold_baseline_us=${operator_cold_baseline_us}\n"
    "operator_cold_verified_us=${operator_cold_operator_us}\n"
    "operator_hot_repetitions=${operator_hot_repetitions}\n"
    "operator_hot_baseline_median_us=${operator_hot_baseline_median_us}\n"
    "operator_hot_verified_median_us=${operator_hot_operator_median_us}\n"
    "operator_runtime_setup_us=${operator_runtime_setup_us}\n"
    "operator_expert_setup_us=${operator_operator_setup_us}\n"
    "operator_rss_before_bytes=${operator_rss_before_bytes}\n"
    "operator_rss_after_setup_bytes=${operator_rss_after_setup_bytes}\n"
    "operator_peak_rss_bytes=${operator_peak_rss_bytes}\n"
    "operator_peak_rss_semantics=${operator_peak_rss_semantics}\n"
    "operator_energy_available=${operator_energy_available}\n"
    "operator_energy_source=${operator_energy_source}\n"
    "operator_paired_median_speedup=${operator_paired_median_speedup}\n"
    "operator_bootstrap_95_lower=${operator_bootstrap_95_lower}\n"
    "operator_bootstrap_95_upper=${operator_bootstrap_95_upper}\n"
    "operator_stable_speedup=${operator_stable_speedup}\n"
    "operator_families=3\n"
    "operator_linear_families=2\n"
    "operator_nonlinear_families=1\n"
    "operator_replication_family=${replication_family}\n"
    "operator_replication_unknowns=${replication_unknowns}\n"
    "operator_replication_structural_nonzeros=${replication_structural_nonzeros}\n"
    "operator_replication_cold_baseline_us=${replication_cold_baseline_us}\n"
    "operator_replication_cold_verified_us=${replication_cold_operator_us}\n"
    "operator_replication_hot_repetitions=${replication_hot_repetitions}\n"
    "operator_replication_hot_baseline_median_us=${replication_hot_baseline_median_us}\n"
    "operator_replication_hot_verified_median_us=${replication_hot_operator_median_us}\n"
    "operator_replication_runtime_setup_us=${replication_runtime_setup_us}\n"
    "operator_replication_expert_setup_us=${replication_operator_setup_us}\n"
    "operator_replication_rss_before_bytes=${replication_rss_before_bytes}\n"
    "operator_replication_rss_after_setup_bytes=${replication_rss_after_setup_bytes}\n"
    "operator_replication_peak_rss_bytes=${replication_peak_rss_bytes}\n"
    "operator_replication_peak_rss_semantics=${replication_peak_rss_semantics}\n"
    "operator_replication_energy_available=${replication_energy_available}\n"
    "operator_replication_energy_source=${replication_energy_source}\n"
    "operator_replication_paired_speedup=${replication_paired_median_speedup}\n"
    "operator_replication_bootstrap_95_lower=${replication_bootstrap_95_lower}\n"
    "operator_replication_bootstrap_95_upper=${replication_bootstrap_95_upper}\n"
    "operator_replication_stable_speedup=${replication_stable_speedup}\n"
    "operator_replication_break_even_met=${replication_break_even_met}\n"
    "operator_replication_failures=${replication_failures}\n"
    "operator_replication_fallbacks=${replication_fallbacks}\n"
    "operator_replication_same_accuracy=${replication_same_accuracy}\n"
    "nonlinear_operator_family=${nonlinear_operator_family}\n"
    "nonlinear_operator_unknowns=${nonlinear_operator_unknowns}\n"
    "nonlinear_operator_training_scenarios=${nonlinear_operator_training_scenarios}\n"
    "nonlinear_operator_evaluation_scenarios=${nonlinear_operator_evaluation_scenarios}\n"
    "nonlinear_operator_repetitions=${nonlinear_operator_repetitions}\n"
    "nonlinear_operator_corrected_accepts=${nonlinear_operator_corrected_accepts}\n"
    "nonlinear_operator_fallbacks=${nonlinear_operator_fallbacks}\n"
    "nonlinear_operator_failures=${nonlinear_operator_failures}\n"
    "nonlinear_operator_gate_mismatches=${nonlinear_operator_gate_mismatches}\n"
    "nonlinear_operator_same_accuracy=${nonlinear_operator_same_accuracy}\n"
    "nonlinear_operator_baseline_median_us=${nonlinear_operator_baseline_median_us}\n"
    "nonlinear_operator_verified_median_us=${nonlinear_operator_operator_median_us}\n"
    "nonlinear_operator_online_speedup=${nonlinear_operator_online_speedup}\n"
    "nonlinear_operator_maximum_full_state_error=${nonlinear_operator_maximum_full_state_error}\n"
    "nonlinear_operator_maximum_qoi_error=${nonlinear_operator_maximum_qoi_error}\n"
    "nonlinear_operator_raw_candidate_median_us=${nonlinear_operator_raw_candidate_median_us}\n"
    "nonlinear_operator_raw_candidate_maximum_full_state_error=${nonlinear_operator_raw_candidate_maximum_full_state_error}\n"
    "nonlinear_operator_raw_candidate_maximum_qoi_error=${nonlinear_operator_raw_candidate_maximum_qoi_error}\n"
    "nonlinear_operator_correction_and_gate_median_us=${nonlinear_operator_correction_and_runtime_gate_median_us}\n"
    "nonlinear_operator_external_corrector_median_us=${nonlinear_operator_external_corrector_median_us}\n"
    "nonlinear_operator_external_corrector_acceptance_rate=${nonlinear_operator_external_corrector_acceptance_rate}\n"
    "nonlinear_operator_external_corrector_maximum_residual=${nonlinear_operator_external_corrector_maximum_residual}\n"
    "nonlinear_operator_router_online_over_forced_median=${nonlinear_operator_router_online_over_forced_median}\n"
    "nonlinear_operator_raw_candidate_rejected_by_gate=${nonlinear_operator_raw_candidate_rejected_by_gate}\n"
    "nonlinear_operator_external_corrector_negative_result_retained=${nonlinear_operator_external_corrector_negative_result_retained}\n"
    "amg_backend=pcg-aggregation-amg-cpu-v1\n"
    "amg_largest_unknowns=${amg_largest_unknowns}\n"
    "amg_largest_nonzeros=${amg_largest_nonzeros}\n"
    "amg_largest_levels=${amg_largest_levels}\n"
    "amg_largest_storage_bytes=${amg_largest_storage_bytes}\n"
    "amg_largest_dense_bytes=${amg_largest_dense_bytes}\n"
    "amg_largest_median_us=${amg_largest_amg_median_us}\n"
    "amg_largest_ic0_median_us=${amg_largest_ic0_median_us}\n"
    "amg_largest_speedup=${amg_largest_amg_speedup}\n"
    "amg_largest_mean_iterations=${amg_largest_amg_mean_iterations}\n"
    "amg_largest_ic0_mean_iterations=${amg_largest_ic0_mean_iterations}\n"
    "amg_largest_residual_inf=${amg_largest_amg_residual_inf}\n"
    "amg_rss_before_bytes=${amg_rss_before_bytes}\n"
    "amg_rss_after_bytes=${amg_rss_after_bytes}\n"
    "amg_router_admitted=${amg_router_admitted}\n"
    "amg_router_irregular_rejected=${amg_router_irregular_rejected}\n"
    "amg_non_spd_rejected=${amg_non_spd_rejected}\n"
    "amg_irregular_topology_rejected=${amg_irregular_topology_rejected}\n"
    "amg_verified_linear_service_backend=${amg_verified_linear_service_backend}\n"
    "amg_verified_linear_service_success=${amg_verified_linear_service_success}\n"
    "amg_verified_linear_service_fallback=${amg_verified_linear_service_fallback}\n"
    "operator_paired_external_baselines=${operator_external_entries}\n"
    "operator_vs_superlu_paired_speedup=${operator_external_superlu_dgssv_cpu_v1_speedup}\n"
    "operator_vs_superlu_bootstrap_95_lower=${operator_external_superlu_dgssv_cpu_v1_lower}\n"
    "operator_vs_superlu_bootstrap_95_upper=${operator_external_superlu_dgssv_cpu_v1_upper}\n"
    "operator_vs_accelerate_paired_speedup=${operator_external_accelerate_sparse_qr_cpu_v1_speedup}\n"
    "operator_vs_accelerate_bootstrap_95_lower=${operator_external_accelerate_sparse_qr_cpu_v1_lower}\n"
    "operator_vs_accelerate_bootstrap_95_upper=${operator_external_accelerate_sparse_qr_cpu_v1_upper}\n"
    "raw_candidate_median_us=${ablation_raw_candidate_median_us}\n"
    "independent_gate_median_us=${ablation_independent_gate_median_us}\n"
    "fused_gate_median_us=${ablation_fused_original_gate_median_us}\n"
    "fused_gate_speedup=${ablation_fused_original_gate_speedup}\n"
    "fused_gate_false_accepts=${ablation_fused_original_gate_false_accepts}\n"
    "fused_gate_false_rejects=${ablation_fused_original_gate_false_rejects}\n"
    "fused_gate_operator_linear_speedup=${gate_operator_linear_100_speedup}\n"
    "fused_gate_operator_linear_bootstrap_95_lower=${gate_operator_linear_100_paired_speedup_ci95_lower}\n"
    "fused_gate_nonlinear_speedup=${gate_cubic_coupled_nonlinear_speedup}\n"
    "fused_gate_nonlinear_bootstrap_95_lower=${gate_cubic_coupled_nonlinear_paired_speedup_ci95_lower}\n"
    "fused_gate_strict_contract=per-request-fp64-original-expression\n"
    "fused_gate_promoted=1\n"
    "risk_adaptive_gate_contract=experimental-immutable-input-token-incremental-certificate\n"
    "risk_adaptive_gate_authority=strict-per-request-fp64-original-expression\n"
    "risk_adaptive_gate_promoted=0\n"
    "risk_adaptive_gate_periodic_full_interval=16\n"
    "risk_adaptive_concurrency_revocation_recheck=1\n"
    "risk_adaptive_reconstructed_process_local_recheck=1\n"
    "risk_adaptive_bundle_version_change_recheck=1\n"
    "risk_adaptive_linear_speedup=${risk_operator_linear_100_paired_speedup}\n"
    "risk_adaptive_linear_bootstrap_95_lower=${risk_operator_linear_100_paired_speedup_ci95_lower}\n"
    "risk_adaptive_linear_certificate_reuses=${risk_operator_linear_100_certificate_reuses}\n"
    "risk_adaptive_linear_false_accepts=${risk_operator_linear_100_false_accepts}\n"
    "risk_adaptive_linear_false_rejects=${risk_operator_linear_100_false_rejects}\n"
    "risk_adaptive_nonlinear_speedup=${risk_cubic_coupled_nonlinear_paired_speedup}\n"
    "risk_adaptive_nonlinear_bootstrap_95_lower=${risk_cubic_coupled_nonlinear_paired_speedup_ci95_lower}\n"
    "risk_adaptive_nonlinear_certificate_reuses=${risk_cubic_coupled_nonlinear_certificate_reuses}\n"
    "risk_adaptive_nonlinear_false_accepts=${risk_cubic_coupled_nonlinear_false_accepts}\n"
    "risk_adaptive_nonlinear_false_rejects=${risk_cubic_coupled_nonlinear_false_rejects}\n"
    "risk_adaptive_full_solve_contract=exact-candidate-process-local-experimental\n"
    "risk_adaptive_full_solve_total_significant_workloads=${risk_full_solve_total_significant_workloads}\n"
    "risk_adaptive_full_solve_gate_significant_workloads=${risk_full_solve_gate_significant_workloads}\n"
    "risk_adaptive_full_solve_linear_total_speedup=${risk_full_solve_linear_total_speedup}\n"
    "risk_adaptive_full_solve_linear_total_bootstrap_95_lower=${risk_full_solve_linear_total_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_linear_gate_bootstrap_95_lower=${risk_full_solve_linear_gate_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_linear_result_mismatches=${risk_full_solve_linear_result_mismatches}\n"
    "risk_adaptive_full_solve_nonlinear_total_speedup=${risk_full_solve_nonlinear_total_speedup}\n"
    "risk_adaptive_full_solve_nonlinear_total_bootstrap_95_lower=${risk_full_solve_nonlinear_total_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_nonlinear_gate_bootstrap_95_lower=${risk_full_solve_nonlinear_gate_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_nonlinear_result_mismatches=${risk_full_solve_nonlinear_result_mismatches}\n"
    "risk_adaptive_full_solve_scaled_nonlinear_total_speedup=${risk_full_solve_scaled_nonlinear_total_speedup}\n"
    "risk_adaptive_full_solve_scaled_nonlinear_total_bootstrap_95_lower=${risk_full_solve_scaled_nonlinear_total_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_scaled_nonlinear_gate_bootstrap_95_lower=${risk_full_solve_scaled_nonlinear_gate_speedup_ci95_lower}\n"
    "risk_adaptive_full_solve_scaled_nonlinear_result_mismatches=${risk_full_solve_scaled_nonlinear_result_mismatches}\n"
    "correction_and_runtime_gate_median_us=${ablation_correction_and_runtime_gate_median_us}\n"
    "external_corrector_contract=weighted-diagonal-residual-jacobi-plus-strict-gate\n"
    "external_corrector_reimplementation=1\n"
    "external_corrector_public_code_used=0\n"
    "external_corrector_median_us=${ablation_external_corrector_median_us}\n"
    "external_corrector_acceptance_rate=${ablation_external_corrector_acceptance_rate}\n"
    "external_corrector_total_iterations=${ablation_external_corrector_total_iterations}\n"
    "external_corrector_maximum_residual=${ablation_external_corrector_maximum_residual}\n"
    "external_corrector_gate_mismatches=${ablation_external_corrector_gate_mismatches}\n"
    "full_verified_median_us=${ablation_full_verified_median_us}\n"
    "full_verified_acceptance_rate=${ablation_full_verified_acceptance_rate}\n"
    "experimental_batch_gate_speedup=${ablation_batched_original_gate_speedup}\n"
    "experimental_batch_gate_false_accepts=${ablation_batched_original_gate_false_accepts}\n"
    "experimental_batch_gate_false_rejects=${ablation_batched_original_gate_false_rejects}\n"
    "experimental_batch_gate_promoted=0\n"
    "limitations=coupled-wall-speedup-inconclusive;online-equation-moe-overhead-family-dependent;paired-oracle-is-post-hoc-reference-not-strict-lower-bound;paired-oracle-excludes-search-cost;uncalibrated-online-router-has-measured-reference-gap;feature-selector-label-and-lookup-cost-excluded;cart-selector-training-and-inference-cost-excluded;structural-cost-selector-is-local-log-runtime-model-with-fixed-ridge;structural-cost-selector-training-and-inference-cost-excluded;feature-selector-exact-winner-label-load-sensitive;selector-and-corrector-literature-baselines-are-local-reimplementations-not-public-code;single-workload-component-ablation-linear-operator-only;nonlinear-operator-is-generated-smooth-single-scc-not-event-or-customer-workload;second-operator-family-safe-without-stable-speedup;portable-process-energy-counter-unavailable;risk-adaptive-exact-candidate-experimental-not-runtime-promoted;small-nonlinear-full-solve-total-speedup-load-sensitive\n"
    "END\n")
