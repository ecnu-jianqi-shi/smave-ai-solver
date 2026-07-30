foreach(required SMAVE FMI2_FMU FMI3_FMU INVALID_DYNAMIC_FMU INVALID_CLOCK_FMU FMI2_REPORT FMI2_REPEAT_REPORT FMI3_REPORT FMI3_REPEAT_REPORT)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing FMI scalar-type artifact: ${required}")
    endif()
endforeach()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${INVALID_DYNAMIC_FMU}"
        --end 0.2 --step 0.1 --array-input dynamic_vector=1,2,3
        --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-invalid-dynamic-report.txt"
    RESULT_VARIABLE invalid_dynamic_result
    ERROR_VARIABLE invalid_dynamic_error)
if(invalid_dynamic_result EQUAL 0 OR
   NOT invalid_dynamic_error MATCHES "fixed unsigned scalar structuralParameter start")
    message(FATAL_ERROR
        "unresolved dynamic array dimension was not rejected: ${invalid_dynamic_error}")
endif()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${INVALID_CLOCK_FMU}"
        --end 0.2 --step 0.1 --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-invalid-clock-report.txt"
    RESULT_VARIABLE invalid_clock_result
    ERROR_VARIABLE invalid_clock_error)
if(invalid_clock_result EQUAL 0 OR
   NOT invalid_clock_error MATCHES "Clock interval/shift is invalid")
    message(FATAL_ERROR
        "invalid Clock interval was not rejected: ${invalid_clock_error}")
endif()
foreach(version FMI2 FMI3)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${${version}_REPORT}" "${${version}_REPEAT_REPORT}"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR "${version} scalar-type reports are not deterministic")
    endif()
endforeach()
file(READ "${FMI2_REPORT}" fmi2_report)
foreach(pattern
    "SMAVE_FMI_SMOKE_REPORT 2"
    "MODEL \"SMAVEFmi2MEScalarTypes\""
    "SUCCESS 1"
    "SAMPLE 0 4 \"count_out\" 5 \"flag_out\" 0 \"mode_out\" 3 \"y\" 0"
    "STRING_SAMPLE 0 1 \"label_out\" \"alpha-fmi2\""
    "STATE_ROUNDTRIP_PASSED 1")
    string(FIND "${fmi2_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 2 scalar report missing: ${pattern}")
    endif()
endforeach()
file(READ "${FMI3_REPORT}" fmi3_report)
foreach(pattern
    "SMAVE_FMI_SMOKE_REPORT 2"
    "MODEL \"SMAVEModelExchangeScalarTypes\""
    "SUCCESS 1"
    "SAMPLE 0 13 \"clock_out\" 0 \"count_out\" 8 \"flag_out\" 1 \"float32_out\" 2.5 \"int16_out\" -4 \"int64_out\" -7 \"int8_out\" -2 \"mode_out\" 3 \"uint16_out\" 7 \"uint32_out\" 8 \"uint64_out\" 10 \"uint8_out\" 5 \"x\" 1"
    "ARRAY_SAMPLE 0 5 \"boolean_array_out\" 3 1 0 1 \"dynamic_vector_out\" 3 3 6 9 \"float32_array_out\" 3 1.5 2.5 3.5 \"int32_array_out\" 3 8 10 14 \"vector_out\" 3 2 4 6"
    "STRING_ARRAY_SAMPLE 0 1 \"string_array_out\" 3 \"alpha,beta=gamma-array\" \"-array\" \"omega-array\""
    "STRING_SAMPLE 0 1 \"label_out\" \"beta-fmi3\""
    "BINARY_SAMPLE 0 1 \"blob_out\" \"ff5a00\""
    "BINARY_ARRAY_SAMPLE 0 1 \"binary_array_out\" 3 \"\" \"ff5a\" \"00\""
    "CLOCK_INTERVALS 1 \"clock_out\" 0.25"
    "CLOCK_SHIFTS 1 \"clock_out\" 0.050000000000000003"
    "CLOCK_INTERVAL_QUALIFIERS 1 \"clock_out\" \"changed\""
    "STATE_ROUNDTRIP_PASSED 1")
    string(FIND "${fmi3_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "FMI 3 scalar report missing: ${pattern}")
    endif()
endforeach()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${FMI3_FMU}"
        --end 0.2 --step 0.1 --input "${fmi3_base}"
        --string-array-input string_array=one
        --string-array-input string_array=two
        --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-string-array-report.txt"
    RESULT_VARIABLE short_string_array_result
    ERROR_VARIABLE short_string_array_error)
if(short_string_array_result EQUAL 0 OR
   NOT short_string_array_error MATCHES "String array input element count")
    message(FATAL_ERROR "short String array was not rejected: ${short_string_array_error}")
endif()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${FMI3_FMU}"
        --end 0.2 --step 0.1 --input "${fmi3_base}"
        --binary-array-input binary_array=00
        --binary-array-input binary_array=0g
        --binary-array-input binary_array=ff
        --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-binary-array-report.txt"
    RESULT_VARIABLE invalid_binary_array_result
    ERROR_VARIABLE invalid_binary_array_error)
if(invalid_binary_array_result EQUAL 0 OR
   NOT invalid_binary_array_error MATCHES "non-hexadecimal")
    message(FATAL_ERROR "invalid Binary array hex was not rejected: ${invalid_binary_array_error}")
endif()
foreach(case
    "vector=1,2|element count"
    "vector=1,2,3,4|element count"
    "vector=1,nan,3|NaN/Inf"
    "int32_array=1.5,2,3|integer input"
    "boolean_array=0,2,1|Boolean input"
    "float32_array=1e39,2,3|Float32 array input")
    string(REPLACE "|" ";" fields "${case}")
    list(GET fields 0 array_input)
    list(GET fields 1 expected)
    execute_process(
        COMMAND "${SMAVE}" smoke-fmu-me "${FMI3_FMU}"
            --end 0.2 --step 0.1 --input "${fmi3_base}"
            --array-input "${array_input}" --allow-native-execution
            --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-array-report.txt"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "array input gate failed for ${array_input}: ${error}")
    endif()
endforeach()
foreach(case
    "${FMI3_FMU}|blob=0|even-length hexadecimal"
    "${FMI3_FMU}|blob=0g|non-hexadecimal"
    "${FMI2_FMU}|blob=00|does not define Binary")
    string(REPLACE "|" ";" fields "${case}")
    list(GET fields 0 fmu)
    list(GET fields 1 binary_input)
    list(GET fields 2 expected)
    execute_process(
        COMMAND "${SMAVE}" smoke-fmu-me "${fmu}"
            --end 0.2 --step 0.1 --input "${fmi3_base}"
            --binary-input "${binary_input}" --allow-native-execution
            --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-binary-report.txt"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "Binary input gate failed for ${binary_input}: ${error}")
    endif()
endforeach()
set(fmi3_base "gain=1,count=1,flag=0,float32_value=1,int8_value=1,uint8_value=1,int16_value=1,uint16_value=1,uint32_value=1,int64_value=1,uint64_value=1,mode=1")
foreach(case
    "${FMI2_FMU}|rate=1,count=1.5,flag=1,mode=1|integer input"
    "${FMI2_FMU}|rate=1,count=1,flag=2,mode=1|Boolean input"
    "${FMI2_FMU}|rate=1,count=1,flag=1,mode=1.5|integer input"
    "${FMI3_FMU}|${fmi3_base},float32_value=1e39|Float32 input"
    "${FMI3_FMU}|${fmi3_base},int8_value=128|integer input"
    "${FMI3_FMU}|${fmi3_base},uint8_value=-1|integer input"
    "${FMI3_FMU}|${fmi3_base},int16_value=32768|integer input"
    "${FMI3_FMU}|${fmi3_base},uint16_value=-1|integer input"
    "${FMI3_FMU}|${fmi3_base},count=1.5|integer input"
    "${FMI3_FMU}|${fmi3_base},uint32_value=-1|integer input"
    "${FMI3_FMU}|${fmi3_base},int64_value=9.223372036854776e18|integer input"
    "${FMI3_FMU}|${fmi3_base},uint64_value=-1|integer input"
    "${FMI3_FMU}|${fmi3_base},mode=1.5|integer input"
    "${FMI3_FMU}|${fmi3_base},flag=2|Boolean input"
    "${FMI3_FMU}|${fmi3_base},clock_value=2|Boolean input"
    "${FMI3_FMU}|${fmi3_base},int64_value=9007199254740992|exactly representable"
    "${FMI3_FMU}|${fmi3_base},uint64_value=9007199254740992|exactly representable")
    string(REPLACE "|" ";" fields "${case}")
    list(GET fields 0 fmu)
    list(GET fields 1 inputs)
    list(GET fields 2 expected)
    execute_process(
        COMMAND "${SMAVE}" smoke-fmu-me "${fmu}"
            --end 0.2 --step 0.1 --input "${inputs}" --allow-native-execution
            --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-scalar-report.txt"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "scalar input/output gate failed for ${inputs}: ${error}")
    endif()
endforeach()
foreach(case
    "${FMI2_FMU}|rate=1,count=1,flag=0,mode=1|count=text|supported type"
    "${FMI3_FMU}|${fmi3_base}|count=text|supported type")
    string(REPLACE "|" ";" fields "${case}")
    list(GET fields 0 fmu)
    list(GET fields 1 inputs)
    list(GET fields 2 string_inputs)
    list(GET fields 3 expected)
    execute_process(
        COMMAND "${SMAVE}" smoke-fmu-me "${fmu}"
            --end 0.2 --step 0.1 --input "${inputs}"
            --string-input "${string_inputs}" --allow-native-execution
            --output "${CMAKE_CURRENT_BINARY_DIR}/unexpected-string-report.txt"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "String type gate failed for ${string_inputs}: ${error}")
    endif()
endforeach()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${FMI2_FMU}"
        --end 0.2 --step 0.1 --input rate=1,count=1,flag=0,mode=1
        --string-input "label=" --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/fmi2-empty-string-report.txt"
    RESULT_VARIABLE empty_result)
if(NOT empty_result EQUAL 0)
    message(FATAL_ERROR "FMI 2 empty String input was rejected")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/fmi2-empty-string-report.txt" empty_report)
string(FIND "${empty_report}" "STRING_SAMPLE 0 1 \"label_out\" \"-fmi2\"" empty_found)
if(empty_found EQUAL -1)
    message(FATAL_ERROR "FMI 2 empty String output was not preserved")
endif()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${FMI3_FMU}"
        --end 0.2 --step 0.1 --input "${fmi3_base}"
        --string-input "label=alpha,beta=gamma" --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/fmi3-punctuation-string-report.txt"
    RESULT_VARIABLE punctuation_result)
if(NOT punctuation_result EQUAL 0)
    message(FATAL_ERROR "FMI 3 punctuation String input was rejected")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/fmi3-punctuation-string-report.txt" punctuation_report)
string(FIND "${punctuation_report}"
    "STRING_SAMPLE 0 1 \"label_out\" \"alpha,beta=gamma-fmi3\"" punctuation_found)
if(punctuation_found EQUAL -1)
    message(FATAL_ERROR "FMI 3 punctuation String output was not preserved")
endif()
execute_process(
    COMMAND "${SMAVE}" smoke-fmu-me "${FMI3_FMU}"
        --end 0.2 --step 0.1 --input "${fmi3_base}"
        --binary-input "blob=" --allow-native-execution
        --output "${CMAKE_CURRENT_BINARY_DIR}/fmi3-empty-binary-report.txt"
    RESULT_VARIABLE empty_binary_result)
if(NOT empty_binary_result EQUAL 0)
    message(FATAL_ERROR "FMI 3 empty Binary input was rejected")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/fmi3-empty-binary-report.txt" empty_binary_report)
string(FIND "${empty_binary_report}"
    "BINARY_SAMPLE 0 1 \"blob_out\" \"\"" empty_binary_found)
if(empty_binary_found EQUAL -1)
    message(FATAL_ERROR "FMI 3 empty Binary output was not preserved")
endif()
message(STATUS "FMI 2/3 scalar, FMI 3 typed arrays, and Clock interval/shift I/O passed")
