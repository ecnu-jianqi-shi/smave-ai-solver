if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "risk-adaptive gate evidence is missing")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_RISK_ADAPTIVE_GATE 1"
        "contract=experimental-immutable-input-token-incremental-certificate"
        "authority_contract=strict-per-request-fp64-original-expression"
        "deployment_promoted=0"
        "assumptions=immutable-model-artifact-tolerance-and-direct-permission;library-owned-immutable-input-identity"
        "high_risk_policy=full-strict-verification"
        "low_risk_policy=immutable-input-token-certificate-with-periodic-full-verification"
        "periodic_full_interval=16"
        "certificate_drift_action=reject-and-invalidate"
        "cached_result_contract=aggregate-decision-and-residual-inf"
        "input_issuance_timing=excluded-from-gate-timing"
        "library_issued_immutable_input=1"
        "explicit_revocation_recheck=1"
        "foreign_session_requires_first_full_verification=1"
        "concurrent_serialized_recheck=1"
        "negative_contract_recheck=1"
        "full_solve_policy_contract_recheck=1"
        "full_solve_concurrency_revocation_recheck=1"
        "reconstructed_process_local_recheck=1"
        "bundle_version_change_recheck=1"
        "workloads=2"
        "full_solve_workloads=3"
        "operator-linear-100.false_accepts=0"
        "operator-linear-100.false_rejects=0"
        "cubic-coupled-nonlinear.false_accepts=0"
        "cubic-coupled-nonlinear.false_rejects=0"
        "offline_strict_equivalence=1"
        "full-solve-linear.result_mismatches=0"
        "full-solve-linear.periodic_mismatches=0"
        "full-solve-nonlinear.result_mismatches=0"
        "full-solve-nonlinear.periodic_mismatches=0"
        "full-solve-scaled-nonlinear.result_mismatches=0"
        "full-solve-scaled-nonlinear.periodic_mismatches=0"
        "full_solve_strict_equivalence=1"
        "full_solve_gate_significant_workloads=3")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "risk-adaptive gate evidence missing marker: ${marker}")
    endif()
endforeach()
foreach(workload operator-linear-100 cubic-coupled-nonlinear)
    foreach(field policy_requests high_risk_full_verifications
                  periodic_full_verifications certificate_reuses low_cost_rejects)
        string(REGEX MATCH "${workload}[.]${field}=([0-9]+)" match "${report}")
        if(NOT match OR NOT CMAKE_MATCH_1 GREATER 0)
            message(FATAL_ERROR "risk-adaptive gate missing positive ${workload}.${field}")
        endif()
    endforeach()
    string(REGEX MATCH
        "${workload}[.]paired_speedup_ci95_lower=([0-9.eE+-]+)" lower_match "${report}")
    if(NOT lower_match)
        message(FATAL_ERROR "risk-adaptive gate missing speedup interval: ${workload}")
    endif()
    if(NOT CMAKE_MATCH_1 GREATER 1.0)
        message(FATAL_ERROR "risk-adaptive gate lacks positive speedup lower bound: ${workload}")
    endif()
endforeach()
foreach(workload full-solve-linear full-solve-nonlinear full-solve-scaled-nonlinear)
    foreach(field policy_requests strict_verifications
                  periodic_strict_verifications certificate_reuses)
        string(REGEX MATCH "${workload}[.]${field}=([0-9]+)" match "${report}")
        if(NOT match OR NOT CMAKE_MATCH_1 GREATER 0)
            message(FATAL_ERROR "full-solve gate evidence missing positive ${workload}.${field}")
        endif()
    endforeach()
    foreach(metric gate_speedup_ci95_lower)
        string(REGEX MATCH
            "${workload}[.]${metric}=([0-9.eE+-]+)" lower_match "${report}")
        if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
            message(FATAL_ERROR
                "full-solve gate evidence lacks positive ${workload}.${metric}")
        endif()
    endforeach()
    string(REGEX MATCH
        "${workload}[.]total_speedup_ci95_lower=([0-9.eE+-]+)"
        total_lower_match "${report}")
    if(NOT total_lower_match)
        message(FATAL_ERROR
            "full-solve gate evidence missing ${workload}.total_speedup_ci95_lower")
    endif()
endforeach()
string(REGEX MATCH
    "full-solve-linear[.]total_speedup_ci95_lower=([0-9.eE+-]+)"
    linear_total_lower "${report}")
if(NOT linear_total_lower OR NOT CMAKE_MATCH_1 GREATER 1.0)
    message(FATAL_ERROR "linear full-solve total speedup is not significant")
endif()
string(REGEX MATCH
    "full-solve-scaled-nonlinear[.]total_speedup_ci95_lower=([0-9.eE+-]+)"
    scaled_nonlinear_total_lower "${report}")
if(NOT scaled_nonlinear_total_lower OR NOT CMAKE_MATCH_1 GREATER 1.0)
    message(FATAL_ERROR "scaled nonlinear full-solve total speedup is not significant")
endif()
message(STATUS "experimental library-owned immutable-input gate evidence passed")
