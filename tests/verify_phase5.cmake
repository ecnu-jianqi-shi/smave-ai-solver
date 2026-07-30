foreach(required IR ARTIFACT CERTIFICATE BUNDLE VALIDATION PERFORMANCE TRACES BATCH_TRACE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Phase 5 artifact: ${required}")
    endif()
endforeach()

file(READ "${IR}" ir)
foreach(pattern "BLOCKS 1" "UNKNOWNS 100")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 IR missing ${pattern}")
    endif()
endforeach()

file(READ "${ARTIFACT}" artifact)
foreach(pattern
    "SMAVE_LATENT_OPERATOR 1"
    "TRAINING 16"
    "PERMISSION \"full-state-corrected\""
    "QOI 3 \"x1\" \"x50\" \"x100\""
    "BASIS 1 100"
    "COEFFICIENTS 1 101")
    string(FIND "${artifact}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 artifact missing ${pattern}")
    endif()
endforeach()

get_filename_component(batch_trace_directory "${BATCH_TRACE}" DIRECTORY)
set(statistics_path "${batch_trace_directory}/operator-statistics.txt")
if(NOT EXISTS "${statistics_path}")
    message(FATAL_ERROR "Phase 5 operator statistics report is missing")
endif()
file(READ "${statistics_path}" statistics)
foreach(pattern
    "SMAVE_OPERATOR_STATISTICS 1"
    "repetitions=100"
    "cold_semantics=first-measured-batch"
    "hot_semantics=remaining-measured-batches"
    "hot_repetitions=99"
    "runtime_setup_semantics=baseline-and-corrector-construction-plus-bundle-validation"
    "operator_setup_semantics=latent-expert-construction-from-in-memory-artifact"
    "peak_rss_semantics=process-lifetime-high-water-mark"
    "energy_available=0"
    "energy_source=unavailable-portable-process-counter"
    "bootstrap_seed=20260720"
    "bootstrap_resamples=10000"
    "stable_speedup=")
    string(FIND "${statistics}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 statistics missing ${pattern}")
    endif()
endforeach()
foreach(field
    cold_baseline_us cold_operator_us
    hot_baseline_median_us hot_operator_median_us
    runtime_setup_us operator_setup_us
    rss_before_bytes rss_after_setup_bytes peak_rss_bytes
    baseline_median_us baseline_p90_us baseline_p99_us baseline_worst_us
    operator_median_us operator_p90_us operator_p99_us operator_worst_us
    paired_median_speedup bootstrap_95_lower bootstrap_95_upper)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${statistics}")
    if(NOT field_match OR CMAKE_MATCH_1 LESS 0.0)
        message(FATAL_ERROR "Phase 5 statistics has invalid ${field}")
    endif()
endforeach()
foreach(field rss_before_bytes rss_after_setup_bytes peak_rss_bytes)
    string(REGEX MATCH "${field}=([0-9]+)" field_match "${statistics}")
    if(NOT field_match)
        message(FATAL_ERROR "Phase 5 statistics has invalid ${field}")
    endif()
endforeach()

set(operator_external_path
    "${batch_trace_directory}/operator-external-baselines.txt")
if(NOT EXISTS "${operator_external_path}")
    message(FATAL_ERROR "Phase 5 Operator external baseline report is missing")
endif()
file(READ "${operator_external_path}" operator_external)
foreach(pattern
    "SMAVE_OPERATOR_EXTERNAL_BASELINES 1"
    "contract=paired-complete-runtime-external-vs-verified-operator"
    "entries=2"
    "BASELINE \"superlu-dgssv-cpu-v1\""
    "BASELINE \"accelerate-sparse-qr-cpu-v1\"")
    string(FIND "${operator_external}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 Operator external baselines missing ${pattern}")
    endif()
endforeach()

set(shared_hybrid_path
    "${batch_trace_directory}/operator-shared-hybrid-baseline.txt")
if(NOT EXISTS "${shared_hybrid_path}")
    message(FATAL_ERROR "Phase 5 shared hybrid baseline report is missing")
endif()
file(READ "${shared_hybrid_path}" shared_hybrid)
foreach(pattern
    "SMAVE_OPERATOR_SHARED_HYBRID_BASELINE 1"
    "contract=paired-complete-runtime-learned-candidate-weighted-jacobi-strict-gate-fallback"
    "candidate=latent-operator-fp64"
    "training_scenarios=16"
    "evaluation_scenarios=64"
    "repetitions=100"
    "attempted=6400"
    "candidate_uses=6400"
    "strict_original_gate=1"
    "independent_runtime_commit_gate_for_accepts=1"
    "mandatory_original_solver_fallback=1"
    "accepted=6400"
    "fallbacks=0"
    "failures=0"
    "gate_mismatches=0"
    "same_accuracy=1"
    "all_failures_retained=1")
    string(FIND "${shared_hybrid}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 shared hybrid baseline missing ${pattern}")
    endif()
endforeach()
foreach(field
    paired_median_speedup bootstrap_95_lower bootstrap_95_upper
    verified_operator_vs_shared_hybrid_paired_median_speedup
    verified_operator_vs_shared_hybrid_bootstrap_95_lower
    verified_operator_vs_shared_hybrid_bootstrap_95_upper
    maximum_mixed_qoi_error)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" shared_match "${shared_hybrid}")
    if(NOT shared_match)
        message(FATAL_ERROR "Phase 5 shared hybrid baseline lacks ${field}")
    endif()
endforeach()
foreach(backend "superlu-dgssv-cpu-v1" "accelerate-sparse-qr-cpu-v1")
    string(REGEX MATCH
        "BASELINE \"${backend}\"[^\n]*attempted=6400[^\n]*native_uses=6400[^\n]*fallbacks=0[^\n]*failures=0[^\n]*same_accuracy=1"
        external_match "${operator_external}")
    if(NOT external_match)
        message(FATAL_ERROR
            "Phase 5 Operator external baseline ${backend} did not execute natively")
    endif()
endforeach()
string(REGEX MATCH "bootstrap_95_lower=([0-9.eE+-]+)" lower_match "${statistics}")
if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
    message(FATAL_ERROR "Phase 5 paired bootstrap does not prove stable speedup")
endif()
string(FIND "${statistics}" "stable_speedup=1" stable_speedup)
if(stable_speedup EQUAL -1)
    message(FATAL_ERROR "Phase 5 stable speedup gate failed")
endif()

set(ablation_path "${batch_trace_directory}/operator-ablation.txt")
if(NOT EXISTS "${ablation_path}")
    message(FATAL_ERROR "Phase 5 operator ablation report is missing")
endif()
file(READ "${ablation_path}" ablation)
foreach(pattern
    "SMAVE_OPERATOR_ABLATION 1"
    "requests=64"
    "repetitions=100"
    "full_verified_acceptance_rate=1"
    "single_workload_component_matrix=1"
    "router_online_failures=0"
    "router_forced_failures=0"
    "router_online_operator_uses=6400"
    "router_forced_operator_uses=6400"
    "router_result_mismatches=0"
    "same_workload_fallback_probe=1"
    "fallback_candidate_correction_invoked=1"
    "fallback_candidate_complete_shape=1"
    "fallback_candidate_finite=1"
    "fallback_correction_maximum_iterations=1"
    "fallback_original_solver_used=1"
    "fallback_probe_success=1"
    "fused_original_gate_mismatches=0"
    "fused_original_gate_false_accepts=0"
    "fused_original_gate_false_rejects=0"
    "batched_original_gate_false_accepts=0"
    "batched_original_gate_false_rejects=0"
    "fallbacks=0"
    "failures=0"
    "production_corrector_budget_sweep=0,1,2,4,8,16,32"
    "production_corrector_minimum_full_acceptance_budget=0"
    "production_budget.0.acceptance_rate=1")
    string(FIND "${ablation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 ablation missing ${pattern}")
    endif()
endforeach()
foreach(field
    classic_median_us raw_candidate_median_us independent_gate_median_us
    fused_original_gate_median_us fused_original_gate_speedup
    batched_original_gate_median_us batched_original_gate_speedup
    correction_and_runtime_gate_median_us full_verified_median_us
    router_online_median_us router_forced_operator_median_us
    router_online_over_forced_median router_online_over_forced_bootstrap_95_lower
    router_online_over_forced_bootstrap_95_upper router_maximum_mixed_qoi_error
    raw_candidate_gate_nonreject_rate raw_candidate_gate_maximum_residual
    raw_candidate_maximum_full_state_error raw_candidate_maximum_qoi_error
    full_verified_maximum_full_state_error full_verified_maximum_qoi_error
    production_corrector_best_complete_median_us
    production_corrector_best_complete_over_classic)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${ablation}")
    if(NOT field_match OR CMAKE_MATCH_1 LESS 0.0)
        message(FATAL_ERROR "Phase 5 ablation has invalid ${field}")
    endif()
endforeach()
string(REGEX MATCH "VERSION \"(latent-operator-[0-9a-f]+)\"" version_match "${artifact}")
if(NOT version_match)
    message(FATAL_ERROR "Phase 5 artifact lacks expert version")
endif()
set(expert_version "${CMAKE_MATCH_1}")
string(REGEX MATCH "HASH \"([0-9a-f]+)\"" artifact_hash_match "${artifact}")
if(NOT artifact_hash_match)
    message(FATAL_ERROR "Phase 5 artifact lacks hash")
endif()
set(artifact_hash "${CMAKE_MATCH_1}")

file(READ "${CERTIFICATE}" certificate)
foreach(pattern
    "EXPERT \"${expert_version}\""
    "ARTIFACT \"${artifact_hash}\""
    "PROBES 203"
    "CELLS 1"
    "COUNTEREXAMPLES 0")
    string(FIND "${certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 certificate missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "HASH \"([0-9a-f]+)\"" certificate_hash_match "${certificate}")
if(NOT certificate_hash_match)
    message(FATAL_ERROR "Phase 5 certificate lacks hash")
endif()
set(certificate_hash "${CMAKE_MATCH_1}")

file(READ "${BUNDLE}" bundle)
foreach(pattern
    "\"${expert_version}\""
    "\"${artifact_hash}\""
    "\"${certificate_hash}\"")
    string(FIND "${bundle}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 Bundle missing ${pattern}")
    endif()
endforeach()

file(READ "${VALIDATION}" validation)
foreach(pattern
    "scenarios=64"
    "successful_scenarios=64"
    "admitted_invocations=64"
    "top_k_passes=64"
    "full_fallbacks=0"
    "original_solver_failures=0"
    "erroneous_accepts=0"
    "confidence_target_met=1")
    string(FIND "${validation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 validation missing ${pattern}")
    endif()
endforeach()

file(GLOB_RECURSE traces "${TRACES}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 64)
    message(FATAL_ERROR "Phase 5 expected 64 validation traces, got ${trace_count}")
endif()
foreach(trace IN LISTS traces)
    file(READ "${trace}" content)
    foreach(pattern
        "BLOCK block-1 CORRECTED_ACCEPT"
        "ATTEMPT \"${expert_version}\" \"accepted\""
        "STATUS success"
        "SUMMARY direct=0 corrected=1 warm_start=0 fallback=0")
        string(FIND "${content}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Phase 5 corrected path missing ${pattern}: ${trace}")
        endif()
    endforeach()
endforeach()

file(READ "${PERFORMANCE}" performance)
foreach(pattern
    "SMAVE_OPERATOR_BENCHMARK 3"
    "requests=64"
    "repetitions=100"
    "batches=100"
    "average_batch=64"
    "accepted=6400"
    "fallbacks=0"
    "failures=0"
    "acceptance_rate=1"
    "online_speedup_semantics=paired-median-of-per-repetition-ratios"
    "break_even_semantics=training-wall-over-paired-median-saving"
    "amortized_speedup_semantics=paired-representative-projected-ratio"
    "projected_queries=10000"
    "candidate_qoi_within_tolerance=1"
    "same_accuracy=1"
    "break_even_met=1"
    "artifact_hash=${artifact_hash}"
    "certificate_hash=${certificate_hash}")
    string(FIND "${performance}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 performance missing ${pattern}")
    endif()
endforeach()
foreach(field online_speedup amortized_speedup)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${performance}")
    if(NOT field_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
        message(FATAL_ERROR "Phase 5 ${field} does not prove benefit")
    endif()
endforeach()
string(REGEX MATCH "paired_speedup_ci95_lower=([0-9.eE+-]+)" lower_match "${performance}")
if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
    message(FATAL_ERROR "Phase 5 paired speedup interval does not prove benefit")
endif()
string(REGEX MATCH "paired_median_saving_us=([0-9.eE+-]+)" saving_match "${performance}")
if(NOT saving_match OR NOT CMAKE_MATCH_1 GREATER 0.0)
    message(FATAL_ERROR "Phase 5 paired median saving is not positive")
endif()
string(REGEX MATCH "break_even_queries=([0-9]+)" break_even_match "${performance}")
if(NOT break_even_match OR NOT CMAKE_MATCH_1 LESS_EQUAL 10000)
    message(FATAL_ERROR "Phase 5 projected demand does not exceed break-even")
endif()
foreach(field maximum_qoi_error maximum_candidate_qoi_error)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${performance}")
    if(NOT field_match OR CMAKE_MATCH_1 GREATER 1.0)
        message(FATAL_ERROR "Phase 5 ${field} exceeds the normalized 1e-4 QoI gate")
    endif()
endforeach()

file(READ "${BATCH_TRACE}" batch_trace)
foreach(pattern
    "expert_version=${expert_version}"
    "shape=100"
    "dtype=fp64"
    "qoi_relative=0.0001"
    "batches=100"
    "average_batch=64"
    "accepted=6400"
    "fallbacks=0")
    string(FIND "${batch_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 5 batch trace missing ${pattern}")
    endif()
endforeach()
message(STATUS "Phase 5 Operator break-even, corrected QoI, certificate, confidence, and batch gates passed")
