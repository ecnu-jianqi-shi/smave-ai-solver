foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing event-iteration artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "event-iteration simulation is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern
    "STATES 3"
    "EVENTS 2"
    "__smave_pre_y")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "event-iteration IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "MAX_RESET_ERROR 0"
    "REFERENCE_ORDER_MATCHED 1"
    "REFERENCE_TIME_MATCHED 1"
    "EVENTS 2"
    "EVENT \"event-1\" 1[^\n]*\"y\" 1[^\n]*\"z\" 0"
    "EVENT \"event-2\" 1[^\n]*\"y\" 1[^\n]*\"z\" 10"
    "FINAL_STATE 3[^\n]*\"y\" 1[^\n]*\"z\" 10"
    "atomic fixed-point event iteration completed")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "event-iteration acceptance evidence missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Cascading event fixed-point, stable pre-state, rollback, and determinism gates passed")
