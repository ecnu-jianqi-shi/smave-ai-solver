foreach(required SMAVE SOURCE DIRECTORY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing complementarity CLI input: ${required}")
    endif()
endforeach()
file(REMOVE_RECURSE "${DIRECTORY}")
file(MAKE_DIRECTORY "${DIRECTORY}")
set(ir "${DIRECTORY}/contact.lcp")
set(assessment "${DIRECTORY}/assessment.txt")
set(report "${DIRECTORY}/report.txt")
execute_process(
    COMMAND "${SMAVE}" compile-complementarity "${SOURCE}"
        --top ContactComplementarity --output "${ir}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "compile-complementarity failed: ${output}${error}")
endif()
execute_process(
    COMMAND "${SMAVE}" assess-complementarity "${ir}" --output "${assessment}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "assess-complementarity failed: ${output}${error}")
endif()
execute_process(
    COMMAND "${SMAVE}" solve-complementarity "${ir}" --output "${report}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "solve-complementarity failed: ${output}${error}")
endif()
file(READ "${assessment}" assessment_text)
foreach(pattern
    "SMAVE_COMPLEMENTARITY_ASSESSMENT 1"
    "FAMILY \"strongly-monotone-linear-complementarity\""
    "EXPERT \"projected-gauss-seidel-cpu-v1\""
    "EXPERT \"fischer-burmeister-newton-cpu-v1\""
    "TERMINAL_FALLBACK \"enumerated-active-set-terminal-cpu-v1\"")
    string(FIND "${assessment_text}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "complementarity assessment missing: ${pattern}")
    endif()
endforeach()
file(READ "${report}" report_text)
foreach(pattern
    "SMAVE_COMPLEMENTARITY_REPORT 1"
    "SUCCESS 1"
    "ACCEPTED_BACKEND \"projected-gauss-seidel-cpu-v1\""
    "original gap equations, inequalities, and complementarity gate passed"
    "PAIR \"lambda1\""
    "PAIR \"lambda2\""
    "PAIR \"lambda3\"")
    string(FIND "${report_text}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "complementarity report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Complementarity CLI compile, route, solve, and gate passed")
