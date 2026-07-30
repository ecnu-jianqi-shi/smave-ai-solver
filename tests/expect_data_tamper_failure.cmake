execute_process(
    COMMAND "${SMAVE}" verify-data --dataset validation-scenarios
        --version "${VERSION}" --store "${STORE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
if(result EQUAL 0)
    message(FATAL_ERROR "tampered dataset snapshot unexpectedly verified: ${stdout}")
endif()
if(NOT stderr MATCHES "dataset payload integrity check failed")
    message(FATAL_ERROR "tamper rejection lacked integrity reason: ${stderr}")
endif()
