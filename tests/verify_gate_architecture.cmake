if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_GATE_ARCHITECTURE 1"
        "workloads=2"
        "operator-linear-100.decision_mismatches=0"
        "operator-linear-100.residual_mismatches=0"
        "cubic-coupled-nonlinear.decision_mismatches=0"
        "cubic-coupled-nonlinear.residual_mismatches=0"
        "strict_equivalence=1")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "gate architecture evidence missing marker: ${marker}")
    endif()
endforeach()
foreach(workload operator-linear-100 cubic-coupled-nonlinear)
    string(REGEX MATCH
        "${workload}\\.paired_speedup_ci95_lower=([0-9.eE+-]+)"
        lower_match "${report}")
    if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
        message(FATAL_ERROR "fused gate does not have a positive 95% lower bound: ${workload}")
    endif()
endforeach()
