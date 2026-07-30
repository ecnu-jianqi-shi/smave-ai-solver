foreach(required SMAVE SYMMETRIC_IR NONSYMMETRIC_IR SYMMETRIC_REPORT MEDIUM_REPORT NONSYMMETRIC_REPORT AI_REPORT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing ${required}")
    endif()
endforeach()

file(READ "${SYMMETRIC_REPORT}" symmetric)
foreach(pattern
    "SMAVE_EQUATION_ASSESSMENT 2"
    "FAMILY \"linear-structurally-symmetric\""
    "STRUCTURALLY_SYMMETRIC 1"
    "RUNTIME_SPD_GATE_REQUIRED 1"
    "NUMERIC_PROBE_AVAILABLE 1"
    "NUMERICALLY_SYMMETRIC 1"
    "NUMERICALLY_POSITIVE_DEFINITE 1"
    "REASON \"[^\"]*numeric probe confirms SPD\""
    "EXPERT \"pcg-ic0-cpu-v1\" ROLE \"linear_solver\""
    "BACKEND_CHAIN [0-9]+ [0-9]+ \"runtime-residual-constraint-gate\""
    "TERMINAL_FALLBACK \"original-damped-newton\""
    )
    if(NOT symmetric MATCHES "${pattern}")
        message(FATAL_ERROR "symmetric assessment missing ${pattern}")
    endif()
endforeach()
if(symmetric MATCHES "EXPERT \"gmres-(ilut|ilu0)-cpu-v1\"")
    message(FATAL_ERROR "SPD assessment retained an ineligible GMRES backend")
endif()

file(READ "${MEDIUM_REPORT}" medium)
foreach(pattern
    "UNKNOWNS 100"
    "SCALE_CLASS \"medium\""
    "DENSE_DIRECT_ELIGIBLE 0"
    "NUMERICALLY_POSITIVE_DEFINITE 1"
    "EXPERT \"pcg-ic0-cpu-v1\""
    "EXPERT \"sparse-ordered-threshold-pivot-cpu-v2\""
    )
    if(NOT medium MATCHES "${pattern}")
        message(FATAL_ERROR "medium sparse assessment missing ${pattern}")
    endif()
endforeach()
if(medium MATCHES "EXPERT \"dense-direct-cpu-v1\"")
    message(FATAL_ERROR "medium sparse assessment retained dense direct")
endif()

file(READ "${NONSYMMETRIC_REPORT}" nonsymmetric)
foreach(pattern
    "SMAVE_EQUATION_ASSESSMENT 2"
    "FAMILY \"linear-(sparse|dense)-nonsymmetric\""
    "STRUCTURALLY_SYMMETRIC 0"
    "NUMERIC_PROBE_AVAILABLE 1"
    "NUMERICALLY_SYMMETRIC 0"
    "NUMERICALLY_POSITIVE_DEFINITE 0"
    "REASON \"[^\"]*numeric probe confirms nonsymmetric matrix\""
    "EXPERT \"gmres-ilut-cpu-v1\" ROLE \"linear_solver\""
    "TERMINAL_FALLBACK \"original-damped-newton\""
    )
    if(NOT nonsymmetric MATCHES "${pattern}")
        message(FATAL_ERROR "nonsymmetric assessment missing ${pattern}")
    endif()
endforeach()
if(nonsymmetric MATCHES "EXPERT \"pcg-(ic0|jacobi)-cpu-v1\"")
    message(FATAL_ERROR "nonsymmetric assessment retained an ineligible PCG backend")
endif()

file(READ "${AI_REPORT}" ai)
foreach(pattern
    "FAMILY \"nonlinear-smooth\""
    "ROLE \"initializer\" PERMISSION \"warm_start\""
    "BACKEND_CHAIN [0-9]+ 0 \"affine-warm-start-[^\"]+\""
    "BACKEND_CHAIN [0-9]+ 1 \"original-damped-newton-corrector\""
    "BACKEND_CHAIN [0-9]+ 2 \"runtime-residual-constraint-gate\""
    "TERMINAL_FALLBACK \"original-damped-newton\""
    )
    if(NOT ai MATCHES "${pattern}")
        message(FATAL_ERROR "AI composite assessment missing ${pattern}")
    endif()
endforeach()

execute_process(
    COMMAND "${SMAVE}" assess-equation "${SYMMETRIC_IR}"
        --block missing-block --output "${SYMMETRIC_REPORT}.invalid"
    RESULT_VARIABLE invalid_status
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_stderr)
if(invalid_status EQUAL 0)
    message(FATAL_ERROR "assessment accepted an unknown block")
endif()
if(NOT invalid_stderr MATCHES "assessment block does not exist")
    message(FATAL_ERROR "unknown block rejection was not explicit: ${invalid_stderr}")
endif()

message(STATUS "Equation assessment CLI, backend chains, and negative gates passed")
