foreach(required PROGRAM STORE KEY OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" release-status --store "${STORE}" --key "${KEY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
if(result EQUAL 0)
    message(FATAL_ERROR "tampered release store unexpectedly passed verification")
endif()
file(WRITE "${OUTPUT}"
    "EXPECTED_FAILURE 1\nEXIT_CODE ${result}\nSTDOUT\n${stdout}STDERR\n${stderr}")
