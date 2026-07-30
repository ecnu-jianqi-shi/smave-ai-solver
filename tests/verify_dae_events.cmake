foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing runtime DAE event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "runtime DAE event simulation is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern
    "SMAVE_INDEX1_DAE 4"
    "EVENTS 2"
    "EVENT \"event-1\""
    "EVENT \"event-2\"")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "runtime DAE event IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "FINAL_TIME 1.1"
    "EVENTS 2"
    "EVENT \"event-1\" 1[^\n]*\"z\" 1[^\n]*\"y\" 1.99"
    "EVENT \"event-2\" 1[^\n]*\"x\" 0[^\n]*\"z\" 1[^\n]*\"y\" 1.00")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "runtime DAE event acceptance evidence missing: ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "\nSTATE \"x\" ([0-9.eE+-]+)" state_match "${report}")
if(NOT state_match OR CMAKE_MATCH_1 LESS 0.09999999 OR CMAKE_MATCH_1 GREATER 0.10000001)
    message(FATAL_ERROR "runtime DAE event final state violates reference")
endif()
string(REGEX MATCH "\nALGEBRAIC \"y\" ([0-9.eE+-]+)" algebraic_match "${report}")
if(NOT algebraic_match OR CMAKE_MATCH_1 LESS 1.09999999 OR CMAKE_MATCH_1 GREATER 1.10000001)
    message(FATAL_ERROR "runtime DAE event algebraic projection violates reference")
endif()
foreach(metric MAX_GUARD_RESIDUAL MAX_EVENT_PROJECTION_RESIDUAL MAX_RESIDUAL)
    string(REGEX MATCH "${metric} ([0-9.eE+-]+)" metric_match "${report}")
    if(NOT metric_match OR CMAKE_MATCH_1 GREATER 0.00000001)
        message(FATAL_ERROR "runtime DAE event ${metric} exceeds gate")
    endif()
endforeach()
message(STATUS "Runtime DAE root localization, reset, manifold projection, and determinism gates passed")
