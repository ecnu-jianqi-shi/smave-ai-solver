if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "process gate scaling evidence is missing")
endif()

file(READ "${EVIDENCE}" evidence)
foreach(pattern
        "SMAVE_GATE_PROCESS_SCALING 1"
        "contract=process-isolated-fused-original-equation-gate-worker-scaling"
        "process_model=posix-fork-pipe"
        "input_transport=fork-inherited-copy-on-write"
        "result_transport=pipe-summary"
        "fresh_processes_per_measurement=1"
        "process_launch_and_wait_timed=1"
        "request_serialization_timed=0"
        "network_transport=0"
        "same_host_only=1"
        "families=linear,nonlinear"
        "workers=1,2,4,8"
        "warmup_configurations_per_family=4"
        "measured_configurations_per_family=120"
        "child_processes_per_family=465"
        "total_child_processes=930"
        "measured_gate_evaluations_per_family=249600"
        "total_measured_gate_evaluations=499200"
        "bootstrap_resamples=10000"
        "bootstrap_seed=20260724"
        "strict_equivalence=1")
    string(FIND "${evidence}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "process gate scaling evidence missing ${pattern}")
    endif()
endforeach()

foreach(family linear nonlinear)
    foreach(pattern
            "${family}.requests_per_repetition=2080"
            "${family}.repetitions=30"
            "${family}.decision_mismatches=0"
            "${family}.residual_mismatches=0")
        string(FIND "${evidence}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "process gate scaling evidence missing ${pattern}")
        endif()
    endforeach()
    foreach(worker 1 2 4 8)
        foreach(field total_median_us paired_speedup bootstrap_95_lower bootstrap_95_upper)
            string(REGEX MATCH
                "${family}\\.worker_${worker}\\.${field}=([0-9.eE+-]+)"
                match "${evidence}")
            if(NOT match)
                message(FATAL_ERROR
                    "process gate scaling evidence missing ${family} worker ${worker} ${field}")
            endif()
        endforeach()
    endforeach()
endforeach()

string(REGEX MATCH
    "linear\\.worker_8\\.bootstrap_95_lower=([0-9.eE+-]+)"
    linear_lower_match "${evidence}")
set(linear_lower "${CMAKE_MATCH_1}")
string(REGEX MATCH
    "nonlinear\\.worker_8\\.bootstrap_95_upper=([0-9.eE+-]+)"
    nonlinear_upper_match "${evidence}")
set(nonlinear_upper "${CMAKE_MATCH_1}")
if(NOT linear_lower_match OR NOT linear_lower GREATER 1.0)
    message(FATAL_ERROR "linear process scaling lower bound no longer exceeds one")
endif()
if(NOT nonlinear_upper_match OR NOT nonlinear_upper LESS 1.0)
    message(FATAL_ERROR "nonlinear process regression upper bound no longer below one")
endif()

message(STATUS "same-host process-isolated gate scaling evidence passed")
