foreach(required FMI2_REPORT FMI2_REPEAT_REPORT FMI3_REPORT FMI3_REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI ME grazing artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${FMI2_REPORT}" "${FMI2_REPEAT_REPORT}"
    RESULT_VARIABLE fmi2_compare)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${FMI3_REPORT}" "${FMI3_REPEAT_REPORT}"
    RESULT_VARIABLE fmi3_compare)
if(NOT fmi2_compare EQUAL 0 OR NOT fmi3_compare EQUAL 0)
    message(FATAL_ERROR "FMI ME grazing reports are not deterministic")
endif()
file(READ "${FMI2_REPORT}" fmi2_report)
file(READ "${FMI3_REPORT}" fmi3_report)
foreach(report IN ITEMS fmi2_report fmi3_report)
    foreach(pattern
        "SUCCESS 1"
        "SAMPLES 3"
        "STATE_ROUNDTRIP_PASSED 1"
        "MAX_STATE_REPLAY_ERROR 0"
        "MODEL_EXCHANGE_ROOTS 2"
        "MODEL_EXCHANGE_GRAZING_ROOTS 2")
        if(NOT ${report} MATCHES "${pattern}")
            message(FATAL_ERROR "FMI ME grazing evidence missing: ${pattern}")
        endif()
    endforeach()
endforeach()
foreach(pattern
    "MODEL \"SMAVEFmi2MEGrazing\""
    "SAMPLE 0.20000000000000001 1 \"y\" 0.6[0-9.eE+-]*")
    if(NOT fmi2_report MATCHES "${pattern}")
        message(FATAL_ERROR "FMI 2 ME grazing evidence missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "MODEL \"SMAVEModelExchangeGrazing\""
    "SAMPLE 0.10000000000000001 1 \"x\" 0.5[0-9.eE+-]*")
    if(NOT fmi3_report MATCHES "${pattern}")
        message(FATAL_ERROR "FMI 3 ME grazing evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI 2/3 Model Exchange grazing, replay, and determinism passed")
