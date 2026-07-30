foreach(required SMAVE VALID_FMU MULTI_FMU NO_CLOCK_FMU MISSING_PRIORITY_FMU
        INVALID_PREEMPTION_FMU REPORT REPEAT_REPORT MULTI_REPORT MULTI_REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Scheduled Execution artifact: ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${REPORT}" "${REPEAT_REPORT}"
    RESULT_VARIABLE report_compare)
if(NOT report_compare EQUAL 0)
    message(FATAL_ERROR "Scheduled Execution smoke report is not deterministic")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${MULTI_REPORT}" "${MULTI_REPEAT_REPORT}"
    RESULT_VARIABLE multi_report_compare)
if(NOT multi_report_compare EQUAL 0)
    message(FATAL_ERROR "multi-Clock Scheduled Execution report is not deterministic")
endif()

file(READ "${REPORT}" report)
foreach(pattern
    "INTERFACE \"ScheduledExecution\" \"SMAVEBlackbox\""
    "SUCCESS 1"
    "SAMPLES 3"
    "SAMPLE 0 1 \"y\" 6"
    "SAMPLE 0.25 1 \"y\" 6.25"
    "SAMPLE 0.5 1 \"y\" 6.5"
    "MODEL_PARTITION_ACTIVATIONS 3"
    "CLOCK_UPDATE_CALLBACKS 3"
    "LOCK_PREEMPTION_CALLBACKS 3"
    "UNLOCK_PREEMPTION_CALLBACKS 3"
    "CLOCK_INTERVALS 1 \"tick\" 0.25"
    "CLOCK_SHIFTS 1 \"tick\" 0"
    "CLOCK_PRIORITIES 1 \"tick\" 10")
    string(FIND "${report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Scheduled Execution report missing: ${pattern}")
    endif()
endforeach()

file(READ "${MULTI_REPORT}" multi_report)
foreach(pattern
    "SUCCESS 1"
    "SAMPLES 5"
    "MODEL_PARTITION_ACTIVATIONS 5"
    "PARTITION_ACTIVATION_ORDER 5 0 \"slow\" 47 10 0 \"fast\" 46 20 0.25 \"fast\" 46 20 0.5 \"slow\" 47 10 0.5 \"fast\" 46 20"
    "CLOCK_UPDATE_CALLBACKS 5"
    "LOCK_PREEMPTION_CALLBACKS 5"
    "UNLOCK_PREEMPTION_CALLBACKS 5"
    "CLOCK_INTERVALS 2 \"fast\" 0.25 \"slow\" 0.5"
    "CLOCK_SHIFTS 2 \"fast\" 0 \"slow\" 0"
    "CLOCK_PRIORITIES 2 \"fast\" 20 \"slow\" 10")
    string(FIND "${multi_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "multi-Clock Scheduled Execution report missing: ${pattern}")
    endif()
endforeach()

execute_process(
    COMMAND "${SMAVE}" smoke-fmu-se "${VALID_FMU}"
        --end 0.2 --interval 0.1 --allow-native-execution
        --output "${REPORT}.mismatch"
    RESULT_VARIABLE mismatch_result ERROR_VARIABLE mismatch_error)
if(mismatch_result EQUAL 0 OR NOT mismatch_error MATCHES "interval must match")
    message(FATAL_ERROR "Scheduled Execution interval mismatch was not rejected")
endif()

execute_process(
    COMMAND "${SMAVE}" smoke-fmu-se "${NO_CLOCK_FMU}"
        --end 0.5 --interval 0.25 --allow-native-execution
        --output "${REPORT}.no-clock"
    RESULT_VARIABLE clock_result ERROR_VARIABLE clock_error)
if(clock_result EQUAL 0 OR NOT clock_error MATCHES "at least one scalar input Clock")
    message(FATAL_ERROR "Scheduled Execution missing Clock was not rejected")
endif()

execute_process(
    COMMAND "${SMAVE}" smoke-fmu-se "${MISSING_PRIORITY_FMU}"
        --end 0.5 --interval 0.25 --allow-native-execution
        --output "${REPORT}.missing-priority"
    RESULT_VARIABLE priority_result ERROR_VARIABLE priority_error)
if(priority_result EQUAL 0 OR
        NOT priority_error MATCHES "input Clock requires priority")
    message(FATAL_ERROR "Scheduled Execution missing priority was not rejected")
endif()

execute_process(
    COMMAND "${SMAVE}" smoke-fmu-se "${INVALID_PREEMPTION_FMU}"
        --end 0.5 --interval 0.25 --allow-native-execution
        --output "${REPORT}.invalid-preemption"
    RESULT_VARIABLE preemption_result ERROR_VARIABLE preemption_error)
if(preemption_result EQUAL 0 OR NOT preemption_error MATCHES "callbacks are not balanced")
    message(FATAL_ERROR "Scheduled Execution callback imbalance was not rejected")
endif()

message(STATUS "FMI 3 Scheduled Execution periodic multi-Clock lifecycle, callbacks, ordering, and determinism passed")
