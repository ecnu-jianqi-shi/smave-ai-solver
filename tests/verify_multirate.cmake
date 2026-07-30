foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing multirate artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "multirate model group execution is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern
    "SMAVE_BLOCK_GRAPH 2"
    "NODE \"fast-delay\" \"unit_delay\" 0.1[0-9]* 0"
    "NODE \"slow-gain\" \"gain\" 0.2[0-9]* 0.1"
    "NODE \"slow-delay\" \"unit_delay\" 0.2[0-9]* 0.1")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "multirate IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "BASE_STEP 0.1"
    "LOCAL_FALLBACKS 0"
    "MAX_CONNECTION_ERROR 0"
    "TICKS 5"
    "TICK 0 0 3 \"one\" \"fast-delay\" \"accumulator\"[^\n]*\"slow-delay.out\" -20[^\n]*\"slow-gain.out\" -10"
    "TICK 1 0.1[^\n]* 5 \"one\" \"fast-delay\" \"accumulator\" \"slow-gain\" \"slow-delay\"[^\n]*\"slow-delay.out\" -20[^\n]*\"slow-gain.out\" 20"
    "TICK 2 0.2[^\n]* 3 \"one\" \"fast-delay\" \"accumulator\"[^\n]*\"slow-delay.out\" -20[^\n]*\"slow-gain.out\" 20"
    "TICK 3 0.3[^\n]*\"slow-delay.out\" 20[^\n]*\"slow-gain.out\" 40"
    "FINAL_OUTPUTS 5[^\n]*\"slow-delay.out\" 20[^\n]*\"slow-gain.out\" 40")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "multirate acceptance evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Phased multirate scheduling, pre-activation hold, rate-boundary delay, and determinism gates passed")
