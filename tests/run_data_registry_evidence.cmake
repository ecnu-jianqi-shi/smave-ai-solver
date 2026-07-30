file(REMOVE_RECURSE "${ROOT}")
file(MAKE_DIRECTORY "${ROOT}/source/nested")
file(WRITE "${ROOT}/source/a.conf" "p=1\n")
file(WRITE "${ROOT}/source/nested/b.conf" "p=2\n")
execute_process(
    COMMAND "${SMAVE}" snapshot-data "${ROOT}/source"
        --dataset validation-scenarios --store "${ROOT}/store"
    RESULT_VARIABLE snapshot_result OUTPUT_FILE "${ROOT}/snapshot.txt")
if(NOT snapshot_result EQUAL 0)
    message(FATAL_ERROR "dataset snapshot failed")
endif()
execute_process(
    COMMAND "${SMAVE}" snapshot-data "${ROOT}/source"
        --dataset validation-scenarios --store "${ROOT}/store"
    RESULT_VARIABLE repeat_result OUTPUT_FILE "${ROOT}/repeat.txt")
if(NOT repeat_result EQUAL 0)
    message(FATAL_ERROR "idempotent dataset snapshot failed")
endif()
file(READ "${ROOT}/snapshot.txt" snapshot)
string(REGEX MATCH "version: ([0-9a-f]+)" _ "${snapshot}")
set(version "${CMAKE_MATCH_1}")
string(LENGTH "${version}" version_length)
if(NOT version_length EQUAL 64)
    message(FATAL_ERROR "snapshot did not produce a SHA-256 version")
endif()
execute_process(
    COMMAND "${SMAVE}" verify-data --dataset validation-scenarios
        --version "${version}" --store "${ROOT}/store"
    RESULT_VARIABLE verify_result OUTPUT_FILE "${ROOT}/verify.txt")
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR "authoritative dataset store did not verify")
endif()
file(COPY "${ROOT}/store" DESTINATION "${ROOT}/tampered")
file(APPEND "${ROOT}/tampered/store/datasets/validation-scenarios/${version}/a.conf"
    "tampered=1\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DSMAVE=${SMAVE}
        -DSTORE=${ROOT}/tampered/store
        -DVERSION=${version}
        -P "${CMAKE_CURRENT_LIST_DIR}/expect_data_tamper_failure.cmake"
    RESULT_VARIABLE tamper_result)
if(NOT tamper_result EQUAL 0)
    message(FATAL_ERROR "copied-store tamper evidence failed")
endif()
execute_process(
    COMMAND "${SMAVE}" verify-data --dataset validation-scenarios
        --version "${version}" --store "${ROOT}/store"
    RESULT_VARIABLE reverify_result OUTPUT_FILE "${ROOT}/reverify.txt")
if(NOT reverify_result EQUAL 0)
    message(FATAL_ERROR "authoritative dataset store was polluted by tamper test")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DSNAPSHOT=${ROOT}/snapshot.txt
        -DREPEAT=${ROOT}/repeat.txt
        -DVERIFY=${ROOT}/verify.txt
        -DSTORE=${ROOT}/store
        -P "${CMAKE_CURRENT_LIST_DIR}/verify_data_registry.cmake"
    RESULT_VARIABLE evidence_result)
if(NOT evidence_result EQUAL 0)
    message(FATAL_ERROR "data registry evidence verification failed")
endif()
message(STATUS "DATASET_VERSION ${version}")
message(STATUS "FILES 2")
message(STATUS "IDEMPOTENT 1")
message(STATUS "TAMPER_REJECTED 1")
message(STATUS "AUTHORITATIVE_REVERIFIED 1")
