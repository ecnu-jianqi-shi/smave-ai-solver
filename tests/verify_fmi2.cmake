foreach(required IR REPORT SMOKE REPEAT_SMOKE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI 2 artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${SMOKE}" "${REPEAT_SMOKE}"
    RESULT_VARIABLE smoke_compare)
if(NOT smoke_compare EQUAL 0)
    message(FATAL_ERROR "FMI 2 smoke reports are not deterministic")
endif()
file(READ "${IR}" ir)
file(READ "${REPORT}" report)
file(READ "${SMOKE}" smoke)
foreach(pattern
    "FMI_VERSION \"2.0\""
    "MODEL \"SMAVEFmi2\""
    "HOST_BINARY_CANDIDATE 1"
    "INTERFACE \"CoSimulation\" \"SMAVEFmi2\""
    "VARIABLE \"gain\" \"Real\""
    "PERMISSIONS 1 1 0 0")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 IR missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "GENERATION_TOOL \"SMAVE C++20 FMI 2 fixture\""
    "CAPABILITY \"CoSimulation\" \"canGetAndSetFMUstate\" \"true\""
    "CAPABILITY \"CoSimulation\" \"canSerializeFMUstate\" \"true\""
    "blackbox-degraded")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 import report missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SMAVE_FMI_SMOKE_REPORT 1"
    "INTERFACE \"CoSimulation\" \"SMAVEFmi2\""
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0 1 \"y\" 6"
    "SAMPLE 0.30000000000000004 1 \"y\" 6.2999999999999998"
    "STATE_ROUNDTRIP_ATTEMPTED 1"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "STATE_SERIALIZATION_ATTEMPTED 1"
    "STATE_SERIALIZATION_PASSED 1"
    "SERIALIZED_STATE_BYTES 32"
    "DO_STEP_CALLS 4"
    "opt-in FMI 2.0 Co-Simulation fixed-step smoke")
    string(FIND "${smoke}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 smoke report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI 2 native Co-Simulation lifecycle and serialized replay passed")
