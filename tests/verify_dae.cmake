file(READ "${REPORT}" report)
file(READ "${REPEAT_REPORT}" repeat_report)
if(NOT report STREQUAL repeat_report)
    message(FATAL_ERROR "DAE reports are not deterministic")
endif()
foreach(required
    "SUCCESS 1"
    "FINAL_TIME 1"
    "STEPS 10"
    "ALGEBRAIC_RANK_CHECKS 11"
    "INITIAL_STATE \"x\" 1"
    "INITIAL_ALGEBRAIC \"y\" 2"
    "STATE \"x\""
    "ALGEBRAIC \"y\"")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "DAE report missing: ${required}")
    endif()
endforeach()
string(REGEX MATCH "MIN_ALGEBRAIC_RANK_MARGIN ([0-9.eE+-]+)" rank_match "${report}")
if(NOT rank_match OR CMAKE_MATCH_1 LESS 0.99)
    message(FATAL_ERROR "DAE algebraic rank margin is missing or too small")
endif()
string(REGEX MATCH "INITIALIZATION_RESIDUAL ([0-9.eE+-]+)" initial_residual_match "${report}")
if(NOT initial_residual_match OR CMAKE_MATCH_1 GREATER 0.00000001)
    message(FATAL_ERROR "DAE consistent initialization residual exceeds gate")
endif()
string(REGEX MATCH "MAX_RESIDUAL ([0-9.eE+-]+)" residual_match "${report}")
if(NOT residual_match)
    message(FATAL_ERROR "DAE report lacks residual metric")
endif()
if(CMAKE_MATCH_1 GREATER 0.00000001)
    message(FATAL_ERROR "DAE residual exceeds gate: ${CMAKE_MATCH_1}")
endif()
string(REGEX MATCH "\nSTATE \"x\" ([0-9.eE+-]+)" state_match "${report}")
if(NOT state_match OR CMAKE_MATCH_1 LESS 0.16150557 OR CMAKE_MATCH_1 GREATER 0.16150560)
    message(FATAL_ERROR "DAE final differential state violates Backward Euler reference")
endif()
string(REGEX MATCH "\nALGEBRAIC \"y\" ([0-9.eE+-]+)" algebraic_match "${report}")
if(NOT algebraic_match OR CMAKE_MATCH_1 LESS 0.32301114 OR CMAKE_MATCH_1 GREATER 0.32301120)
    message(FATAL_ERROR "DAE final algebraic state violates constraint reference")
endif()
message(STATUS "Index-1 DAE compilation, implicit stepping, algebraic constraint, and determinism gates passed")
