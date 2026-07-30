foreach(required SMAVE SOURCE DIRECTORY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing index-2 CLI input: ${required}")
    endif()
endforeach()
file(REMOVE_RECURSE "${DIRECTORY}")
file(MAKE_DIRECTORY "${DIRECTORY}")
set(ir "${DIRECTORY}/model.index2")
set(assessment "${DIRECTORY}/assessment.txt")
set(report "${DIRECTORY}/report.txt")
execute_process(
    COMMAND "${SMAVE}" compile-index2-dae "${SOURCE}"
        --top IndexTwoConstraint --output "${ir}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "compile-index2-dae failed: ${output}${error}")
endif()
execute_process(
    COMMAND "${SMAVE}" assess-index2-dae "${ir}" --output "${assessment}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "assess-index2-dae failed: ${output}${error}")
endif()
execute_process(
    COMMAND "${SMAVE}" simulate-index2-dae "${ir}"
        --end 0.3 --max-step 0.1 --output "${report}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "simulate-index2-dae failed: ${output}${error}")
endif()
file(READ "${assessment}" assessment_text)
foreach(pattern
    "SMAVE_INDEX2_DAE_ASSESSMENT 1"
    "FAMILY \"hessenberg-index2-affine-constraint-dae\""
    "symbolic-first-constraint-differentiation"
    "hidden-jacobian-rank-gate"
    "TERMINAL_FALLBACK \"index2-dense-kkt-terminal-cpu-v1\"")
    string(FIND "${assessment_text}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "index-2 assessment missing: ${pattern}")
    endif()
endforeach()
file(READ "${report}" report_text)
foreach(pattern
    "SMAVE_INDEX2_DAE_REPORT 1"
    "SUCCESS 1"
    "SOLVER_BACKEND \"index2-differentiated-constraint-newton-cpu-v1\""
    "INITIAL_CONSTRAINT_RESIDUAL 0"
    "INITIAL_HIDDEN_RESIDUAL 0"
    "HIDDEN_RANK_CHECKS 4"
    "STATE \"q\" 0"
    "STATE \"v\" 1"
    "MULTIPLIER \"lambda\" -1"
    "original dynamics, affine constraints, differentiated hidden constraints, and rank gates passed")
    string(FIND "${report_text}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "index-2 report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Index-2 DAE compile, reduction, route, simulation, and gates passed")
