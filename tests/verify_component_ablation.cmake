if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "system component ablation evidence is missing")
endif()
file(READ "${EVIDENCE}" evidence)
foreach(marker
        "SMAVE_SYSTEM_COMPONENT_ABLATION 1"
        "contract=single-workload-component-evidence-matrix"
        "cross_workload_component_matrix=0"
        "single_workload_complete_ablation=1"
        "single_workload=phase5-operator-poisson-grid10"
        "router_component_evidence=paired-online-vs-forced-operator-complete-runtime"
        "external_selector_context=loo-1nn-and-literature-cart-reimplementation-on-phase4-heldout-family"
        "external_selector_public_code_used=0"
        "router_result_mismatches=0"
        "candidate_component_evidence=raw-latent-operator-candidate"
        "correction_component_evidence=operator-correction-plus-runtime-gate"
        "external_corrector_component_evidence=weighted-diagonal-residual-jacobi-plus-strict-gate"
        "external_corrector_reimplementation=1"
        "external_corrector_public_code_used=0"
        "external_corrector_acceptance_rate=1"
        "external_corrector_gate_mismatches=0"
        "gate_component_evidence=independent-versus-fused-original-expression"
        "gate_mismatches=0"
        "fallback_component_evidence=same-workload-bad-candidate-correction-to-original-solver"
        "fallback_dynamic_rejection_verified=1"
        "artifact_authority_component_evidence=artifact-certificate-bundle-tamper-rejection"
        "artifact_tamper_rejected=1"
        "certificate_tamper_rejected=1"
        "bundle_tamper_rejected=1"
        "all_components_present=1"
        "END")
    string(FIND "${evidence}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "system component ablation missing ${marker}")
    endif()
endforeach()
foreach(field
        router_online_median_us router_forced_operator_median_us
        router_online_over_forced_median router_online_over_forced_bootstrap_95_lower
        router_online_over_forced_bootstrap_95_upper router_maximum_mixed_qoi_error
        external_selector_cart_over_calibrated_median
        external_selector_cart_over_calibrated_bootstrap_95_lower
        external_selector_cart_over_calibrated_bootstrap_95_upper
        candidate_raw_median_us candidate_raw_maximum_residual
        correction_and_gate_median_us external_corrector_median_us
        external_corrector_total_iterations external_corrector_maximum_residual
        gate_independent_median_us
        gate_fused_median_us gate_fused_speedup full_verified_median_us)
    string(REGEX MATCH "(^|\n)${field}=([0-9.eE+-]+)" match "${evidence}")
    if(NOT match OR CMAKE_MATCH_2 LESS 0)
        message(FATAL_ERROR "system component ablation has invalid ${field}")
    endif()
endforeach()
message(STATUS "single-workload system component ablation evidence passed")
