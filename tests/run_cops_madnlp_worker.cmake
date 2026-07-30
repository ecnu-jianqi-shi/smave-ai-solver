if(NOT DEFINED JULIA OR NOT DEFINED ENVIRONMENT OR NOT DEFINED SOURCE_ROOT OR
   NOT DEFINED OUTPUT OR NOT DEFINED BRIDGE OR NOT DEFINED CASE_NAMES)
    message(FATAL_ERROR
        "JULIA, ENVIRONMENT, SOURCE_ROOT, OUTPUT, BRIDGE and CASE_NAMES are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT}/madnlp-cases" "${OUTPUT}/madnlp-logs")
foreach(case_name IN LISTS CASE_NAMES)
    set(checkpoint "${OUTPUT}/madnlp-cases/${case_name}.txt")
    set(valid_checkpoint FALSE)
    if(EXISTS "${checkpoint}")
        file(READ "${checkpoint}" checkpoint_text)
        if(checkpoint_text MATCHES "^SMAVE_COPS_MADNLP_CASE 12" AND
           checkpoint_text MATCHES "END")
            set(valid_checkpoint TRUE)
        endif()
    endif()
    if(valid_checkpoint)
        continue()
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            "SMAVE_COPS_BRIDGE=${BRIDGE}"
            "${JULIA}" --startup-file=no --project=${ENVIRONMENT}
            "${SOURCE_ROOT}/tests/run_cops_madnlp_case.jl"
            "${case_name}" "${OUTPUT}/cases/${case_name}.txt" "${checkpoint}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 1800)
    file(WRITE "${OUTPUT}/madnlp-logs/${case_name}.log" "${stdout}${stderr}")
    if(result MATCHES "timeout")
        file(WRITE "${checkpoint}"
            "SMAVE_COPS_MADNLP_CASE 12\n"
            "CASE \"${case_name}\"\n"
            "CASE_TIMEOUT 1\n"
            "TIMEOUT_SECONDS 1800\n"
            "TRADITIONAL_STATUS \"TIMEOUT_OR_INCOMPLETE\"\n"
            "SMAVE_STATUS \"TIMEOUT_OR_NOT_RUN\"\n"
            "SMAVE_KKT_ATTEMPTS 0\n"
            "SMAVE_KKT_SOLVES 0\n"
            "SMAVE_INDUSTRIAL_SOLVES 0\n"
            "SMAVE_SUPERLU_SOLVES 0\n"
            "SMAVE_ITERATIVE_SOLVES 0\n"
            "EXTERNAL_FALLBACK_SOLVES 0\n"
            "FALLBACK_ONLY 0\n"
            "RESOURCE_GATED 0\n"
            "CORRECTNESS_AGREEMENT 0\n"
            "END\n")
        message(STATUS "COPS MadNLP worker timed out: ${case_name}")
    elseif(NOT result EQUAL 0)
        message(STATUS "COPS MadNLP worker failed (${result}): ${case_name}")
    else()
        message(STATUS "COPS MadNLP worker completed: ${case_name}")
    endif()
endforeach()
