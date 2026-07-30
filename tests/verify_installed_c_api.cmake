if(NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR OR NOT DEFINED PREFIX OR
   NOT DEFINED C_COMPILER OR NOT DEFINED CXX_COMPILER OR
   NOT DEFINED INSTALL_LIBDIR OR
   NOT DEFINED HOST_SOURCE OR NOT DEFINED NONLINEAR_SOURCE OR
   NOT DEFINED COMPLEMENTARITY_SOURCE OR
   NOT DEFINED CPP_COMPLEMENTARITY_SOURCE OR NOT DEFINED BLOCK_GRAPH_SOURCE OR
   NOT DEFINED CPP_BLOCK_GRAPH_SOURCE OR NOT DEFINED DAE_EVENT_SOURCE OR
   NOT DEFINED INDEX_TWO_DAE_SOURCE OR NOT DEFINED CPP_INDEX_TWO_DAE_SOURCE OR
   NOT DEFINED CANCELLATION_SOURCE OR NOT DEFINED CPP_CANCELLATION_SOURCE OR
   NOT DEFINED EXTERNAL_LINEAR_FALLBACK_SOURCE OR
   NOT DEFINED CPP_EXTERNAL_LINEAR_FALLBACK_SOURCE OR
   NOT DEFINED EXTERNAL_STEPPER_FALLBACK_SOURCE OR
   NOT DEFINED ERROR_STACK_SOURCE OR
   NOT DEFINED CPP_RAII_SOURCE OR
   NOT DEFINED HYBRID_SOURCE OR NOT DEFINED HYBRID_DAE_SOURCE)
    message(FATAL_ERROR "BUILD_DIR, SOURCE_DIR, PREFIX, C/CXX compilers, host sources and hybrid sources are required")
endif()

file(REMOVE_RECURSE "${PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "SDK install failed: ${install_stdout}${install_stderr}")
endif()

if(APPLE)
    set(runtime_path "-Wl,-rpath,${PREFIX}/lib")
    set(executable "${PREFIX}/bin/smave-installed-c-host")
elseif(WIN32)
    message(FATAL_ERROR "installed C host verifier is not implemented for Windows yet")
else()
    set(runtime_path "-Wl,-rpath,${PREFIX}/lib")
    set(executable "${PREFIX}/bin/smave-installed-c-host")
endif()
file(MAKE_DIRECTORY "${PREFIX}/bin")

set(metadata_prefix "${BUILD_DIR}/c-api-sdk/relocated-install")
file(REMOVE_RECURSE "${metadata_prefix}")
file(MAKE_DIRECTORY "${metadata_prefix}")
file(COPY "${PREFIX}/" DESTINATION "${metadata_prefix}")
file(GLOB_RECURSE package_metadata_files
    "${metadata_prefix}/${INSTALL_LIBDIR}/cmake/SMAVE/*.cmake"
    "${metadata_prefix}/${INSTALL_LIBDIR}/pkgconfig/*.pc")
if(package_metadata_files STREQUAL "")
    message(FATAL_ERROR "installed package metadata files are missing")
endif()
foreach(metadata_file IN LISTS package_metadata_files)
    file(READ "${metadata_file}" metadata_contents)
    foreach(forbidden_path "${SOURCE_DIR}" "${BUILD_DIR}" "${PREFIX}")
        string(FIND "${metadata_contents}" "${forbidden_path}" forbidden_position)
        if(NOT forbidden_position EQUAL -1)
            message(FATAL_ERROR
                "installed package metadata leaks absolute path ${forbidden_path}: ${metadata_file}")
        endif()
    endforeach()
endforeach()
set(package_consumer_build "${BUILD_DIR}/c-api-sdk/cmake-package-consumer")
file(REMOVE_RECURSE "${package_consumer_build}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${SOURCE_DIR}/tests/cmake_package_consumer"
        -B "${package_consumer_build}"
        "-DCMAKE_PREFIX_PATH=${metadata_prefix}"
        -DCMAKE_BUILD_TYPE=Release
    RESULT_VARIABLE package_consumer_configure_result
    OUTPUT_VARIABLE package_consumer_configure_stdout
    ERROR_VARIABLE package_consumer_configure_stderr)
if(NOT package_consumer_configure_result EQUAL 0)
    message(FATAL_ERROR
        "installed CMake package consumer configure failed: ${package_consumer_configure_stdout}${package_consumer_configure_stderr}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${package_consumer_build}" --config Release
    RESULT_VARIABLE package_consumer_build_result
    OUTPUT_VARIABLE package_consumer_build_stdout
    ERROR_VARIABLE package_consumer_build_stderr)
if(NOT package_consumer_build_result EQUAL 0)
    message(FATAL_ERROR
        "installed CMake package consumer build failed: ${package_consumer_build_stdout}${package_consumer_build_stderr}")
endif()
set(package_consumer_executable
    "${package_consumer_build}/smave-package-consumer${CMAKE_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${package_consumer_executable}")
    set(package_consumer_executable
        "${package_consumer_build}/Release/smave-package-consumer${CMAKE_EXECUTABLE_SUFFIX}")
endif()
execute_process(
    COMMAND "${package_consumer_executable}"
    RESULT_VARIABLE package_consumer_run_result
    OUTPUT_VARIABLE package_consumer_run_stdout
    ERROR_VARIABLE package_consumer_run_stderr)
if(NOT package_consumer_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed CMake package consumer failed: ${package_consumer_run_stdout}${package_consumer_run_stderr}")
endif()
foreach(marker
        "SMAVE_CMAKE_PACKAGE_CONSUMER 1"
        "SMAVE_IMPORTED_TARGET 1"
        "SMAVE_RELOCATABLE_CONFIG 1")
    string(FIND "${package_consumer_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed CMake package consumer missing marker: ${marker}")
    endif()
endforeach()
set(package_raii_consumer_executable
    "${package_consumer_build}/smave-raii-package-consumer${CMAKE_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${package_raii_consumer_executable}")
    set(package_raii_consumer_executable
        "${package_consumer_build}/Release/smave-raii-package-consumer${CMAKE_EXECUTABLE_SUFFIX}")
endif()
execute_process(
    COMMAND "${package_raii_consumer_executable}"
    RESULT_VARIABLE package_raii_consumer_run_result
    OUTPUT_VARIABLE package_raii_consumer_run_stdout
    ERROR_VARIABLE package_raii_consumer_run_stderr)
if(NOT package_raii_consumer_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed CMake RAII consumer failed: ${package_raii_consumer_run_stdout}${package_raii_consumer_run_stderr}")
endif()
foreach(marker
        "SMAVE_CMAKE_RAII_CONSUMER 1"
        "SMAVE_IMPORTED_CPP_TARGET 1"
        "SMAVE_CPP20_INTERFACE 1")
    string(FIND "${package_raii_consumer_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed CMake RAII consumer missing marker: ${marker}")
    endif()
endforeach()

find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config REQUIRED)
set(pkg_config_path "${metadata_prefix}/${INSTALL_LIBDIR}/pkgconfig")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PKG_CONFIG_PATH=${pkg_config_path}"
        "${PKG_CONFIG_EXECUTABLE}" --modversion smave
    RESULT_VARIABLE pkg_config_version_result
    OUTPUT_VARIABLE pkg_config_version
    ERROR_VARIABLE pkg_config_version_stderr)
string(STRIP "${pkg_config_version}" pkg_config_version)
if(NOT pkg_config_version_result EQUAL 0 OR NOT pkg_config_version STREQUAL "0.1.0")
    message(FATAL_ERROR
        "installed pkg-config version failed: ${pkg_config_version}${pkg_config_version_stderr}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PKG_CONFIG_PATH=${pkg_config_path}"
        "${PKG_CONFIG_EXECUTABLE}" --cflags --libs smave
    RESULT_VARIABLE pkg_config_flags_result
    OUTPUT_VARIABLE pkg_config_flags_text
    ERROR_VARIABLE pkg_config_flags_stderr)
if(NOT pkg_config_flags_result EQUAL 0)
    message(FATAL_ERROR
        "installed pkg-config flags failed: ${pkg_config_flags_text}${pkg_config_flags_stderr}")
endif()
string(STRIP "${pkg_config_flags_text}" pkg_config_flags_text)
separate_arguments(pkg_config_flags NATIVE_COMMAND "${pkg_config_flags_text}")
set(pkg_config_consumer_executable
    "${metadata_prefix}/bin/smave-pkg-config-consumer${CMAKE_EXECUTABLE_SUFFIX}")
file(MAKE_DIRECTORY "${metadata_prefix}/bin")
if(APPLE OR NOT WIN32)
    set(metadata_runtime_path "-Wl,-rpath,${metadata_prefix}/${INSTALL_LIBDIR}")
endif()
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        "${SOURCE_DIR}/tests/pkg_config_consumer.c" ${pkg_config_flags}
        "${metadata_runtime_path}" -o "${pkg_config_consumer_executable}"
    RESULT_VARIABLE pkg_config_consumer_compile_result
    OUTPUT_VARIABLE pkg_config_consumer_compile_stdout
    ERROR_VARIABLE pkg_config_consumer_compile_stderr)
if(NOT pkg_config_consumer_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed pkg-config consumer compile failed: ${pkg_config_consumer_compile_stdout}${pkg_config_consumer_compile_stderr}")
endif()
execute_process(
    COMMAND "${pkg_config_consumer_executable}"
    RESULT_VARIABLE pkg_config_consumer_run_result
    OUTPUT_VARIABLE pkg_config_consumer_run_stdout
    ERROR_VARIABLE pkg_config_consumer_run_stderr)
if(NOT pkg_config_consumer_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed pkg-config consumer failed: ${pkg_config_consumer_run_stdout}${pkg_config_consumer_run_stderr}")
endif()
foreach(marker
        "SMAVE_PKG_CONFIG_CONSUMER 1"
        "SMAVE_PKG_CONFIG_CFLAGS_LIBS 1"
        "SMAVE_PKG_CONFIG_RELOCATABLE_PREFIX 1")
    string(FIND "${pkg_config_consumer_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed pkg-config consumer missing marker: ${marker}")
    endif()
endforeach()

execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${HOST_SOURCE}"
        -L "${PREFIX}/lib" -lsmave -pthread "${runtime_path}"
        -o "${executable}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "installed host compile failed: ${compile_stdout}${compile_stderr}")
endif()

execute_process(
    COMMAND "${executable}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "installed host failed: ${run_stdout}${run_stderr}")
endif()
foreach(marker "SUCCESS 1" "DENSE 1" "CSR 1" "NONLINEAR 1" "ODE 1" "DAE 1"
        "EVENTS 1" "COMPLEMENTARITY_CAPABILITY 1"
        "NONLINEAR_FALLBACK 1" "NONLINEAR_SHARED_SERVICE 1"
        "DAE_SHARED_SERVICE 1" "EVENT_SHARED_SERVICE 1" "STABLE_DIAGNOSTIC_CODES 1")
    string(FIND "${run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed host missing marker: ${marker}")
    endif()
endforeach()

set(nonlinear_executable "${PREFIX}/bin/smave-installed-nonlinear-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${NONLINEAR_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${nonlinear_executable}"
    RESULT_VARIABLE nonlinear_compile_result
    OUTPUT_VARIABLE nonlinear_compile_stdout
    ERROR_VARIABLE nonlinear_compile_stderr)
if(NOT nonlinear_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed nonlinear probe compile failed: ${nonlinear_compile_stdout}${nonlinear_compile_stderr}")
endif()
execute_process(
    COMMAND "${nonlinear_executable}"
    RESULT_VARIABLE nonlinear_run_result
    OUTPUT_VARIABLE nonlinear_run_stdout
    ERROR_VARIABLE nonlinear_run_stderr)
if(NOT nonlinear_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed nonlinear probe failed: ${nonlinear_run_stdout}${nonlinear_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_NONLINEAR_SERVICE 1"
        "EXTERNAL_NONLINEAR_FALLBACK_CAPABILITY 1"
        "EXTERNAL_NONLINEAR_FALLBACK_AFTER_BUILTINS 1"
        "EXTERNAL_NONLINEAR_FALLBACK_ORIGINAL_GATE 1"
        "EXTERNAL_NONLINEAR_FALLBACK_CALLBACK_FAILURE 1"
        "EXTERNAL_NONLINEAR_FALLBACK_CANCEL_PRECHECK 1"
        "EXTERNAL_NONLINEAR_FALLBACK_NEGATIVE_CONTRACTS 1")
    string(FIND "${nonlinear_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed nonlinear probe missing marker: ${marker}")
    endif()
endforeach()

set(complementarity_executable "${PREFIX}/bin/smave-installed-complementarity-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${COMPLEMENTARITY_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${complementarity_executable}"
    RESULT_VARIABLE complementarity_compile_result
    OUTPUT_VARIABLE complementarity_compile_stdout
    ERROR_VARIABLE complementarity_compile_stderr)
if(NOT complementarity_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed complementarity probe compile failed: ${complementarity_compile_stdout}${complementarity_compile_stderr}")
endif()
execute_process(
    COMMAND "${complementarity_executable}"
    RESULT_VARIABLE complementarity_run_result
    OUTPUT_VARIABLE complementarity_run_stdout
    ERROR_VARIABLE complementarity_run_stderr)
if(NOT complementarity_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed complementarity probe failed: ${complementarity_run_stdout}${complementarity_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_COMPLEMENTARITY_SERVICE 1"
        "COMPLEMENTARITY_CAPABILITY 1"
        "COMPLEMENTARITY_DENSE_CSR_EQUIVALENT 1"
        "COMPLEMENTARITY_ORIGINAL_GAP_GATE 1"
        "COMPLEMENTARITY_ACTIVE_SET_FALLBACK 1"
        "COMPLEMENTARITY_NONMONOTONE_REJECTED 1")
    string(FIND "${complementarity_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed complementarity probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_complementarity_executable
    "${PREFIX}/bin/smave-installed-complementarity-cpp-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_COMPLEMENTARITY_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_complementarity_executable}"
    RESULT_VARIABLE cpp_complementarity_compile_result
    OUTPUT_VARIABLE cpp_complementarity_compile_stdout
    ERROR_VARIABLE cpp_complementarity_compile_stderr)
if(NOT cpp_complementarity_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed complementarity C++ host compile failed: ${cpp_complementarity_compile_stdout}${cpp_complementarity_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_complementarity_executable}"
    RESULT_VARIABLE cpp_complementarity_run_result
    OUTPUT_VARIABLE cpp_complementarity_run_stdout
    ERROR_VARIABLE cpp_complementarity_run_stderr)
if(NOT cpp_complementarity_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed complementarity C++ host failed: ${cpp_complementarity_run_stdout}${cpp_complementarity_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_COMPLEMENTARITY_HOST 1"
        "CPP_HEADER_ONLY_PUBLIC_ABI 1"
        "CPP_COMPLEMENTARITY_SOLVE 1")
    string(FIND "${cpp_complementarity_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed complementarity C++ host missing marker: ${marker}")
    endif()
endforeach()

set(block_graph_executable "${PREFIX}/bin/smave-installed-block-graph-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${BLOCK_GRAPH_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${block_graph_executable}"
    RESULT_VARIABLE block_graph_compile_result
    OUTPUT_VARIABLE block_graph_compile_stdout
    ERROR_VARIABLE block_graph_compile_stderr)
if(NOT block_graph_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed block-graph probe compile failed: ${block_graph_compile_stdout}${block_graph_compile_stderr}")
endif()
execute_process(
    COMMAND "${block_graph_executable}"
    RESULT_VARIABLE block_graph_run_result
    OUTPUT_VARIABLE block_graph_run_stdout
    ERROR_VARIABLE block_graph_run_stderr)
if(NOT block_graph_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed block-graph probe failed: ${block_graph_run_stdout}${block_graph_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_BLOCK_GRAPH_SERVICE 1"
        "BLOCK_GRAPH_CAPABILITY 1"
        "BLOCK_GRAPH_MULTIRATE_ZERO_ORDER_HOLD 1"
        "BLOCK_GRAPH_CALLBACK_ORIGINAL_GATE 1"
        "BLOCK_GRAPH_LOCAL_FALLBACK 1"
        "BLOCK_GRAPH_INPUTS_COPIED 1"
        "BLOCK_GRAPH_ALGEBRAIC_FIXED_POINT 1"
        "BLOCK_GRAPH_DIVERGENT_FEEDBACK_REJECTED 1"
        "BLOCK_GRAPH_MIXED_RATE_FEEDBACK_REJECTED 1")
    string(FIND "${block_graph_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed block-graph probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_block_graph_executable
    "${PREFIX}/bin/smave-installed-block-graph-cpp-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_BLOCK_GRAPH_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_block_graph_executable}"
    RESULT_VARIABLE cpp_block_graph_compile_result
    OUTPUT_VARIABLE cpp_block_graph_compile_stdout
    ERROR_VARIABLE cpp_block_graph_compile_stderr)
if(NOT cpp_block_graph_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed block-graph C++ host compile failed: ${cpp_block_graph_compile_stdout}${cpp_block_graph_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_block_graph_executable}"
    RESULT_VARIABLE cpp_block_graph_run_result
    OUTPUT_VARIABLE cpp_block_graph_run_stdout
    ERROR_VARIABLE cpp_block_graph_run_stderr)
if(NOT cpp_block_graph_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed block-graph C++ host failed: ${cpp_block_graph_run_stdout}${cpp_block_graph_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_BLOCK_GRAPH_HOST 1"
        "CPP_HEADER_ONLY_PUBLIC_ABI 1"
        "CPP_BLOCK_GRAPH_SOLVE 1")
    string(FIND "${cpp_block_graph_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed block-graph C++ host missing marker: ${marker}")
    endif()
endforeach()

set(index_two_dae_executable "${PREFIX}/bin/smave-installed-index-two-dae-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${INDEX_TWO_DAE_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${index_two_dae_executable}"
    RESULT_VARIABLE index_two_dae_compile_result
    OUTPUT_VARIABLE index_two_dae_compile_stdout
    ERROR_VARIABLE index_two_dae_compile_stderr)
if(NOT index_two_dae_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed index-2 DAE probe compile failed: ${index_two_dae_compile_stdout}${index_two_dae_compile_stderr}")
endif()
execute_process(
    COMMAND "${index_two_dae_executable}"
    RESULT_VARIABLE index_two_dae_run_result
    OUTPUT_VARIABLE index_two_dae_run_stdout
    ERROR_VARIABLE index_two_dae_run_stderr)
if(NOT index_two_dae_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed index-2 DAE probe failed: ${index_two_dae_run_stdout}${index_two_dae_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_INDEX_TWO_DAE_SERVICE 1"
        "INDEX_TWO_DAE_CAPABILITY 1"
        "INDEX_TWO_DAE_HIDDEN_CONSISTENCY_GATE 1"
        "INDEX_TWO_DAE_HIDDEN_RANK_GATE 1"
        "INDEX_TWO_DAE_RESULT_ABI_MISMATCH 1"
        "INDEX_TWO_DAE_EVENT_RESULT_PREFIX_PRESERVED 1"
        "INDEX_TWO_DAE_EXTENDED_RESULT_TAIL_PRESERVED 1")
    string(FIND "${index_two_dae_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed index-2 DAE probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_index_two_dae_executable
    "${PREFIX}/bin/smave-installed-index-two-dae-cpp-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_INDEX_TWO_DAE_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_index_two_dae_executable}"
    RESULT_VARIABLE cpp_index_two_dae_compile_result
    OUTPUT_VARIABLE cpp_index_two_dae_compile_stdout
    ERROR_VARIABLE cpp_index_two_dae_compile_stderr)
if(NOT cpp_index_two_dae_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed index-2 DAE C++ host compile failed: ${cpp_index_two_dae_compile_stdout}${cpp_index_two_dae_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_index_two_dae_executable}"
    RESULT_VARIABLE cpp_index_two_dae_run_result
    OUTPUT_VARIABLE cpp_index_two_dae_run_stdout
    ERROR_VARIABLE cpp_index_two_dae_run_stderr)
if(NOT cpp_index_two_dae_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed index-2 DAE C++ host failed: ${cpp_index_two_dae_run_stdout}${cpp_index_two_dae_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_INDEX_TWO_DAE_HOST 1"
        "CPP_HEADER_ONLY_PUBLIC_ABI 1"
        "CPP_INDEX_TWO_DAE_SOLVE 1")
    string(FIND "${cpp_index_two_dae_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed index-2 DAE C++ host missing marker: ${marker}")
    endif()
endforeach()

set(cancellation_executable "${PREFIX}/bin/smave-installed-cancellation-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CANCELLATION_SOURCE}"
        -L "${PREFIX}/lib" -lsmave -pthread "${runtime_path}"
        -o "${cancellation_executable}"
    RESULT_VARIABLE cancellation_compile_result
    OUTPUT_VARIABLE cancellation_compile_stdout
    ERROR_VARIABLE cancellation_compile_stderr)
if(NOT cancellation_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed cancellation probe compile failed: ${cancellation_compile_stdout}${cancellation_compile_stderr}")
endif()
execute_process(
    COMMAND "${cancellation_executable}"
    RESULT_VARIABLE cancellation_run_result
    OUTPUT_VARIABLE cancellation_run_stdout
    ERROR_VARIABLE cancellation_run_stderr)
if(NOT cancellation_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed cancellation probe failed: ${cancellation_run_stdout}${cancellation_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_CANCELLATION_SERVICE 1"
        "CANCELLATION_CROSS_THREAD_REQUEST 1"
        "CANCELLATION_ATOMIC_COMMIT_BOUNDARY 1"
        "CANCELLATION_TOKEN_STICKY_RESET_REUSE 1"
        "DEADLINE_CAPABILITY 1"
        "DEADLINE_STABLE_STATUS_DIAGNOSTIC 1"
        "DEADLINE_ATOMIC_COMMIT_BOUNDARY 1"
        "DEADLINE_TOKEN_UNCHANGED_UNLIMITED_REUSE 1"
        "CANCELLATION_ALLOCATOR_BALANCED 1")
    string(FIND "${cancellation_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed cancellation probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_cancellation_executable
    "${PREFIX}/bin/smave-installed-cancellation-cpp-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_CANCELLATION_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_cancellation_executable}"
    RESULT_VARIABLE cpp_cancellation_compile_result
    OUTPUT_VARIABLE cpp_cancellation_compile_stdout
    ERROR_VARIABLE cpp_cancellation_compile_stderr)
if(NOT cpp_cancellation_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed cancellation C++ host compile failed: ${cpp_cancellation_compile_stdout}${cpp_cancellation_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_cancellation_executable}"
    RESULT_VARIABLE cpp_cancellation_run_result
    OUTPUT_VARIABLE cpp_cancellation_run_stdout
    ERROR_VARIABLE cpp_cancellation_run_stderr)
if(NOT cpp_cancellation_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed cancellation C++ host failed: ${cpp_cancellation_run_stdout}${cpp_cancellation_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_CANCELLATION_HOST 1"
        "CPP_HEADER_ONLY_PUBLIC_ABI 1"
        "CPP_CANCELLATION_RESET_REUSE 1"
        "CPP_DEADLINE_STATUS_DIAGNOSTIC 1"
        "CPP_CANCELLATION_PRECEDENCE 1")
    string(FIND "${cpp_cancellation_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed cancellation C++ host missing marker: ${marker}")
    endif()
endforeach()

set(external_linear_fallback_executable
    "${PREFIX}/bin/smave-installed-external-linear-fallback-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${EXTERNAL_LINEAR_FALLBACK_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${external_linear_fallback_executable}"
    RESULT_VARIABLE external_linear_fallback_compile_result
    OUTPUT_VARIABLE external_linear_fallback_compile_stdout
    ERROR_VARIABLE external_linear_fallback_compile_stderr)
if(NOT external_linear_fallback_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed external linear fallback probe compile failed: ${external_linear_fallback_compile_stdout}${external_linear_fallback_compile_stderr}")
endif()
execute_process(
    COMMAND "${external_linear_fallback_executable}"
    RESULT_VARIABLE external_linear_fallback_run_result
    OUTPUT_VARIABLE external_linear_fallback_run_stdout
    ERROR_VARIABLE external_linear_fallback_run_stderr)
if(NOT external_linear_fallback_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed external linear fallback probe failed: ${external_linear_fallback_run_stdout}${external_linear_fallback_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_EXTERNAL_LINEAR_FALLBACK 1"
        "EXTERNAL_LINEAR_FALLBACK_CAPABILITY 1"
        "EXTERNAL_LINEAR_FALLBACK_ORIGINAL_GATE 1"
        "EXTERNAL_LINEAR_FALLBACK_CALLBACK_FAILURE 1"
        "EXTERNAL_LINEAR_FALLBACK_NEGATIVE_CONTRACTS 1"
        "EXTERNAL_LINEAR_FALLBACK_SHARED_SERVICE 1")
    string(FIND "${external_linear_fallback_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed external linear fallback probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_external_linear_fallback_executable
    "${PREFIX}/bin/smave-installed-external-linear-fallback-cpp-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_EXTERNAL_LINEAR_FALLBACK_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_external_linear_fallback_executable}"
    RESULT_VARIABLE cpp_external_linear_fallback_compile_result
    OUTPUT_VARIABLE cpp_external_linear_fallback_compile_stdout
    ERROR_VARIABLE cpp_external_linear_fallback_compile_stderr)
if(NOT cpp_external_linear_fallback_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed external linear fallback C++ host compile failed: ${cpp_external_linear_fallback_compile_stdout}${cpp_external_linear_fallback_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_external_linear_fallback_executable}"
    RESULT_VARIABLE cpp_external_linear_fallback_run_result
    OUTPUT_VARIABLE cpp_external_linear_fallback_run_stdout
    ERROR_VARIABLE cpp_external_linear_fallback_run_stderr)
if(NOT cpp_external_linear_fallback_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed external linear fallback C++ host failed: ${cpp_external_linear_fallback_run_stdout}${cpp_external_linear_fallback_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_EXTERNAL_LINEAR_FALLBACK_HOST 1"
        "CPP_EXTERNAL_LINEAR_FALLBACK_PUBLIC_ABI 1"
        "CPP_EXTERNAL_LINEAR_FALLBACK_VERIFIED 1")
    string(FIND "${cpp_external_linear_fallback_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed external linear fallback C++ host missing marker: ${marker}")
    endif()
endforeach()

set(external_stepper_fallback_executable
    "${PREFIX}/bin/smave-installed-external-stepper-fallback-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${EXTERNAL_STEPPER_FALLBACK_SOURCE}"
        -L "${PREFIX}/lib" -lsmave -lm "${runtime_path}"
        -o "${external_stepper_fallback_executable}"
    RESULT_VARIABLE external_stepper_fallback_compile_result
    OUTPUT_VARIABLE external_stepper_fallback_compile_stdout
    ERROR_VARIABLE external_stepper_fallback_compile_stderr)
if(NOT external_stepper_fallback_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed external stepper fallback probe compile failed: ${external_stepper_fallback_compile_stdout}${external_stepper_fallback_compile_stderr}")
endif()
execute_process(
    COMMAND "${external_stepper_fallback_executable}"
    RESULT_VARIABLE external_stepper_fallback_run_result
    OUTPUT_VARIABLE external_stepper_fallback_run_stdout
    ERROR_VARIABLE external_stepper_fallback_run_stderr)
if(NOT external_stepper_fallback_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed external stepper fallback probe failed: ${external_stepper_fallback_run_stdout}${external_stepper_fallback_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_EXTERNAL_STEPPER_FALLBACK 1"
        "EXTERNAL_ODE_STEPPER_CAPABILITY 1"
        "EXTERNAL_ODE_STEPPER_DENSE_OUTPUT_GATE 1"
        "EXTERNAL_DAE_STEPPER_CAPABILITY 1"
        "EXTERNAL_DAE_STEPPER_KINEMATIC_RESIDUAL_GATE 1"
        "EXTERNAL_STEPPER_FRESH_BUFFERS 1"
        "EXTERNAL_STEPPER_CONTROL_BOUNDARIES 1"
        "EXTERNAL_STEPPER_NEGATIVE_CONTRACTS 1")
    string(FIND "${external_stepper_fallback_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed external stepper fallback probe missing marker: ${marker}")
    endif()
endforeach()

set(error_stack_executable "${PREFIX}/bin/smave-installed-error-stack-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${ERROR_STACK_SOURCE}"
        -L "${PREFIX}/lib" -lsmave -pthread "${runtime_path}"
        -o "${error_stack_executable}"
    RESULT_VARIABLE error_stack_compile_result
    OUTPUT_VARIABLE error_stack_compile_stdout
    ERROR_VARIABLE error_stack_compile_stderr)
if(NOT error_stack_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed error-stack probe compile failed: ${error_stack_compile_stdout}${error_stack_compile_stderr}")
endif()
execute_process(
    COMMAND "${error_stack_executable}"
    RESULT_VARIABLE error_stack_run_result
    OUTPUT_VARIABLE error_stack_run_stdout
    ERROR_VARIABLE error_stack_run_stderr)
if(NOT error_stack_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed error-stack probe failed: ${error_stack_run_stdout}${error_stack_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_ERROR_STACK 1"
        "ERROR_STACK_CAPABILITY 1"
        "ERROR_STACK_NEWEST_FIRST 1"
        "ERROR_STACK_THREAD_LOCAL 1"
        "ERROR_STACK_LIFECYCLE 1"
        "ERROR_STACK_ABI_VALIDATION 1"
        "ERROR_STACK_BOUNDED 1"
        "ERROR_STACK_CLEAR 1")
    string(FIND "${error_stack_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed error-stack probe missing marker: ${marker}")
    endif()
endforeach()

set(cpp_raii_executable "${PREFIX}/bin/smave-installed-cpp-raii-host")
execute_process(
    COMMAND "${CXX_COMPILER}" -std=c++20 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${CPP_RAII_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${cpp_raii_executable}"
    RESULT_VARIABLE cpp_raii_compile_result
    OUTPUT_VARIABLE cpp_raii_compile_stdout
    ERROR_VARIABLE cpp_raii_compile_stderr)
if(NOT cpp_raii_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed C++ RAII host compile failed: ${cpp_raii_compile_stdout}${cpp_raii_compile_stderr}")
endif()
execute_process(
    COMMAND "${cpp_raii_executable}"
    RESULT_VARIABLE cpp_raii_run_result
    OUTPUT_VARIABLE cpp_raii_run_stdout
    ERROR_VARIABLE cpp_raii_run_stderr)
if(NOT cpp_raii_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed C++ RAII host failed: ${cpp_raii_run_stdout}${cpp_raii_run_stderr}")
endif()
foreach(marker
        "SMAVE_CPP_RAII_HOST 1"
        "CPP_RAII_PARENT_LIFETIME 1"
        "CPP_RAII_RESULT_LIFETIME 1"
        "CPP_RAII_CANCELLATION_DEADLINE 1"
        "CPP_RAII_EXTERNAL_LINEAR_FALLBACK 1"
        "CPP_RAII_EXTERNAL_NONLINEAR_FALLBACK 1"
        "CPP_RAII_EXTERNAL_ODE_STEPPER_FALLBACK 1"
        "CPP_RAII_EXTERNAL_DAE_STEPPER_FALLBACK 1"
        "CPP_RAII_STATUS_EXCEPTION 1"
        "CPP_RAII_ERROR_RECORDS 1"
        "CPP_RAII_ALLOCATOR_BALANCED 1")
    string(FIND "${cpp_raii_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed C++ RAII host missing marker: ${marker}")
    endif()
endforeach()

set(dae_event_executable "${PREFIX}/bin/smave-installed-dae-event-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${DAE_EVENT_SOURCE}"
        -L "${PREFIX}/lib" -lsmave -pthread "${runtime_path}"
        -o "${dae_event_executable}"
    RESULT_VARIABLE dae_event_compile_result
    OUTPUT_VARIABLE dae_event_compile_stdout
    ERROR_VARIABLE dae_event_compile_stderr)
if(NOT dae_event_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed DAE event probe compile failed: ${dae_event_compile_stdout}${dae_event_compile_stderr}")
endif()
execute_process(
    COMMAND "${dae_event_executable}"
    RESULT_VARIABLE dae_event_run_result
    OUTPUT_VARIABLE dae_event_run_stdout
    ERROR_VARIABLE dae_event_run_stderr)
if(NOT dae_event_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed DAE event probe failed: ${dae_event_run_stdout}${dae_event_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_DAE_EVENT_SERVICE 1"
        "DAE_EVENT_IMPLICIT_ROOT 1"
        "DAE_EVENT_ATOMIC_REINIT 1"
        "DAE_EVENT_INCONSISTENT_REINIT_REJECTED 1"
        "DAE_EVENT_STABLE_DIAGNOSTIC_CODES 1"
        "DAE_LEGACY_RESULT_PREFIX_COMPATIBLE 1"
        "DAE_EVENT_CONCURRENT_SHARED_SOLVER 1"
        "DAE_EVENT_CONCURRENT_BITWISE_DETERMINISTIC 1"
        "DAE_EVENT_ALLOCATOR_BALANCED 1")
    string(FIND "${dae_event_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed DAE event probe missing marker: ${marker}")
    endif()
endforeach()

set(hybrid_executable "${PREFIX}/bin/smave-installed-hybrid-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${HYBRID_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}"
        -o "${hybrid_executable}"
    RESULT_VARIABLE hybrid_compile_result
    OUTPUT_VARIABLE hybrid_compile_stdout
    ERROR_VARIABLE hybrid_compile_stderr)
if(NOT hybrid_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed hybrid probe compile failed: ${hybrid_compile_stdout}${hybrid_compile_stderr}")
endif()
execute_process(
    COMMAND "${hybrid_executable}"
    RESULT_VARIABLE hybrid_run_result
    OUTPUT_VARIABLE hybrid_run_stdout
    ERROR_VARIABLE hybrid_run_stderr)
if(NOT hybrid_run_result EQUAL 0)
    message(FATAL_ERROR "installed hybrid probe failed: ${hybrid_run_stdout}${hybrid_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_HYBRID_SERVICE 1"
        "HYBRID_MODE_SPECIFIC_RHS 1"
        "HYBRID_TWO_MODE_SWITCHES 1"
        "HYBRID_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1"
        "HYBRID_PRIORITY_TRANSACTION_ROLLBACK 1"
        "HYBRID_SUPERDENSE_CASCADE 1"
        "HYBRID_SUPERDENSE_CYCLE_ROLLBACK 1"
        "HYBRID_STABLE_PRE_ACROSS_MICROSTEPS 1"
        "HYBRID_DISJOINT_WRITESET_ATOMIC_MERGE 1"
        "HYBRID_OVERLAPPING_WRITESET_REJECTED 1"
        "HYBRID_TARGET_MODE_CONFLICT_REJECTED 1"
        "HYBRID_STABLE_DIAGNOSTIC_CODES 1")
    string(FIND "${hybrid_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed hybrid probe missing marker: ${marker}")
    endif()
endforeach()
set(hybrid_dae_executable "${PREFIX}/bin/smave-installed-hybrid-dae-probe")
execute_process(
    COMMAND "${C_COMPILER}" -std=c11 -Wall -Wextra -Wpedantic
        -I "${PREFIX}/include" "${HYBRID_DAE_SOURCE}"
        -L "${PREFIX}/lib" -lsmave "${runtime_path}" -pthread
        -o "${hybrid_dae_executable}"
    RESULT_VARIABLE hybrid_dae_compile_result
    OUTPUT_VARIABLE hybrid_dae_compile_stdout
    ERROR_VARIABLE hybrid_dae_compile_stderr)
if(NOT hybrid_dae_compile_result EQUAL 0)
    message(FATAL_ERROR
        "installed hybrid DAE probe compile failed: ${hybrid_dae_compile_stdout}${hybrid_dae_compile_stderr}")
endif()
execute_process(
    COMMAND "${hybrid_dae_executable}"
    RESULT_VARIABLE hybrid_dae_run_result
    OUTPUT_VARIABLE hybrid_dae_run_stdout
    ERROR_VARIABLE hybrid_dae_run_stderr)
if(NOT hybrid_dae_run_result EQUAL 0)
    message(FATAL_ERROR
        "installed hybrid DAE probe failed: ${hybrid_dae_run_stdout}${hybrid_dae_run_stderr}")
endif()
foreach(marker
        "SMAVE_C_API_HYBRID_DAE_SERVICE 1"
        "HYBRID_DAE_MODE_RESIDUALS 1"
        "HYBRID_DAE_TARGET_CONSISTENCY_GATE 1"
        "HYBRID_DAE_AUTOMATIC_CONSISTENCY_PROJECTION 1"
        "HYBRID_DAE_UNPROJECTABLE_REJECTED 1"
        "HYBRID_DAE_SUPERDENSE_CASCADE 1"
        "HYBRID_DAE_TRANSACTION_ROLLBACK 1"
        "HYBRID_DAE_PROJECTION_METADATA_ATOMIC 1"
        "HYBRID_DAE_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1"
        "HYBRID_DAE_STABLE_PRE_ACROSS_MICROSTEPS 1"
        "HYBRID_DAE_STATE_DERIVATIVE_WRITESET_MERGE 1"
        "HYBRID_DAE_WRITESET_CONFLICTS_REJECTED 1"
        "HYBRID_DAE_TARGET_MODE_CONFLICT_REJECTED 1"
        "HYBRID_DAE_STABLE_SHARED_SOLVER_DETERMINISTIC 1"
        "HYBRID_DAE_CONCURRENT_BITWISE_DETERMINISTIC 1")
    string(FIND "${hybrid_dae_run_stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "installed hybrid DAE probe missing marker: ${marker}")
    endif()
endforeach()
file(WRITE "${BUILD_DIR}/c-api-sdk/installed-evidence.txt"
    "SMAVE_INSTALLED_C_API 1\nSMAVE_CMAKE_PACKAGE_INSTALLED 1\n"
    "SMAVE_PKG_CONFIG_INSTALLED 1\nSMAVE_CPP_RAII_INSTALLED 1\n"
    "SMAVE_PACKAGE_METADATA_NO_ABSOLUTE_PATHS 1\n"
    "COMPLEMENTARITY_INSTALLED_C_API 1\n"
    "COMPLEMENTARITY_INSTALLED_CPP_API 1\n"
    "BLOCK_GRAPH_INSTALLED_C_API 1\n"
    "BLOCK_GRAPH_INSTALLED_CPP_API 1\n"
    "INDEX_TWO_DAE_INSTALLED_C_API 1\n"
    "INDEX_TWO_DAE_INSTALLED_CPP_API 1\n"
    "CANCELLATION_INSTALLED_C_API 1\n"
    "CANCELLATION_INSTALLED_CPP_API 1\n"
    "DEADLINE_INSTALLED_C_API 1\n"
    "DEADLINE_INSTALLED_CPP_API 1\n"
    "EXTERNAL_LINEAR_FALLBACK_INSTALLED_C_API 1\n"
    "EXTERNAL_LINEAR_FALLBACK_INSTALLED_CPP_API 1\n"
    "EXTERNAL_NONLINEAR_FALLBACK_INSTALLED_C_API 1\n"
    "EXTERNAL_NONLINEAR_FALLBACK_INSTALLED_CPP_API 1\n"
    "EXTERNAL_ODE_STEPPER_FALLBACK_INSTALLED_C_API 1\n"
    "EXTERNAL_DAE_STEPPER_FALLBACK_INSTALLED_C_API 1\n"
    "EXTERNAL_STEPPER_FALLBACK_INSTALLED_CPP_RAII 1\n"
    "ERROR_STACK_INSTALLED_C_API 1\n"
    "ERROR_STACK_INSTALLED_CPP_RAII 1\n"
    "DAE_EVENT_INSTALLED_C_API 1\n"
    "HYBRID_INSTALLED_C_API 1\nHYBRID_DAE_INSTALLED_C_API 1\nPREFIX ${PREFIX}\n"
    "${package_consumer_run_stdout}${package_raii_consumer_run_stdout}"
    "${pkg_config_consumer_run_stdout}${cpp_raii_run_stdout}"
    "${run_stdout}${nonlinear_run_stdout}${complementarity_run_stdout}${cpp_complementarity_run_stdout}"
    "${block_graph_run_stdout}${cpp_block_graph_run_stdout}"
    "${index_two_dae_run_stdout}${cpp_index_two_dae_run_stdout}"
    "${cancellation_run_stdout}${cpp_cancellation_run_stdout}"
    "${external_linear_fallback_run_stdout}${cpp_external_linear_fallback_run_stdout}"
    "${external_stepper_fallback_run_stdout}${error_stack_run_stdout}"
    "${dae_event_run_stdout}"
    "${hybrid_run_stdout}${hybrid_dae_run_stdout}")
