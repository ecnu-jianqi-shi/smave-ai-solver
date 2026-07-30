if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "shared operator baseline evidence is missing")
endif()

file(READ "${EVIDENCE}" evidence)
foreach(marker
    "SMAVE_OPERATOR_SHARED_BASELINE 1"
    "contract=two-family-learned-candidate-weighted-jacobi-strict-gate-fallback"
    "families=2"
    "linear.attempted=6400"
    "linear.accepted=6400"
    "linear.fallbacks=0"
    "linear.failures=0"
    "linear.gate_mismatches=0"
    "linear.same_accuracy=1"
    "nonlinear.attempted=6400"
    "nonlinear.accepted=0"
    "nonlinear.fallbacks=6400"
    "nonlinear.failures=0"
    "nonlinear.gate_mismatches=0"
    "nonlinear.same_accuracy=1"
    "total_attempted=12800"
    "total_failures=0"
    "total_gate_mismatches=0"
    "END")
    string(FIND "${evidence}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "shared operator baseline evidence missing ${marker}")
    endif()
endforeach()

foreach(family linear nonlinear)
    string(REGEX MATCH
        "${family}\\.verified_operator_vs_shared_baseline_bootstrap_95_lower=([0-9.eE+-]+)"
        lower_match "${evidence}")
    if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
        message(FATAL_ERROR
            "${family} verified operator does not stably beat the shared baseline")
    endif()
endforeach()

message(STATUS "shared operator baseline contract and paired intervals passed")
