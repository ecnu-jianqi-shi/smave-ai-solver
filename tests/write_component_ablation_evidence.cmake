foreach(required SMAVE IR ARTIFACT CERTIFICATE BUNDLE SCENARIO PAIRED_ORACLE
                 OPERATOR_ABLATION NONLINEAR_CASCADE OUTPUT_DIR OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "component ablation input is missing: ${required}")
    endif()
endforeach()
foreach(path "${SMAVE}" "${IR}" "${ARTIFACT}" "${CERTIFICATE}" "${BUNDLE}"
             "${SCENARIO}" "${PAIRED_ORACLE}" "${OPERATOR_ABLATION}"
             "${NONLINEAR_CASCADE}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "component ablation path is missing: ${path}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(READ "${PAIRED_ORACLE}" paired_oracle)
file(READ "${OPERATOR_ABLATION}" operator_ablation)
file(READ "${NONLINEAR_CASCADE}" nonlinear_cascade)

function(require_marker content marker)
    string(FIND "${content}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "component ablation source missing ${marker}")
    endif()
endfunction()

function(read_field content field output)
    string(REGEX MATCH "(^|\n)${field}=([^\n]+)" match "${content}")
    if(NOT match)
        message(FATAL_ERROR "component ablation source missing field ${field}")
    endif()
    set(${output} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

foreach(marker
        "feature_selector_compared=1"
        "online_failures=0"
        "online_gate_mismatches=0"
        "calibrated_failures=0"
        "calibrated_gate_mismatches=0"
        "selector_failures=0"
        "selector_gate_mismatches=0")
    require_marker("${paired_oracle}" "${marker}")
endforeach()
foreach(marker
        "SMAVE_OPERATOR_ABLATION 1"
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
        "fallback_original_solver_used=1"
        "fallback_probe_success=1"
        "fused_original_gate_mismatches=0"
        "fused_original_gate_false_accepts=0"
        "fused_original_gate_false_rejects=0"
        "external_corrector_contract=weighted-diagonal-residual-jacobi-plus-strict-gate"
        "external_corrector_reimplementation=1"
        "external_corrector_public_code_used=0"
        "external_corrector_maximum_iterations=32"
        "external_corrector_failures=0"
        "external_corrector_acceptance_rate=1"
        "external_corrector_gate_mismatches=0"
        "full_verified_acceptance_rate=1"
        "fallbacks=0"
        "failures=0")
    require_marker("${operator_ablation}" "${marker}")
endforeach()
foreach(marker
        "FIXED_CASCADE_PLAN_VERIFIED 1"
        "ONLINE_EQUATION_MOE_PLAN_VERIFIED 1"
        "DYNAMIC_REJECTION_CASCADE_VERIFIED 1"
        "PAIRED_ACCURACY_VERIFIED 1")
    require_marker("${nonlinear_cascade}" "${marker}")
endforeach()

foreach(field
        paired_median_online_over_calibrated
        calibration_gain_bootstrap_95_lower
        calibration_gain_bootstrap_95_upper
        paired_median_selector_over_calibrated
        selector_calibrated_bootstrap_95_lower
        selector_calibrated_bootstrap_95_upper
        paired_median_cart_over_calibrated
        cart_calibrated_bootstrap_95_lower
        cart_calibrated_bootstrap_95_upper)
    read_field("${paired_oracle}" "${field}" "router_${field}")
endforeach()
foreach(field
        router_online_median_us
        router_forced_operator_median_us
        router_online_over_forced_median
        router_online_over_forced_bootstrap_95_lower
        router_online_over_forced_bootstrap_95_upper
        router_maximum_mixed_qoi_error
        raw_candidate_median_us
        raw_candidate_gate_maximum_residual
        correction_and_runtime_gate_median_us
        external_corrector_median_us
        external_corrector_acceptance_rate
        external_corrector_total_iterations
        external_corrector_maximum_residual
        independent_gate_median_us
        fused_original_gate_median_us
        fused_original_gate_speedup
        full_verified_median_us)
    read_field("${operator_ablation}" "${field}" "operator_${field}")
endforeach()

set(tampered_artifact "${OUTPUT_DIR}/tampered.expert")
set(tampered_certificate "${OUTPUT_DIR}/tampered.verify")
set(tampered_bundle "${OUTPUT_DIR}/tampered.bundle")
file(COPY_FILE "${ARTIFACT}" "${tampered_artifact}")
file(COPY_FILE "${CERTIFICATE}" "${tampered_certificate}")
file(COPY_FILE "${BUNDLE}" "${tampered_bundle}")
file(APPEND "${tampered_artifact}" "\nTAMPER\n")
file(APPEND "${tampered_certificate}" "\nTAMPER\n")
file(APPEND "${tampered_bundle}" "\nTAMPER\n")

function(expect_solve_rejection label artifact certificate bundle)
    execute_process(
        COMMAND "${SMAVE}" solve "${IR}"
            --scenario "${SCENARIO}"
            --expert "${artifact}"
            --certificate "${certificate}"
            --bundle "${bundle}"
            --trace-dir "${OUTPUT_DIR}/${label}-traces"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr)
    if(result EQUAL 0)
        message(FATAL_ERROR "${label} tamper unexpectedly passed solve: ${stdout}")
    endif()
    string(REPLACE "\n" " " stderr_single_line "${stderr}")
    string(STRIP "${stderr_single_line}" stderr_single_line)
    set(${label}_rejected 1 PARENT_SCOPE)
    set(${label}_reason "${stderr_single_line}" PARENT_SCOPE)
endfunction()

expect_solve_rejection(
    artifact "${tampered_artifact}" "${CERTIFICATE}" "${BUNDLE}")
expect_solve_rejection(
    certificate "${ARTIFACT}" "${tampered_certificate}" "${BUNDLE}")
expect_solve_rejection(
    bundle "${ARTIFACT}" "${CERTIFICATE}" "${tampered_bundle}")

file(WRITE "${OUTPUT}"
    "SMAVE_SYSTEM_COMPONENT_ABLATION 1\n"
    "contract=single-workload-component-evidence-matrix\n"
    "cross_workload_component_matrix=0\n"
    "single_workload_complete_ablation=1\n"
    "single_workload=phase5-operator-poisson-grid10\n"
    "router_component_evidence=paired-online-vs-forced-operator-complete-runtime\n"
    "router_online_median_us=${operator_router_online_median_us}\n"
    "router_forced_operator_median_us=${operator_router_forced_operator_median_us}\n"
    "router_online_over_forced_median=${operator_router_online_over_forced_median}\n"
    "router_online_over_forced_bootstrap_95_lower=${operator_router_online_over_forced_bootstrap_95_lower}\n"
    "router_online_over_forced_bootstrap_95_upper=${operator_router_online_over_forced_bootstrap_95_upper}\n"
    "router_result_mismatches=0\n"
    "router_maximum_mixed_qoi_error=${operator_router_maximum_mixed_qoi_error}\n"
    "external_selector_context=loo-1nn-and-literature-cart-reimplementation-on-phase4-heldout-family\n"
    "external_selector_online_over_calibrated_median=${router_paired_median_online_over_calibrated}\n"
    "external_selector_1nn_over_calibrated_median=${router_paired_median_selector_over_calibrated}\n"
    "external_selector_cart_over_calibrated_median=${router_paired_median_cart_over_calibrated}\n"
    "external_selector_cart_over_calibrated_bootstrap_95_lower=${router_cart_calibrated_bootstrap_95_lower}\n"
    "external_selector_cart_over_calibrated_bootstrap_95_upper=${router_cart_calibrated_bootstrap_95_upper}\n"
    "external_selector_public_code_used=0\n"
    "candidate_component_evidence=raw-latent-operator-candidate\n"
    "candidate_raw_median_us=${operator_raw_candidate_median_us}\n"
    "candidate_raw_maximum_residual=${operator_raw_candidate_gate_maximum_residual}\n"
    "correction_component_evidence=operator-correction-plus-runtime-gate\n"
    "correction_and_gate_median_us=${operator_correction_and_runtime_gate_median_us}\n"
    "external_corrector_component_evidence=weighted-diagonal-residual-jacobi-plus-strict-gate\n"
    "external_corrector_reimplementation=1\n"
    "external_corrector_public_code_used=0\n"
    "external_corrector_median_us=${operator_external_corrector_median_us}\n"
    "external_corrector_acceptance_rate=${operator_external_corrector_acceptance_rate}\n"
    "external_corrector_total_iterations=${operator_external_corrector_total_iterations}\n"
    "external_corrector_maximum_residual=${operator_external_corrector_maximum_residual}\n"
    "external_corrector_gate_mismatches=0\n"
    "gate_component_evidence=independent-versus-fused-original-expression\n"
    "gate_independent_median_us=${operator_independent_gate_median_us}\n"
    "gate_fused_median_us=${operator_fused_original_gate_median_us}\n"
    "gate_fused_speedup=${operator_fused_original_gate_speedup}\n"
    "gate_mismatches=0\n"
    "full_verified_median_us=${operator_full_verified_median_us}\n"
    "fallback_component_evidence=same-workload-bad-candidate-correction-to-original-solver\n"
    "fallback_dynamic_rejection_verified=1\n"
    "artifact_authority_component_evidence=artifact-certificate-bundle-tamper-rejection\n"
    "artifact_tamper_rejected=${artifact_rejected}\n"
    "certificate_tamper_rejected=${certificate_rejected}\n"
    "bundle_tamper_rejected=${bundle_rejected}\n"
    "artifact_rejection_reason=${artifact_reason}\n"
    "certificate_rejection_reason=${certificate_reason}\n"
    "bundle_rejection_reason=${bundle_reason}\n"
    "all_components_present=1\n"
    "END\n")
