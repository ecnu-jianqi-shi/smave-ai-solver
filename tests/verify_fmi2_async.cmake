foreach(required IR REPORT SMOKE REPEAT_SMOKE INVALID_TIMEOUT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI 2 async artifact: ${required}")
    endif()
endforeach()
file(READ "${INVALID_TIMEOUT}" invalid_timeout)
foreach(pattern "EXPECTED_FAILURE 1" "between 1 and 60000")
    string(FIND "${invalid_timeout}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 invalid timeout rejection missing: ${pattern}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${SMOKE}" "${REPEAT_SMOKE}"
    RESULT_VARIABLE smoke_compare)
if(NOT smoke_compare EQUAL 0)
    message(FATAL_ERROR "FMI 2 async smoke reports are not deterministic")
endif()
file(READ "${IR}" ir)
file(READ "${REPORT}" report)
file(READ "${SMOKE}" smoke)
foreach(pattern
    "FMI_VERSION \"2.0\""
    "MODEL \"SMAVEFmi2Async\""
    "INTERFACE \"CoSimulation\" \"SMAVEFmi2Async\" 2"
    "\"canRunAsynchronuously\" \"true\"")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 async IR missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "GENERATION_TOOL \"SMAVE C++20 FMI 2 async fixture\""
    "CAPABILITY \"CoSimulation\" \"canRunAsynchronuously\" \"true\""
    "blackbox-degraded")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 async import report missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SUCCESS 1"
    "SAMPLES 4"
    "SAMPLE 0.20000000000000001 1 \"y\" 6.2000000000000002"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "DO_STEP_CALLS 4"
    "PENDING_STEPS 1"
    "STEP_FINISHED_CALLBACKS 1"
    "CROSS_THREAD_CALLBACKS 1"
    "CANCELLED_STEPS 0"
    "ASYNCHRONOUS_TIMEOUT_MS 25"
    "asynchronous Pending smoke and state replay completed")
    string(FIND "${smoke}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 async smoke report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI 2 Co-Simulation Pending completion, callback, replay and determinism passed")
