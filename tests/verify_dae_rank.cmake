foreach(required SMAVE IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing DAE rank argument: ${required}")
    endif()
endforeach()
foreach(output IN ITEMS "${REPORT}" "${REPEAT_REPORT}")
    execute_process(
        COMMAND "${SMAVE}" simulate-dae "${IR}"
            --end 0.3 --max-step 0.1 --output "${output}"
        RESULT_VARIABLE simulation_result)
    if(NOT simulation_result EQUAL 18)
        message(FATAL_ERROR
            "DAE rank-loss simulation returned ${simulation_result}, expected 18")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "DAE rank-failure reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 0"
    "FINAL_TIME 0.10000000000000001"
    "STEPS 1"
    "ALGEBRAIC_RANK_CHECKS 3"
    "MIN_ALGEBRAIC_RANK_MARGIN 0"
    "MESSAGE \"DAE algebraic Jacobian numerical rank gate failed at t=0.200000\"")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "DAE numerical-rank evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "DAE algebraic Jacobian rank loss was rejected before step commit")
