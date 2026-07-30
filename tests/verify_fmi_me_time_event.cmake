foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME time-event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI ME time-event reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "MODEL \"SMAVEModelExchangeTimeEvent\""
    "INTERFACE \"ModelExchange\" \"SMAVEModelExchangeTimeEvent\""
    "SUCCESS 1"
    "SAMPLE 0 1 \"x\" 1"
    "SAMPLE 0.10000000000000001 1 \"x\" 11"
    "SAMPLE 0.20000000000000001 1 \"x\" 11"
    "SAMPLE 0.30000000000000004 1 \"x\" 11"
    "STATE_ROUNDTRIP_PASSED 1"
    "TIME_EVENT_SPLITS 2"
    "TIME_EVENTS 2"
    "MODEL_EXCHANGE_ROOTS 0"
    "EVENT_MODE_ENTRIES 3"
    "DISCRETE_UPDATE_ITERATIONS 3"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI ME time-event report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI ModelExchange nextEventTime scheduling, state replay, and determinism passed")
