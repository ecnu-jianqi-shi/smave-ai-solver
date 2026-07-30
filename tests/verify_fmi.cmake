foreach(required IR REPEAT_IR REPORT REPEAT_REPORT INSPECT SMOKE REPEAT_SMOKE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${IR}" "${REPEAT_IR}"
    RESULT_VARIABLE ir_compare)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE report_compare)
if(NOT ir_compare EQUAL 0 OR NOT report_compare EQUAL 0)
    message(FATAL_ERROR "FMI import is not deterministic")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${SMOKE}" "${REPEAT_SMOKE}"
    RESULT_VARIABLE smoke_compare)
if(NOT smoke_compare EQUAL 0)
    message(FATAL_ERROR "FMI smoke reports are not deterministic")
endif()
file(READ "${IR}" ir)
file(READ "${REPORT}" report)
file(READ "${INSPECT}" inspect)
file(READ "${SMOKE}" smoke)
foreach(pattern
    "SMAVE_FMI_BLACKBOX_4"
    "FMI_VERSION \"3.0\""
    "MODEL \"SMAVEBlackbox\""
    "HOST_BINARY_CANDIDATE 1"
    "PERMISSIONS 1 1 0 0"
    "INTERFACE \"CoSimulation\" \"SMAVEBlackbox\""
    "VARIABLES 4")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI IR missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SMAVE_FMI_IMPORT_REPORT 1"
    "GENERATION_TOOL \"SMAVE C++20 fixture\""
    "DEFAULT_EXPERIMENT 4"
    "CAPABILITY \"CoSimulation\" \"canGetAndSetFMUState\" \"true\""
    "CAPABILITY \"CoSimulation\" \"canSerializeFMUState\" \"true\""
    "VARIABLE \"gain\" \"Float64\" 1 \"parameter\" \"tunable\" \"exact\" \"1\" \"2\" 0"
    "CAUSALITY \"input\" 2"
    "BINARY_PLATFORMS 1"
    "TRAJECTORY_PROXY_ALLOWED 1"
    "DIFFERENTIAL_TEST_ALLOWED 1"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0"
    "blackbox-degraded"
    "never loaded or executed")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI report missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "model: SMAVEBlackbox"
    "fmi_version: 3.0"
    "host_binary_candidate: 1"
    "equation_level_validation_allowed: 0"
    "direct_expert_allowed: 0")
    string(FIND "${inspect}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI inspect output missing: ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SMAVE_FMI_SMOKE_REPORT 1"
    "INTERFACE \"CoSimulation\" \"SMAVEBlackbox\""
    "SUCCESS 1"
    "END_TIME 0.29999999999999999"
    "STEP_SIZE 0.10000000000000001"
    "SAMPLES 4"
    "SAMPLE 0 1 \"y\" 6"
    "SAMPLE 0.10000000000000001 1 \"y\" 6.0999999999999996"
    "SAMPLE 0.30000000000000004 1 \"y\" 6.2999999999999998"
    "STATE_ROUNDTRIP_ATTEMPTED 1"
    "STATE_ROUNDTRIP_PASSED 1"
    "MAX_STATE_REPLAY_ERROR 0"
    "STATE_SERIALIZATION_ATTEMPTED 1"
    "STATE_SERIALIZATION_PASSED 1"
    "SERIALIZED_STATE_BYTES 32"
    "EQUATION_LEVEL_VALIDATION_ALLOWED 0"
    "DIRECT_EXPERT_ALLOWED 0")
    string(FIND "${smoke}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI smoke report missing: ${pattern}")
    endif()
endforeach()
message(STATUS "FMI archive metadata, platform inventory, permissions, and determinism passed")
