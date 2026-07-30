if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "router shift evidence is missing")
endif()

file(READ "${EVIDENCE}" content)
foreach(required
        "SMAVE_ROUTER_SHIFT_ANALYSIS 1"
        "contract=source-5x5-to-heldout-6x6-complete-cost-shift"
        "winner_preserved=1"
        "gate_status_mismatches=0"
        "source_gate_passing_max_calibration_error=0.02"
        "heldout_gate_passing_max_calibration_error=0.02"
        "END")
    string(FIND "${content}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing router shift field: ${required}")
    endif()
endforeach()

string(REGEX MATCH "gate_passing_cost_rank_spearman=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 0.8)
    message(FATAL_ERROR "gate-passing expert cost ranking is unstable")
endif()

string(REGEX MATCH
    "source_selected_holdout_vs_fastest_gate_passing_median=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 1.0)
    message(FATAL_ERROR "invalid non-negative held-out regret")
endif()

message(STATUS "router shift analysis evidence passed")
