foreach(required SMAVE MODEL SCENARIO TRACE_DIR OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing non-symmetric runner argument: ${required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${TRACE_DIR}")
execute_process(
    COMMAND "${SMAVE}" solve "${MODEL}" --scenario "${SCENARIO}"
        --trace-dir "${TRACE_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
file(WRITE "${OUTPUT}" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "non-symmetric solve failed with ${result}: ${stderr}")
endif()
file(GLOB traces "${TRACE_DIR}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 1)
    message(FATAL_ERROR "expected one non-symmetric trace, got ${trace_count}")
endif()
list(GET traces 0 trace)
file(WRITE "${OUTPUT}.trace-path" "${trace}\n")
