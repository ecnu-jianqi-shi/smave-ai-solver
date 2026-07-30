if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "router shift matrix evidence is missing")
endif()

file(READ "${EVIDENCE}" content)
foreach(required
        "SMAVE_ROUTER_SHIFT_MATRIX 1"
        "axes=conditioning,topology"
        "scenarios=64"
        "repetitions_per_axis=20"
        "conditioning.gate_status_changes=0"
        "all_axes_zero_gate_mismatches=1"
        "all_axes_zero_dangerous_misroutes=1"
        "all_axes_safe=1"
        "END")
    string(FIND "${content}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing router shift matrix field: ${required}")
    endif()
endforeach()

string(REGEX MATCH "topology.gate_status_changes=([0-9]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 2)
    message(FATAL_ERROR "topology shift did not change solver eligibility")
endif()

string(REGEX MATCH "minimum_paired_speedup_ci95_lower=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 1.05)
    message(FATAL_ERROR "router shift matrix lacks stable complete-cost benefit")
endif()

string(REGEX MATCH
    "maximum_structurally_filtered_calibration_error=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 GREATER 0.02)
    message(FATAL_ERROR "structurally filtered acceptance calibration regressed")
endif()

string(REGEX MATCH "maximum_selected_complete_cost_regret=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 GREATER 1.5)
    message(FATAL_ERROR "source-calibrated router regret is too large")
endif()

message(STATUS "router shift matrix evidence passed")
