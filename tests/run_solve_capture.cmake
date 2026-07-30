foreach(required SMAVE MODEL SCENARIO EXPERT CERTIFICATE BUNDLE TRACE_DIR OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing solve capture argument: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${SMAVE}" solve "${MODEL}"
        --scenario "${SCENARIO}"
        --expert "${EXPERT}"
        --certificate "${CERTIFICATE}"
        --bundle "${BUNDLE}"
        --trace-dir "${TRACE_DIR}"
    OUTPUT_FILE "${OUTPUT}"
    ERROR_VARIABLE error
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "captured solve failed (${result}): ${error}")
endif()
