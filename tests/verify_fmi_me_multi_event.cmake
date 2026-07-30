foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME multi-event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI ME multi-event reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "MODEL \"SMAVEModelExchangeMultiEvent\""
    "INTERFACE \"ModelExchange\" \"SMAVEModelExchangeMultiEvent\""
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0 1 \"x\" 1"
    "SAMPLE 0.10000000000000001 1 \"x\" 1.1051709168394022"
    "STATE_ROUNDTRIP_PASSED 1"
    "MODEL_EXCHANGE_ROOTS 4"
    "EVENT_MODE_ENTRIES 5"
    "DISCRETE_UPDATE_ITERATIONS 5"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI ME multi-event report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI ModelExchange earliest-root ordering, multiple roots, replay, and determinism passed")
