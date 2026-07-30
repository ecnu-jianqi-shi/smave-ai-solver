if(NOT DEFINED SOURCE OR NOT DEFINED OUTPUT OR NOT DEFINED H5DUMP)
    message(FATAL_ERROR "SOURCE, OUTPUT, and H5DUMP are required")
endif()

file(STRINGS "${SOURCE}/files.tsv" rows)
file(MAKE_DIRECTORY "${OUTPUT}/headers")
set(total 0)
set(verified 0)
set(case_lines "")
foreach(row IN LISTS rows)
    if(row MATCHES "^#" OR row STREQUAL "")
        continue()
    endif()
    string(REPLACE "\t" ";" fields "${row}")
    list(GET fields 0 relative_path)
    list(GET fields 2 expected_size)
    list(GET fields 3 expected_md5)
    set(path "${SOURCE}/${relative_path}")
    math(EXPR total "${total}+1")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing PDEBench file: ${relative_path}")
    endif()
    file(SIZE "${path}" actual_size)
    if(NOT actual_size EQUAL expected_size)
        message(FATAL_ERROR
            "truncated PDEBench file ${relative_path}: ${actual_size} != ${expected_size}")
    endif()
    file(MD5 "${path}" actual_md5)
    if(NOT actual_md5 STREQUAL expected_md5)
        message(FATAL_ERROR "MD5 mismatch for ${relative_path}")
    endif()
    string(REPLACE "/" "_" header_name "${relative_path}")
    execute_process(
        COMMAND "${H5DUMP}" -H "${path}"
        RESULT_VARIABLE h5_result
        OUTPUT_FILE "${OUTPUT}/headers/${header_name}.txt"
        ERROR_VARIABLE h5_error
        TIMEOUT 600)
    if(NOT h5_result EQUAL 0)
        message(FATAL_ERROR "HDF5 validation failed for ${relative_path}: ${h5_error}")
    endif()
    math(EXPR verified "${verified}+1")
    string(APPEND case_lines
        "CASE \"${relative_path}\" SIZE ${actual_size} MD5 \"${actual_md5}\" STATUS verified\n")
endforeach()

file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_PDEBENCH_DATA 1\n"
    "CASES ${total}\n"
    "VERIFIED ${verified}\n"
    "${case_lines}END\n")
message(STATUS "PDEBench authoritative files verified=${verified}/${total}")
