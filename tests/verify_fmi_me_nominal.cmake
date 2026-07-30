foreach(required FMI2_REPORT FMI2_REPEAT_REPORT FMI3_REPORT FMI3_REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME nominal artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${FMI2_REPORT}" "${FMI2_REPEAT_REPORT}"
    RESULT_VARIABLE fmi2_compare)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${FMI3_REPORT}" "${FMI3_REPEAT_REPORT}"
    RESULT_VARIABLE fmi3_compare)
if(NOT fmi2_compare EQUAL 0 OR NOT fmi3_compare EQUAL 0)
    message(FATAL_ERROR "FMI ME nominal reports are not deterministic")
endif()
file(READ "${FMI2_REPORT}" fmi2_report)
file(READ "${FMI3_REPORT}" fmi3_report)
foreach(report IN ITEMS fmi2_report fmi3_report)
    foreach(pattern
        "SUCCESS 1"
        "STATE_ROUNDTRIP_PASSED 1"
        "MAX_STATE_REPLAY_ERROR 0"
        "CONTINUOUS_STATE_NOMINAL_UPDATES 2"
        "MIN_CONTINUOUS_STATE_NOMINAL 2"
        "MAX_CONTINUOUS_STATE_NOMINAL 2")
        if(NOT ${report} MATCHES "${pattern}")
            message(FATAL_ERROR "FMI ME nominal evidence missing: ${pattern}")
        endif()
    endforeach()
endforeach()
message(STATUS "FMI 2/3 Model Exchange nominal update, replay, and determinism passed")
