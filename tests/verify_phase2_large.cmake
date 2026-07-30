foreach(required IR VALIDATION PERFORMANCE TRACES)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing large Phase 2 artifact: ${required}")
    endif()
endforeach()
file(READ "${IR}" ir)
foreach(pattern "BLOCKS 1" "UNKNOWNS 100")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "large Phase 2 IR missing ${pattern}")
    endif()
endforeach()
file(READ "${VALIDATION}" validation)
foreach(required
    "scenarios=64"
    "successful_scenarios=64"
    "admitted_invocations=64"
    "top_k_passes=64"
    "full_fallbacks=0"
    "original_solver_failures=0"
    "erroneous_accepts=0"
    "confidence_target_met=1")
    string(FIND "${validation}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "large Phase 2 validation missing ${required}")
    endif()
endforeach()
file(GLOB_RECURSE traces "${TRACES}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 64)
    message(FATAL_ERROR "large Phase 2 expected 64 validation traces, got ${trace_count}")
endif()
foreach(trace_path IN LISTS traces)
    file(READ "${trace_path}" trace)
    foreach(pattern "BLOCK block-1 CORRECTED_ACCEPT" " 1 0\n" "SUMMARY direct=0 corrected=1 warm_start=0 fallback=0")
        string(FIND "${trace}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "large Phase 2 corrected PCG path missing ${pattern}: ${trace_path}")
        endif()
    endforeach()
    if(NOT trace MATCHES "ATTEMPT \"learned-linear-pc-[0-9a-f]+\" \"accepted\"")
        message(FATAL_ERROR "large Phase 2 trace did not accept learned preconditioner: ${trace_path}")
    endif()
endforeach()
file(READ "${PERFORMANCE}" performance)
foreach(required
    "scenarios=64"
    "samples=1280"
    "baseline_failures=0"
    "accelerated_failures=0"
    "gate_mismatches=0"
    "same_accuracy=1"
    "p99_not_regressed=1"
    "bootstrap_samples=2000")
    string(FIND "${performance}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "large Phase 2 performance missing ${required}")
    endif()
endforeach()
string(REGEX MATCH "paired_win_rate=([0-9.eE+-]+)" win_rate_match "${performance}")
if(NOT win_rate_match)
    message(FATAL_ERROR "large Phase 2 paired win rate telemetry is missing")
endif()
string(REGEX MATCH "paired_speedup_ci95_lower=([0-9.eE+-]+)" lower_match "${performance}")
if(NOT lower_match OR NOT CMAKE_MATCH_1 GREATER 1.0)
    message(FATAL_ERROR "large Phase 2 paired speedup 95% CI does not prove acceleration")
endif()
string(REGEX MATCH "p99_speedup=([0-9.eE+-]+)" p99_match "${performance}")
if(NOT p99_match OR CMAKE_MATCH_1 LESS 1.0)
    message(FATAL_ERROR "large Phase 2 P99 regressed")
endif()
string(REGEX MATCH "baseline_mean_iterations=([0-9.eE+-]+)" baseline_match "${performance}")
set(baseline_iterations "${CMAKE_MATCH_1}")
string(REGEX MATCH "accelerated_mean_iterations=([0-9.eE+-]+)" accelerated_match "${performance}")
set(accelerated_iterations "${CMAKE_MATCH_1}")
if(NOT baseline_match OR NOT accelerated_match OR NOT accelerated_iterations LESS baseline_iterations)
    message(FATAL_ERROR "large Phase 2 Krylov iterations did not improve")
endif()
message(STATUS "Large Phase 2 SCC stable paired acceleration, P99, accuracy, confidence, and corrected-path gates passed")
