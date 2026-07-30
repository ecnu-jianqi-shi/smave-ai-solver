foreach(required IR REPORT SMOKE REPEAT_SMOKE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI 2 event artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${SMOKE}" "${REPEAT_SMOKE}"
    RESULT_VARIABLE smoke_compare)
if(NOT smoke_compare EQUAL 0)
    message(FATAL_ERROR "FMI 2 event smoke reports are not deterministic")
endif()
file(READ "${IR}" ir)
file(READ "${REPORT}" report)
file(READ "${SMOKE}" smoke)
foreach(pattern
    "FMI_VERSION \"2.0\""
    "MODEL \"SMAVEFmi2Event\""
    "INTERFACE \"CoSimulation\" \"SMAVEFmi2Event\" 2"
    "\"canHandleVariableCommunicationStepSize\" \"true\"")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 event IR missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "GENERATION_TOOL \"SMAVE C++20 FMI 2 event fixture\""
    "CAPABILITY \"CoSimulation\" \"canHandleVariableCommunicationStepSize\" \"true\""
    "blackbox-degraded")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 event import report missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0.10000000000000001 1 \"y\" 6.0999999999999996"
    "SAMPLE 0.20000000000000001 1 \"y\" 16.199999999999999"
    "SAMPLE 0.30000000000000004 1 \"y\" 16.300000000000001"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "DO_STEP_CALLS 5"
    "DISCARD_RECOVERIES 1"
    "communication-point event smoke and state replay completed")
    string(FIND "${smoke}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 event smoke report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI 2 Co-Simulation Discard event, continuation, replay and determinism passed")
