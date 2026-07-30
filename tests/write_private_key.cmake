if(NOT DEFINED OUTPUT OR NOT DEFINED CONTENT OR NOT DEFINED PERMISSION_REPORT)
    message(FATAL_ERROR "OUTPUT, CONTENT and PERMISSION_REPORT are required")
endif()

file(WRITE "${OUTPUT}" "${CONTENT}")
file(CHMOD "${OUTPUT}" PERMISSIONS OWNER_READ OWNER_WRITE)

if(UNIX)
    execute_process(
        COMMAND ls -ld "${OUTPUT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE permissions
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "cannot inspect private key permissions: ${error}")
    endif()
    if(NOT permissions MATCHES "^-rw-------")
        message(FATAL_ERROR "release key is not owner-only: ${permissions}")
    endif()
    file(WRITE "${PERMISSION_REPORT}" "OWNER_ONLY 1\nLISTING ${permissions}\n")
else()
    file(WRITE "${PERMISSION_REPORT}" "OWNER_ONLY NOT_APPLICABLE\n")
endif()
