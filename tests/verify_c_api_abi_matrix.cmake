if(NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR OR NOT DEFINED PREFIX OR
   NOT DEFINED C_COMPILER OR NOT DEFINED FROZEN_HOST OR NOT DEFINED CURRENT_HOST)
    message(FATAL_ERROR
        "BUILD_DIR, SOURCE_DIR, PREFIX, C_COMPILER, FROZEN_HOST and CURRENT_HOST are required")
endif()

file(MAKE_DIRECTORY "${BUILD_DIR}/c-api-abi-matrix")
execute_process(
    COMMAND "${CURRENT_HOST}"
    RESULT_VARIABLE current_result
    OUTPUT_VARIABLE current_stdout
    ERROR_VARIABLE current_stderr)
if(NOT current_result EQUAL 0)
    message(FATAL_ERROR "current header/current library host failed: ${current_stderr}")
endif()
execute_process(
    COMMAND "${FROZEN_HOST}"
    RESULT_VARIABLE frozen_result
    OUTPUT_VARIABLE frozen_stdout
    ERROR_VARIABLE frozen_stderr)
if(NOT frozen_result EQUAL 0)
    message(FATAL_ERROR "frozen v1 header/current library host failed: ${frozen_stderr}")
endif()

file(REMOVE_RECURSE "${PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "ABI matrix SDK install failed: ${install_stdout}${install_stderr}")
endif()
file(MAKE_DIRECTORY "${PREFIX}/bin")
if(APPLE)
    set(runtime_path "-Wl,-rpath,${PREFIX}/lib")
    set(installed_host "${PREFIX}/bin/smave-frozen-v1-host")
elseif(WIN32)
    message(FATAL_ERROR "ABI matrix installed host is not implemented for Windows yet")
else()
    set(runtime_path "-Wl,-rpath,${PREFIX}/lib")
    set(installed_host "${PREFIX}/bin/smave-frozen-v1-host")
endif()
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${SOURCE_DIR}/tests/abi/v1"
        "${SOURCE_DIR}/tests/c_api_abi_v1_host.c"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}" -o "${installed_host}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "frozen v1 installed host compile failed: ${compile_stdout}${compile_stderr}")
endif()
execute_process(
    COMMAND "${installed_host}"
    RESULT_VARIABLE installed_result
    OUTPUT_VARIABLE installed_stdout
    ERROR_VARIABLE installed_stderr)
if(NOT installed_result EQUAL 0)
    message(FATAL_ERROR "frozen v1 installed host failed: ${installed_stderr}")
endif()

foreach(marker
        "FROZEN_V1_HEADER 1"
        "CURRENT_LIBRARY 1"
        "EXTENDED_STRUCT_INPUT 1"
        "EXTENDED_STRUCT_OUTPUT 1"
        "UNKNOWN_TAIL_PRESERVED 1"
        "ABI_MISMATCH_REJECTED 1")
    string(FIND "${frozen_stdout}" "${marker}" frozen_position)
    string(FIND "${installed_stdout}" "${marker}" installed_position)
    if(frozen_position EQUAL -1 OR installed_position EQUAL -1)
        message(FATAL_ERROR "ABI matrix missing marker: ${marker}")
    endif()
endforeach()
file(WRITE "${BUILD_DIR}/c-api-abi-matrix/evidence.txt"
    "SMAVE_C_API_ABI_MATRIX 1\n"
    "CURRENT_HEADER_CURRENT_LIBRARY 1\n"
    "FROZEN_V1_HEADER_CURRENT_LIBRARY 1\n"
    "FROZEN_V1_HEADER_INSTALLED_LIBRARY 1\n"
    "FORWARD_EXTENDED_V1_STRUCTS 1\n"
    "ABI_VERSION_MISMATCH_REJECTED 1\n"
    "HISTORICAL_NEW_HEADER_OLD_LIBRARY NOT_AVAILABLE_BEFORE_NEXT_RELEASE\n"
    "${frozen_stdout}")
