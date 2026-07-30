foreach(required PROGRAM FMU OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" smoke-fmu "${FMU}"
        --end 0.2 --step 0.1 --async-timeout-ms 0
        --allow-native-execution --output "${OUTPUT}.unexpected"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
if(result EQUAL 0)
    message(FATAL_ERROR "zero FMI asynchronous timeout unexpectedly succeeded")
endif()
if(NOT stderr MATCHES "between 1 and 60000")
    message(FATAL_ERROR "zero-timeout rejection reason missing: ${stderr}")
endif()
file(WRITE "${OUTPUT}"
    "EXPECTED_FAILURE 1\nEXIT_CODE ${result}\nSTDOUT\n${stdout}STDERR\n${stderr}")
