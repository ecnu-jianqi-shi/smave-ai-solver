foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing DAE initial-event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "DAE initial-event simulation is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern
    "SMAVE_INDEX1_DAE 4"
    "EVENTS 2"
    "__smave_pre_z"
    "__smave_pre_x")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "DAE initial-event IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "FINAL_TIME 0.1"
    "INITIAL_EVENTS 2"
    "INITIAL_EVENT \"event-1\""
    "INITIAL_EVENT \"event-2\""
    "INITIAL_STATE \"x\" 2"
    "INITIAL_STATE \"z\" 1"
    "INITIAL_ALGEBRAIC \"y\" 3")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "DAE initial-event acceptance evidence missing: ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "INITIAL_EVENT_PROJECTION_RESIDUAL ([0-9.eE+-]+)" residual_match "${report}")
if(NOT residual_match OR CMAKE_MATCH_1 GREATER 0.00000001)
    message(FATAL_ERROR "DAE initial-event projection residual exceeds gate")
endif()
message(STATUS "DAE consistent initialization, initial-event cascade, manifold projection, and determinism gates passed")
