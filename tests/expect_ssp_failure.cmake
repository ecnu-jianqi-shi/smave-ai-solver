foreach(required SMAVE INPUT ERROR_PATTERN)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing SSP failure argument: ${required}")
    endif()
endforeach()
set(command "${SMAVE}" simulate-ssp "${INPUT}" --end 0.3 --step 0.1
    --output "${INPUT}.unexpected-report.txt")
if(ALLOW_NATIVE_EXECUTION)
    list(APPEND command --allow-native-execution)
endif()
execute_process(
    COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
if(result EQUAL 0)
    message(FATAL_ERROR "SSP failure fixture unexpectedly succeeded")
endif()
set(text "${stdout}${stderr}")
if(NOT text MATCHES "${ERROR_PATTERN}")
    message(FATAL_ERROR "SSP failure missing ${ERROR_PATTERN}: ${text}")
endif()
