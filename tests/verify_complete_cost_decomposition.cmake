if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "complete-cost decomposition evidence is missing")
endif()

file(READ "${EVIDENCE}" content)
foreach(required
        "SMAVE_COMPLETE_COST_DECOMPOSITION 1"
        "contract=two-family-layered-complete-runtime-breakdown"
        "families=linear,nonlinear"
        "linear.full_acceptance_rate=1"
        "linear.fallbacks=0"
        "linear.failures=0"
        "linear.correction_runtime_dominant=1"
        "linear.production_corrector_minimum_full_acceptance_budget=0"
        "linear.production_corrector_budget0_acceptance_rate=1"
        "linear.production_corrector_sweep_failures=0"
        "nonlinear.full_acceptance_rate=1"
        "nonlinear.fallbacks=0"
        "nonlinear.failures=0"
        "nonlinear.correction_runtime_dominant=1"
        "nonlinear.production_corrector_minimum_full_acceptance_budget=2"
        "nonlinear.production_corrector_budget0_acceptance_rate=0"
        "nonlinear.production_corrector_budget2_acceptance_rate=1"
        "nonlinear.production_corrector_sweep_failures=0"
        "END")
    string(FIND "${content}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing complete-cost field: ${required}")
    endif()
endforeach()

string(REGEX MATCH "linear.correction_runtime_gate_share=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 0.9)
    message(FATAL_ERROR "linear correction/runtime gate is not dominant")
endif()

string(REGEX MATCH "nonlinear.correction_runtime_gate_share=([0-9.]+)" _ "${content}")
if(CMAKE_MATCH_1 LESS 0.9)
    message(FATAL_ERROR "nonlinear correction/runtime gate is not dominant")
endif()

string(REGEX MATCH
    "nonlinear.production_corrector_budget2_vs_budget0_complete_ratio=([0-9.]+)"
    _ "${content}")
if(CMAKE_MATCH_1 GREATER_EQUAL 1.0)
    message(FATAL_ERROR "nonlinear budget 2 does not improve over zero correction")
endif()

message(STATUS "complete-cost decomposition evidence passed")
