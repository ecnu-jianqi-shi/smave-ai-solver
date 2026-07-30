foreach(required ARTIFACT CERTIFICATE VALIDATION TRACE_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing learned multigrid argument: ${required}")
    endif()
endforeach()
foreach(path "${ARTIFACT}" "${CERTIFICATE}" "${VALIDATION}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing learned multigrid artifact: ${path}")
    endif()
endforeach()
file(READ "${ARTIFACT}" artifact)
foreach(pattern
    "SMAVE_LEARNED_MULTIGRID 3"
    "TRAINING_DATASET_ID \"multigrid-training\""
    "TRAINING_DATASET_VERSION"
    "TRAINING_DATASET_MANIFEST_HASH"
    "TRAINING 3"
    "SMOOTHER 1 1"
    "LEVELS 4"
    "LEVEL_OPERATOR 25 25"
    "LEVEL_PROLONGATION 25 13"
    "LEVEL_OPERATOR 13 13"
    "LEVEL_PROLONGATION 13 7"
    "LEVEL_OPERATOR 7 7"
    "LEVEL_PROLONGATION 7 4"
    "LEVEL_OPERATOR 4 4"
    "COARSE_INVERSE 4 4"
    "END")
    string(FIND "${artifact}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "multigrid artifact missing: ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "SMOOTHER 1 1 [^ ]+ ([^\n]+)" smoother "${artifact}")
if(NOT CMAKE_MATCH_1 LESS 1.0)
    message(FATAL_ERROR "multigrid contraction evidence is not below one")
endif()
file(READ "${CERTIFICATE}" certificate)
foreach(pattern "SMAVE_VERIFIED_CELLS 2" "TRAINING_DATASET_ID \"multigrid-training\"" "TRAINING_DATASET_VERSION" "TRAINING_DATASET_MANIFEST_HASH" "CELLS 1" "COUNTEREXAMPLES 0" "END")
    string(FIND "${certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "multigrid certificate missing: ${pattern}")
    endif()
endforeach()
file(READ "${VALIDATION}" validation)
foreach(pattern
    "scenarios=2"
    "successful_scenarios=2"
    "top_k_passes=2"
    "full_fallbacks=0"
    "erroneous_accepts=0"
    "top_k_target_met=1"
    "safety_target_met=1")
    string(FIND "${validation}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "multigrid validation missing: ${pattern}")
    endif()
endforeach()
file(GLOB_RECURSE traces "${TRACE_DIR}/*.trace")
list(LENGTH traces trace_count)
if(NOT trace_count EQUAL 2)
    message(FATAL_ERROR "expected two learned multigrid runtime traces")
endif()
foreach(trace IN LISTS traces)
    file(READ "${trace}" content)
    foreach(pattern "preconditioner=\"learned-multigrid-" "ATTEMPT \"learned-multigrid-" "\"accepted\"")
        string(FIND "${content}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "multigrid trace ${trace} missing: ${pattern}")
        endif()
    endforeach()
endforeach()
message(STATUS "Learned multigrid training, contraction, CEGIS, routing, gate, and fallback audit passed")
