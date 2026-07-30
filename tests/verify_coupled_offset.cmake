file(READ "${REPORT}" report)
file(READ "${REPEAT_REPORT}" repeat_report)

if(NOT report STREQUAL repeat_report)
    message(FATAL_ERROR "offset coupled reports are not deterministic")
endif()

foreach(required
    "SAMPLE_TIME 0.5"
    "SAMPLE_OFFSET 0.25"
    "SUCCESS 1"
    "FINAL_TIME 1"
    "FINAL_MODE \"cooling\""
    "FINAL_CONTINUOUS 1 \"x\" -0.49999999999999989"
    "FINAL_DISCRETE 1 \"u\" -1"
    "SAMPLED_EVENTS 1"
    "SAMPLED_EVENT 0 0.25 \"switch-to-cooling\" \"heating\" \"cooling\""
    "SAMPLES 2"
    "SAMPLE 0 0.25 \"heating\" \"cooling\" \"x\" 0.24999999999999997 \"u\" -1"
    "SAMPLE 1 0.75 \"cooling\" \"cooling\" \"x\" -0.24999999999999989 \"u\" -1")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "offset coupled report missing: ${required}")
    endif()
endforeach()

message(STATUS "Coupled sample offset, pre-offset hold, and determinism gates passed")
