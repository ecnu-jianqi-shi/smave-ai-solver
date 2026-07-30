foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "FMI ModelExchange smoke reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SMAVE_FMI_SMOKE_REPORT 1"
    "MODEL \"SMAVEModelExchange\""
    "INTERFACE \"ModelExchange\" \"SMAVEModelExchange\""
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0 1 \"x\" 1"
    "SAMPLE 0.10000000000000001 1 \"x\" 1.1051708333333334"
    "SAMPLE 0.20000000000000001 1 \"x\" 1.2214025708506946"
    "SAMPLE 0.30000000000000004 1 \"x\" 1.3498584970625378"
    "STATE_ROUNDTRIP_ATTEMPTED 1"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "STATE_SERIALIZATION_ATTEMPTED 1"
    "STATE_SERIALIZATION_PASSED 1"
    "SERIALIZED_STATE_BYTES 32"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0"
    "ModelExchange RK4/event smoke")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI ME report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI ModelExchange RK4 lifecycle, serialized state replay, and determinism passed")
