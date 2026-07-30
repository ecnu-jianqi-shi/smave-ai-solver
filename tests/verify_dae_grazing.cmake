foreach(required IR REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing DAE grazing artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "DAE grazing simulation is not deterministic")
endif()
file(READ "${IR}" ir)
foreach(pattern "SMAVE_INDEX1_DAE 4" "EVENTS 1" "EVENT \"event-1\"")
    if(NOT ir MATCHES "${pattern}")
        message(FATAL_ERROR "DAE grazing IR evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${REPORT}" report)
foreach(pattern
    "SMAVE_INDEX1_DAE_REPORT 6"
    "SUCCESS 1"
    "FINAL_TIME 1.5"
    "GRAZING_EVENTS 1"
    "EVENTS 1"
    "EVENT \"event-1\" [0-9.eE+-]+ 1 2 [^\n]*\"z\" 1 1 [^\n]*\"y\" [0-9.eE+-]+")
    if(NOT report MATCHES "${pattern}")
        message(FATAL_ERROR "DAE grazing acceptance evidence missing: ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "EVENT \"event-1\" ([0-9.eE+-]+)" event_match "${report}")
if(NOT event_match OR CMAKE_MATCH_1 LESS 0.89999999 OR CMAKE_MATCH_1 GREATER 0.90000001)
    message(FATAL_ERROR "DAE grazing event time is outside tolerance")
endif()
foreach(metric MAX_GUARD_RESIDUAL MAX_EVENT_PROJECTION_RESIDUAL MAX_RESIDUAL)
    string(REGEX MATCH "${metric} ([0-9.eE+-]+)" metric_match "${report}")
    if(NOT metric_match OR CMAKE_MATCH_1 GREATER 0.00000001)
        message(FATAL_ERROR "DAE grazing ${metric} exceeds gate")
    endif()
endforeach()
string(REGEX MATCH "MIN_ALGEBRAIC_RANK_MARGIN ([0-9.eE+-]+)" rank_match "${report}")
if(NOT rank_match OR CMAKE_MATCH_1 LESS 0.9)
    message(FATAL_ERROR "DAE grazing algebraic rank margin is insufficient")
endif()
message(STATUS "DAE grazing root, reset, manifold projection, rank, and determinism gates passed")
