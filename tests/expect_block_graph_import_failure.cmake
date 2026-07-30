foreach(required SMAVE INPUT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing block graph failure argument: ${required}")
    endif()
endforeach()
if(NOT DEFINED ERROR_PATTERN OR ERROR_PATTERN STREQUAL "")
    message(FATAL_ERROR "missing block graph failure argument: ERROR_PATTERN")
endif()
execute_process(
    COMMAND "${SMAVE}" import-block-graph "${INPUT}"
        --output "${INPUT}.unexpected.ir"
    RESULT_VARIABLE result
    ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "${ERROR_PATTERN}")
    message(FATAL_ERROR
        "block graph import did not fail as expected: result=${result}, error=${error}")
endif()
