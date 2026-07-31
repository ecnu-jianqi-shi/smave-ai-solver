foreach(required EVALUATION VALIDATION TRACES)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Phase 4 artifact: ${required}")
    endif()
endforeach()
file(READ "${EVALUATION}" evaluation)
foreach(pattern
    "SMAVE_FAMILY_ROUTER_EVALUATION 3"
    "source_dataset_id=phase4-source-competition"
    "heldout_dataset_id=phase4-heldout-family"
    "embedding_similarity=0."
    "scenarios=64"
    "paired_samples=1280"
    "bootstrap_samples=2000"
    "fixed_failures=0"
    "calibrated_failures=0"
    "gate_mismatches=0"
    "same_accuracy=1"
    "calibrated_dangerous_misroutes=0"
    "improved=1"
    "safe=1")
    string(FIND "${evaluation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 4 evaluation missing ${pattern}")
    endif()
endforeach()
foreach(pattern
    "source_dataset_version=[0-9a-f]+"
    "source_dataset_manifest_hash=[0-9a-f]+"
    "heldout_dataset_version=[0-9a-f]+"
    "heldout_dataset_manifest_hash=[0-9a-f]+")
    if(NOT evaluation MATCHES "${pattern}")
        message(FATAL_ERROR "Phase 4 evaluation lineage missing ${pattern}")
    endif()
endforeach()
get_filename_component(phase4_directory "${EVALUATION}" DIRECTORY)
set(heldout_competition_path "${phase4_directory}/heldout-traces/heldout.competition")
if(NOT EXISTS "${heldout_competition_path}")
    message(FATAL_ERROR "Phase 4 heldout competition artifact is missing")
endif()
file(READ "${heldout_competition_path}" heldout_competition)
string(REGEX MATCH "heldout_competition_hash=([^\n]+)" evaluation_hash_match "${evaluation}")
set(evaluation_heldout_hash "${CMAKE_MATCH_1}")
string(REGEX MATCH "report_hash=([^\n]+)" heldout_hash_match "${heldout_competition}")
set(heldout_report_hash "${CMAKE_MATCH_1}")
if(NOT evaluation_hash_match OR NOT heldout_hash_match OR
   NOT evaluation_heldout_hash STREQUAL heldout_report_hash)
    message(FATAL_ERROR "Phase 4 heldout competition hash is missing or mismatched")
endif()
set(external_baselines_path "${phase4_directory}/heldout-traces/external-baselines.txt")
if(NOT EXISTS "${external_baselines_path}")
    message(FATAL_ERROR "Phase 4 external baseline report is missing")
endif()
file(READ "${external_baselines_path}" external_baselines)
foreach(pattern
    "SMAVE_EXTERNAL_BASELINES 1"
    "contract=paired-complete-runtime-external-vs-calibrated")
    string(FIND "${external_baselines}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 4 external baselines missing ${pattern}")
    endif()
endforeach()
# External sparse baselines depend on platform-specific libraries (SuperLU on
# Linux, Apple Accelerate on macOS). Require at least one available baseline
# and validate every baseline that appears in the report.
string(REGEX MATCH "entries=([0-9]+)" entries_match "${external_baselines}")
if(NOT entries_match OR NOT CMAKE_MATCH_1 GREATER 0)
    message(FATAL_ERROR "Phase 4 external baselines must report at least one entry")
endif()
foreach(backend "superlu-dgssv-cpu-v1" "accelerate-sparse-qr-cpu-v1")
    string(FIND "${external_baselines}" "BASELINE \"${backend}\"" backend_present)
    if(backend_present EQUAL -1)
        continue()
    endif()
    string(REGEX MATCH
        "BASELINE \"${backend}\"[^\n]*competition_fallbacks=0[^\n]*competition_failures=0[^\n]*competition_erroneous_accepts=0[^\n]*external_failures=0[^\n]*calibrated_failures=0[^\n]*gate_mismatches=0[^\n]*same_accuracy=1"
        external_match "${external_baselines}")
    if(NOT external_match)
        message(FATAL_ERROR "Phase 4 external baseline ${backend} failed accuracy/fallback gates")
    endif()
endforeach()
file(READ "${phase4_directory}/source.competition" source_competition)
file(READ "${phase4_directory}/source-repeat.competition" source_repeat_competition)
foreach(pattern
    "SMAVE_COMPETITION 4"
    "dataset_id=phase4-source-competition"
    "dataset_version=[0-9a-f]+"
    "dataset_manifest_hash=[0-9a-f]+")
    if(NOT source_competition MATCHES "${pattern}")
        message(FATAL_ERROR "Phase 4 source competition lineage missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "winner=([^\n]+)" source_winner_match "${source_competition}")
set(source_winner "${CMAKE_MATCH_1}")
string(REGEX MATCH "winner=([^\n]+)" repeat_winner_match "${source_repeat_competition}")
set(repeat_winner "${CMAKE_MATCH_1}")
if(NOT source_winner_match OR NOT repeat_winner_match OR
   NOT source_winner STREQUAL repeat_winner)
    message(FATAL_ERROR
        "Phase 4 competition winner is unstable: ${source_winner} vs ${repeat_winner}")
endif()
string(REGEX MATCH "paired_speedup_ci95_lower=([0-9.eE+-]+)" lower_match "${evaluation}")
if(NOT lower_match OR CMAKE_MATCH_1 LESS 1.01)
    message(FATAL_ERROR "Phase 4 paired speedup CI lower bound is below 1.01")
endif()

file(READ "${VALIDATION}" validation)
foreach(pattern
    "SMAVE_VALIDATION 3"
    "dataset_id=phase4-heldout-family"
    "scenarios=64"
    "successful_scenarios=64"
    "admitted_invocations=64"
    "top_k_passes=64"
    "full_fallbacks=0"
    "original_solver_failures=0"
    "erroneous_accepts=0"
    "confidence_target_met=1")
    string(FIND "${validation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 4 validation missing ${pattern}")
    endif()
endforeach()
foreach(pattern "dataset_version=[0-9a-f]+" "dataset_manifest_hash=[0-9a-f]+")
    if(NOT validation MATCHES "${pattern}")
        message(FATAL_ERROR "Phase 4 validation lineage missing ${pattern}")
    endif()
endforeach()
file(GLOB_RECURSE traces "${TRACES}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 64)
    message(FATAL_ERROR "Phase 4 expected 64 runtime traces, got ${trace_count}")
endif()
foreach(trace IN LISTS traces)
    file(READ "${trace}" content)
    foreach(pattern "STATUS success" "fallback=0")
        string(FIND "${content}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Phase 4 trace missing ${pattern}: ${trace}")
        endif()
    endforeach()
endforeach()
message(STATUS "Phase 4 held-out Router paired acceleration, safety, accuracy, and confidence gates passed; P99 retained as telemetry")
