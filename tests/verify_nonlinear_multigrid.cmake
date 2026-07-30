foreach(required ARTIFACT CERTIFICATE ACCELERATED FALLBACK ACCELERATED_TRACE_DIR FALLBACK_TRACE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing nonlinear multigrid argument: ${required}")
    endif()
endforeach()
foreach(path "${ARTIFACT}" "${CERTIFICATE}" "${ACCELERATED}" "${FALLBACK}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing nonlinear multigrid artifact: ${path}")
    endif()
endforeach()
file(READ "${ARTIFACT}" artifact)
foreach(pattern
    "SMAVE_LEARNED_MULTIGRID 2"
    "TRAINING 3"
    "MODE 1"
    "SMOOTHER 1 1"
    "LEVELS 2"
    "LEVEL_OPERATOR 8 8"
    "LEVEL_PROLONGATION 8 4"
    "LEVEL_OPERATOR 4 4"
    "COARSE_INVERSE 4 4")
    string(FIND "${artifact}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid artifact missing: ${pattern}")
    endif()
endforeach()
file(READ "${CERTIFICATE}" certificate)
foreach(pattern "SMAVE_VERIFIED_CELLS 1" "CELLS 1" "COUNTEREXAMPLES 0")
    string(FIND "${certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid certificate missing: ${pattern}")
    endif()
endforeach()
file(READ "${ACCELERATED}" accelerated)
foreach(pattern "status: success" "path=CORRECTED_ACCEPT" "fallback_iterations=0" "krylov_iterations=")
    string(FIND "${accelerated}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid accelerated report missing: ${pattern}")
    endif()
endforeach()
file(READ "${FALLBACK}" fallback)
foreach(pattern "status: success" "path=FULL_FALLBACK" "fallback=1")
    string(FIND "${fallback}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid fallback report missing: ${pattern}")
    endif()
endforeach()
file(GLOB accelerated_traces "${ACCELERATED_TRACE_DIR}/*.trace")
file(GLOB fallback_traces "${FALLBACK_TRACE_DIR}/*.trace")
list(LENGTH accelerated_traces accelerated_count)
list(LENGTH fallback_traces fallback_count)
if(NOT accelerated_count EQUAL 1 OR NOT fallback_count EQUAL 1)
    message(FATAL_ERROR "nonlinear multigrid expected one trace per path")
endif()
file(READ "${accelerated_traces}" accelerated_trace)
foreach(pattern
    "CORRECTED_ACCEPT"
    "jacobian_preconditioner=1"
    "preconditioner=\"learned-multigrid-"
    "ATTEMPT \"learned-multigrid-"
    "\"accepted\"")
    string(FIND "${accelerated_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid accelerated trace missing: ${pattern}")
    endif()
endforeach()
file(READ "${fallback_traces}" fallback_trace)
foreach(pattern
    "FULL_FALLBACK"
    "\"rejected\" \"Newton Jacobian PCG failed, became non-SPD, or correction stalled\""
    "ATTEMPT \"original-damped-newton\" \"fallback\"")
    string(FIND "${fallback_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "nonlinear multigrid fallback trace missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Nonlinear Jacobian multigrid PCG, gate, non-SPD rejection, and original fallback passed")
