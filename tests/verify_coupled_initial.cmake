foreach(required REPORT REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing coupled-initial artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "coupled-initial reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SUCCESS 1"
    "FINAL_MODE \"fired\""
    "FINAL_CONTINUOUS 2 \"x\" 1 \"y\" 2"
    "FINAL_DISCRETE 1 \"u\" -1"
    "CONTINUOUS_EVENTS 1"
    "CONTINUOUS_EVENT \"event-1\" 0"
    "SAMPLED_EVENTS 1"
    "SAMPLED_EVENT 0 0 \"initial-fire\" \"waiting\" \"fired\""
    "SAMPLES 2"
    "SAMPLE 0 0 \"waiting\" \"fired\" \"x\" 1 \"y\" 2 \"u\" -1")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "coupled-initial report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Coupled tick-zero continuous/sample initialization ordering passed")
