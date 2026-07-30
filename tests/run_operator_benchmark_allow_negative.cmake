foreach(required CLI MODEL SCENARIOS ARTIFACT CERTIFICATE BUNDLE TRACE_DIR OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Operator benchmark runner requires ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CLI}" benchmark-operator "${MODEL}"
        --scenarios "${SCENARIOS}"
        --expert "${ARTIFACT}"
        --certificate "${CERTIFICATE}"
        --bundle "${BUNDLE}"
        --repetitions 100
        --projected-queries 10000
        --trace-dir "${TRACE_DIR}"
        --output "${OUTPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
message("${stdout}")
if(stderr)
    message("${stderr}")
endif()
if(NOT result EQUAL 0 AND NOT result EQUAL 10)
    message(FATAL_ERROR "Operator benchmark failed with unexpected status ${result}")
endif()
if(NOT EXISTS "${OUTPUT}" OR
   NOT EXISTS "${TRACE_DIR}/operator-statistics.txt")
    message(FATAL_ERROR "Operator benchmark did not emit required reports")
endif()
