foreach(required IR ARTIFACT CERTIFICATE BUNDLE VALIDATION PERFORMANCE ABLATION TRACES)
  if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
    message(FATAL_ERROR "missing nonlinear Operator evidence ${required}")
  endif()
endforeach()
if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "nonlinear Operator evidence OUTPUT is required")
endif()
file(READ "${IR}" ir)
foreach(marker "MODEL \"NonlinearOperatorGrid4\"" "UNKNOWNS 16" " 0 1 0 0 \"continuous\"")
  string(FIND "${ir}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "nonlinear Operator IR missing ${marker}")
  endif()
endforeach()
file(READ "${VALIDATION}" validation)
foreach(marker "scenarios=64" "successful_scenarios=64" "admitted_invocations=64" "full_fallbacks=0" "original_solver_failures=0" "erroneous_accepts=0" "confidence_target_met=1")
  string(FIND "${validation}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "nonlinear Operator validation missing ${marker}")
  endif()
endforeach()
file(READ "${PERFORMANCE}" performance)
foreach(marker "SMAVE_OPERATOR_BENCHMARK 3" "requests=64" "repetitions=100"
        "accepted=6400" "fallbacks=0" "failures=0" "same_accuracy=1"
        "online_speedup_semantics=paired-median-of-per-repetition-ratios"
        "break_even_met=1")
  string(FIND "${performance}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "nonlinear Operator performance missing ${marker}")
  endif()
endforeach()
string(REGEX MATCH "paired_speedup_ci95_lower=([0-9.eE+-]+)" lower_match "${performance}")
if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
  message(FATAL_ERROR "nonlinear Operator paired speedup interval does not prove benefit")
endif()
file(READ "${ABLATION}" ablation)
foreach(marker "router_online_failures=0" "router_forced_failures=0" "router_result_mismatches=0" "fallback_original_solver_used=1" "fused_original_gate_mismatches=0" "external_corrector_gate_mismatches=0" "full_verified_acceptance_rate=1" "failures=0")
  string(FIND "${ablation}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "nonlinear Operator ablation missing ${marker}")
  endif()
endforeach()
get_filename_component(benchmark_trace_directory "${ABLATION}" DIRECTORY)
set(shared_hybrid "${benchmark_trace_directory}/operator-shared-hybrid-baseline.txt")
if(NOT EXISTS "${shared_hybrid}")
  message(FATAL_ERROR "nonlinear shared hybrid baseline report is missing")
endif()
file(READ "${shared_hybrid}" shared_hybrid_report)
foreach(marker
    "SMAVE_OPERATOR_SHARED_HYBRID_BASELINE 1"
    "contract=paired-complete-runtime-learned-candidate-weighted-jacobi-strict-gate-fallback"
    "training_scenarios=16"
    "evaluation_scenarios=64"
    "repetitions=100"
    "attempted=6400"
    "candidate_uses=6400"
    "strict_original_gate=1"
    "independent_runtime_commit_gate_for_accepts=1"
    "mandatory_original_solver_fallback=1"
    "accepted=0"
    "fallbacks=6400"
    "failures=0"
    "gate_mismatches=0"
    "same_accuracy=1"
    "all_failures_retained=1")
  string(FIND "${shared_hybrid_report}" "${marker}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "nonlinear shared hybrid baseline missing ${marker}")
  endif()
endforeach()
foreach(field
    paired_median_speedup bootstrap_95_lower bootstrap_95_upper
    verified_operator_vs_shared_hybrid_paired_median_speedup
    verified_operator_vs_shared_hybrid_bootstrap_95_lower
    verified_operator_vs_shared_hybrid_bootstrap_95_upper
    maximum_mixed_qoi_error)
  string(REGEX MATCH "${field}=([0-9.eE+-]+)" match "${shared_hybrid_report}")
  if(NOT match)
    message(FATAL_ERROR "nonlinear shared hybrid baseline lacks ${field}")
  endif()
endforeach()
foreach(field baseline_median_us operator_median_us online_speedup maximum_full_state_error maximum_qoi_error maximum_candidate_full_state_error maximum_candidate_qoi_error)
  string(REGEX MATCH "${field}=([0-9.eE+-]+)" match "${performance}")
  if(NOT match)
    message(FATAL_ERROR "nonlinear Operator performance lacks ${field}")
  endif()
  set(${field} "${CMAKE_MATCH_1}")
endforeach()
foreach(field raw_candidate_median_us correction_and_runtime_gate_median_us external_corrector_median_us external_corrector_acceptance_rate external_corrector_maximum_residual router_online_over_forced_median)
  string(REGEX MATCH "${field}=([0-9.eE+-]+)" match "${ablation}")
  if(NOT match)
    message(FATAL_ERROR "nonlinear Operator ablation lacks ${field}")
  endif()
  set(${field} "${CMAKE_MATCH_1}")
endforeach()
foreach(marker
    "production_corrector_budget_sweep=0,1,2,4,8,16,32"
    "production_corrector_minimum_full_acceptance_budget=2"
    "production_budget.0.acceptance_rate=0"
    "production_budget.2.acceptance_rate=1")
  string(FIND "${ablation}" "${marker}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "nonlinear Operator ablation lacks ${marker}")
  endif()
endforeach()
file(GLOB_RECURSE traces "${TRACES}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 64)
  message(FATAL_ERROR "nonlinear Operator expected 64 traces, got ${trace_count}")
endif()
file(WRITE "${OUTPUT}" "SMAVE_NONLINEAR_OPERATOR_REPLICATION 1\nfamily=smooth-cubic-grid-single-scc\nunknowns=16\ntraining_scenarios=16\nevaluation_scenarios=64\nrepetitions=100\ncorrected_accepts=6400\nfallbacks=0\nfailures=0\ngate_mismatches=0\nsame_accuracy=1\nbaseline_median_us=${baseline_median_us}\noperator_median_us=${operator_median_us}\nonline_speedup=${online_speedup}\nmaximum_full_state_error=${maximum_full_state_error}\nmaximum_qoi_error=${maximum_qoi_error}\nraw_candidate_median_us=${raw_candidate_median_us}\nraw_candidate_maximum_full_state_error=${maximum_candidate_full_state_error}\nraw_candidate_maximum_qoi_error=${maximum_candidate_qoi_error}\ncorrection_and_runtime_gate_median_us=${correction_and_runtime_gate_median_us}\nexternal_corrector=weighted-diagonal-residual-jacobi\nexternal_corrector_median_us=${external_corrector_median_us}\nexternal_corrector_acceptance_rate=${external_corrector_acceptance_rate}\nexternal_corrector_maximum_residual=${external_corrector_maximum_residual}\nrouter_online_over_forced_median=${router_online_over_forced_median}\nraw_candidate_rejected_by_gate=1\nexternal_corrector_negative_result_retained=1\nEND\n")
