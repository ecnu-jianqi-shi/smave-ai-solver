foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing coupled-superdense artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "coupled-superdense reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "FINAL_MODE \"done\""
    "FINAL_CONTINUOUS 2 \"x\" 1 \"y\" 2"
    "FINAL_DISCRETE 1 \"u\" -1"
    "SUPERDENSE_MICROSTEPS 4"
    "MAX_SUPERDENSE_ITERATIONS 2"
    "CONTINUOUS_EVENTS 2"
    "SAMPLED_EVENTS 2"
    "SUPERDENSE_STEPS 4"
    "SUPERDENSE_STEP 1 1 0 \"sampled\" \"arm\""
    "SUPERDENSE_STEP 1 1 1 \"continuous\" \"event-1\""
    "SUPERDENSE_STEP 1 1 2 \"continuous\" \"event-2\""
    "SUPERDENSE_STEP 1 1 3 \"sampled\" \"finish\"")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "coupled-superdense report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Cross-domain superdense fixed-point and deterministic trace passed")
