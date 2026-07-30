foreach(required IR REPORT SMOKE REPEAT_SMOKE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI 2 ME artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${SMOKE}" "${REPEAT_SMOKE}"
    RESULT_VARIABLE smoke_compare)
if(NOT smoke_compare EQUAL 0)
    message(FATAL_ERROR "FMI 2 ME smoke reports are not deterministic")
endif()
file(READ "${IR}" ir)
file(READ "${REPORT}" report)
file(READ "${SMOKE}" smoke)
foreach(pattern
    "SMAVE_FMI_BLACKBOX_4"
    "FMI_VERSION \"2.0\""
    "MODEL \"SMAVEFmi2ME\""
    "INTERFACE \"ModelExchange\" \"SMAVEFmi2ME\""
    "VARIABLE \"der_x\" \"Real\" 2 \"local\" \"continuous\" \"calculated\" \"\" \"\" 0 1"
    "DERIVATIVE_ORDER 1 2")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 ME IR missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "GENERATION_TOOL \"SMAVE C++20 FMI 2 ME fixture\""
    "CAPABILITY \"ModelExchange\" \"canGetAndSetFMUstate\" \"true\""
    "CAPABILITY \"ModelExchange\" \"canSerializeFMUstate\" \"true\""
    "EVENT_INDICATORS 1")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 ME import report missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "INTERFACE \"ModelExchange\" \"SMAVEFmi2ME\""
    "SUCCESS 1"
    "SAMPLES 3"
    "SAMPLE 0 1 \"y\" 0"
    "SAMPLE 0.10000000000000001 1 \"y\" 1.5499999999996361"
    "SAMPLE 0.20000000000000001 1 \"y\" 1.6499999999996362"
    "STATE_ROUNDTRIP_ATTEMPTED 1"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "STATE_SERIALIZATION_ATTEMPTED 1"
    "STATE_SERIALIZATION_PASSED 1"
    "SERIALIZED_STATE_BYTES 35"
    "TIME_EVENT_SPLITS 4"
    "TIME_EVENTS 2"
    "MODEL_EXCHANGE_ROOTS 2"
    "EVENT_MODE_ENTRIES 4"
    "DISCRETE_UPDATE_ITERATIONS 5"
    "opt-in FMI 2.0 ModelExchange RK4/event smoke")
    string(FIND "${smoke}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 ME smoke report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI 2 Model Exchange RK4, root, time event, and serialized replay passed")
