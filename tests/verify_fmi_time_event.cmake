foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI time-event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI time-event smoke reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "MODEL \"SMAVETimeEventCS\""
    "INTERFACE \"CoSimulation\" \"SMAVETimeEventCS\""
    "SUCCESS 1"
    "SAMPLE 0 1 \"y\" 6"
    "SAMPLE 0.10000000000000001 1 \"y\" 6.0999999999999996"
    "SAMPLE 0.20000000000000001 1 \"y\" 36.200000000000003"
    "SAMPLE 0.30000000000000004 1 \"y\" 36.299999999999997"
    "STATE_ROUNDTRIP_PASSED 1"
    "DO_STEP_CALLS 5"
    "EARLY_RETURNS 0"
    "TIME_EVENT_SPLITS 1"
    "TIME_EVENTS 1"
    "EVENT_MODE_ENTRIES 3"
    "DISCRETE_UPDATE_ITERATIONS 3"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI time-event report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI Co-Simulation nextEventTime scheduling, state replay, and determinism passed")
