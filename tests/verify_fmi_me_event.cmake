foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI ME event reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "MODEL \"SMAVEModelExchangeEvent\""
    "INTERFACE \"ModelExchange\" \"SMAVEModelExchangeEvent\""
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0 1 \"x\" 1"
    "STATE_ROUNDTRIP_PASSED 1"
    "MODEL_EXCHANGE_ROOTS 2"
    "EVENT_MODE_ENTRIES 3"
    "DISCRETE_UPDATE_ITERATIONS 3"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0"
    "ModelExchange RK4/event smoke")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI ME event report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI ModelExchange root localization, event reset, replay, and determinism passed")
