foreach(required IR ARTIFACT CERTIFICATE BUNDLE VALIDATION PERFORMANCE TRACES STATISTICS)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Operator replication artifact: ${required}")
    endif()
endforeach()
if(NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Operator replication OUTPUT is required")
endif()

file(READ "${IR}" ir)
foreach(pattern "MODEL \"OperatorPeriodicHelmholtz144\"" "UNKNOWNS 144" "SPARSITY_CSR 144 144 720")
    string(FIND "${ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication IR missing ${pattern}")
    endif()
endforeach()

file(READ "${ARTIFACT}" artifact)
foreach(pattern "SMAVE_LATENT_OPERATOR 1" "TRAINING 16"
                "PERMISSION \"full-state-corrected\""
                "QOI 3 \"x1\" \"x72\" \"x144\"")
    string(FIND "${artifact}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication artifact missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "VERSION \"(latent-operator-[0-9a-f]+)\"" version_match "${artifact}")
set(expert_version "${CMAKE_MATCH_1}")
string(REGEX MATCH "HASH \"([0-9a-f]+)\"" artifact_hash_match "${artifact}")
set(artifact_hash "${CMAKE_MATCH_1}")
if(NOT version_match OR NOT artifact_hash_match)
    message(FATAL_ERROR "Operator replication artifact identity is incomplete")
endif()

file(READ "${CERTIFICATE}" certificate)
foreach(pattern "EXPERT \"${expert_version}\"" "ARTIFACT \"${artifact_hash}\""
                "CELLS 1" "COUNTEREXAMPLES 0")
    string(FIND "${certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication certificate missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "HASH \"([0-9a-f]+)\"" certificate_hash_match "${certificate}")
set(certificate_hash "${CMAKE_MATCH_1}")
file(READ "${BUNDLE}" bundle)
foreach(pattern "\"${expert_version}\"" "\"${artifact_hash}\"" "\"${certificate_hash}\"")
    string(FIND "${bundle}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication Bundle missing ${pattern}")
    endif()
endforeach()

file(READ "${VALIDATION}" validation)
foreach(pattern "scenarios=64" "successful_scenarios=64" "admitted_invocations=64"
                "top_k_passes=64" "full_fallbacks=0" "original_solver_failures=0"
                "erroneous_accepts=0" "confidence_target_met=1")
    string(FIND "${validation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication validation missing ${pattern}")
    endif()
endforeach()

file(GLOB_RECURSE traces "${TRACES}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 64)
    message(FATAL_ERROR "Operator replication expected 64 traces, got ${trace_count}")
endif()
foreach(trace IN LISTS traces)
    file(READ "${trace}" content)
    foreach(pattern "BLOCK block-1 CORRECTED_ACCEPT"
                    "ATTEMPT \"${expert_version}\" \"accepted\""
                    "SUMMARY direct=0 corrected=1 warm_start=0 fallback=0")
        string(FIND "${content}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Operator replication corrected path missing ${pattern}: ${trace}")
        endif()
    endforeach()
endforeach()

file(READ "${PERFORMANCE}" performance)
foreach(pattern "SMAVE_OPERATOR_BENCHMARK 3" "requests=64" "repetitions=100"
                "accepted=6400" "fallbacks=0" "failures=0" "same_accuracy=1"
                "candidate_qoi_within_tolerance=1"
                "online_speedup_semantics=paired-median-of-per-repetition-ratios")
    string(FIND "${performance}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication performance missing ${pattern}")
    endif()
endforeach()
file(READ "${STATISTICS}" statistics)
foreach(pattern "repetitions=100" "cold_semantics=first-measured-batch"
                "hot_semantics=remaining-measured-batches" "hot_repetitions=99"
                "runtime_setup_semantics=baseline-and-corrector-construction-plus-bundle-validation"
                "operator_setup_semantics=latent-expert-construction-from-in-memory-artifact"
                "peak_rss_semantics=process-lifetime-high-water-mark"
                "energy_available=0"
                "energy_source=unavailable-portable-process-counter"
                "bootstrap_resamples=10000" "stable_speedup=")
    string(FIND "${statistics}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Operator replication statistics missing ${pattern}")
    endif()
endforeach()
foreach(field cold_baseline_us cold_operator_us hot_baseline_median_us
              hot_operator_median_us runtime_setup_us operator_setup_us
              rss_before_bytes rss_after_setup_bytes peak_rss_bytes)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" field_match "${statistics}")
    if(NOT field_match OR CMAKE_MATCH_1 LESS 0.0)
        message(FATAL_ERROR "Operator replication statistics has invalid ${field}")
    endif()
    set(${field} "${CMAKE_MATCH_1}")
endforeach()
string(REGEX MATCH "paired_median_speedup=([0-9.eE+-]+)" speedup_match "${statistics}")
set(speedup "${CMAKE_MATCH_1}")
string(REGEX MATCH "bootstrap_95_lower=([0-9.eE+-]+)" lower_match "${statistics}")
set(lower "${CMAKE_MATCH_1}")
string(REGEX MATCH "bootstrap_95_upper=([0-9.eE+-]+)" upper_match "${statistics}")
set(upper "${CMAKE_MATCH_1}")
string(REGEX MATCH "stable_speedup=([01])" stable_match "${statistics}")
set(stable_speedup "${CMAKE_MATCH_1}")
string(REGEX MATCH "break_even_met=([01])" break_even_match "${performance}")
set(break_even_met "${CMAKE_MATCH_1}")
if(NOT speedup_match OR NOT lower_match OR NOT upper_match OR
   NOT stable_match OR NOT break_even_match)
    message(FATAL_ERROR "Operator replication statistics are incomplete")
endif()

file(WRITE "${OUTPUT}"
    "SMAVE_OPERATOR_REPLICATION 1\n"
    "family=periodic-nonsymmetric-five-point-helmholtz\n"
    "unknowns=144\n"
    "structural_nonzeros=720\n"
    "training_scenarios=16\n"
    "evaluation_scenarios=64\n"
    "repetitions=100\n"
    "bootstrap_samples=10000\n"
    "cold_baseline_us=${cold_baseline_us}\n"
    "cold_operator_us=${cold_operator_us}\n"
    "hot_repetitions=99\n"
    "hot_baseline_median_us=${hot_baseline_median_us}\n"
    "hot_operator_median_us=${hot_operator_median_us}\n"
    "runtime_setup_us=${runtime_setup_us}\n"
    "operator_setup_us=${operator_setup_us}\n"
    "rss_before_bytes=${rss_before_bytes}\n"
    "rss_after_setup_bytes=${rss_after_setup_bytes}\n"
    "peak_rss_bytes=${peak_rss_bytes}\n"
    "peak_rss_semantics=process-lifetime-high-water-mark\n"
    "energy_available=0\n"
    "energy_source=unavailable-portable-process-counter\n"
    "paired_median_speedup=${speedup}\n"
    "bootstrap_95_lower=${lower}\n"
    "bootstrap_95_upper=${upper}\n"
    "stable_speedup=${stable_speedup}\n"
    "break_even_met=${break_even_met}\n"
    "failures=0\n"
    "fallbacks=0\n"
    "gate_mismatches=0\n"
    "same_accuracy=1\n"
    "END\n")
message(STATUS "second Operator family safety and performance replication passed")
