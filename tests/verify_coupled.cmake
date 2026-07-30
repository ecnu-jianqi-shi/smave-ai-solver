file(READ "${REPORT}" report)
file(READ "${REPEAT_REPORT}" repeat_report)

if(NOT report STREQUAL repeat_report)
    message(FATAL_ERROR "coupled reports are not deterministic")
endif()

foreach(required
    "SUCCESS 1"
    "FINAL_TIME 2.5"
    "FINAL_MODE \"heating\""
    "FINAL_CONTINUOUS 1 \"x\" 0.5"
    "FINAL_DISCRETE 1 \"u\" 1"
    "CONTINUOUS_EVENTS 0"
    "SAMPLED_EVENTS 2"
    "SAMPLED_EVENT 2 1 \"switch-to-cooling\" \"heating\" \"cooling\""
    "SAMPLED_EVENT 4 2 \"switch-to-heating\" \"cooling\" \"heating\""
    "SAMPLES 6"
    "SAMPLE 0 0 \"heating\" \"heating\"")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "coupled report missing: ${required}")
    endif()
endforeach()

message(STATUS "Coupled continuous/sample scheduling, hold, reset, and determinism gates passed")
