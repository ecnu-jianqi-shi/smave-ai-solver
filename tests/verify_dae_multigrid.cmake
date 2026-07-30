foreach(required ARTIFACT HIERARCHY REPORT REPEAT_REPORT FALLBACK_REPORT REPEAT_FALLBACK_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing DAE multigrid artifact: ${required}")
    endif()
endforeach()
file(READ "${ARTIFACT}" artifact)
file(READ "${HIERARCHY}" hierarchy)
foreach(pattern
    "SMAVE_DAE_MULTIGRID 2"
    "TRAINING_DATASET_ID \"dae-multigrid-training\""
    "TRAINING_DATASET_VERSION \"[0-9a-f]+\""
    "TRAINING_DATASET_MANIFEST_HASH \"[0-9a-f]+\"")
    if(NOT artifact MATCHES "${pattern}")
        message(FATAL_ERROR "DAE wrapper training lineage missing ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SMAVE_LEARNED_MULTIGRID 3"
    "TRAINING_DATASET_ID \"dae-multigrid-training\""
    "TRAINING_DATASET_VERSION \"[0-9a-f]+\""
    "TRAINING_DATASET_MANIFEST_HASH \"[0-9a-f]+\"")
    if(NOT hierarchy MATCHES "${pattern}")
        message(FATAL_ERROR "DAE hierarchy training lineage missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "TRAINING_DATASET_VERSION \"([0-9a-f]+)\"" _ "${artifact}")
set(wrapper_version "${CMAKE_MATCH_1}")
string(REGEX MATCH "TRAINING_DATASET_VERSION \"([0-9a-f]+)\"" _ "${hierarchy}")
set(hierarchy_version "${CMAKE_MATCH_1}")
if(NOT wrapper_version STREQUAL hierarchy_version)
    message(FATAL_ERROR "DAE wrapper and hierarchy training versions differ")
endif()
file(READ "${REPORT}" report)
file(READ "${REPEAT_REPORT}" repeat_report)
file(READ "${FALLBACK_REPORT}" fallback_report)
file(READ "${REPEAT_FALLBACK_REPORT}" repeat_fallback_report)
if(NOT report STREQUAL repeat_report)
    message(FATAL_ERROR "accelerated DAE multigrid reports are not deterministic")
endif()
if(NOT fallback_report STREQUAL repeat_fallback_report)
    message(FATAL_ERROR "DAE multigrid fallback reports are not deterministic")
endif()
foreach(required
    "SUCCESS 1"
    "LEARNED_PRECONDITIONED_STEPS 3"
    "LEARNED_PRECONDITIONED_NEWTON_ITERATIONS 3"
    "LEARNED_REJECTIONS 0"
    "DENSE_STEP_FALLBACKS 0"
    "ALGEBRAIC_RANK_CHECKS 4")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "accelerated DAE report missing: ${required}")
    endif()
endforeach()
string(REGEX MATCH "LEARNED_KRYLOV_ITERATIONS ([0-9]+)" krylov_match "${report}")
if(NOT krylov_match OR CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "accelerated DAE report lacks Krylov evidence")
endif()
string(REGEX MATCH "MAX_RESIDUAL ([0-9.eE+-]+)" residual_match "${report}")
if(NOT residual_match OR CMAKE_MATCH_1 GREATER 0.00000001)
    message(FATAL_ERROR "accelerated DAE residual exceeds gate")
endif()
foreach(required
    "SUCCESS 1"
    "LEARNED_PRECONDITIONED_STEPS 0"
    "LEARNED_REJECTIONS 2"
    "DENSE_STEP_FALLBACKS 2"
    "STEPS 2")
    string(FIND "${fallback_report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "DAE fallback report missing: ${required}")
    endif()
endforeach()
string(REGEX MATCH "MAX_RESIDUAL ([0-9.eE+-]+)" fallback_residual_match "${fallback_report}")
if(NOT fallback_residual_match OR CMAKE_MATCH_1 GREATER 0.00000001)
    message(FATAL_ERROR "DAE dense fallback residual exceeds gate")
endif()
message(STATUS "DAE joint Jacobian multigrid, PCG gates, OOD rejection, dense retry, and determinism passed")
