foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing continuous artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "continuous simulation is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern
    "SMAVE_CONTINUOUS_HYBRID 1"
    "STATE \"h\""
    "STATE \"v\""
    "EVENT \"event-1\" -1")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "continuous IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "MAX_RESET_ERROR 0"
    "REFERENCE_ORDER_MATCHED 1"
    "REFERENCE_TIME_MATCHED 1"
    "EVENTS 2")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "continuous acceptance evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Continuous ODE, zero-crossing, atomic reset, reference, and determinism gates passed")
