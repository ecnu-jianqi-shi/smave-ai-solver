foreach(required REPORT REPEAT_REPORT MIXED_REPORT MIXED_REPEAT_REPORT
    TRANSFORMED_REPORT TRANSFORMED_REPEAT_REPORT EVENT_REPORT EVENT_REPEAT_REPORT
    TIME_EVENT_REPORT TIME_EVENT_REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing SSP artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TRANSFORMED_REPORT}" "${TRANSFORMED_REPEAT_REPORT}"
    RESULT_VARIABLE transformed_comparison)
if(NOT transformed_comparison EQUAL 0)
    message(FATAL_ERROR "transformed SSP reports are not deterministic")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TIME_EVENT_REPORT}" "${TIME_EVENT_REPEAT_REPORT}"
    RESULT_VARIABLE time_event_comparison)
if(NOT time_event_comparison EQUAL 0)
    message(FATAL_ERROR "time-event SSP reports are not deterministic")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${EVENT_REPORT}" "${EVENT_REPEAT_REPORT}"
    RESULT_VARIABLE event_comparison)
if(NOT event_comparison EQUAL 0)
    message(FATAL_ERROR "event SSP reports are not deterministic")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${MIXED_REPORT}" "${MIXED_REPEAT_REPORT}"
    RESULT_VARIABLE mixed_comparison)
if(NOT mixed_comparison EQUAL 0)
    message(FATAL_ERROR "mixed-version SSP reports are not deterministic")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE comparison)
if(NOT comparison EQUAL 0)
    message(FATAL_ERROR "SSP reports are not deterministic")
endif()
file(READ "${REPORT}" report)
foreach(pattern
    "SMAVE_SSP_REPORT 5"
    "SYSTEM \"FeedForward\""
    "SUCCESS 1"
    "END_TIME 0.29999999999999999"
    "STEP_SIZE 0.10000000000000001"
    "COMMUNICATION_STEPS 3"
    "SIGNAL_EXCHANGES 4"
    "EVENT_MODE_ENTRIES 0"
    "DISCRETE_UPDATE_ITERATIONS 0"
    "TIME_EVENT_SPLITS 0"
    "TIME_EVENTS 0"
    "COMPONENTS 2"
    "CONNECTION \"A\" \"y\" \"B\" \"u\" \"\" \"\" 1 0 1 0"
    "STEP_ORDER 2 \"A\" \"B\""
    "SAMPLES 4"
    "SAMPLE 0 2 \"A.y\" 2 \"B.y\" 4"
    "SAMPLE 0.29999999999999999 2 \"A.y\" 2.2999999999999998 \"B.y\" 4.8999999999999995")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "SSP report missing: ${pattern}")
    endif()
endforeach()
file(READ "${TIME_EVENT_REPORT}" time_event_report)
foreach(pattern
    "SYSTEM \"EventFeedForward\""
    "SUCCESS 1"
    "COMMUNICATION_STEPS 3"
    "SIGNAL_EXCHANGES 5"
    "EVENT_MODE_ENTRIES 2"
    "DISCRETE_UPDATE_ITERATIONS 2"
    "TIME_EVENT_SPLITS 1"
    "TIME_EVENTS 1"
    "COMPONENT \"A\" \"resources/a.fmu\" \"3.0\" \"SMAVETimeEventCS\""
    "COMPONENT \"B\" \"resources/b.fmu\" \"2.0\" \"SMAVEFmi2Variable\""
    "SAMPLES 5"
    "SAMPLE 0.14999999999999999 2 \"A.y\" 22.149999999999999 \"B.y\" 44.449999999999996"
    "SAMPLE 0.29999999999999999 2 \"A.y\" 22.300000000000001 \"B.y\" 44.899999999999999")
    string(FIND "${time_event_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "time-event SSP report missing: ${pattern}")
    endif()
endforeach()
file(READ "${EVENT_REPORT}" event_report)
foreach(pattern
    "SYSTEM \"EventFeedForward\""
    "SUCCESS 1"
    "EVENT_MODE_ENTRIES 1"
    "DISCRETE_UPDATE_ITERATIONS 2"
    "COMPONENT \"A\" \"resources/a.fmu\" \"3.0\" \"SMAVEEventCS\""
    "COMPONENT \"B\" \"resources/b.fmu\" \"2.0\" \"SMAVEFmi2\""
    "STEP_ORDER 2 \"A\" \"B\""
    "SAMPLE 0 2 \"A.y\" 2 \"B.y\" 4"
    "SAMPLE 0.20000000000000001 2 \"A.y\" 22.199999999999999 \"B.y\" 44.600000000000001"
    "SAMPLE 0.29999999999999999 2 \"A.y\" 22.300000000000001 \"B.y\" 44.899999999999999")
    string(FIND "${event_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "event SSP report missing: ${pattern}")
    endif()
endforeach()
file(READ "${TRANSFORMED_REPORT}" transformed_report)
foreach(pattern
    "SYSTEM \"Transformed\""
    "SUCCESS 1"
    "COMPONENT \"A\" \"resources/a.fmu\" \"2.0\" \"SMAVEFmi2\""
    "COMPONENT \"B\" \"resources/b.fmu\" \"3.0\" \"SMAVEBlackbox\""
    "CONNECTION \"A\" \"y\" \"B\" \"u\" \"degC\" \"K\" 1 273.14999999999998 0.5 1"
    "SAMPLE 0 2 \"A.y\" 2 \"B.y\" 277.14999999999998"
    "SAMPLE 0.29999999999999999 2 \"A.y\" 2.2999999999999998 \"B.y\" 277.75")
    string(FIND "${transformed_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "transformed SSP report missing: ${pattern}")
    endif()
endforeach()
file(READ "${MIXED_REPORT}" mixed_report)
foreach(pattern
    "SYSTEM \"Mixed\""
    "SUCCESS 1"
    "COMPONENT \"A\" \"resources/a.fmu\" \"2.0\" \"SMAVEFmi2\""
    "COMPONENT \"B\" \"resources/b.fmu\" \"3.0\" \"SMAVEBlackbox\""
    "STEP_ORDER 2 \"A\" \"B\""
    "SAMPLE 0 2 \"A.y\" 2 \"B.y\" 4"
    "SAMPLE 0.29999999999999999 2 \"A.y\" 2.2999999999999998 \"B.y\" 4.8999999999999995")
    string(FIND "${mixed_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "mixed-version SSP report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "Restricted SSP 1.0 FMI 2/3 transformed/time-event feed-forward master gates passed")
