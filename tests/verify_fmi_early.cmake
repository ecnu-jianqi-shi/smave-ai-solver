foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI early-return artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI early-return smoke reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "MODEL \"SMAVEEarlyCS\""
    "INTERFACE \"CoSimulation\" \"SMAVEEarlyCS\""
    "SUCCESS 1"
    "SAMPLE 0 1 \"y\" 6"
    "SAMPLE 0.10000000000000001 1 \"y\" 36.100000000000001"
    "SAMPLE 0.20000000000000001 1 \"y\" 36.200000000000003"
    "SAMPLE 0.30000000000000004 1 \"y\" 36.299999999999997"
    "STATE_ROUNDTRIP_PASSED 1"
    "DO_STEP_CALLS 6"
    "EARLY_RETURNS 2"
    "EVENT_MODE_ENTRIES 2"
    "DISCRETE_UPDATE_ITERATIONS 4"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI early-return report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI Co-Simulation early return, event fixed point, continuation, and determinism passed")
