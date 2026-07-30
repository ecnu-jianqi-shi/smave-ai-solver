foreach(required ARTIFACT CERTIFICATE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Phase 2 lineage artifact: ${required}")
    endif()
endforeach()
file(READ "${ARTIFACT}" artifact)
file(READ "${CERTIFICATE}" certificate)
foreach(pattern
    "SMAVE_LINEAR_PC 2"
    "TRAINING_DATASET_ID \"phase2-preconditioner-training\""
    "TRAINING_DATASET_VERSION \"[0-9a-f]+\""
    "TRAINING_DATASET_MANIFEST_HASH \"[0-9a-f]+\"")
    if(NOT artifact MATCHES "${pattern}")
        message(FATAL_ERROR "Phase 2 preconditioner lineage missing ${pattern}")
    endif()
endforeach()
foreach(pattern
    "SMAVE_VERIFIED_CELLS 2"
    "TRAINING_DATASET_ID \"phase2-preconditioner-training\""
    "TRAINING_DATASET_VERSION \"[0-9a-f]+\""
    "TRAINING_DATASET_MANIFEST_HASH \"[0-9a-f]+\"")
    if(NOT certificate MATCHES "${pattern}")
        message(FATAL_ERROR "Phase 2 certificate lineage missing ${pattern}")
    endif()
endforeach()
string(REGEX MATCH "TRAINING_DATASET_VERSION \"([0-9a-f]+)\"" _ "${artifact}")
set(artifact_version "${CMAKE_MATCH_1}")
string(REGEX MATCH "TRAINING_DATASET_VERSION \"([0-9a-f]+)\"" _ "${certificate}")
set(certificate_version "${CMAKE_MATCH_1}")
if(NOT artifact_version STREQUAL certificate_version)
    message(FATAL_ERROR "Phase 2 artifact and certificate training versions differ")
endif()
message(STATUS "Phase 2 preconditioner artifact and certificate training lineage passed")
