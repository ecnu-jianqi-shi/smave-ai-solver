cmake_minimum_required(VERSION 3.20)
project(smave_ai_solver VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)

option(SMAVE_CORE_REPRO_BUNDLE
    "Build from the core reproduction bundle without excluded benchmark datasets"
    OFF)
set(SMAVE_HINTS_UPSTREAM_ROOT "" CACHE PATH
    "Clean checkout of the pinned official HINTS repository")
set(SMAVE_HINTS_PYTHON "" CACHE FILEPATH
    "Python interpreter with the official HINTS NumPy dependencies")
if(SMAVE_CORE_REPRO_BUNDLE)
    set(SMAVE_SPARSE_SUITE_EXPECTED_CASES 5)
else()
    set(SMAVE_SPARSE_SUITE_EXPECTED_CASES 66)
endif()

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(smave_core
    src/block_graph.cpp
    src/benchmark.cpp
    src/competition.cpp
    src/complementarity.cpp
    src/continuous.cpp
    src/cosimulation.cpp
    src/dae.cpp
    src/implicit_dae.cpp
    src/dae_learning.cpp
    src/expression.cpp
    src/fmi.cpp
    src/family_routing.cpp
    src/embedding.cpp
    src/ir.cpp
    src/learning.cpp
    src/linear.cpp
    src/geometric_multigrid.cpp
    src/industrial_sparse.cpp
    src/incremental_gate.cpp
    src/superlu_sparse.cpp
    src/operator.cpp
    src/compiler.cpp
    src/config.cpp
    src/expert.cpp
    src/routing.cpp
    src/release.cpp
    src/runtime.cpp
    src/solve_service.cpp
    src/residency.cpp
    src/data_registry.cpp
    src/pdebench_training.cpp
    src/device.cpp
    src/tensor.cpp
    src/validation.cpp
    src/verification.cpp
    src/model_group.cpp
    src/hybrid.cpp
    src/high_index_dae.cpp
    src/benchmark/sparse_suite.cpp
    src/benchmark/suite_manifest.cpp
)
if(DEFINED SMAVE_CUDA_TOOLKIT_ROOT)
    enable_language(CUDA)
    add_executable(smave_cuda_device_evidence tests/cuda_device_evidence.cpp)
    target_link_libraries(smave_cuda_device_evidence PRIVATE smave_core)
    set(SMAVE_CUDA_INCLUDE "${SMAVE_CUDA_TOOLKIT_ROOT}/include")
    set(SMAVE_CUDA_LIB "${SMAVE_CUDA_TOOLKIT_ROOT}/lib64")
    add_library(smave_cuda_cudart SHARED IMPORTED)
    set_target_properties(smave_cuda_cudart PROPERTIES
        IMPORTED_LOCATION "${SMAVE_CUDA_LIB}/libcudart.so"
        INTERFACE_INCLUDE_DIRECTORIES "${SMAVE_CUDA_INCLUDE}")
    add_library(smave_cuda_kernels STATIC src/device_cuda.cu)
    set_property(TARGET smave_core PROPERTY CUDA_ARCHITECTURES native)
    set_target_properties(smave_cuda_kernels PROPERTIES
        CUDA_ARCHITECTURES native
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(smave_cuda_kernels PRIVATE "${SMAVE_CUDA_INCLUDE}")
    target_link_libraries(smave_cuda_kernels PRIVATE smave_cuda_cudart)
    target_sources(smave_core PRIVATE $<TARGET_OBJECTS:smave_cuda_kernels>)
    target_link_libraries(smave_core PRIVATE smave_cuda_cudart)
    target_include_directories(smave_core PRIVATE "${SMAVE_CUDA_INCLUDE}")
    target_compile_definitions(smave_core PRIVATE SMAVE_HAVE_CUDA_RUNTIME=1)
endif()
set_target_properties(smave_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(smave_core PUBLIC include)
target_link_libraries(smave_core PRIVATE ZLIB::ZLIB Threads::Threads ${CMAKE_DL_LIBS})
if(APPLE)
    find_library(SMAVE_ACCELERATE_FRAMEWORK Accelerate REQUIRED)
    target_link_libraries(smave_core PRIVATE ${SMAVE_ACCELERATE_FRAMEWORK})
    target_compile_definitions(smave_core PRIVATE SMAVE_HAVE_ACCELERATE_SPARSE=1)
    target_link_libraries(smave_core PRIVATE objc)
    target_compile_definitions(smave_core PRIVATE SMAVE_HAVE_METAL_RUNTIME=1)
    set_source_files_properties(src/device.cpp PROPERTIES COMPILE_OPTIONS "-fblocks")
endif()
target_compile_options(smave_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(smave src/main.cpp)
target_link_libraries(smave PRIVATE smave_core)

add_library(smave_sdk SHARED src/c_api.cpp)
target_link_libraries(smave_sdk PRIVATE smave_core)
target_include_directories(smave_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_definitions(smave_sdk PRIVATE SMAVE_C_API_BUILD=1)
target_compile_options(smave_sdk PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(smave_sdk PROPERTIES
    OUTPUT_NAME smave
    EXPORT_NAME smave
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

add_library(smave_cpp_sdk INTERFACE)
target_link_libraries(smave_cpp_sdk INTERFACE smave_sdk)
target_compile_features(smave_cpp_sdk INTERFACE cxx_std_20)
set_target_properties(smave_cpp_sdk PROPERTIES EXPORT_NAME cpp)

add_executable(smave_c_api_host tests/c_api_host.c)
target_link_libraries(smave_c_api_host PRIVATE smave_sdk Threads::Threads)
target_compile_options(smave_c_api_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_host COMMAND smave_c_api_host)

add_executable(smave_c_api_linear_service_probe tests/c_api_linear_service_probe.c)
target_link_libraries(smave_c_api_linear_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_linear_service_probe PRIVATE -Wall -Wextra -Wpedantic)

add_executable(smave_c_api_nonlinear_service_probe tests/c_api_nonlinear_service_probe.c)
target_link_libraries(smave_c_api_nonlinear_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_nonlinear_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_executable(
    smave_c_api_complementarity_service_probe
    tests/c_api_complementarity_service_probe.c)
target_link_libraries(smave_c_api_complementarity_service_probe PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_complementarity_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_complementarity_service_probe
    COMMAND smave_c_api_complementarity_service_probe)
add_executable(
    smave_c_api_complementarity_cpp_host
    tests/c_api_complementarity_cpp_host.cpp)
target_link_libraries(smave_c_api_complementarity_cpp_host PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_complementarity_cpp_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_complementarity_cpp_host
    COMMAND smave_c_api_complementarity_cpp_host)
add_executable(
    smave_c_api_block_graph_service_probe
    tests/c_api_block_graph_service_probe.c)
target_link_libraries(smave_c_api_block_graph_service_probe PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_block_graph_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_block_graph_service_probe
    COMMAND smave_c_api_block_graph_service_probe)
add_executable(
    smave_c_api_block_graph_cpp_host
    tests/c_api_block_graph_cpp_host.cpp)
target_link_libraries(smave_c_api_block_graph_cpp_host PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_block_graph_cpp_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_block_graph_cpp_host
    COMMAND smave_c_api_block_graph_cpp_host)
add_executable(smave_c_api_ode_service_probe tests/c_api_ode_service_probe.c)
target_link_libraries(smave_c_api_ode_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_ode_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_executable(smave_c_api_dae_service_probe tests/c_api_dae_service_probe.c)
target_link_libraries(smave_c_api_dae_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_dae_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_dae_service_probe COMMAND smave_c_api_dae_service_probe)
add_executable(
    smave_c_api_index_two_dae_service_probe
    tests/c_api_index_two_dae_service_probe.c)
target_link_libraries(smave_c_api_index_two_dae_service_probe PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_index_two_dae_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_index_two_dae_service_probe
    COMMAND smave_c_api_index_two_dae_service_probe)
add_executable(
    smave_c_api_index_two_dae_cpp_host
    tests/c_api_index_two_dae_cpp_host.cpp)
target_link_libraries(smave_c_api_index_two_dae_cpp_host PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_index_two_dae_cpp_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_index_two_dae_cpp_host
    COMMAND smave_c_api_index_two_dae_cpp_host)
add_executable(
    smave_c_api_cancellation_service_probe
    tests/c_api_cancellation_service_probe.c)
target_link_libraries(
    smave_c_api_cancellation_service_probe PRIVATE smave_sdk Threads::Threads)
target_compile_options(
    smave_c_api_cancellation_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_cancellation_service_probe
    COMMAND smave_c_api_cancellation_service_probe)
set_tests_properties(smave_c_api_cancellation_service_probe PROPERTIES
    RUN_SERIAL TRUE)
add_executable(
    smave_c_api_cancellation_cpp_host
    tests/c_api_cancellation_cpp_host.cpp)
target_link_libraries(smave_c_api_cancellation_cpp_host PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_cancellation_cpp_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_cancellation_cpp_host
    COMMAND smave_c_api_cancellation_cpp_host)
add_executable(
    smave_c_api_external_linear_fallback_probe
    tests/c_api_external_linear_fallback_probe.c)
target_link_libraries(smave_c_api_external_linear_fallback_probe PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_external_linear_fallback_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_external_linear_fallback_probe
    COMMAND smave_c_api_external_linear_fallback_probe)
add_executable(
    smave_c_api_external_linear_fallback_cpp_host
    tests/c_api_external_linear_fallback_cpp_host.cpp)
target_link_libraries(smave_c_api_external_linear_fallback_cpp_host PRIVATE smave_sdk)
target_compile_options(
    smave_c_api_external_linear_fallback_cpp_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_external_linear_fallback_cpp_host
    COMMAND smave_c_api_external_linear_fallback_cpp_host)
add_executable(
    smave_c_api_external_stepper_fallback_probe
    tests/c_api_external_stepper_fallback_probe.c)
target_link_libraries(
    smave_c_api_external_stepper_fallback_probe PRIVATE smave_sdk)
if(UNIX)
    target_link_libraries(smave_c_api_external_stepper_fallback_probe PRIVATE m)
endif()
target_compile_options(
    smave_c_api_external_stepper_fallback_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_external_stepper_fallback_probe
    COMMAND smave_c_api_external_stepper_fallback_probe)
add_executable(smave_c_api_error_stack_probe tests/c_api_error_stack_probe.c)
target_link_libraries(
    smave_c_api_error_stack_probe PRIVATE smave_sdk Threads::Threads)
target_compile_options(
    smave_c_api_error_stack_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_error_stack_probe COMMAND smave_c_api_error_stack_probe)
add_executable(smave_cpp_api_raii_host tests/cpp_api_raii_host.cpp)
target_link_libraries(smave_cpp_api_raii_host PRIVATE smave_sdk)
target_compile_options(smave_cpp_api_raii_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_cpp_api_raii_host COMMAND smave_cpp_api_raii_host)
add_executable(smave_c_api_event_service_probe tests/c_api_event_service_probe.c)
target_link_libraries(smave_c_api_event_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_event_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_event_service_probe COMMAND smave_c_api_event_service_probe)
add_executable(smave_c_api_dae_event_service_probe tests/c_api_dae_event_service_probe.c)
target_link_libraries(smave_c_api_dae_event_service_probe PRIVATE smave_sdk Threads::Threads)
target_compile_options(smave_c_api_dae_event_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_dae_event_service_probe COMMAND smave_c_api_dae_event_service_probe)
add_executable(smave_c_api_hybrid_service_probe tests/c_api_hybrid_service_probe.c)
target_link_libraries(smave_c_api_hybrid_service_probe PRIVATE smave_sdk)
target_compile_options(smave_c_api_hybrid_service_probe PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_hybrid_service_probe COMMAND smave_c_api_hybrid_service_probe)
add_executable(
    smave_c_api_hybrid_dae_service_probe
    tests/c_api_hybrid_dae_service_probe.c)
target_link_libraries(
    smave_c_api_hybrid_dae_service_probe
    PRIVATE smave_sdk Threads::Threads)
target_compile_options(
    smave_c_api_hybrid_dae_service_probe
    PRIVATE -Wall -Wextra -Wpedantic)
add_test(
    NAME smave_c_api_hybrid_dae_service_probe
    COMMAND smave_c_api_hybrid_dae_service_probe)

add_executable(smave_c_api_abi_v1_host tests/c_api_abi_v1_host.c)
target_include_directories(smave_c_api_abi_v1_host BEFORE PRIVATE tests/abi/v1)
target_link_libraries(smave_c_api_abi_v1_host PRIVATE smave_sdk)
target_compile_options(smave_c_api_abi_v1_host PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_c_api_abi_v1_host COMMAND smave_c_api_abi_v1_host)
add_test(
    NAME smave_solve_service_reuse
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:smave>
        -DC_API_PROBE=$<TARGET_FILE:smave_c_api_linear_service_probe>
        -DC_API_NONLINEAR_PROBE=$<TARGET_FILE:smave_c_api_nonlinear_service_probe>
        -DC_API_COMPLEMENTARITY_PROBE=$<TARGET_FILE:smave_c_api_complementarity_service_probe>
        -DC_API_BLOCK_GRAPH_PROBE=$<TARGET_FILE:smave_c_api_block_graph_service_probe>
        -DC_API_ODE_PROBE=$<TARGET_FILE:smave_c_api_ode_service_probe>
        -DC_API_DAE_PROBE=$<TARGET_FILE:smave_c_api_dae_service_probe>
        -DC_API_INDEX_TWO_DAE_PROBE=$<TARGET_FILE:smave_c_api_index_two_dae_service_probe>
        -DC_API_CANCELLATION_PROBE=$<TARGET_FILE:smave_c_api_cancellation_service_probe>
        -DC_API_EXTERNAL_LINEAR_FALLBACK_PROBE=$<TARGET_FILE:smave_c_api_external_linear_fallback_probe>
        -DC_API_EXTERNAL_STEPPER_FALLBACK_PROBE=$<TARGET_FILE:smave_c_api_external_stepper_fallback_probe>
        -DC_API_ERROR_STACK_PROBE=$<TARGET_FILE:smave_c_api_error_stack_probe>
        -DC_API_EVENT_PROBE=$<TARGET_FILE:smave_c_api_event_service_probe>
        -DC_API_DAE_EVENT_PROBE=$<TARGET_FILE:smave_c_api_dae_event_service_probe>
        -DC_API_HYBRID_PROBE=$<TARGET_FILE:smave_c_api_hybrid_service_probe>
        -DC_API_HYBRID_DAE_PROBE=$<TARGET_FILE:smave_c_api_hybrid_dae_service_probe>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/solve-service
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_solve_service_reuse.cmake)
set_tests_properties(smave_solve_service_reuse PROPERTIES RUN_SERIAL TRUE)

add_custom_target(reproduce-solve-service
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:smave>
        -DC_API_PROBE=$<TARGET_FILE:smave_c_api_linear_service_probe>
        -DC_API_NONLINEAR_PROBE=$<TARGET_FILE:smave_c_api_nonlinear_service_probe>
        -DC_API_COMPLEMENTARITY_PROBE=$<TARGET_FILE:smave_c_api_complementarity_service_probe>
        -DC_API_BLOCK_GRAPH_PROBE=$<TARGET_FILE:smave_c_api_block_graph_service_probe>
        -DC_API_ODE_PROBE=$<TARGET_FILE:smave_c_api_ode_service_probe>
        -DC_API_DAE_PROBE=$<TARGET_FILE:smave_c_api_dae_service_probe>
        -DC_API_INDEX_TWO_DAE_PROBE=$<TARGET_FILE:smave_c_api_index_two_dae_service_probe>
        -DC_API_CANCELLATION_PROBE=$<TARGET_FILE:smave_c_api_cancellation_service_probe>
        -DC_API_EXTERNAL_LINEAR_FALLBACK_PROBE=$<TARGET_FILE:smave_c_api_external_linear_fallback_probe>
        -DC_API_EXTERNAL_STEPPER_FALLBACK_PROBE=$<TARGET_FILE:smave_c_api_external_stepper_fallback_probe>
        -DC_API_ERROR_STACK_PROBE=$<TARGET_FILE:smave_c_api_error_stack_probe>
        -DC_API_EVENT_PROBE=$<TARGET_FILE:smave_c_api_event_service_probe>
        -DC_API_DAE_EVENT_PROBE=$<TARGET_FILE:smave_c_api_dae_event_service_probe>
        -DC_API_HYBRID_PROBE=$<TARGET_FILE:smave_c_api_hybrid_service_probe>
        -DC_API_HYBRID_DAE_PROBE=$<TARGET_FILE:smave_c_api_hybrid_dae_service_probe>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/solve-service
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_solve_service_reuse.cmake
    DEPENDS smave smave_c_api_linear_service_probe smave_c_api_nonlinear_service_probe smave_c_api_complementarity_service_probe smave_c_api_block_graph_service_probe smave_c_api_ode_service_probe smave_c_api_dae_service_probe smave_c_api_index_two_dae_service_probe smave_c_api_cancellation_service_probe smave_c_api_external_linear_fallback_probe smave_c_api_external_stepper_fallback_probe smave_c_api_error_stack_probe smave_c_api_event_service_probe smave_c_api_dae_event_service_probe smave_c_api_hybrid_service_probe smave_c_api_hybrid_dae_service_probe
    VERBATIM)

add_custom_target(reproduce-c-api-abi-matrix
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/c-api-abi-matrix/install
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -DFROZEN_HOST=$<TARGET_FILE:smave_c_api_abi_v1_host>
        -DCURRENT_HOST=$<TARGET_FILE:smave_c_api_host>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_c_api_abi_matrix.cmake
    DEPENDS smave_c_api_abi_v1_host smave_c_api_host
    VERBATIM)

set(SMAVE_C_API_EVIDENCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/c-api-sdk")
add_custom_target(reproduce-c-api-sdk
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_C_API_EVIDENCE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE=$<TARGET_FILE:smave_c_api_host>
        -DOUTPUT=${SMAVE_C_API_EVIDENCE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_c_api_evidence.cmake
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DPREFIX=${SMAVE_C_API_EVIDENCE_DIR}/install
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
        -DHOST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_host.c
        -DNONLINEAR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_nonlinear_service_probe.c
        -DCOMPLEMENTARITY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_complementarity_service_probe.c
        -DCPP_COMPLEMENTARITY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_complementarity_cpp_host.cpp
        -DBLOCK_GRAPH_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_block_graph_service_probe.c
        -DCPP_BLOCK_GRAPH_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_block_graph_cpp_host.cpp
        -DINDEX_TWO_DAE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_index_two_dae_service_probe.c
        -DCPP_INDEX_TWO_DAE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_index_two_dae_cpp_host.cpp
        -DCANCELLATION_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_cancellation_service_probe.c
        -DCPP_CANCELLATION_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_cancellation_cpp_host.cpp
        -DEXTERNAL_LINEAR_FALLBACK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_external_linear_fallback_probe.c
        -DCPP_EXTERNAL_LINEAR_FALLBACK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_external_linear_fallback_cpp_host.cpp
        -DEXTERNAL_STEPPER_FALLBACK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_external_stepper_fallback_probe.c
        -DERROR_STACK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_error_stack_probe.c
        -DCPP_RAII_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_api_raii_host.cpp
        -DDAE_EVENT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_dae_event_service_probe.c
        -DHYBRID_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_hybrid_service_probe.c
        -DHYBRID_DAE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/c_api_hybrid_dae_service_probe.c
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_installed_c_api.cmake
    DEPENDS smave_c_api_host smave_c_api_nonlinear_service_probe smave_c_api_complementarity_service_probe smave_c_api_complementarity_cpp_host smave_c_api_block_graph_service_probe smave_c_api_block_graph_cpp_host smave_c_api_index_two_dae_service_probe smave_c_api_index_two_dae_cpp_host smave_c_api_cancellation_service_probe smave_c_api_cancellation_cpp_host smave_c_api_external_linear_fallback_probe smave_c_api_external_linear_fallback_cpp_host smave_c_api_external_stepper_fallback_probe smave_c_api_error_stack_probe smave_cpp_api_raii_host smave_c_api_hybrid_dae_service_probe
    VERBATIM)

include(CMakePackageConfigHelpers)
install(TARGETS smave_sdk smave_cpp_sdk
    EXPORT SMAVETargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/smave/c_api.h include/smave/cpp_api.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smave)
install(EXPORT SMAVETargets
    FILE SMAVETargets.cmake
    NAMESPACE SMAVE::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SMAVE)
configure_package_config_file(
    cmake/SMAVEConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SMAVEConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SMAVE)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SMAVEConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
configure_file(cmake/smave.pc.in ${CMAKE_CURRENT_BINARY_DIR}/smave.pc @ONLY)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SMAVEConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SMAVEConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SMAVE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smave.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

add_executable(smave_periodic_linear_unit tests/periodic_linear_unit.cpp)
target_link_libraries(smave_periodic_linear_unit PRIVATE smave_core)
target_compile_options(smave_periodic_linear_unit PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_periodic_linear_unit COMMAND smave_periodic_linear_unit)

add_executable(smave_solve_service_unit tests/solve_service_unit.cpp)
target_link_libraries(smave_solve_service_unit PRIVATE smave_core)
target_compile_options(smave_solve_service_unit PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_solve_service_unit COMMAND smave_solve_service_unit)

add_executable(smave_gate_architecture_evidence tests/gate_architecture_evidence.cpp)
target_link_libraries(smave_gate_architecture_evidence PRIVATE smave_core)
add_executable(smave_gate_parallel_scaling_evidence
    tests/gate_parallel_scaling_evidence.cpp)
target_link_libraries(smave_gate_parallel_scaling_evidence PRIVATE smave_core)
if(UNIX)
    add_executable(smave_gate_process_scaling_evidence EXCLUDE_FROM_ALL
        tests/gate_process_scaling_evidence.cpp)
    target_link_libraries(smave_gate_process_scaling_evidence PRIVATE smave_core)
    target_compile_options(smave_gate_process_scaling_evidence PRIVATE
        -Wall -Wextra -Wpedantic)
    add_executable(smave_gate_network_authority_evidence EXCLUDE_FROM_ALL
        tests/gate_network_authority_evidence.cpp)
    target_link_libraries(smave_gate_network_authority_evidence PRIVATE smave_core)
    target_compile_options(smave_gate_network_authority_evidence PRIVATE
        -Wall -Wextra -Wpedantic)
endif()
add_executable(smave_risk_adaptive_gate_evidence
    tests/risk_adaptive_gate_evidence.cpp)
target_link_libraries(smave_risk_adaptive_gate_evidence PRIVATE smave_core)
add_executable(smave_nonlinear_cascade_evidence
    tests/nonlinear_cascade_evidence.cpp)
target_link_libraries(smave_nonlinear_cascade_evidence PRIVATE smave_core)
add_executable(smave_paired_oracle_evidence tests/paired_oracle_evidence.cpp)
target_link_libraries(smave_paired_oracle_evidence PRIVATE smave_core)
add_executable(smave_cascade_ordering_evidence tests/cascade_ordering_evidence.cpp)
target_link_libraries(smave_cascade_ordering_evidence PRIVATE smave_core)
target_compile_options(smave_cascade_ordering_evidence PRIVATE -Wall -Wextra -Wpedantic)
add_executable(smave_calibrated_correction_router_evidence
    tests/calibrated_correction_router_evidence.cpp)
target_link_libraries(smave_calibrated_correction_router_evidence PRIVATE smave_core)
target_compile_options(smave_calibrated_correction_router_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_structural_ood_filter_evidence
    tests/structural_ood_filter_evidence.cpp)
target_link_libraries(smave_structural_ood_filter_evidence PRIVATE smave_core)
target_compile_options(smave_structural_ood_filter_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_joint_route_budget_evidence
    tests/joint_route_budget_evidence.cpp)
target_link_libraries(smave_joint_route_budget_evidence PRIVATE smave_core)
target_compile_options(smave_joint_route_budget_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_joint_route_scaling_evidence
    tests/joint_route_scaling_evidence.cpp)
target_link_libraries(smave_joint_route_scaling_evidence PRIVATE smave_core)
target_compile_options(smave_joint_route_scaling_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_joint_route_budget_shift_evidence
    tests/joint_route_budget_shift_evidence.cpp)
target_link_libraries(smave_joint_route_budget_shift_evidence PRIVATE smave_core)
target_compile_options(smave_joint_route_budget_shift_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_request_conditioned_joint_route_evidence
    tests/request_conditioned_joint_route_evidence.cpp)
target_link_libraries(smave_request_conditioned_joint_route_evidence PRIVATE smave_core)
target_compile_options(smave_request_conditioned_joint_route_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_suitesparse_request_conditioned_route_evidence
    tests/suitesparse_request_conditioned_route_evidence.cpp)
target_link_libraries(
    smave_suitesparse_request_conditioned_route_evidence PRIVATE smave_core)
target_compile_options(
    smave_suitesparse_request_conditioned_route_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_suitesparse_structure_audit
    tests/suitesparse_structure_audit.cpp)
target_link_libraries(smave_suitesparse_structure_audit PRIVATE smave_core)
target_compile_options(smave_suitesparse_structure_audit PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_suitesparse_identity_gmres_probe EXCLUDE_FROM_ALL
    tests/suitesparse_identity_gmres_probe.cpp)
target_link_libraries(smave_suitesparse_identity_gmres_probe PRIVATE smave_core)
target_compile_options(smave_suitesparse_identity_gmres_probe PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_suitesparse_lsqr_probe EXCLUDE_FROM_ALL
    tests/suitesparse_lsqr_probe.cpp)
target_link_libraries(smave_suitesparse_lsqr_probe PRIVATE smave_core)
target_compile_options(smave_suitesparse_lsqr_probe PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_superlu_repeat_probe EXCLUDE_FROM_ALL
    tests/superlu_repeat_probe.cpp)
target_link_libraries(smave_superlu_repeat_probe PRIVATE smave_core)
target_compile_options(smave_superlu_repeat_probe PRIVATE
    -Wall -Wextra -Wpedantic)
add_executable(smave_hints_native_smave_evidence EXCLUDE_FROM_ALL
    tests/hints_native_smave_evidence.cpp)
target_link_libraries(smave_hints_native_smave_evidence PRIVATE smave_core)
target_compile_options(smave_hints_native_smave_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_options(smave_gate_architecture_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_gate_parallel_scaling_evidence PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_options(smave_risk_adaptive_gate_evidence PRIVATE -Wall -Wextra -Wpedantic)
set(SMAVE_GATE_ARCHITECTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gate-architecture")
set(SMAVE_CASCADE_ORDERING_DIR "${CMAKE_CURRENT_BINARY_DIR}/cascade-ordering")
add_custom_target(reproduce-cascade-ordering
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_CASCADE_ORDERING_DIR}"
    COMMAND $<TARGET_FILE:smave_cascade_ordering_evidence>
        "${SMAVE_CASCADE_ORDERING_DIR}/evidence.txt"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_CASCADE_ORDERING_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_cascade_ordering.cmake
    DEPENDS smave_cascade_ordering_evidence
    VERBATIM)

set(SMAVE_CALIBRATED_CORRECTION_ROUTER_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/calibrated-correction-router")
add_custom_target(reproduce-calibrated-correction-router
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_CALIBRATED_CORRECTION_ROUTER_DIR}"
    COMMAND $<TARGET_FILE:smave_calibrated_correction_router_evidence>
        "${SMAVE_CALIBRATED_CORRECTION_ROUTER_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_CALIBRATED_CORRECTION_ROUTER_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_calibrated_correction_router.cmake
    DEPENDS smave_calibrated_correction_router_evidence
    VERBATIM)

set(SMAVE_JOINT_ROUTE_BUDGET_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/joint-route-budget")
add_custom_target(reproduce-joint-route-budget
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_JOINT_ROUTE_BUDGET_DIR}"
    COMMAND $<TARGET_FILE:smave_joint_route_budget_evidence>
        "${SMAVE_JOINT_ROUTE_BUDGET_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_JOINT_ROUTE_BUDGET_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_joint_route_budget.cmake
    DEPENDS smave_joint_route_budget_evidence
    VERBATIM)

set(SMAVE_JOINT_ROUTE_SCALING_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/joint-route-scaling-round51")
set(SMAVE_JOINT_ROUTE_SCALING_CONTRACT
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/data-lock/joint-route-scaling-round51-prefirst-run-contract.txt")
add_custom_target(reproduce-joint-route-scaling-round51
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${SMAVE_JOINT_ROUTE_SCALING_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_JOINT_ROUTE_SCALING_DIR}"
    COMMAND $<TARGET_FILE:smave_joint_route_scaling_evidence>
        "${SMAVE_JOINT_ROUTE_SCALING_DIR}"
        "${SMAVE_JOINT_ROUTE_SCALING_CONTRACT}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_JOINT_ROUTE_SCALING_DIR}/evidence.txt
        -DSCALING=${SMAVE_JOINT_ROUTE_SCALING_DIR}/scaling.tsv
        -DPREFLIGHT=${SMAVE_JOINT_ROUTE_SCALING_DIR}/preflight.tsv
        -DCONTRACT=${SMAVE_JOINT_ROUTE_SCALING_CONTRACT}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_joint_route_scaling.cmake
    DEPENDS smave_joint_route_scaling_evidence
    VERBATIM)

set(SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/frozen-interaction-prevalence-round52")
set(SMAVE_FROZEN_INTERACTION_PREVALENCE_CONTRACT
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/data-lock/frozen-interaction-prevalence-round52-analysis-contract.txt")
add_custom_target(reproduce-frozen-interaction-prevalence-round52
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_frozen_interaction_prevalence.py
        --v5 ${CMAKE_CURRENT_SOURCE_DIR}/build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run
        --v6 ${CMAKE_CURRENT_SOURCE_DIR}/build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run
        --contract ${SMAVE_FROZEN_INTERACTION_PREVALENCE_CONTRACT}
        --output ${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}/evidence.txt
        -DREQUEST_COVERAGE=${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}/request-coverage.tsv
        -DPAIR_SUPPORT=${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}/pair-support.tsv
        -DPAIR_CLASSES=${SMAVE_FROZEN_INTERACTION_PREVALENCE_DIR}/pair-classes.tsv
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_frozen_interaction_prevalence.cmake
    VERBATIM)

set(SMAVE_FROZEN_TRANSITION_ATTRITION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/frozen-transition-attrition-round53")
set(SMAVE_FROZEN_TRANSITION_ATTRITION_CONTRACT
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/data-lock/frozen-transition-attrition-round53-analysis-contract.txt")
set(SMAVE_FROZEN_TRANSITION_ATTRITION_V5
    "${CMAKE_CURRENT_SOURCE_DIR}/build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run")
set(SMAVE_FROZEN_TRANSITION_ATTRITION_V6
    "${CMAKE_CURRENT_SOURCE_DIR}/build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run")
add_custom_target(reproduce-frozen-transition-attrition-round53
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}"
    COMMAND $<TARGET_FILE:smave_suitesparse_request_conditioned_route_evidence>
        --frozen-transition-attrition
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_V5}"
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_V6}"
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_CONTRACT}"
        "${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/evidence.txt
        -DTRANSITIONS=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/transition-attrition.tsv
        -DCANDIDATES=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/candidate-transitions.tsv
        -DV5_PLANS=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/v5-request-plans.tsv
        -DV6_PLANS=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/v6-request-plans.tsv
        -DCONTRACT=${SMAVE_FROZEN_TRANSITION_ATTRITION_CONTRACT}
        -DCONTRACT_COPY=${SMAVE_FROZEN_TRANSITION_ATTRITION_DIR}/frozen-transition-attrition-round53-analysis-contract.txt
        -DV5_ACTION_OBSERVATIONS=${SMAVE_FROZEN_TRANSITION_ATTRITION_V5}/action-observations.tsv
        -DV5_MODEL=${SMAVE_FROZEN_TRANSITION_ATTRITION_V5}/request-conditioned-model.txt
        -DV6_ACTION_OBSERVATIONS=${SMAVE_FROZEN_TRANSITION_ATTRITION_V6}/action-observations.tsv
        -DV6_MODEL=${SMAVE_FROZEN_TRANSITION_ATTRITION_V6}/request-conditioned-model.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_frozen_transition_attrition.cmake
    DEPENDS smave_suitesparse_request_conditioned_route_evidence
    VERBATIM)

set(SMAVE_JOINT_ROUTE_BUDGET_SHIFT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/joint-route-budget-shift")
add_custom_target(reproduce-joint-route-budget-shift
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_JOINT_ROUTE_BUDGET_SHIFT_DIR}"
    COMMAND $<TARGET_FILE:smave_joint_route_budget_shift_evidence>
        "${SMAVE_JOINT_ROUTE_BUDGET_SHIFT_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_JOINT_ROUTE_BUDGET_SHIFT_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_joint_route_budget_shift.cmake
    DEPENDS smave_joint_route_budget_shift_evidence
    VERBATIM)

set(SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/request-conditioned-joint-route")
add_custom_target(reproduce-request-conditioned-joint-route
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE_DIR}"
    COMMAND $<TARGET_FILE:smave_request_conditioned_joint_route_evidence>
        "${SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_REQUEST_CONDITIONED_JOINT_ROUTE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_request_conditioned_joint_route.cmake
    DEPENDS smave_request_conditioned_joint_route_evidence
    VERBATIM)

set(SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/suitesparse-request-conditioned-route-v6-reproduction")
add_custom_target(reproduce-suitesparse-request-conditioned-route
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE_DIR}"
    COMMAND $<TARGET_FILE:smave_suitesparse_request_conditioned_route_evidence>
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/suitesparse"
        "${SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_suitesparse_request_conditioned_route.cmake
    DEPENDS smave_suitesparse_request_conditioned_route_evidence
    VERBATIM)
set(SMAVE_SUITESPARSE_STRUCTURE_AUDIT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/suitesparse-structure-audit")
add_custom_target(reproduce-suitesparse-structure-audit
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "${SMAVE_SUITESPARSE_STRUCTURE_AUDIT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_SUITESPARSE_STRUCTURE_AUDIT_DIR}"
    COMMAND $<TARGET_FILE:smave_suitesparse_structure_audit>
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/suitesparse"
        "${SMAVE_SUITESPARSE_STRUCTURE_AUDIT_DIR}"
        50000
    DEPENDS smave_suitesparse_structure_audit
    VERBATIM)

set(SMAVE_ROUTER_SHIFT_DIR "${CMAKE_CURRENT_BINARY_DIR}/router-shift")
add_custom_target(reproduce-router-shift
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_ROUTER_SHIFT_DIR}"
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_router_shift.py
        --source ${CMAKE_CURRENT_BINARY_DIR}/phase4/source-repeat.competition
        --heldout ${CMAKE_CURRENT_BINARY_DIR}/phase4/heldout-traces/heldout.competition
        --output ${SMAVE_ROUTER_SHIFT_DIR}/evidence.txt
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_ROUTER_SHIFT_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_router_shift.cmake
    USES_TERMINAL
    VERBATIM)

set(SMAVE_COMPLETE_COST_DECOMPOSITION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/complete-cost-decomposition")
add_custom_target(reproduce-complete-cost-decomposition
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_COMPLETE_COST_DECOMPOSITION_DIR}"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_complete_cost_decomposition.py
        --linear ${CMAKE_CURRENT_BINARY_DIR}/phase5/benchmark-traces/operator-ablation.txt
        --nonlinear
            ${CMAKE_CURRENT_BINARY_DIR}/nonlinear-operator/benchmark-traces/operator-ablation.txt
        --output ${SMAVE_COMPLETE_COST_DECOMPOSITION_DIR}/evidence.txt
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_COMPLETE_COST_DECOMPOSITION_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_complete_cost_decomposition.cmake
    USES_TERMINAL
    VERBATIM)

add_custom_target(reproduce-gate-architecture
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_GATE_ARCHITECTURE_DIR}"
    COMMAND $<TARGET_FILE:smave_gate_architecture_evidence>
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo
        ${SMAVE_GATE_ARCHITECTURE_DIR}/evidence.txt
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_GATE_ARCHITECTURE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_gate_architecture.cmake
    DEPENDS smave_gate_architecture_evidence
    VERBATIM)

set(SMAVE_GATE_PARALLEL_SCALING_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/gate-parallel-scaling")
add_custom_target(reproduce-gate-parallel-scaling
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_GATE_PARALLEL_SCALING_DIR}"
    COMMAND $<TARGET_FILE:smave_gate_parallel_scaling_evidence>
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo
        ${SMAVE_GATE_PARALLEL_SCALING_DIR}/evidence.txt
    DEPENDS smave_gate_parallel_scaling_evidence
    VERBATIM)

if(TARGET smave_gate_process_scaling_evidence)
    set(SMAVE_GATE_PROCESS_SCALING_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/gate-process-scaling")
    add_custom_target(reproduce-gate-process-scaling
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SMAVE_GATE_PROCESS_SCALING_DIR}"
        COMMAND $<TARGET_FILE:smave_gate_process_scaling_evidence>
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo
            ${SMAVE_GATE_PROCESS_SCALING_DIR}/evidence.txt
        COMMAND python3
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_gate_process_scaling.py
            --evidence ${SMAVE_GATE_PROCESS_SCALING_DIR}/evidence.txt
            --tex ${CMAKE_CURRENT_SOURCE_DIR}/paper/generated/gate_process_scaling_values.tex
        COMMAND ${CMAKE_COMMAND}
            -DEVIDENCE=${SMAVE_GATE_PROCESS_SCALING_DIR}/evidence.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_gate_process_scaling.cmake
        DEPENDS smave_gate_process_scaling_evidence
        USES_TERMINAL
        VERBATIM)
endif()

if(TARGET smave_gate_network_authority_evidence)
    set(SMAVE_GATE_NETWORK_AUTHORITY_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/gate-network-authority")
    add_custom_target(reproduce-gate-network-authority
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SMAVE_GATE_NETWORK_AUTHORITY_DIR}"
        COMMAND python3
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_gate_network_authority.py
            --executable $<TARGET_FILE:smave_gate_network_authority_evidence>
            --linear ${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo
            --scenarios ${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
            --nonlinear ${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo
            --output ${SMAVE_GATE_NETWORK_AUTHORITY_DIR}/evidence.txt
        COMMAND ${CMAKE_COMMAND}
            -DEVIDENCE=${SMAVE_GATE_NETWORK_AUTHORITY_DIR}/evidence.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_gate_network_authority.cmake
        DEPENDS smave_gate_network_authority_evidence
        USES_TERMINAL
        VERBATIM)
endif()

if(TARGET smave_pdebench_advection_benchmark AND
   TARGET smave_pdebench_darcy_benchmark AND
   TARGET smave_pdebench_burgers_benchmark AND
   TARGET smave_pdebench_diffusion_sorption_benchmark AND
   TARGET smave_pdebench_shallow_water_benchmark AND
   TARGET smave_pdebench_ns_incompressible_benchmark AND
   TARGET smave_pdebench_cfd_1d_benchmark)
    add_custom_target(reproduce-pdebench-order-sensitivity
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/run_pdebench_order_sensitivity.sh
            ${CMAKE_CURRENT_BINARY_DIR} 30
        DEPENDS smave_pdebench_advection_benchmark
            smave_pdebench_darcy_benchmark
            smave_pdebench_burgers_benchmark
            smave_pdebench_diffusion_sorption_benchmark
            smave_pdebench_shallow_water_benchmark
            smave_pdebench_ns_incompressible_benchmark
            smave_pdebench_cfd_1d_benchmark
            pdebench-diffusion-sorption-learned-operator
        USES_TERMINAL)
endif()

set(SMAVE_RISK_ADAPTIVE_GATE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/risk-adaptive-gate")
add_custom_target(reproduce-risk-adaptive-gate
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_RISK_ADAPTIVE_GATE_DIR}"
    COMMAND $<TARGET_FILE:smave_risk_adaptive_gate_evidence>
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo
        ${SMAVE_RISK_ADAPTIVE_GATE_DIR}/evidence.txt
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_RISK_ADAPTIVE_GATE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_risk_adaptive_gate.cmake
    DEPENDS smave_risk_adaptive_gate_evidence
    VERBATIM)

add_executable(smave_pdebench_training_unit tests/pdebench_training_unit.cpp)
target_link_libraries(smave_pdebench_training_unit PRIVATE smave_core)
target_compile_options(smave_pdebench_training_unit PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME smave_pdebench_training_unit COMMAND smave_pdebench_training_unit)

add_executable(smave_residency_evidence tests/residency_evidence.cpp)
target_link_libraries(smave_residency_evidence PRIVATE smave_core)

add_executable(smave_sparse_direct_evidence tests/sparse_direct_evidence.cpp)
target_link_libraries(smave_sparse_direct_evidence PRIVATE smave_core)
add_executable(smave_industrial_sparse_evidence
    tests/industrial_sparse_evidence.cpp)
target_link_libraries(smave_industrial_sparse_evidence PRIVATE smave_core)
add_executable(smave_complementarity_evidence
    tests/complementarity_evidence.cpp)
target_link_libraries(smave_complementarity_evidence PRIVATE smave_core)
add_executable(smave_index_two_dae_evidence
    tests/index_two_dae_evidence.cpp)
target_link_libraries(smave_index_two_dae_evidence PRIVATE smave_core)
if(APPLE)
    add_executable(smave_device_execution_evidence
        tests/device_execution_evidence.cpp)
    target_link_libraries(smave_device_execution_evidence PRIVATE smave_core)
endif()
add_executable(smave_equation_routing_evidence tests/equation_routing_evidence.cpp)
target_link_libraries(smave_equation_routing_evidence PRIVATE smave_core)
add_executable(smave_large_sparse_evidence tests/large_sparse_evidence.cpp)
target_link_libraries(smave_large_sparse_evidence PRIVATE smave_core)
add_executable(smave_amg_backend_evidence tests/amg_backend_evidence.cpp)
target_link_libraries(smave_amg_backend_evidence PRIVATE smave_core)
target_compile_options(smave_amg_backend_evidence PRIVATE -Wall -Wextra -Wpedantic)
add_executable(smave_large_nonlinear_evidence tests/large_nonlinear_evidence.cpp)
target_link_libraries(smave_large_nonlinear_evidence PRIVATE smave_core)
add_executable(smave_large_dae_evidence tests/large_dae_evidence.cpp)
target_link_libraries(smave_large_dae_evidence PRIVATE smave_core)
add_executable(smave_fully_implicit_dae_evidence
    tests/fully_implicit_dae_evidence.cpp)
target_link_libraries(smave_fully_implicit_dae_evidence PRIVATE smave_core)
add_executable(smave_fully_implicit_dae_event_evidence
    tests/fully_implicit_dae_event_evidence.cpp)
target_link_libraries(smave_fully_implicit_dae_event_evidence PRIVATE smave_core)
add_executable(smave_fully_implicit_dae_multigrid_evidence
    tests/fully_implicit_dae_multigrid_evidence.cpp)
target_link_libraries(smave_fully_implicit_dae_multigrid_evidence PRIVATE smave_core)
add_executable(smave_large_dae_initialization_evidence
    tests/large_dae_initialization_evidence.cpp)
target_link_libraries(smave_large_dae_initialization_evidence PRIVATE smave_core)
add_executable(smave_large_dae_event_evidence tests/large_dae_event_evidence.cpp)
target_link_libraries(smave_large_dae_event_evidence PRIVATE smave_core)
target_compile_options(smave_sparse_direct_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_industrial_sparse_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_complementarity_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_index_two_dae_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
if(APPLE)
    target_compile_options(smave_device_execution_evidence
        PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_options(smave_residency_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_equation_routing_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_large_sparse_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_large_nonlinear_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_large_dae_evidence PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_fully_implicit_dae_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_fully_implicit_dae_event_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_fully_implicit_dae_multigrid_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_large_dae_initialization_evidence
    PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(smave_large_dae_event_evidence PRIVATE -Wall -Wextra -Wpedantic)

set(SMAVE_BENCH_SUPERLU_ROOT "$ENV{HOME}/.local/smave-bench/superlu-7.0.1-accelerate"
    CACHE PATH "Optional standalone SuperLU installation for benchmark comparison")
# Detect SuperLU from either the standalone install or a system package.
# Debian/Ubuntu libsuperlu-dev installs headers under /usr/include/superlu/
# while the source uses #include <slu_ddefs.h>, so PATH_SUFFIXES superlu makes
# the include directory resolve to /usr/include/superlu directly.
find_path(SMAVE_BENCH_SUPERLU_INCLUDE slu_ddefs.h
    HINTS "${SMAVE_BENCH_SUPERLU_ROOT}/include"
    PATH_SUFFIXES superlu)
find_library(SMAVE_BENCH_SUPERLU_LIBRARY superlu
    HINTS "${SMAVE_BENCH_SUPERLU_ROOT}/lib")
add_executable(smave_sparse_case_benchmark tests/sparse_case_benchmark.cpp)
target_link_libraries(smave_sparse_case_benchmark PRIVATE smave_core)
target_compile_options(smave_sparse_case_benchmark PRIVATE -Wall -Wextra -Wpedantic)
add_executable(smave_sparse_suite_benchmark tests/sparse_suite_benchmark.cpp)
target_link_libraries(smave_sparse_suite_benchmark PRIVATE smave_core)
target_compile_options(smave_sparse_suite_benchmark PRIVATE -Wall -Wextra -Wpedantic)
add_executable(smave_benchmark_readiness tests/benchmark_readiness.cpp)
target_link_libraries(smave_benchmark_readiness PRIVATE smave_core)
target_compile_options(smave_benchmark_readiness PRIVATE -Wall -Wextra -Wpedantic)

set(SMAVE_BENCHMARK_READINESS_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-readiness")
add_custom_target(benchmark-readiness
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SMAVE_BENCHMARK_READINESS_DIR}
    COMMAND $<TARGET_FILE:smave_benchmark_readiness>
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
        ${SMAVE_BENCHMARK_READINESS_DIR}/manifest.txt
    DEPENDS smave_benchmark_readiness
    USES_TERMINAL)
if(SMAVE_BENCH_SUPERLU_INCLUDE AND SMAVE_BENCH_SUPERLU_LIBRARY)
    target_include_directories(smave_core PRIVATE
        "${SMAVE_BENCH_SUPERLU_INCLUDE}")
    target_link_libraries(smave_core PRIVATE
        "${SMAVE_BENCH_SUPERLU_LIBRARY}")
    target_compile_definitions(smave_core PRIVATE
        SMAVE_HAVE_SUPERLU_SPARSE=1)
    target_include_directories(smave_sparse_case_benchmark PRIVATE
        "${SMAVE_BENCH_SUPERLU_INCLUDE}")
    target_link_libraries(smave_sparse_case_benchmark PRIVATE
        "${SMAVE_BENCH_SUPERLU_LIBRARY}")
    if(APPLE)
        target_link_libraries(smave_sparse_case_benchmark PRIVATE
            ${SMAVE_ACCELERATE_FRAMEWORK})
    endif()
    target_compile_definitions(smave_sparse_case_benchmark PRIVATE
        SMAVE_HAVE_BENCH_SUPERLU=1)
endif()
set(SMAVE_BENCH_PETSC_ROOT "$ENV{HOME}/.local/smave-bench/petsc-3.25.3"
    CACHE PATH "Optional standalone PETSc installation for benchmark comparison")
find_path(SMAVE_BENCH_PETSC_INCLUDE petscksp.h
    PATHS "${SMAVE_BENCH_PETSC_ROOT}/include" NO_DEFAULT_PATH)
find_library(SMAVE_BENCH_PETSC_LIBRARY petsc
    PATHS "${SMAVE_BENCH_PETSC_ROOT}/lib" NO_DEFAULT_PATH)
find_library(SMAVE_BENCH_PETSC_LAPACK_LIBRARY f2clapack
    PATHS "${SMAVE_BENCH_PETSC_ROOT}/lib" NO_DEFAULT_PATH)
find_library(SMAVE_BENCH_PETSC_BLAS_LIBRARY f2cblas
    PATHS "${SMAVE_BENCH_PETSC_ROOT}/lib" NO_DEFAULT_PATH)
if(SMAVE_BENCH_PETSC_INCLUDE AND SMAVE_BENCH_PETSC_LIBRARY AND
   SMAVE_BENCH_PETSC_LAPACK_LIBRARY AND SMAVE_BENCH_PETSC_BLAS_LIBRARY)
    target_include_directories(smave_sparse_case_benchmark PRIVATE
        "${SMAVE_BENCH_PETSC_INCLUDE}")
    target_link_libraries(smave_sparse_case_benchmark PRIVATE
        "${SMAVE_BENCH_PETSC_LIBRARY}"
        "${SMAVE_BENCH_PETSC_LAPACK_LIBRARY}"
        "${SMAVE_BENCH_PETSC_BLAS_LIBRARY}")
    target_compile_definitions(smave_sparse_case_benchmark PRIVATE
        SMAVE_HAVE_BENCH_PETSC=1)
    add_executable(smave_petsc_ts_comparison tests/petsc_ts_smave_comparison.cpp)
    add_library(smave_petsc_ex5_equations OBJECT benchmark/petsc-ts/ex5.c)
    target_include_directories(smave_petsc_ex5_equations PRIVATE
        "${SMAVE_BENCH_PETSC_INCLUDE}")
    target_compile_definitions(smave_petsc_ex5_equations PRIVATE
        main=smave_petsc_ex5_original_main)
    target_sources(smave_petsc_ts_comparison PRIVATE
        $<TARGET_OBJECTS:smave_petsc_ex5_equations>)
    add_library(smave_petsc_ex30_equations OBJECT benchmark/petsc-ts/ex30.c)
    target_include_directories(smave_petsc_ex30_equations PRIVATE
        "${SMAVE_BENCH_PETSC_INCLUDE}")
    target_compile_definitions(smave_petsc_ex30_equations PRIVATE
        main=smave_petsc_ex30_original_main
        SMAVE_EX30_COMPARISON_BRIDGE=1)
    add_executable(smave_petsc_ex30_comparison
        tests/petsc_ex30_comparison.cpp
        $<TARGET_OBJECTS:smave_petsc_ex30_equations>)
    target_include_directories(smave_petsc_ex30_comparison PRIVATE
        "${SMAVE_BENCH_PETSC_INCLUDE}")
    target_link_libraries(smave_petsc_ex30_comparison PRIVATE smave_core
        "${SMAVE_BENCH_PETSC_LIBRARY}"
        "${SMAVE_BENCH_PETSC_LAPACK_LIBRARY}"
        "${SMAVE_BENCH_PETSC_BLAS_LIBRARY}")
    target_include_directories(smave_petsc_ts_comparison PRIVATE
        "${SMAVE_BENCH_PETSC_INCLUDE}")
    target_link_libraries(smave_petsc_ts_comparison PRIVATE smave_core
        "${SMAVE_BENCH_PETSC_LIBRARY}"
        "${SMAVE_BENCH_PETSC_LAPACK_LIBRARY}"
        "${SMAVE_BENCH_PETSC_BLAS_LIBRARY}")
endif()

set(SMAVE_SPARSE_BENCHMARK_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/benchmark-suitesparse-routed")
add_custom_target(benchmark-suitesparse-full
    COMMAND $<TARGET_FILE:smave_sparse_suite_benchmark>
        --case-runner $<TARGET_FILE:smave_sparse_case_benchmark>
        --suite ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/suitesparse
        --output ${SMAVE_SPARSE_BENCHMARK_DIR}
        --timeout-seconds 1800
        --max-iterations 1000
        --restart 40
        --direct-limit 150000
    DEPENDS smave_sparse_case_benchmark smave_sparse_suite_benchmark
    USES_TERMINAL)

set(SMAVE_PETSC_TS_BENCHMARK_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-petsc-ts")
add_custom_target(benchmark-petsc-ts-full
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/benchmark/petsc-ts
        -DOUTPUT=${SMAVE_PETSC_TS_BENCHMARK_DIR}
        -DPETSC_ROOT=${SMAVE_BENCH_PETSC_ROOT}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_petsc_ts_suite.cmake
    USES_TERMINAL)
add_custom_target(benchmark-petsc-ts-classification
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/benchmark/petsc-ts
        -DOUTPUT=${SMAVE_PETSC_TS_BENCHMARK_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/classify_petsc_ts.cmake
    USES_TERMINAL)
if(TARGET smave_petsc_ts_comparison)
    add_custom_target(benchmark-petsc-ts-comparison
        COMMAND $<TARGET_FILE:smave_petsc_ts_comparison>
            ${SMAVE_PETSC_TS_BENCHMARK_DIR}/comparison-core.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/petsc-ts/ex5_control.txt
        COMMAND $<TARGET_FILE:smave_petsc_ex30_comparison>
            ${SMAVE_PETSC_TS_BENCHMARK_DIR}/comparison-ex30.txt
            -dim 2
            -petscspace_degree 3
            -dm_landau_num_species_grid 1
            -dm_refine 0
            -number_particles_per_dimension 5
            -dm_landau_batch_size 1
            -number_spatial_vertices 1
            -dm_landau_n 1
            -dm_landau_thermal_temps 1
            -ts_time_step 0.1
            -ts_max_steps 1
            -ts_type beuler
            -dm_landau_device_type cpu
            -pc_type jacobi
        COMMAND ${CMAKE_COMMAND}
            -DCORE=${SMAVE_PETSC_TS_BENCHMARK_DIR}/comparison-core.txt
            -DEX30=${SMAVE_PETSC_TS_BENCHMARK_DIR}/comparison-ex30.txt
            -DOUTPUT=${SMAVE_PETSC_TS_BENCHMARK_DIR}/comparison.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/merge_petsc_ts_comparisons.cmake
        DEPENDS smave_petsc_ts_comparison smave_petsc_ex30_comparison
        USES_TERMINAL)
endif()

set(SMAVE_OPENMODELICA_MSL_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/benchmark-multiphysics-msl")
add_custom_target(benchmark-multiphysics-msl-full
    COMMAND ${CMAKE_COMMAND} -E env UID=$ENV{UID} GID=$ENV{GID}
        ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/benchmark/multiphysics-msl
        -DOUTPUT=${SMAVE_OPENMODELICA_MSL_DIR}
        -DIMAGE=openmodelica/openmodelica:v1.27.0-minimal
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_openmodelica_msl_suite.cmake
    USES_TERMINAL)
add_custom_target(benchmark-multiphysics-msl-comparison
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_OPENMODELICA_MSL_DIR}
        -DIMAGE=openmodelica/openmodelica:v1.27.0-minimal
        -DCXX=${CMAKE_CXX_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_openmodelica_msl_comparison.cmake
    DEPENDS benchmark-multiphysics-msl-full
    USES_TERMINAL)

set(SMAVE_BENCH_AMPL_BIN
    "$ENV{HOME}/Library/Python/3.9/lib/python/site-packages/ampl_module_base/bin/ampl"
    CACHE FILEPATH "AMPL executable used for COPS benchmark translation")
set(SMAVE_BENCH_AMPL_SOLVER_BIN
    "$ENV{HOME}/Library/Python/3.9/lib/python/site-packages/ampl_module_coin/bin"
    CACHE PATH "AMPL solver module directory used for COPS")
set(SMAVE_COPS_BENCHMARK_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-cops")
add_custom_target(benchmark-cops-full
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/benchmark/cops/cops-3.0
        -DOUTPUT=${SMAVE_COPS_BENCHMARK_DIR}
        -DAMPL_BIN=${SMAVE_BENCH_AMPL_BIN}
        -DSOLVER_BIN_DIR=${SMAVE_BENCH_AMPL_SOLVER_BIN}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cops_suite.cmake
    USES_TERMINAL)

set(SMAVE_BENCH_JULIA_BIN
    "$ENV{HOME}/.local/smave-bench/julia-1.11.7/bin/julia"
    CACHE FILEPATH "Julia executable used for license-free COPS benchmarks")
set(SMAVE_COPS_JULIA_ENV
    "$ENV{HOME}/.local/smave-bench/cops-julia-env"
    CACHE PATH "Julia environment used for COPS benchmarks")
set(SMAVE_COPS_JULIA_SOURCE
    "$ENV{HOME}/.local/smave-bench/COPSBenchmark.jl"
    CACHE PATH "Pinned COPSBenchmark.jl source used for COPS benchmarks")
set(SMAVE_COPS_JULIA_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-cops-julia")
add_custom_target(benchmark-cops-julia-setup
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${SMAVE_COPS_JULIA_SOURCE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/prepare_cops_julia_source.cmake
    COMMAND "${SMAVE_BENCH_JULIA_BIN}" --startup-file=no
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/setup_cops_julia.jl
        "${SMAVE_COPS_JULIA_ENV}"
        "${SMAVE_COPS_JULIA_SOURCE}"
    USES_TERMINAL)
add_custom_target(benchmark-cops-julia-baseline
    COMMAND ${CMAKE_COMMAND}
        -DJULIA=${SMAVE_BENCH_JULIA_BIN}
        -DENVIRONMENT=${SMAVE_COPS_JULIA_ENV}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_COPS_JULIA_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cops_julia_suite.cmake
    DEPENDS benchmark-cops-julia-setup
    USES_TERMINAL)
add_custom_target(benchmark-cops-kkt-comparison
    COMMAND ${CMAKE_COMMAND}
        -DJULIA=${SMAVE_BENCH_JULIA_BIN}
        -DENVIRONMENT=${SMAVE_COPS_JULIA_ENV}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_COPS_JULIA_DIR}
        -DCASE_RUNNER=$<TARGET_FILE:smave_sparse_case_benchmark>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cops_kkt_comparison.cmake
    DEPENDS benchmark-cops-julia-setup smave_sparse_case_benchmark
    USES_TERMINAL)
add_library(smave_cops_madnlp_bridge SHARED
    tests/cops_madnlp_smave_bridge.cpp)
target_link_libraries(smave_cops_madnlp_bridge PRIVATE smave_core)
set_target_properties(smave_cops_madnlp_bridge PROPERTIES
    CXX_VISIBILITY_PRESET default)
add_custom_target(benchmark-cops-madnlp-comparison
    COMMAND ${CMAKE_COMMAND}
        -DJULIA=${SMAVE_BENCH_JULIA_BIN}
        -DENVIRONMENT=${SMAVE_COPS_JULIA_ENV}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_COPS_JULIA_DIR}
        -DBRIDGE=$<TARGET_FILE:smave_cops_madnlp_bridge>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cops_madnlp_suite.cmake
    DEPENDS benchmark-cops-julia-setup smave_cops_madnlp_bridge
    USES_TERMINAL)

set(SMAVE_BENCH_H5DUMP
    "$ENV{HOME}/.local/smave-bench/hdf5-2.1.1/bin/h5dump"
    CACHE FILEPATH "h5dump executable used for PDEBench verification")
set(SMAVE_BENCH_HDF5_ROOT "$ENV{HOME}/.local/smave-bench/hdf5-2.1.1"
    CACHE PATH "Standalone HDF5 installation used for PDEBench")
set(SMAVE_BENCH_HDF5_PLUGIN_PATH
    "$ENV{HOME}/.local/smave-bench/hdf5-lzf-plugin"
    CACHE PATH "HDF5 filter plugin directory used for compressed PDEBench assets")
set(SMAVE_BENCH_HDF5_LZF_SOURCE
    "$ENV{HOME}/.local/smave-bench/hdf5-lzf-src"
    CACHE PATH "Source cache used to build the PDEBench LZF HDF5 plugin")
add_custom_target(benchmark-pdebench-lzf-setup
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/setup_hdf5_lzf_plugin.sh
        ${SMAVE_BENCH_HDF5_ROOT}
        ${SMAVE_BENCH_HDF5_LZF_SOURCE}
        ${SMAVE_BENCH_HDF5_PLUGIN_PATH}
    USES_TERMINAL)
find_path(SMAVE_BENCH_HDF5_INCLUDE hdf5.h
    PATHS "${SMAVE_BENCH_HDF5_ROOT}/include" NO_DEFAULT_PATH)
find_library(SMAVE_BENCH_HDF5_LIBRARY hdf5
    PATHS "${SMAVE_BENCH_HDF5_ROOT}/lib" NO_DEFAULT_PATH)
if(SMAVE_BENCH_HDF5_INCLUDE AND SMAVE_BENCH_HDF5_LIBRARY)
    add_executable(smave_pdebench_advection_benchmark
        tests/pdebench_advection_benchmark.cpp)
    add_executable(smave_pdebench_darcy_benchmark
        tests/pdebench_darcy_benchmark.cpp)
    add_executable(smave_pdebench_burgers_benchmark
        tests/pdebench_burgers_benchmark.cpp)
    add_executable(smave_pdebench_diffusion_sorption_benchmark
        tests/pdebench_diffusion_sorption_benchmark.cpp)
    add_executable(smave_pdebench_shallow_water_benchmark
        tests/pdebench_shallow_water_benchmark.cpp)
    add_executable(smave_pdebench_ns_incompressible_benchmark
        tests/pdebench_ns_incompressible_benchmark.cpp)
    add_executable(smave_pdebench_cfd_1d_benchmark
        tests/pdebench_cfd_1d_benchmark.cpp)
    add_executable(smave_pdebench_training_extract
        tests/pdebench_training_extract.cpp)
    add_executable(smave_pdebench_advection_labels
        tests/pdebench_advection_labels.cpp)
    add_executable(smave_pdebench_advection_fit
        tests/pdebench_advection_fit.cpp)
    add_executable(smave_pdebench_advection_device_probe
        tests/pdebench_advection_device_probe.cpp)
    add_executable(smave_pdebench_burgers_labels
        tests/pdebench_burgers_labels.cpp)
    add_executable(smave_pdebench_burgers_fit
        tests/pdebench_burgers_fit.cpp)
    add_executable(smave_pdebench_burgers_device_probe
        tests/pdebench_burgers_device_probe.cpp)
    add_executable(smave_pdebench_diffusion_sorption_labels
        tests/pdebench_diffusion_sorption_labels.cpp)
    add_executable(smave_pdebench_diffusion_sorption_fit
        tests/pdebench_diffusion_sorption_fit.cpp)
    add_executable(smave_pdebench_diffusion_sorption_device_probe
        tests/pdebench_diffusion_sorption_device_probe.cpp)
    add_executable(smave_pdebench_darcy_labels
        tests/pdebench_darcy_labels.cpp)
    add_executable(smave_pdebench_darcy_fit
        tests/pdebench_darcy_fit.cpp)
    add_executable(smave_pdebench_shallow_water_labels
        tests/pdebench_shallow_water_labels.cpp)
    add_executable(smave_pdebench_shallow_water_fit
        tests/pdebench_shallow_water_fit.cpp)
    add_executable(smave_pdebench_shallow_water_device_probe
        tests/pdebench_shallow_water_device_probe.cpp)
    add_executable(smave_pdebench_ns_incompressible_labels
        tests/pdebench_ns_incompressible_labels.cpp)
    add_executable(smave_pdebench_ns_incompressible_fit
        tests/pdebench_ns_incompressible_fit.cpp)
    target_include_directories(smave_pdebench_advection_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_darcy_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_burgers_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_diffusion_sorption_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_shallow_water_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_ns_incompressible_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_cfd_1d_benchmark PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_training_extract PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_advection_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_advection_device_probe PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_burgers_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_burgers_device_probe PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_diffusion_sorption_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(
        smave_pdebench_diffusion_sorption_device_probe PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_darcy_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_shallow_water_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_shallow_water_device_probe PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_include_directories(smave_pdebench_ns_incompressible_labels PRIVATE
        "${SMAVE_BENCH_HDF5_INCLUDE}")
    target_link_libraries(smave_pdebench_advection_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_darcy_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_burgers_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_diffusion_sorption_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_shallow_water_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_ns_incompressible_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_cfd_1d_benchmark PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_training_extract PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_advection_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_advection_fit PRIVATE smave_core)
    target_link_libraries(smave_pdebench_advection_device_probe PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_burgers_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_burgers_fit PRIVATE smave_core)
    target_link_libraries(smave_pdebench_burgers_device_probe PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_diffusion_sorption_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_diffusion_sorption_fit PRIVATE smave_core)
    target_link_libraries(smave_pdebench_diffusion_sorption_device_probe PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_darcy_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_darcy_fit PRIVATE smave_core)
    target_link_libraries(smave_pdebench_shallow_water_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_shallow_water_fit PRIVATE smave_core)
    target_link_libraries(smave_pdebench_shallow_water_device_probe PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_ns_incompressible_labels PRIVATE
        smave_core "${SMAVE_BENCH_HDF5_LIBRARY}")
    target_link_libraries(smave_pdebench_ns_incompressible_fit PRIVATE smave_core)
    target_compile_options(smave_pdebench_advection_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_darcy_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_burgers_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_diffusion_sorption_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_shallow_water_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_ns_incompressible_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_cfd_1d_benchmark PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_training_extract PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_advection_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_advection_fit PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_advection_device_probe PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_burgers_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_burgers_fit PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_burgers_device_probe PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_diffusion_sorption_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_diffusion_sorption_fit PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(
        smave_pdebench_diffusion_sorption_device_probe PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_darcy_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_darcy_fit PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_shallow_water_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_shallow_water_fit PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_shallow_water_device_probe PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_ns_incompressible_labels PRIVATE
        -Wall -Wextra -Wpedantic)
    target_compile_options(smave_pdebench_ns_incompressible_fit PRIVATE
        -Wall -Wextra -Wpedantic)
endif()

if(TARGET smave_pdebench_advection_labels)
    add_custom_target(pdebench-advection-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND $<TARGET_FILE:smave_pdebench_advection_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection-train"
            3 32 50
        DEPENDS smave_pdebench_advection_labels
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_advection_fit)
    add_custom_target(pdebench-advection-learned-operator
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND $<TARGET_FILE:smave_pdebench_advection_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection-heldout"
            35 8 50
        COMMAND $<TARGET_FILE:smave_pdebench_advection_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection.operator"
        DEPENDS pdebench-advection-solver-labels smave_pdebench_advection_fit
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_advection_device_probe)
    add_custom_target(pdebench-advection-device-probe
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/advection-coreml"
        COMMAND $<TARGET_FILE:smave_pdebench_advection_device_probe>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection.operator"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/advection-coreml"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/advection.txt"
        DEPENDS pdebench-advection-learned-operator
            smave_pdebench_advection_device_probe
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_burgers_labels)
    add_custom_target(pdebench-burgers-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND $<TARGET_FILE:smave_pdebench_burgers_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers-train"
            3 32 50
        COMMAND $<TARGET_FILE:smave_pdebench_burgers_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers-heldout"
            35 8 50
        DEPENDS smave_pdebench_burgers_labels
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_burgers_fit)
    add_custom_target(pdebench-burgers-learned-operator
        COMMAND $<TARGET_FILE:smave_pdebench_burgers_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers.operator"
        DEPENDS pdebench-burgers-solver-labels smave_pdebench_burgers_fit
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_burgers_device_probe)
    add_custom_target(pdebench-burgers-device-probe
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device"
        COMMAND $<TARGET_FILE:smave_pdebench_burgers_device_probe>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers.operator"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/burgers.txt"
        DEPENDS pdebench-burgers-learned-operator
            smave_pdebench_burgers_device_probe
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_diffusion_sorption_labels)
    add_custom_target(pdebench-diffusion-sorption-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_diffusion_sorption_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption-train"
            3 32 50
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_diffusion_sorption_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption-heldout"
            35 8 50
        DEPENDS smave_pdebench_diffusion_sorption_labels
            benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_diffusion_sorption_fit)
    add_custom_target(pdebench-diffusion-sorption-learned-operator
        COMMAND $<TARGET_FILE:smave_pdebench_diffusion_sorption_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption.operator"
        DEPENDS pdebench-diffusion-sorption-solver-labels
            smave_pdebench_diffusion_sorption_fit
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_diffusion_sorption_device_probe)
    add_custom_target(pdebench-diffusion-sorption-device-probe
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device"
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_diffusion_sorption_device_probe>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption.operator"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/diffusion-sorption.txt"
        DEPENDS pdebench-diffusion-sorption-learned-operator
            smave_pdebench_diffusion_sorption_device_probe
            benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_darcy_labels)
    add_custom_target(pdebench-darcy-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND $<TARGET_FILE:smave_pdebench_darcy_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy-train"
            9000 512
        COMMAND $<TARGET_FILE:smave_pdebench_darcy_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy-heldout"
            9512 64
        DEPENDS smave_pdebench_darcy_labels
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_darcy_fit)
    add_custom_target(pdebench-darcy-learned-operator
        COMMAND $<TARGET_FILE:smave_pdebench_darcy_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy.operator"
        DEPENDS pdebench-darcy-solver-labels smave_pdebench_darcy_fit
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_shallow_water_labels)
    add_custom_target(pdebench-shallow-water-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND ${CMAKE_COMMAND} -E env HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_shallow_water_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/shallow-water/2D_rdb.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water-train"
            3 32 20
        COMMAND ${CMAKE_COMMAND} -E env HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_shallow_water_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/shallow-water/2D_rdb.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water-heldout"
            35 8 20
        DEPENDS smave_pdebench_shallow_water_labels benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_shallow_water_fit)
    add_custom_target(pdebench-shallow-water-learned-operator
        COMMAND $<TARGET_FILE:smave_pdebench_shallow_water_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water.operator"
        DEPENDS pdebench-shallow-water-solver-labels
            smave_pdebench_shallow_water_fit
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_shallow_water_device_probe)
    add_custom_target(pdebench-shallow-water-device-probe
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/shallow-water-coreml"
        COMMAND ${CMAKE_COMMAND} -E env HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_shallow_water_device_probe>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/shallow-water/2D_rdb.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water.operator"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/shallow-water-coreml"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-device/shallow-water.txt"
        DEPENDS pdebench-shallow-water-learned-operator
            smave_pdebench_shallow_water_device_probe benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_ns_incompressible_labels)
    add_custom_target(pdebench-ns-incompressible-solver-labels
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels"
        COMMAND ${CMAKE_COMMAND} -E env HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_ns_incompressible_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible-train"
            2 20
        COMMAND ${CMAKE_COMMAND} -E env HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_ns_incompressible_labels>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible-heldout"
            3 20
        DEPENDS smave_pdebench_ns_incompressible_labels benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

if(TARGET smave_pdebench_ns_incompressible_fit)
    add_custom_target(pdebench-ns-incompressible-learned-operator
        COMMAND $<TARGET_FILE:smave_pdebench_ns_incompressible_fit>
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible-train"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible-heldout"
            "${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible.operator"
        DEPENDS pdebench-ns-incompressible-solver-labels
            smave_pdebench_ns_incompressible_fit
        USES_TERMINAL)
endif()
set(SMAVE_PDEBENCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-pdebench")
add_custom_target(benchmark-pdebench-download
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/download_parallel_resume.sh
    USES_TERMINAL)
add_custom_target(benchmark-pdebench-verify
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench
        -DOUTPUT=${SMAVE_PDEBENCH_DIR}
        -DH5DUMP=${SMAVE_BENCH_H5DUMP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_pdebench_data.cmake
    USES_TERMINAL)
add_custom_target(benchmark-data-lock
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/data-lock/verify_data_lock.py
        --root ${CMAKE_CURRENT_SOURCE_DIR}
        --output ${CMAKE_CURRENT_BINARY_DIR}/data-lock/evidence.txt
        --verify-upstream
    USES_TERMINAL)
if(TARGET smave_pdebench_advection_benchmark)
    add_custom_target(benchmark-pdebench-advection
        COMMAND $<TARGET_FILE:smave_pdebench_advection_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5
            ${SMAVE_PDEBENCH_DIR}/advection-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/advection.operator
        DEPENDS smave_pdebench_advection_benchmark
            pdebench-advection-learned-operator
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_darcy_benchmark)
    add_custom_target(benchmark-pdebench-darcy
        COMMAND $<TARGET_FILE:smave_pdebench_darcy_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5
            ${SMAVE_PDEBENCH_DIR}/darcy-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/darcy.operator
        DEPENDS smave_pdebench_darcy_benchmark
            pdebench-darcy-learned-operator
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_burgers_benchmark)
    add_custom_target(benchmark-pdebench-burgers
        COMMAND $<TARGET_FILE:smave_pdebench_burgers_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5
            ${SMAVE_PDEBENCH_DIR}/burgers-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/burgers.operator
        DEPENDS smave_pdebench_burgers_benchmark
            pdebench-burgers-learned-operator
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_diffusion_sorption_benchmark)
    add_custom_target(benchmark-pdebench-diffusion-sorption
        COMMAND $<TARGET_FILE:smave_pdebench_diffusion_sorption_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5
            ${SMAVE_PDEBENCH_DIR}/diffusion-sorption-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/diffusion-sorption.operator
        DEPENDS smave_pdebench_diffusion_sorption_benchmark
            pdebench-diffusion-sorption-learned-operator
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_shallow_water_benchmark)
    add_custom_target(benchmark-pdebench-shallow-water
        COMMAND $<TARGET_FILE:smave_pdebench_shallow_water_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/shallow-water/2D_rdb.h5
            ${SMAVE_PDEBENCH_DIR}/shallow-water-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/shallow-water.operator
        DEPENDS smave_pdebench_shallow_water_benchmark
            pdebench-shallow-water-learned-operator
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_ns_incompressible_benchmark)
    add_custom_target(benchmark-pdebench-ns-incompressible
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_ns_incompressible_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5
            ${SMAVE_PDEBENCH_DIR}/ns-incompressible-summary.txt
            ${CMAKE_CURRENT_BINARY_DIR}/pdebench-solver-labels/ns-incompressible.operator
        DEPENDS smave_pdebench_ns_incompressible_benchmark
            pdebench-ns-incompressible-learned-operator
            benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()
if(TARGET smave_pdebench_cfd_1d_benchmark)
    add_custom_target(benchmark-pdebench-cfd-1d
        COMMAND $<TARGET_FILE:smave_pdebench_cfd_1d_benchmark>
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/CFD/1D_CFD_Rand_Train.hdf5
            ${SMAVE_PDEBENCH_DIR}/cfd-1d-summary.txt
        DEPENDS smave_pdebench_cfd_1d_benchmark
        USES_TERMINAL)
endif()

set(SMAVE_PDEBENCH_COMPARISON_TARGETS)
foreach(target IN ITEMS
    benchmark-pdebench-advection
    benchmark-pdebench-burgers
    benchmark-pdebench-darcy
    benchmark-pdebench-diffusion-sorption
    benchmark-pdebench-shallow-water
    benchmark-pdebench-ns-incompressible
    benchmark-pdebench-cfd-1d)
    if(TARGET ${target})
        list(APPEND SMAVE_PDEBENCH_COMPARISON_TARGETS ${target})
    endif()
endforeach()
if(SMAVE_PDEBENCH_COMPARISON_TARGETS)
    add_custom_target(benchmark-pdebench-comparisons
        DEPENDS ${SMAVE_PDEBENCH_COMPARISON_TARGETS})
endif()

if(TARGET smave_pdebench_training_extract)
    set(SMAVE_PDEBENCH_TRAINING_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/pdebench-training")
    add_custom_target(pdebench-training-smoke
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SMAVE_PDEBENCH_TRAINING_DIR}"
        COMMAND $<TARGET_FILE:smave_pdebench_training_extract>
            advection
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/advection" 64 3
        COMMAND $<TARGET_FILE:smave_pdebench_training_extract>
            burgers
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/burgers" 64 3
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_training_extract>
            diffusion-sorption
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/diffusion-sorption" 64 3
        COMMAND $<TARGET_FILE:smave_pdebench_training_extract>
            darcy
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/darcy" 32 3
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_training_extract>
            shallow-water
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/shallow-water/2D_rdb.h5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/shallow-water" 32 3
        COMMAND ${CMAKE_COMMAND} -E env
            HDF5_PLUGIN_PATH=${SMAVE_BENCH_HDF5_PLUGIN_PATH}
            $<TARGET_FILE:smave_pdebench_training_extract>
            ns-incompressible
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5
            "${SMAVE_PDEBENCH_TRAINING_DIR}/ns-incompressible" 32 3
        COMMAND ${CMAKE_COMMAND}
            -DDIRECTORY=${SMAVE_PDEBENCH_TRAINING_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_pdebench_training.cmake
        DEPENDS smave_pdebench_training_extract benchmark-pdebench-lzf-setup
        USES_TERMINAL)
endif()

set(SMAVE_BENCHMARK_OVERALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark-overall")
add_custom_target(benchmark-report
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_BENCHMARK_OVERALL_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_benchmark_overall.cmake
    DEPENDS benchmark-petsc-ts-classification
    USES_TERMINAL)
add_custom_target(benchmark-100x-gate
    COMMAND ${CMAKE_COMMAND}
        -DSUMMARY=${SMAVE_BENCHMARK_OVERALL_DIR}/summary.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_benchmark_100x.cmake
    DEPENDS benchmark-report
    USES_TERMINAL)
add_custom_target(benchmark-100x-feasibility
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_benchmark_100x_feasibility.sh
        ${SMAVE_PDEBENCH_DIR}
        ${SMAVE_BENCHMARK_OVERALL_DIR}/feasibility-100x.txt
    DEPENDS benchmark-report
    USES_TERMINAL)

set(SMAVE_CROSS_CHECK_TARGETS
    benchmark-suitesparse-full
    benchmark-petsc-ts-comparison
    benchmark-petsc-ts-classification
    benchmark-multiphysics-msl-comparison
    benchmark-cops-kkt-comparison)
if(TARGET benchmark-pdebench-comparisons)
    list(APPEND SMAVE_CROSS_CHECK_TARGETS benchmark-pdebench-comparisons)
endif()
add_custom_target(benchmark-cross-checks
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT=${SMAVE_BENCHMARK_OVERALL_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_benchmark_overall.cmake
    DEPENDS ${SMAVE_CROSS_CHECK_TARGETS}
    USES_TERMINAL)

set(SMAVE_EQUATION_ROUTING_DIR "${CMAKE_CURRENT_BINARY_DIR}/equation-routing")
add_custom_target(reproduce-equation-routing
    COMMAND $<TARGET_FILE:smave_equation_routing_evidence>
        "${SMAVE_EQUATION_ROUTING_DIR}"
    DEPENDS smave_equation_routing_evidence
    USES_TERMINAL
)

set(SMAVE_ROUTER_SHIFT_MATRIX_DIR "${CMAKE_CURRENT_BINARY_DIR}/router-shift-matrix")
add_custom_target(reproduce-router-shift-matrix
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_router_shift_matrix.py
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/inputs"
    COMMAND $<TARGET_FILE:smave> compile
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/inputs/source.mo"
        --top RouterShiftSource
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.ir"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.ir"
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.bundle"
    COMMAND $<TARGET_FILE:smave> compile
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/inputs/conditioning.mo"
        --top RouterShiftConditioning
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning.ir"
    COMMAND $<TARGET_FILE:smave> compile
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/inputs/topology.mo"
        --top RouterShiftTopology
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/inputs/scenarios"
        --dataset router-shift-matrix-scenarios
        --store "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/data-store"
        > "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset-snapshot.txt
        -DSTORE=${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/data-store
        -DDATASET_ID=router-shift-matrix-scenarios
        -DOUTPUT=${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> compete
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.ir"
        --dataset-store "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/data-store"
        --dataset-manifest "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset.manifest"
        --bundle "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.bundle"
        --repetitions 20
        --trace-dir "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source-traces"
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.competition"
    COMMAND $<TARGET_FILE:smave> evaluate-family-router
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.ir"
        --profile "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.competition"
        --heldout "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning.ir"
        --dataset-store "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/data-store"
        --dataset-manifest "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset.manifest"
        --minimum-similarity 0.99
        --minimum-speedup 1.001
        --repetitions 20
        --trace-dir "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning-traces"
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning-evaluation.txt"
    COMMAND $<TARGET_FILE:smave> evaluate-family-router
        "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.ir"
        --profile "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.competition"
        --heldout "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology.ir"
        --dataset-store "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/data-store"
        --dataset-manifest "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/dataset.manifest"
        --minimum-similarity 0.99
        --minimum-speedup 1.001
        --repetitions 20
        --trace-dir "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology-traces"
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology-evaluation.txt"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_router_shift_matrix.py
        --source "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/source.competition"
        --conditioning-competition
            "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning-traces/heldout.competition"
        --conditioning-evaluation
            "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/conditioning-evaluation.txt"
        --topology-competition
            "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology-traces/heldout.competition"
        --topology-evaluation
            "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/topology-evaluation.txt"
        --output "${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/evidence.txt"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_ROUTER_SHIFT_MATRIX_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_router_shift_matrix.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_STRUCTURAL_OOD_FILTER_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/structural-ood-filter")
add_custom_target(reproduce-structural-ood-filter
    COMMAND $<TARGET_FILE:smave_structural_ood_filter_evidence>
        "${SMAVE_STRUCTURAL_OOD_FILTER_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_STRUCTURAL_OOD_FILTER_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_structural_ood_filter.cmake
    DEPENDS smave_structural_ood_filter_evidence
    USES_TERMINAL
)

set(SMAVE_LARGE_SPARSE_DIR "${CMAKE_CURRENT_BINARY_DIR}/large-sparse")
add_custom_target(reproduce-large-sparse
    COMMAND $<TARGET_FILE:smave_large_sparse_evidence>
        "${SMAVE_LARGE_SPARSE_DIR}"
    DEPENDS smave_large_sparse_evidence
    USES_TERMINAL
)

set(SMAVE_AMG_BACKEND_DIR "${CMAKE_CURRENT_BINARY_DIR}/amg-backend")
add_custom_target(reproduce-amg-backend
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_AMG_BACKEND_DIR}"
    COMMAND $<TARGET_FILE:smave_amg_backend_evidence>
        "${SMAVE_AMG_BACKEND_DIR}/evidence.txt"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_AMG_BACKEND_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_amg_backend.cmake
    DEPENDS smave_amg_backend_evidence
    USES_TERMINAL
)

set(SMAVE_LARGE_NONLINEAR_DIR "${CMAKE_CURRENT_BINARY_DIR}/large-nonlinear")
add_custom_target(reproduce-large-nonlinear
    COMMAND $<TARGET_FILE:smave_large_nonlinear_evidence>
        "${SMAVE_LARGE_NONLINEAR_DIR}"
    DEPENDS smave_large_nonlinear_evidence
    USES_TERMINAL
)

set(SMAVE_LARGE_DAE_DIR "${CMAKE_CURRENT_BINARY_DIR}/large-dae")
add_custom_target(reproduce-large-dae
    COMMAND $<TARGET_FILE:smave_large_dae_evidence>
        "${SMAVE_LARGE_DAE_DIR}"
    DEPENDS smave_large_dae_evidence
    USES_TERMINAL
)

set(SMAVE_FULLY_IMPLICIT_DAE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/fully-implicit-dae")
add_custom_target(reproduce-fully-implicit-dae
    COMMAND $<TARGET_FILE:smave_fully_implicit_dae_evidence>
        "${SMAVE_FULLY_IMPLICIT_DAE_DIR}"
    DEPENDS smave_fully_implicit_dae_evidence
    USES_TERMINAL
)

set(SMAVE_FULLY_IMPLICIT_DAE_EVENT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/fully-implicit-dae-event")
add_custom_target(reproduce-fully-implicit-dae-events
    COMMAND $<TARGET_FILE:smave_fully_implicit_dae_event_evidence>
        "${SMAVE_FULLY_IMPLICIT_DAE_EVENT_DIR}"
    DEPENDS smave_fully_implicit_dae_event_evidence
    USES_TERMINAL
)

set(SMAVE_FULLY_IMPLICIT_DAE_MULTIGRID_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/fully-implicit-dae-multigrid")
add_custom_target(reproduce-fully-implicit-dae-multigrid
    COMMAND $<TARGET_FILE:smave_fully_implicit_dae_multigrid_evidence>
        "${SMAVE_FULLY_IMPLICIT_DAE_MULTIGRID_DIR}"
    DEPENDS smave_fully_implicit_dae_multigrid_evidence
    USES_TERMINAL
)

set(SMAVE_FULLY_IMPLICIT_DAE_MULTIGRID_CLI_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/fully-implicit-dae-multigrid-cli")
add_custom_target(reproduce-fully-implicit-dae-multigrid-cli
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DEVIDENCE=$<TARGET_FILE:smave_fully_implicit_dae_multigrid_evidence>
        -DWORK=${SMAVE_FULLY_IMPLICIT_DAE_MULTIGRID_CLI_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fully_implicit_multigrid_cli.cmake
    DEPENDS smave smave_fully_implicit_dae_multigrid_evidence
    USES_TERMINAL
)

set(SMAVE_FULLY_IMPLICIT_CLI_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/fully-implicit-cli")
add_custom_target(reproduce-fully-implicit-cli
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DEVIDENCE=$<TARGET_FILE:smave_fully_implicit_dae_evidence>
        -DWORK=${SMAVE_FULLY_IMPLICIT_CLI_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fully_implicit_cli.cmake
    DEPENDS smave smave_fully_implicit_dae_evidence
    USES_TERMINAL
)

set(SMAVE_LARGE_DAE_INITIALIZATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/large-dae-initialization")
add_custom_target(reproduce-large-dae-initialization
    COMMAND $<TARGET_FILE:smave_large_dae_initialization_evidence>
        "${SMAVE_LARGE_DAE_INITIALIZATION_DIR}"
    DEPENDS smave_large_dae_initialization_evidence
    USES_TERMINAL
)

set(SMAVE_LARGE_DAE_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/large-dae-event")
add_custom_target(reproduce-large-dae-events
    COMMAND $<TARGET_FILE:smave_large_dae_event_evidence>
        "${SMAVE_LARGE_DAE_EVENT_DIR}"
    DEPENDS smave_large_dae_event_evidence
    USES_TERMINAL
)

set(SMAVE_EQUATION_ASSESSMENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/equation-assessment")
add_custom_target(reproduce-equation-assessment
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_EQUATION_ASSESSMENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_EQUATION_ASSESSMENT_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/PoissonGrid.mo"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/symmetric.ir"
    COMMAND $<TARGET_FILE:smave> assess-equation
        "${SMAVE_EQUATION_ASSESSMENT_DIR}/symmetric.ir"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/symmetric.txt"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/medium-sparse.ir"
    COMMAND $<TARGET_FILE:smave> assess-equation
        "${SMAVE_EQUATION_ASSESSMENT_DIR}/medium-sparse.ir"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/medium-sparse.txt"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/EquationRoutingNonsymmetric.mo"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonsymmetric.ir"
    COMMAND $<TARGET_FILE:smave> assess-equation
        "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonsymmetric.ir"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonsymmetric.txt"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/Coupled.mo" --top Coupled
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonlinear.ir"
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonlinear.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/training"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/warm-start.expert"
    COMMAND $<TARGET_FILE:smave> assess-equation
        "${SMAVE_EQUATION_ASSESSMENT_DIR}/nonlinear.ir" --block block-1
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios/id-1.conf"
        --expert "${SMAVE_EQUATION_ASSESSMENT_DIR}/warm-start.expert"
        --output "${SMAVE_EQUATION_ASSESSMENT_DIR}/ai-composite.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DSYMMETRIC_IR=${SMAVE_EQUATION_ASSESSMENT_DIR}/symmetric.ir
        -DNONSYMMETRIC_IR=${SMAVE_EQUATION_ASSESSMENT_DIR}/nonsymmetric.ir
        -DSYMMETRIC_REPORT=${SMAVE_EQUATION_ASSESSMENT_DIR}/symmetric.txt
        -DMEDIUM_REPORT=${SMAVE_EQUATION_ASSESSMENT_DIR}/medium-sparse.txt
        -DNONSYMMETRIC_REPORT=${SMAVE_EQUATION_ASSESSMENT_DIR}/nonsymmetric.txt
        -DAI_REPORT=${SMAVE_EQUATION_ASSESSMENT_DIR}/ai-composite.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_equation_assessment.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_RESIDENCY_DIR "${CMAKE_CURRENT_BINARY_DIR}/residency")
add_custom_target(reproduce-residency
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_RESIDENCY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_RESIDENCY_DIR}"
    COMMAND $<TARGET_FILE:smave_residency_evidence> "${SMAVE_RESIDENCY_DIR}"
    DEPENDS smave_residency_evidence
    USES_TERMINAL
)

set(SMAVE_DATA_REGISTRY_DIR "${CMAKE_CURRENT_BINARY_DIR}/data-registry")
add_custom_target(reproduce-data-registry
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DROOT=${SMAVE_DATA_REGISTRY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_data_registry_evidence.cmake
    DEPENDS smave
    USES_TERMINAL
)

add_library(smave_fmi_fixture SHARED examples/fmi/SMAVEBlackbox/SMAVEBlackbox.cpp)
target_compile_options(smave_fmi_fixture PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(smave_fmi_fixture PROPERTIES PREFIX "" OUTPUT_NAME "SMAVEBlackbox")
add_library(smave_fmi2_fixture SHARED examples/fmi/SMAVEBlackbox/SMAVEFmi2.cpp)
target_compile_options(smave_fmi2_fixture PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(smave_fmi2_fixture PROPERTIES PREFIX "" OUTPUT_NAME "SMAVEFmi2")
add_library(smave_fmi2_event_fixture SHARED examples/fmi/SMAVEBlackbox/SMAVEFmi2Event.cpp)
target_compile_options(smave_fmi2_event_fixture PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(smave_fmi2_event_fixture PROPERTIES PREFIX "" OUTPUT_NAME "SMAVEFmi2Event")
add_library(smave_fmi2_async_fixture SHARED examples/fmi/SMAVEBlackbox/SMAVEFmi2Async.cpp)
target_compile_options(smave_fmi2_async_fixture PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(smave_fmi2_async_fixture PRIVATE Threads::Threads)
set_target_properties(smave_fmi2_async_fixture PROPERTIES PREFIX "" OUTPUT_NAME "SMAVEFmi2Async")
add_library(smave_fmi2_me_fixture SHARED examples/fmi/SMAVEBlackbox/SMAVEFmi2ME.cpp)
target_compile_options(smave_fmi2_me_fixture PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(smave_fmi2_me_fixture PROPERTIES PREFIX "" OUTPUT_NAME "SMAVEFmi2ME")
if(APPLE)
    set_target_properties(smave_fmi_fixture PROPERTIES SUFFIX ".dylib")
    foreach(fixture
        smave_fmi_fixture
        smave_fmi2_fixture
        smave_fmi2_event_fixture
        smave_fmi2_async_fixture
        smave_fmi2_me_fixture)
        add_custom_command(TARGET ${fixture} POST_BUILD
            COMMAND codesign --force --sign - $<TARGET_FILE:${fixture}>
            VERBATIM)
    endforeach()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(SMAVE_FMI_PLATFORM "aarch64-darwin")
    else()
        set(SMAVE_FMI_PLATFORM "x86_64-darwin")
    endif()
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(SMAVE_FMI_PLATFORM "aarch64-windows")
    else()
        set(SMAVE_FMI_PLATFORM "x86_64-windows")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(SMAVE_FMI_PLATFORM "aarch64-linux")
    else()
        set(SMAVE_FMI_PLATFORM "x86_64-linux")
    endif()
else()
    message(FATAL_ERROR "reproduce-fmi does not support this platform")
endif()

set(SMAVE_DEMO_DIR "${CMAKE_CURRENT_BINARY_DIR}/demo")
add_custom_target(reproduce
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DEMO_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DEMO_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/Coupled.mo"
        --top Coupled --output "${SMAVE_DEMO_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_DEMO_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/training"
        --output "${SMAVE_DEMO_DIR}/affine.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_DEMO_DIR}/model.ir"
        --expert "${SMAVE_DEMO_DIR}/affine.expert"
        --output "${SMAVE_DEMO_DIR}/affine.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_DEMO_DIR}/model.ir"
        --expert "${SMAVE_DEMO_DIR}/affine.expert"
        --certificate "${SMAVE_DEMO_DIR}/affine.verify"
        --output "${SMAVE_DEMO_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_DEMO_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_DEMO_DIR}/affine.expert"
        --certificate "${SMAVE_DEMO_DIR}/affine.verify"
        --bundle "${SMAVE_DEMO_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_DEMO_DIR}/traces"
        --output "${SMAVE_DEMO_DIR}/validation.txt"
    COMMAND $<TARGET_FILE:smave> compete
        "${SMAVE_DEMO_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_DEMO_DIR}/affine.expert"
        --certificate "${SMAVE_DEMO_DIR}/affine.verify"
        --bundle "${SMAVE_DEMO_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_DEMO_DIR}/competition-traces"
        --output "${SMAVE_DEMO_DIR}/competition.txt"
    COMMAND $<TARGET_FILE:smave> solve
        "${SMAVE_DEMO_DIR}/model.ir"
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios/id-1.conf"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --profile "${SMAVE_DEMO_DIR}/competition.txt"
        --expert "${SMAVE_DEMO_DIR}/affine.expert"
        --certificate "${SMAVE_DEMO_DIR}/affine.verify"
        --bundle "${SMAVE_DEMO_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_DEMO_DIR}/profile-traces"
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_PHASE1_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase1")
add_custom_target(reproduce-phase1
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE1_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_PHASE1_DIR}/coupled" "${SMAVE_PHASE1_DIR}/cubic"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/Coupled.mo"
        --top Coupled --output "${SMAVE_PHASE1_DIR}/coupled/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/training"
        --dataset phase1-coupled-training
        --store "${SMAVE_PHASE1_DIR}/data-store"
        > "${SMAVE_PHASE1_DIR}/coupled/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE1_DIR}/coupled/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE1_DIR}/data-store
        -DDATASET_ID=phase1-coupled-training
        -DOUTPUT=${SMAVE_PHASE1_DIR}/coupled/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_PHASE1_DIR}/coupled/model.ir" --block block-1
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/coupled/training-dataset.manifest"
        --output "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE1_DIR}/coupled/model.ir"
        --expert "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
        --output "${SMAVE_PHASE1_DIR}/coupled/affine.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE1_DIR}/coupled/model.ir"
        --expert "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/coupled/affine.verify"
        --output "${SMAVE_PHASE1_DIR}/coupled/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios"
        --dataset phase1-coupled-evaluation
        --store "${SMAVE_PHASE1_DIR}/data-store"
        > "${SMAVE_PHASE1_DIR}/coupled/dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE1_DIR}/coupled/dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE1_DIR}/data-store
        -DDATASET_ID=phase1-coupled-evaluation
        -DOUTPUT=${SMAVE_PHASE1_DIR}/coupled/dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE1_DIR}/coupled/model.ir"
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/coupled/dataset.manifest"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/coupled/affine.verify"
        --bundle "${SMAVE_PHASE1_DIR}/coupled/runtime.bundle"
        --trace-dir "${SMAVE_PHASE1_DIR}/coupled/validation-traces"
        --output "${SMAVE_PHASE1_DIR}/coupled/validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark
        "${SMAVE_PHASE1_DIR}/coupled/model.ir"
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/coupled/dataset.manifest"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/coupled/affine.verify"
        --bundle "${SMAVE_PHASE1_DIR}/coupled/runtime.bundle"
        --trace-dir "${SMAVE_PHASE1_DIR}/coupled/benchmark-traces"
        --output "${SMAVE_PHASE1_DIR}/coupled/performance.txt"
        --repetitions 100 --bootstrap-samples 10000
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CubicCoupled.mo"
        --top CubicCoupled --output "${SMAVE_PHASE1_DIR}/cubic/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase1-cubic-training"
        --dataset phase1-cubic-training
        --store "${SMAVE_PHASE1_DIR}/data-store"
        > "${SMAVE_PHASE1_DIR}/cubic/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE1_DIR}/cubic/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE1_DIR}/data-store
        -DDATASET_ID=phase1-cubic-training
        -DOUTPUT=${SMAVE_PHASE1_DIR}/cubic/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_PHASE1_DIR}/cubic/model.ir" --block block-1
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/cubic/training-dataset.manifest"
        --output "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE1_DIR}/cubic/model.ir"
        --expert "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
        --output "${SMAVE_PHASE1_DIR}/cubic/affine.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE1_DIR}/cubic/model.ir"
        --expert "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/cubic/affine.verify"
        --output "${SMAVE_PHASE1_DIR}/cubic/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase1-cubic-scenarios"
        --dataset phase1-cubic-evaluation
        --store "${SMAVE_PHASE1_DIR}/data-store"
        > "${SMAVE_PHASE1_DIR}/cubic/dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE1_DIR}/cubic/dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE1_DIR}/data-store
        -DDATASET_ID=phase1-cubic-evaluation
        -DOUTPUT=${SMAVE_PHASE1_DIR}/cubic/dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE1_DIR}/cubic/model.ir"
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/cubic/dataset.manifest"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/cubic/affine.verify"
        --bundle "${SMAVE_PHASE1_DIR}/cubic/runtime.bundle"
        --trace-dir "${SMAVE_PHASE1_DIR}/cubic/validation-traces"
        --output "${SMAVE_PHASE1_DIR}/cubic/validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark
        "${SMAVE_PHASE1_DIR}/cubic/model.ir"
        --dataset-store "${SMAVE_PHASE1_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE1_DIR}/cubic/dataset.manifest"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
        --certificate "${SMAVE_PHASE1_DIR}/cubic/affine.verify"
        --bundle "${SMAVE_PHASE1_DIR}/cubic/runtime.bundle"
        --trace-dir "${SMAVE_PHASE1_DIR}/cubic/benchmark-traces"
        --output "${SMAVE_PHASE1_DIR}/cubic/performance.txt"
        --repetitions 100 --bootstrap-samples 10000
    COMMAND ${CMAKE_COMMAND}
        -DCOUPLED_REPORT=${SMAVE_PHASE1_DIR}/coupled/performance.txt
        -DCUBIC_REPORT=${SMAVE_PHASE1_DIR}/cubic/performance.txt
        -DCOUPLED_VALIDATION=${SMAVE_PHASE1_DIR}/coupled/validation.txt
        -DCUBIC_VALIDATION=${SMAVE_PHASE1_DIR}/cubic/validation.txt
        -DCOUPLED_TRACES=${SMAVE_PHASE1_DIR}/coupled/validation-traces
        -DCUBIC_TRACES=${SMAVE_PHASE1_DIR}/cubic/validation-traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase1.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_NONLINEAR_CASCADE_DIR "${CMAKE_CURRENT_BINARY_DIR}/nonlinear-cascade")
add_custom_target(reproduce-nonlinear-cascade
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_NONLINEAR_CASCADE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_NONLINEAR_CASCADE_DIR}/coupled"
        "${SMAVE_NONLINEAR_CASCADE_DIR}/cubic"
    COMMAND $<TARGET_FILE:smave_nonlinear_cascade_evidence>
        "${SMAVE_PHASE1_DIR}/coupled/model.ir"
        "${SMAVE_PHASE1_DIR}/coupled/affine.expert"
        "${SMAVE_PHASE1_DIR}/coupled/affine.verify"
        "${SMAVE_PHASE1_DIR}/coupled/runtime.bundle"
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/scenarios"
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        "${SMAVE_NONLINEAR_CASCADE_DIR}/coupled"
    COMMAND $<TARGET_FILE:smave_nonlinear_cascade_evidence>
        "${SMAVE_PHASE1_DIR}/cubic/model.ir"
        "${SMAVE_PHASE1_DIR}/cubic/affine.expert"
        "${SMAVE_PHASE1_DIR}/cubic/affine.verify"
        "${SMAVE_PHASE1_DIR}/cubic/runtime.bundle"
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase1-cubic-scenarios"
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        "${SMAVE_NONLINEAR_CASCADE_DIR}/cubic"
    COMMAND ${CMAKE_COMMAND}
        -DCOUPLED_DIR=${SMAVE_NONLINEAR_CASCADE_DIR}/coupled
        -DCUBIC_DIR=${SMAVE_NONLINEAR_CASCADE_DIR}/cubic
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_nonlinear_cascade.cmake
    DEPENDS reproduce-phase1 smave_nonlinear_cascade_evidence
    USES_TERMINAL
)

set(SMAVE_PHASE2_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase2")
add_custom_target(reproduce-phase2
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE2_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE2_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/PoissonGrid.mo"
        --top PoissonGrid --output "${SMAVE_PHASE2_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-training"
        --dataset phase2-preconditioner-training
        --store "${SMAVE_PHASE2_DIR}/data-store"
        > "${SMAVE_PHASE2_DIR}/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE2_DIR}/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE2_DIR}/data-store
        -DDATASET_ID=phase2-preconditioner-training
        -DOUTPUT=${SMAVE_PHASE2_DIR}/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-preconditioner
        "${SMAVE_PHASE2_DIR}/model.ir" --block block-1
        --dataset-store "${SMAVE_PHASE2_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE2_DIR}/training-dataset.manifest"
        --output "${SMAVE_PHASE2_DIR}/linear-pc.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE2_DIR}/model.ir"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --output "${SMAVE_PHASE2_DIR}/linear-pc.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE2_DIR}/model.ir"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_DIR}/linear-pc.verify"
        --output "${SMAVE_PHASE2_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE2_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_DIR}/traces"
        --output "${SMAVE_PHASE2_DIR}/validation.txt"
    COMMAND $<TARGET_FILE:smave> compete
        "${SMAVE_PHASE2_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_DIR}/competition-traces"
        --output "${SMAVE_PHASE2_DIR}/competition.txt"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE2_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --profile "${SMAVE_PHASE2_DIR}/competition.txt"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_DIR}/profile-traces"
        --output "${SMAVE_PHASE2_DIR}/profile-validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark
        "${SMAVE_PHASE2_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_DIR}/benchmark-traces"
        --output "${SMAVE_PHASE2_DIR}/performance.txt"
        --repetitions 30
    COMMAND $<TARGET_FILE:smave> batch-solve
        "${SMAVE_PHASE2_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-batch"
        --expert "${SMAVE_PHASE2_DIR}/linear-pc.expert"
        --max-batch 32
        --trace-dir "${SMAVE_PHASE2_DIR}/batch-traces"
        --output "${SMAVE_PHASE2_DIR}/batch.txt"
    COMMAND ${CMAKE_COMMAND}
        -DARTIFACT=${SMAVE_PHASE2_DIR}/linear-pc.expert
        -DCERTIFICATE=${SMAVE_PHASE2_DIR}/linear-pc.verify
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase2_lineage.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_PHASE2_LARGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase2-large")
add_custom_target(reproduce-phase2-large
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE2_LARGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE2_LARGE_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo"
        --top OperatorPoissonGrid10
        --output "${SMAVE_PHASE2_LARGE_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-preconditioner
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-training"
        --output "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
    COMMAND $<TARGET_FILE:smave> train-preconditioner
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-training"
        --output "${SMAVE_PHASE2_LARGE_DIR}/repeat-linear-pc.expert"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
        "${SMAVE_PHASE2_LARGE_DIR}/repeat-linear-pc.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir"
        --expert "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
        --output "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir"
        --expert "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.verify"
        --output "${SMAVE_PHASE2_LARGE_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_LARGE_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_LARGE_DIR}/validation-traces"
        --output "${SMAVE_PHASE2_LARGE_DIR}/validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark
        "${SMAVE_PHASE2_LARGE_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/examples/smave.yaml"
        --expert "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.expert"
        --certificate "${SMAVE_PHASE2_LARGE_DIR}/linear-pc.verify"
        --bundle "${SMAVE_PHASE2_LARGE_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE2_LARGE_DIR}/benchmark-traces"
        --output "${SMAVE_PHASE2_LARGE_DIR}/performance.txt"
        --repetitions 20
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_PHASE2_LARGE_DIR}/model.ir
        -DVALIDATION=${SMAVE_PHASE2_LARGE_DIR}/validation.txt
        -DPERFORMANCE=${SMAVE_PHASE2_LARGE_DIR}/performance.txt
        -DTRACES=${SMAVE_PHASE2_LARGE_DIR}/validation-traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase2_large.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_PHASE3_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase3")
add_custom_target(reproduce-phase3
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE3_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE3_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/Phase3Degenerate.mo"
        --top Phase3Degenerate --output "${SMAVE_PHASE3_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_PHASE3_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-training"
        --trace-dir "${SMAVE_PHASE3_DIR}/label-traces"
        --output "${SMAVE_PHASE3_DIR}/affine.expert"
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_PHASE3_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-training"
        --trace-dir "${SMAVE_PHASE3_DIR}/repeat-label-traces"
        --output "${SMAVE_PHASE3_DIR}/repeat-affine.expert"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE3_DIR}/affine.expert"
        "${SMAVE_PHASE3_DIR}/repeat-affine.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE3_DIR}/model.ir"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --max-depth 4
        --trace-dir "${SMAVE_PHASE3_DIR}/verification-probes"
        --output "${SMAVE_PHASE3_DIR}/affine.verify"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE3_DIR}/model.ir"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --max-depth 4
        --trace-dir "${SMAVE_PHASE3_DIR}/repeat-verification-probes"
        --output "${SMAVE_PHASE3_DIR}/repeat-affine.verify"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE3_DIR}/affine.verify"
        "${SMAVE_PHASE3_DIR}/repeat-affine.verify"
    COMMAND $<TARGET_FILE:smave> export-counterexamples
        "${SMAVE_PHASE3_DIR}/affine.verify"
        --output "${SMAVE_PHASE3_DIR}/counterexamples"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-training"
        "${SMAVE_PHASE3_DIR}/refined-training"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_PHASE3_DIR}/counterexamples/counterexample-1.conf"
        "${SMAVE_PHASE3_DIR}/refined-training/counterexample-1.conf"
    COMMAND $<TARGET_FILE:smave> train-expert
        "${SMAVE_PHASE3_DIR}/model.ir" --block block-1
        --scenarios "${SMAVE_PHASE3_DIR}/refined-training"
        --trace-dir "${SMAVE_PHASE3_DIR}/refined-label-traces"
        --output "${SMAVE_PHASE3_DIR}/refined-affine.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE3_DIR}/model.ir"
        --expert "${SMAVE_PHASE3_DIR}/refined-affine.expert"
        --max-depth 4
        --trace-dir "${SMAVE_PHASE3_DIR}/refined-verification-probes"
        --output "${SMAVE_PHASE3_DIR}/refined-affine.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE3_DIR}/model.ir"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --certificate "${SMAVE_PHASE3_DIR}/affine.verify"
        --output "${SMAVE_PHASE3_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> solve
        "${SMAVE_PHASE3_DIR}/model.ir"
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-scenarios/safe.conf"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --certificate "${SMAVE_PHASE3_DIR}/affine.verify"
        --bundle "${SMAVE_PHASE3_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE3_DIR}/safe-traces"
    COMMAND $<TARGET_FILE:smave> solve
        "${SMAVE_PHASE3_DIR}/model.ir"
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-scenarios/difficult.conf"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --certificate "${SMAVE_PHASE3_DIR}/affine.verify"
        --bundle "${SMAVE_PHASE3_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE3_DIR}/difficult-traces"
    COMMAND $<TARGET_FILE:smave> solve
        "${SMAVE_PHASE3_DIR}/model.ir"
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/phase3-scenarios/ood.conf"
        --expert "${SMAVE_PHASE3_DIR}/affine.expert"
        --certificate "${SMAVE_PHASE3_DIR}/affine.verify"
        --bundle "${SMAVE_PHASE3_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE3_DIR}/ood-traces"
    COMMAND ${CMAKE_COMMAND}
        -DCERTIFICATE=${SMAVE_PHASE3_DIR}/affine.verify
        -DREFINED_CERTIFICATE=${SMAVE_PHASE3_DIR}/refined-affine.verify
        -DBUNDLE=${SMAVE_PHASE3_DIR}/runtime.bundle
        -DCOUNTEREXAMPLES=${SMAVE_PHASE3_DIR}/counterexamples
        -DSAFE_TRACES=${SMAVE_PHASE3_DIR}/safe-traces
        -DDIFFICULT_TRACES=${SMAVE_PHASE3_DIR}/difficult-traces
        -DOOD_TRACES=${SMAVE_PHASE3_DIR}/ood-traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase3.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_MULTIGRID_DIR "${CMAKE_CURRENT_BINARY_DIR}/multigrid")
add_custom_target(reproduce-multigrid
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_MULTIGRID_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_MULTIGRID_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/PoissonGrid.mo"
        --top PoissonGrid --output "${SMAVE_MULTIGRID_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-training"
        --dataset multigrid-training
        --store "${SMAVE_MULTIGRID_DIR}/data-store"
        > "${SMAVE_MULTIGRID_DIR}/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_MULTIGRID_DIR}/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_MULTIGRID_DIR}/data-store
        -DDATASET_ID=multigrid-training
        -DOUTPUT=${SMAVE_MULTIGRID_DIR}/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-multigrid
        "${SMAVE_MULTIGRID_DIR}/model.ir" --block block-1
        --dataset-store "${SMAVE_MULTIGRID_DIR}/data-store"
        --dataset-manifest "${SMAVE_MULTIGRID_DIR}/training-dataset.manifest"
        --output "${SMAVE_MULTIGRID_DIR}/multigrid.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_MULTIGRID_DIR}/model.ir"
        --expert "${SMAVE_MULTIGRID_DIR}/multigrid.expert"
        --output "${SMAVE_MULTIGRID_DIR}/multigrid.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_MULTIGRID_DIR}/model.ir"
        --expert "${SMAVE_MULTIGRID_DIR}/multigrid.expert"
        --certificate "${SMAVE_MULTIGRID_DIR}/multigrid.verify"
        --output "${SMAVE_MULTIGRID_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_MULTIGRID_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --expert "${SMAVE_MULTIGRID_DIR}/multigrid.expert"
        --certificate "${SMAVE_MULTIGRID_DIR}/multigrid.verify"
        --bundle "${SMAVE_MULTIGRID_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_MULTIGRID_DIR}/traces"
        --output "${SMAVE_MULTIGRID_DIR}/validation.txt"
    COMMAND ${CMAKE_COMMAND}
        -DARTIFACT=${SMAVE_MULTIGRID_DIR}/multigrid.expert
        -DCERTIFICATE=${SMAVE_MULTIGRID_DIR}/multigrid.verify
        -DVALIDATION=${SMAVE_MULTIGRID_DIR}/validation.txt
        -DTRACE_DIR=${SMAVE_MULTIGRID_DIR}/traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_multigrid.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_NONLINEAR_MULTIGRID_DIR "${CMAKE_CURRENT_BINARY_DIR}/nonlinear-multigrid")
add_custom_target(reproduce-nonlinear-multigrid
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_NONLINEAR_MULTIGRID_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_NONLINEAR_MULTIGRID_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/NonlinearJacobianMultigrid.mo"
        --top NonlinearJacobianMultigrid
        --output "${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-multigrid
        "${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir" --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/nonlinear-multigrid-training"
        --output "${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir"
        --expert "${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert"
        --output "${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir"
        --expert "${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert"
        --certificate "${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.verify"
        --output "${SMAVE_NONLINEAR_MULTIGRID_DIR}/runtime.bundle"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonlinear-multigrid-scenarios/accelerated.conf
        -DEXPERT=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert
        -DCERTIFICATE=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.verify
        -DBUNDLE=${SMAVE_NONLINEAR_MULTIGRID_DIR}/runtime.bundle
        -DTRACE_DIR=${SMAVE_NONLINEAR_MULTIGRID_DIR}/accelerated-traces
        -DOUTPUT=${SMAVE_NONLINEAR_MULTIGRID_DIR}/accelerated.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_solve_capture.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONLINEAR_MULTIGRID_DIR}/model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonlinear-multigrid-scenarios/fallback.conf
        -DEXPERT=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert
        -DCERTIFICATE=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.verify
        -DBUNDLE=${SMAVE_NONLINEAR_MULTIGRID_DIR}/runtime.bundle
        -DTRACE_DIR=${SMAVE_NONLINEAR_MULTIGRID_DIR}/fallback-traces
        -DOUTPUT=${SMAVE_NONLINEAR_MULTIGRID_DIR}/fallback.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_solve_capture.cmake
    COMMAND ${CMAKE_COMMAND}
        -DARTIFACT=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.expert
        -DCERTIFICATE=${SMAVE_NONLINEAR_MULTIGRID_DIR}/multigrid.verify
        -DACCELERATED=${SMAVE_NONLINEAR_MULTIGRID_DIR}/accelerated.txt
        -DFALLBACK=${SMAVE_NONLINEAR_MULTIGRID_DIR}/fallback.txt
        -DACCELERATED_TRACE_DIR=${SMAVE_NONLINEAR_MULTIGRID_DIR}/accelerated-traces
        -DFALLBACK_TRACE_DIR=${SMAVE_NONLINEAR_MULTIGRID_DIR}/fallback-traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_nonlinear_multigrid.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_NONSYMMETRIC_DIR "${CMAKE_CURRENT_BINARY_DIR}/nonsymmetric")
set(SMAVE_INDUSTRIAL_SPARSE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/industrial-sparse")
add_custom_target(reproduce-industrial-sparse
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_INDUSTRIAL_SPARSE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_INDUSTRIAL_SPARSE_DIR}"
    COMMAND $<TARGET_FILE:smave_industrial_sparse_evidence>
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/suitesparse"
        "${SMAVE_INDUSTRIAL_SPARSE_DIR}/evidence.txt"
    DEPENDS smave_industrial_sparse_evidence
    USES_TERMINAL
)

set(SMAVE_COMPLEMENTARITY_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/complementarity")
add_custom_target(reproduce-complementarity
    COMMAND $<TARGET_FILE:smave_complementarity_evidence>
        "${SMAVE_COMPLEMENTARITY_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/ContactComplementarity.mo
        -DDIRECTORY=${SMAVE_COMPLEMENTARITY_DIR}/cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_complementarity_cli.cmake
    DEPENDS smave smave_complementarity_evidence
    USES_TERMINAL
)

set(SMAVE_INDEX_TWO_DAE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/index-two-dae")
add_custom_target(reproduce-index-two-dae
    COMMAND $<TARGET_FILE:smave_index_two_dae_evidence>
        "${SMAVE_INDEX_TWO_DAE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/IndexTwoConstraint.mo
        -DDIRECTORY=${SMAVE_INDEX_TWO_DAE_DIR}/cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_index_two_dae_cli.cmake
    DEPENDS smave smave_index_two_dae_evidence
    USES_TERMINAL
)

if(APPLE)
    set(SMAVE_DEVICE_EXECUTION_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/device-execution")
    add_custom_target(reproduce-device-execution
        COMMAND $<TARGET_FILE:smave_device_execution_evidence>
            "${SMAVE_DEVICE_EXECUTION_DIR}"
        DEPENDS smave_device_execution_evidence
        USES_TERMINAL
    )
endif()

if(DEFINED SMAVE_CUDA_TOOLKIT_ROOT)
    set(SMAVE_NATIVE_CUDA_CAMPAIGN_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/native-cuda")
    add_custom_target(reproduce-native-cuda-campaign
        COMMAND ${CMAKE_COMMAND} -E env
            SMAVE_CUDA_TOOLKIT_ROOT=${SMAVE_CUDA_TOOLKIT_ROOT}
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/run_native_cuda_campaign.sh
            --build "${SMAVE_NATIVE_CUDA_CAMPAIGN_DIR}/build"
            --output "${SMAVE_NATIVE_CUDA_CAMPAIGN_DIR}/campaign"
            --replicate-id 1
        USES_TERMINAL
        VERBATIM)
endif()

add_custom_target(reproduce-nonsymmetric
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_NONSYMMETRIC_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_NONSYMMETRIC_DIR}/traces"
        "${SMAVE_NONSYMMETRIC_DIR}/repeat-traces"
        "${SMAVE_NONSYMMETRIC_DIR}/pivot-traces"
        "${SMAVE_NONSYMMETRIC_DIR}/pivot-repeat-traces"
    COMMAND $<TARGET_FILE:smave_sparse_direct_evidence>
        "${SMAVE_NONSYMMETRIC_DIR}/sparse-direct-evidence.txt"
    COMMAND $<TARGET_FILE:smave_sparse_direct_evidence>
        "${SMAVE_NONSYMMETRIC_DIR}/sparse-direct-repeat-evidence.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_NONSYMMETRIC_DIR}/sparse-direct-evidence.txt"
        "${SMAVE_NONSYMMETRIC_DIR}/sparse-direct-repeat-evidence.txt"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/NonsymmetricLinear.mo"
        --top NonsymmetricLinear
        --output "${SMAVE_NONSYMMETRIC_DIR}/model.ir"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONSYMMETRIC_DIR}/model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonsymmetric.conf
        -DTRACE_DIR=${SMAVE_NONSYMMETRIC_DIR}/traces
        -DOUTPUT=${SMAVE_NONSYMMETRIC_DIR}/output.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_nonsymmetric.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONSYMMETRIC_DIR}/model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonsymmetric.conf
        -DTRACE_DIR=${SMAVE_NONSYMMETRIC_DIR}/repeat-traces
        -DOUTPUT=${SMAVE_NONSYMMETRIC_DIR}/repeat-output.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_nonsymmetric.cmake
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/NonsymmetricPivot.mo"
        --top NonsymmetricPivot
        --output "${SMAVE_NONSYMMETRIC_DIR}/pivot-model.ir"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONSYMMETRIC_DIR}/pivot-model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonsymmetric-pivot.conf
        -DTRACE_DIR=${SMAVE_NONSYMMETRIC_DIR}/pivot-traces
        -DOUTPUT=${SMAVE_NONSYMMETRIC_DIR}/pivot-output.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_nonsymmetric.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONSYMMETRIC_DIR}/pivot-model.ir
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/nonsymmetric-pivot.conf
        -DTRACE_DIR=${SMAVE_NONSYMMETRIC_DIR}/pivot-repeat-traces
        -DOUTPUT=${SMAVE_NONSYMMETRIC_DIR}/pivot-repeat-output.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_nonsymmetric.cmake
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${SMAVE_NONSYMMETRIC_DIR}/output.txt
        -DREPEAT_OUTPUT=${SMAVE_NONSYMMETRIC_DIR}/repeat-output.txt
        -DTRACE_PATH=${SMAVE_NONSYMMETRIC_DIR}/output.txt.trace-path
        -DREPEAT_TRACE_PATH=${SMAVE_NONSYMMETRIC_DIR}/repeat-output.txt.trace-path
        -DPIVOT_OUTPUT=${SMAVE_NONSYMMETRIC_DIR}/pivot-output.txt
        -DPIVOT_REPEAT_OUTPUT=${SMAVE_NONSYMMETRIC_DIR}/pivot-repeat-output.txt
        -DPIVOT_TRACE_PATH=${SMAVE_NONSYMMETRIC_DIR}/pivot-output.txt.trace-path
        -DPIVOT_REPEAT_TRACE_PATH=${SMAVE_NONSYMMETRIC_DIR}/pivot-repeat-output.txt.trace-path
        -DSPARSE_DIRECT_EVIDENCE=${SMAVE_NONSYMMETRIC_DIR}/sparse-direct-evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_nonsymmetric_paths.cmake
    DEPENDS smave smave_sparse_direct_evidence
    USES_TERMINAL
)

set(SMAVE_PHASE4_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase4")
add_custom_target(reproduce-phase4
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE4_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE4_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/PoissonGrid.mo"
        --top PoissonGrid --output "${SMAVE_PHASE4_DIR}/source.ir"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE4_DIR}/source.ir"
        --output "${SMAVE_PHASE4_DIR}/source.bundle"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/poisson-scenarios"
        --dataset phase4-source-competition
        --store "${SMAVE_PHASE4_DIR}/data-store"
        > "${SMAVE_PHASE4_DIR}/source-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE4_DIR}/source-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE4_DIR}/data-store
        -DDATASET_ID=phase4-source-competition
        -DOUTPUT=${SMAVE_PHASE4_DIR}/source-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> compete
        "${SMAVE_PHASE4_DIR}/source.ir"
        --dataset-store "${SMAVE_PHASE4_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE4_DIR}/source-dataset.manifest"
        --bundle "${SMAVE_PHASE4_DIR}/source.bundle"
        --repetitions 100
        --trace-dir "${SMAVE_PHASE4_DIR}/source-traces"
        --output "${SMAVE_PHASE4_DIR}/source.competition"
    COMMAND $<TARGET_FILE:smave> compete
        "${SMAVE_PHASE4_DIR}/source.ir"
        --dataset-store "${SMAVE_PHASE4_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE4_DIR}/source-dataset.manifest"
        --bundle "${SMAVE_PHASE4_DIR}/source.bundle"
        --repetitions 100
        --trace-dir "${SMAVE_PHASE4_DIR}/source-repeat-traces"
        --output "${SMAVE_PHASE4_DIR}/source-repeat.competition"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/HeldoutPoissonGrid6.mo"
        --top HeldoutPoissonGrid6 --output "${SMAVE_PHASE4_DIR}/heldout.ir"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios
        -DOUTPUT=${SMAVE_PHASE4_DIR}/heldout-scenarios
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_phase4_scenarios.cmake
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${SMAVE_PHASE4_DIR}/heldout-scenarios"
        --dataset phase4-heldout-family
        --store "${SMAVE_PHASE4_DIR}/data-store"
        > "${SMAVE_PHASE4_DIR}/heldout-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_PHASE4_DIR}/heldout-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE4_DIR}/data-store
        -DDATASET_ID=phase4-heldout-family
        -DOUTPUT=${SMAVE_PHASE4_DIR}/heldout-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE4_DIR}/heldout.ir"
        --output "${SMAVE_PHASE4_DIR}/heldout.bundle"
    COMMAND $<TARGET_FILE:smave> evaluate-family-router
        "${SMAVE_PHASE4_DIR}/source.ir"
        --profile "${SMAVE_PHASE4_DIR}/source.competition"
        --heldout "${SMAVE_PHASE4_DIR}/heldout.ir"
        --dataset-store "${SMAVE_PHASE4_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE4_DIR}/heldout-dataset.manifest"
        --minimum-similarity 0.90
        --minimum-speedup 1.01
        --repetitions 20
        --trace-dir "${SMAVE_PHASE4_DIR}/heldout-traces"
        --output "${SMAVE_PHASE4_DIR}/family-router-evaluation.txt"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE4_DIR}/heldout.ir"
        --dataset-store "${SMAVE_PHASE4_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE4_DIR}/heldout-dataset.manifest"
        --family-profile "${SMAVE_PHASE4_DIR}/family-router-evaluation.txt"
        --bundle "${SMAVE_PHASE4_DIR}/heldout.bundle"
        --trace-dir "${SMAVE_PHASE4_DIR}/runtime-traces"
        --output "${SMAVE_PHASE4_DIR}/runtime-validation.txt"
    COMMAND ${CMAKE_COMMAND}
        -DEVALUATION=${SMAVE_PHASE4_DIR}/family-router-evaluation.txt
        -DVALIDATION=${SMAVE_PHASE4_DIR}/runtime-validation.txt
        -DTRACES=${SMAVE_PHASE4_DIR}/runtime-traces
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase4.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_PAIRED_ORACLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/paired-oracle")
add_custom_target(reproduce-paired-oracle
    COMMAND $<TARGET_FILE:smave_paired_oracle_evidence>
        "${SMAVE_PHASE4_DIR}/heldout.ir"
        "${SMAVE_PHASE4_DIR}/heldout.bundle"
        "${SMAVE_PHASE4_DIR}/heldout-traces/heldout.competition"
        "${SMAVE_PHASE4_DIR}/family-router-evaluation.txt"
        "${SMAVE_PHASE4_DIR}/heldout-scenarios"
        "${SMAVE_PAIRED_ORACLE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_PAIRED_ORACLE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_paired_oracle.cmake
    DEPENDS reproduce-phase4 smave_paired_oracle_evidence
    USES_TERMINAL
)

set(SMAVE_PHASE5_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase5")
add_custom_target(reproduce-phase5
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE5_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE5_DIR}"
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo"
        --top OperatorPoissonGrid10 --output "${SMAVE_PHASE5_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-operator
        "${SMAVE_PHASE5_DIR}/model.ir"
        --block block-1
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-training"
        --rank 4
        --qoi x1,x50,x100
        --trace-dir "${SMAVE_PHASE5_DIR}/label-traces"
        --output "${SMAVE_PHASE5_DIR}/operator.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE5_DIR}/model.ir"
        --expert "${SMAVE_PHASE5_DIR}/operator.expert"
        --max-depth 1
        --trace-dir "${SMAVE_PHASE5_DIR}/verification-probes"
        --output "${SMAVE_PHASE5_DIR}/operator.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE5_DIR}/model.ir"
        --expert "${SMAVE_PHASE5_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE5_DIR}/operator.verify"
        --output "${SMAVE_PHASE5_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE5_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios"
        --expert "${SMAVE_PHASE5_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE5_DIR}/operator.verify"
        --bundle "${SMAVE_PHASE5_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE5_DIR}/runtime-traces"
        --output "${SMAVE_PHASE5_DIR}/validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark-operator
        "${SMAVE_PHASE5_DIR}/model.ir"
        --scenarios "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios"
        --expert "${SMAVE_PHASE5_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE5_DIR}/operator.verify"
        --bundle "${SMAVE_PHASE5_DIR}/runtime.bundle"
        --repetitions 100
        --projected-queries 10000
        --trace-dir "${SMAVE_PHASE5_DIR}/benchmark-traces"
        --output "${SMAVE_PHASE5_DIR}/performance.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_PHASE5_DIR}/model.ir
        -DARTIFACT=${SMAVE_PHASE5_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_PHASE5_DIR}/operator.verify
        -DBUNDLE=${SMAVE_PHASE5_DIR}/runtime.bundle
        -DVALIDATION=${SMAVE_PHASE5_DIR}/validation.txt
        -DPERFORMANCE=${SMAVE_PHASE5_DIR}/performance.txt
        -DTRACES=${SMAVE_PHASE5_DIR}/runtime-traces
        -DBATCH_TRACE=${SMAVE_PHASE5_DIR}/benchmark-traces/operator-batch.trace
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase5.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_HINTS_SCHEDULE_BASELINE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/hints-schedule-baseline")
add_custom_target(reproduce-hints-schedule-baseline
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_HINTS_SCHEDULE_BASELINE_DIR}"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_hints_schedule_baseline.py
        --report
            ${SMAVE_PHASE5_DIR}/benchmark-traces/operator-hints-schedule-baseline.txt
        --evidence ${SMAVE_HINTS_SCHEDULE_BASELINE_DIR}/evidence.txt
        --tex ${CMAKE_CURRENT_SOURCE_DIR}/paper/generated/hints_schedule_values.tex
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_HINTS_SCHEDULE_BASELINE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_hints_schedule_baseline.cmake
    USES_TERMINAL
    VERBATIM)

if(SMAVE_HINTS_UPSTREAM_ROOT AND SMAVE_HINTS_PYTHON)
    set(SMAVE_HINTS_NATIVE_BASELINE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/hints-native-baseline")
    add_custom_target(reproduce-hints-native-baseline
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${SMAVE_HINTS_NATIVE_BASELINE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SMAVE_HINTS_NATIVE_BASELINE_DIR}"
        COMMAND ${SMAVE_HINTS_PYTHON}
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/prepare_hints_native_baseline.py
            --upstream ${SMAVE_HINTS_UPSTREAM_ROOT}
            --output ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/workload
        COMMAND $<TARGET_FILE:smave> compile
            ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/workload/HintsPoisson1D.mo
            --top HintsPoisson1D
            --output ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/model.ir
        COMMAND ${CMAKE_COMMAND} -E env
            MPLBACKEND=Agg
            ${SMAVE_HINTS_PYTHON}
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/run_hints_native_baseline.py
            --upstream ${SMAVE_HINTS_UPSTREAM_ROOT}
            --output ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/official
            --repetitions 5
            --warmups 3
        COMMAND $<TARGET_FILE:smave_hints_native_smave_evidence>
            ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/model.ir
            ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/workload/scenarios
            ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/smave
        COMMAND python3
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_hints_native_baseline.py
            --official ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/official
            --smave ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/smave
            --workload ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/workload/contract.txt
            --output ${SMAVE_HINTS_NATIVE_BASELINE_DIR}/evidence.txt
            --tex ${CMAKE_CURRENT_SOURCE_DIR}/paper/generated/hints_native_values.tex
        COMMAND ${CMAKE_COMMAND}
            -DEVIDENCE=${SMAVE_HINTS_NATIVE_BASELINE_DIR}/evidence.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_hints_native_baseline.cmake
        DEPENDS smave smave_hints_native_smave_evidence
        USES_TERMINAL
        VERBATIM)
endif()

set(SMAVE_COMPONENT_ABLATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/component-ablation")
add_custom_target(reproduce-component-ablation
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_COMPONENT_ABLATION_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_COMPONENT_ABLATION_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DIR=${SMAVE_PHASE5_DIR}/model.ir
        -DARTIFACT=${SMAVE_PHASE5_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_PHASE5_DIR}/operator.verify
        -DBUNDLE=${SMAVE_PHASE5_DIR}/runtime.bundle
        -DSCENARIO=${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios/case-00.conf
        -DPAIRED_ORACLE=${SMAVE_PAIRED_ORACLE_DIR}/evidence.txt
        -DOPERATOR_ABLATION=${SMAVE_PHASE5_DIR}/benchmark-traces/operator-ablation.txt
        -DNONLINEAR_CASCADE=${SMAVE_NONLINEAR_CASCADE_DIR}/summary.txt
        -DOUTPUT_DIR=${SMAVE_COMPONENT_ABLATION_DIR}
        -DOUTPUT=${SMAVE_COMPONENT_ABLATION_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_component_ablation_evidence.cmake
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_COMPONENT_ABLATION_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_component_ablation.cmake
    DEPENDS reproduce-paired-oracle reproduce-phase5 reproduce-nonlinear-cascade smave
    USES_TERMINAL
)

set(SMAVE_OPERATOR_REPLICATION_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/operator-replication")
add_custom_target(reproduce-operator-replication
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_OPERATOR_REPLICATION_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_OPERATOR_REPLICATION_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_MODEL=${SMAVE_OPERATOR_REPLICATION_DIR}/OperatorPeriodicHelmholtz144.mo
        -DTRAINING_DIR=${SMAVE_OPERATOR_REPLICATION_DIR}/training
        -DSCENARIO_DIR=${SMAVE_OPERATOR_REPLICATION_DIR}/scenarios
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_operator_replication.cmake
    COMMAND $<TARGET_FILE:smave> compile
        "${SMAVE_OPERATOR_REPLICATION_DIR}/OperatorPeriodicHelmholtz144.mo"
        --top OperatorPeriodicHelmholtz144
        --output "${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-operator
        "${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir"
        --block block-1
        --scenarios "${SMAVE_OPERATOR_REPLICATION_DIR}/training"
        --rank 6
        --qoi x1,x72,x144
        --trace-dir "${SMAVE_OPERATOR_REPLICATION_DIR}/label-traces"
        --output "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir"
        --expert "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert"
        --max-depth 1
        --trace-dir "${SMAVE_OPERATOR_REPLICATION_DIR}/verification-probes"
        --output "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir"
        --expert "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert"
        --certificate "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.verify"
        --output "${SMAVE_OPERATOR_REPLICATION_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir"
        --scenarios "${SMAVE_OPERATOR_REPLICATION_DIR}/scenarios"
        --expert "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert"
        --certificate "${SMAVE_OPERATOR_REPLICATION_DIR}/operator.verify"
        --bundle "${SMAVE_OPERATOR_REPLICATION_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_OPERATOR_REPLICATION_DIR}/runtime-traces"
        --output "${SMAVE_OPERATOR_REPLICATION_DIR}/validation.txt"
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir
        -DSCENARIOS=${SMAVE_OPERATOR_REPLICATION_DIR}/scenarios
        -DARTIFACT=${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_OPERATOR_REPLICATION_DIR}/operator.verify
        -DBUNDLE=${SMAVE_OPERATOR_REPLICATION_DIR}/runtime.bundle
        -DTRACE_DIR=${SMAVE_OPERATOR_REPLICATION_DIR}/benchmark-traces
        -DOUTPUT=${SMAVE_OPERATOR_REPLICATION_DIR}/performance.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_operator_benchmark_allow_negative.cmake
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_OPERATOR_REPLICATION_DIR}/model.ir
        -DARTIFACT=${SMAVE_OPERATOR_REPLICATION_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_OPERATOR_REPLICATION_DIR}/operator.verify
        -DBUNDLE=${SMAVE_OPERATOR_REPLICATION_DIR}/runtime.bundle
        -DVALIDATION=${SMAVE_OPERATOR_REPLICATION_DIR}/validation.txt
        -DPERFORMANCE=${SMAVE_OPERATOR_REPLICATION_DIR}/performance.txt
        -DTRACES=${SMAVE_OPERATOR_REPLICATION_DIR}/runtime-traces
        -DSTATISTICS=${SMAVE_OPERATOR_REPLICATION_DIR}/benchmark-traces/operator-statistics.txt
        -DOUTPUT=${SMAVE_OPERATOR_REPLICATION_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_operator_replication.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_NONLINEAR_OPERATOR_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/nonlinear-operator")
add_custom_target(reproduce-nonlinear-operator
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_NONLINEAR_OPERATOR_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_MODEL=${SMAVE_NONLINEAR_OPERATOR_DIR}/NonlinearOperatorGrid4.mo
        -DTRAINING_DIR=${SMAVE_NONLINEAR_OPERATOR_DIR}/training
        -DSCENARIO_DIR=${SMAVE_NONLINEAR_OPERATOR_DIR}/scenarios
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_nonlinear_operator.cmake
    COMMAND $<TARGET_FILE:smave> compile
        "${SMAVE_NONLINEAR_OPERATOR_DIR}/NonlinearOperatorGrid4.mo"
        --top NonlinearOperatorGrid4
        --output "${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> train-operator
        "${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir"
        --block block-1
        --scenarios "${SMAVE_NONLINEAR_OPERATOR_DIR}/training"
        --rank 4
        --qoi x1,x8,x16
        --trace-dir "${SMAVE_NONLINEAR_OPERATOR_DIR}/label-traces"
        --output "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir"
        --expert "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert"
        --max-depth 1
        --trace-dir "${SMAVE_NONLINEAR_OPERATOR_DIR}/verification-probes"
        --output "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir"
        --expert "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert"
        --certificate "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.verify"
        --output "${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir"
        --scenarios "${SMAVE_NONLINEAR_OPERATOR_DIR}/scenarios"
        --expert "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert"
        --certificate "${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.verify"
        --bundle "${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime-traces"
        --output "${SMAVE_NONLINEAR_OPERATOR_DIR}/validation.txt"
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:smave>
        -DMODEL=${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir
        -DSCENARIOS=${SMAVE_NONLINEAR_OPERATOR_DIR}/scenarios
        -DARTIFACT=${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.verify
        -DBUNDLE=${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime.bundle
        -DTRACE_DIR=${SMAVE_NONLINEAR_OPERATOR_DIR}/benchmark-traces
        -DOUTPUT=${SMAVE_NONLINEAR_OPERATOR_DIR}/performance.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_operator_benchmark_allow_negative.cmake
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_NONLINEAR_OPERATOR_DIR}/model.ir
        -DARTIFACT=${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.expert
        -DCERTIFICATE=${SMAVE_NONLINEAR_OPERATOR_DIR}/operator.verify
        -DBUNDLE=${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime.bundle
        -DVALIDATION=${SMAVE_NONLINEAR_OPERATOR_DIR}/validation.txt
        -DPERFORMANCE=${SMAVE_NONLINEAR_OPERATOR_DIR}/performance.txt
        -DABLATION=${SMAVE_NONLINEAR_OPERATOR_DIR}/benchmark-traces/operator-ablation.txt
        -DTRACES=${SMAVE_NONLINEAR_OPERATOR_DIR}/runtime-traces
        -DOUTPUT=${SMAVE_NONLINEAR_OPERATOR_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_nonlinear_operator.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_OPERATOR_SHARED_BASELINE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/operator-shared-baseline")
add_custom_target(reproduce-operator-shared-baseline
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_OPERATOR_SHARED_BASELINE_DIR}"
    COMMAND python3
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/analyze_operator_shared_baseline.py
        --build ${CMAKE_CURRENT_BINARY_DIR}
        --evidence ${SMAVE_OPERATOR_SHARED_BASELINE_DIR}/evidence.txt
        --data ${CMAKE_CURRENT_SOURCE_DIR}/paper/data/operator_speedups.dat
        --tex ${CMAKE_CURRENT_SOURCE_DIR}/paper/generated/operator_shared_baseline_values.tex
    COMMAND ${CMAKE_COMMAND}
        -DEVIDENCE=${SMAVE_OPERATOR_SHARED_BASELINE_DIR}/evidence.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_operator_shared_baseline.cmake
    DEPENDS reproduce-phase4 reproduce-phase5 reproduce-nonlinear-operator
        reproduce-gate-architecture
    USES_TERMINAL)
add_dependencies(
    reproduce-hints-schedule-baseline
    reproduce-operator-shared-baseline)

set(SMAVE_ACADEMIC_EVIDENCE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/academic-evidence")
add_custom_target(academic-evidence
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_ACADEMIC_EVIDENCE_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DPHASE1_COUPLED=${SMAVE_PHASE1_DIR}/coupled/performance.txt
        -DPHASE1_CUBIC=${SMAVE_PHASE1_DIR}/cubic/performance.txt
        -DNONLINEAR_CASCADE=${SMAVE_NONLINEAR_CASCADE_DIR}
        -DPHASE4_EVALUATION=${SMAVE_PHASE4_DIR}/family-router-evaluation.txt
        -DPHASE4_HELDOUT=${SMAVE_PHASE4_DIR}/heldout-traces/heldout.competition
        -DPHASE4_EXTERNAL=${SMAVE_PHASE4_DIR}/heldout-traces/external-baselines.txt
        -DPAIRED_ORACLE=${SMAVE_PAIRED_ORACLE_DIR}/evidence.txt
        -DPHASE5_STATISTICS=${SMAVE_PHASE5_DIR}/benchmark-traces/operator-statistics.txt
        -DPHASE5_ABLATION=${SMAVE_PHASE5_DIR}/benchmark-traces/operator-ablation.txt
        -DPHASE5_EXTERNAL=${SMAVE_PHASE5_DIR}/benchmark-traces/operator-external-baselines.txt
        -DOPERATOR_REPLICATION=${SMAVE_OPERATOR_REPLICATION_DIR}/evidence.txt
        -DNONLINEAR_OPERATOR=${SMAVE_NONLINEAR_OPERATOR_DIR}/evidence.txt
        -DAMG_BACKEND=${SMAVE_AMG_BACKEND_DIR}/evidence.txt
        -DGATE_ARCHITECTURE=${SMAVE_GATE_ARCHITECTURE_DIR}/evidence.txt
        -DRISK_ADAPTIVE_GATE=${SMAVE_RISK_ADAPTIVE_GATE_DIR}/evidence.txt
        -DCOMPONENT_ABLATION=${SMAVE_COMPONENT_ABLATION_DIR}/evidence.txt
        -DOUTPUT=${SMAVE_ACADEMIC_EVIDENCE_DIR}/summary.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_academic_evidence.cmake
    DEPENDS reproduce-cascade-ordering reproduce-router-shift
        reproduce-complete-cost-decomposition reproduce-component-ablation
        reproduce-operator-replication reproduce-nonlinear-operator
        reproduce-amg-backend reproduce-gate-architecture reproduce-risk-adaptive-gate
    VERBATIM)
set(SMAVE_PHASE6_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase6")
add_custom_target(reproduce-phase6
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE6_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_PHASE6_DIR}/block-graphs"
        "${SMAVE_PHASE6_DIR}/slx-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-branch-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-switch-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-switch-invalid-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-subsystem-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-subsystem-invalid-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-duplicate-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-unresolved-source/simulink"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-chain-source/simulink"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/block-graphs/fallback-group.sbg"
        "${SMAVE_PHASE6_DIR}/block-graphs/fallback-group.sbg"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-basic/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-branch/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-branch-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-switch/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-switch-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-switch-invalid/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-switch-invalid-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-subsystem/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-subsystem-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-subsystem-invalid/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-subsystem-invalid-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-goto-from/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-goto-from-duplicate/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-duplicate-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-goto-from-unresolved/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-unresolved-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/slx/native-goto-from-chain/simulink/blockdiagram.xml"
        "${SMAVE_PHASE6_DIR}/slx-goto-from-chain-source/simulink/blockdiagram.xml"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-basic.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-branch-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-branch.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-switch-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-switch.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-switch-invalid-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-switch-invalid.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-subsystem-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-subsystem.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-subsystem-invalid-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-subsystem-invalid.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-goto-from-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-goto-from.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-goto-from-duplicate-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-goto-from-duplicate.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-goto-from-unresolved-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-goto-from-unresolved.slx"
        --format=zip -- simulink
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_PHASE6_DIR}/slx-goto-from-chain-source"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_PHASE6_DIR}/native-goto-from-chain.slx"
        --format=zip -- simulink
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/GroupFallback.mo"
        --top GroupFallback --output "${SMAVE_PHASE6_DIR}/GroupFallback.ir"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/block-graphs/fallback-group.sbg"
        --output "${SMAVE_PHASE6_DIR}/model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/model-group-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/repeat-model-group-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/model-group-report.txt"
        "${SMAVE_PHASE6_DIR}/repeat-model-group-report.txt"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/native-basic.slx"
        --output "${SMAVE_PHASE6_DIR}/native-model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group-multirate
        "${SMAVE_PHASE6_DIR}/native-model-group.ir"
        --end 0.1 --base-step 0.1
        --trace-dir "${SMAVE_PHASE6_DIR}/native-traces"
        --output "${SMAVE_PHASE6_DIR}/native-model-group-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group-multirate
        "${SMAVE_PHASE6_DIR}/native-model-group.ir"
        --end 0.1 --base-step 0.1
        --trace-dir "${SMAVE_PHASE6_DIR}/native-repeat-traces"
        --output "${SMAVE_PHASE6_DIR}/native-repeat-model-group-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/native-model-group-report.txt"
        "${SMAVE_PHASE6_DIR}/native-repeat-model-group-report.txt"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/native-branch.slx"
        --output "${SMAVE_PHASE6_DIR}/native-branch-model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-branch-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-branch-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-branch-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-branch-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/native-branch-report.txt"
        "${SMAVE_PHASE6_DIR}/native-branch-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/native-switch.slx"
        --output "${SMAVE_PHASE6_DIR}/native-switch-model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-switch-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-switch-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-switch-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-switch-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/native-switch-report.txt"
        "${SMAVE_PHASE6_DIR}/native-switch-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/native-subsystem.slx"
        --output "${SMAVE_PHASE6_DIR}/native-subsystem-model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-subsystem-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-subsystem-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-subsystem-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-subsystem-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/native-subsystem-report.txt"
        "${SMAVE_PHASE6_DIR}/native-subsystem-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${SMAVE_PHASE6_DIR}/native-goto-from.slx"
        --output "${SMAVE_PHASE6_DIR}/native-goto-from-model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-goto-from-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-goto-from-report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group
        "${SMAVE_PHASE6_DIR}/native-goto-from-model-group.ir"
        --output "${SMAVE_PHASE6_DIR}/native-goto-from-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/native-goto-from-report.txt"
        "${SMAVE_PHASE6_DIR}/native-goto-from-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_PHASE6_DIR}/native-switch-invalid.slx
        -DERROR_PATTERN=unsupported.SLX.Switch.Criteria
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_block_graph_import_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_PHASE6_DIR}/native-subsystem-invalid.slx
        -DERROR_PATTERN=duplicate.SubSystem.connector.Port
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_block_graph_import_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_PHASE6_DIR}/native-goto-from-duplicate.slx
        -DERROR_PATTERN=duplicate.local.Goto.tag
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_block_graph_import_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_PHASE6_DIR}/native-goto-from-unresolved.slx
        -DERROR_PATTERN=unresolved.local.From.tag
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_block_graph_import_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_PHASE6_DIR}/native-goto-from-chain.slx
        -DERROR_PATTERN=unsupported.connector/Goto/From.chain
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_block_graph_import_failure.cmake
    COMMAND $<TARGET_FILE:smave> run-hybrid
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/sampled-priority-reset.hybrid"
        --ticks 3
        --candidates "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/event-candidates.txt"
        --output "${SMAVE_PHASE6_DIR}/event-report.txt"
    COMMAND $<TARGET_FILE:smave> run-hybrid
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/sampled-priority-reset.hybrid"
        --ticks 3
        --candidates "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/event-candidates.txt"
        --output "${SMAVE_PHASE6_DIR}/repeat-event-report.txt"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_PHASE6_DIR}/event-report.txt"
        "${SMAVE_PHASE6_DIR}/repeat-event-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DGROUP_REPORT=${SMAVE_PHASE6_DIR}/model-group-report.txt
        -DNATIVE_GROUP_REPORT=${SMAVE_PHASE6_DIR}/native-model-group-report.txt
        -DNATIVE_GROUP_IR=${SMAVE_PHASE6_DIR}/native-model-group.ir
        -DNATIVE_BRANCH_REPORT=${SMAVE_PHASE6_DIR}/native-branch-report.txt
        -DNATIVE_BRANCH_IR=${SMAVE_PHASE6_DIR}/native-branch-model-group.ir
        -DNATIVE_SWITCH_REPORT=${SMAVE_PHASE6_DIR}/native-switch-report.txt
        -DNATIVE_SWITCH_IR=${SMAVE_PHASE6_DIR}/native-switch-model-group.ir
        -DNATIVE_SUBSYSTEM_REPORT=${SMAVE_PHASE6_DIR}/native-subsystem-report.txt
        -DNATIVE_SUBSYSTEM_IR=${SMAVE_PHASE6_DIR}/native-subsystem-model-group.ir
        -DNATIVE_GOTO_FROM_REPORT=${SMAVE_PHASE6_DIR}/native-goto-from-report.txt
        -DNATIVE_GOTO_FROM_IR=${SMAVE_PHASE6_DIR}/native-goto-from-model-group.ir
        -DEVENT_REPORT=${SMAVE_PHASE6_DIR}/event-report.txt
        -DGROUP_IR=${SMAVE_PHASE6_DIR}/model-group.ir
        -DMODEL_IR=${SMAVE_PHASE6_DIR}/GroupFallback.ir
        -DPLANT_TRACES=${SMAVE_PHASE6_DIR}/plant-runtime
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase6.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_PHASE7_DIR "${CMAKE_CURRENT_BINARY_DIR}/phase7")
add_custom_target(reproduce-phase7
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_PHASE7_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_PHASE7_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${SMAVE_PHASE7_DIR}/release.key
        -DCONTENT=smave-phase7-ephemeral-signing-key-2026
        -DPERMISSION_REPORT=${SMAVE_PHASE7_DIR}/key-permissions.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/write_private_key.cmake
    COMMAND $<TARGET_FILE:smave> compile
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/OperatorPoissonGrid10.mo"
        --top OperatorPoissonGrid10 --output "${SMAVE_PHASE7_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-training"
        --dataset phase7-operator-training
        --store "${SMAVE_PHASE7_DIR}/data-store"
        > "${SMAVE_PHASE7_DIR}/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DSNAPSHOT=${SMAVE_PHASE7_DIR}/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE7_DIR}/data-store
        -DDATASET_ID=phase7-operator-training
        -DOUTPUT=${SMAVE_PHASE7_DIR}/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-operator
        "${SMAVE_PHASE7_DIR}/model.ir" --block block-1
        --dataset-store "${SMAVE_PHASE7_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/training-dataset.manifest"
        --rank 4 --qoi x1,x50,x100
        --trace-dir "${SMAVE_PHASE7_DIR}/label-traces"
        --output "${SMAVE_PHASE7_DIR}/operator.expert"
    COMMAND $<TARGET_FILE:smave> verify-expert
        "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --max-depth 1 --output "${SMAVE_PHASE7_DIR}/operator.verify"
    COMMAND $<TARGET_FILE:smave> bundle
        "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --output "${SMAVE_PHASE7_DIR}/runtime.bundle"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios"
        --dataset phase7-validation-scenarios
        --store "${SMAVE_PHASE7_DIR}/data-store"
        > "${SMAVE_PHASE7_DIR}/dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DSNAPSHOT=${SMAVE_PHASE7_DIR}/dataset-snapshot.txt
        -DSTORE=${SMAVE_PHASE7_DIR}/data-store
        -DDATASET_ID=phase7-validation-scenarios
        -DOUTPUT=${SMAVE_PHASE7_DIR}/dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> validate
        "${SMAVE_PHASE7_DIR}/model.ir"
        --dataset-store "${SMAVE_PHASE7_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --trace-dir "${SMAVE_PHASE7_DIR}/runtime-traces"
        --output "${SMAVE_PHASE7_DIR}/validation.txt"
    COMMAND $<TARGET_FILE:smave> benchmark-operator
        "${SMAVE_PHASE7_DIR}/model.ir"
        --dataset-store "${SMAVE_PHASE7_DIR}/data-store"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --repetitions 10 --projected-queries 10000
        --trace-dir "${SMAVE_PHASE7_DIR}/benchmark-traces"
        --output "${SMAVE_PHASE7_DIR}/performance.txt"
    COMMAND $<TARGET_FILE:smave> audit-release
        "${SMAVE_PHASE7_DIR}/model.ir"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --validation "${SMAVE_PHASE7_DIR}/validation.txt"
        --performance "${SMAVE_PHASE7_DIR}/performance.txt"
        --stage shadow --traffic 0 --observed-hours 24
        --minimum-hours 24 --minimum-requests 64
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --output "${SMAVE_PHASE7_DIR}/shadow.audit"
    COMMAND $<TARGET_FILE:smave> audit-release
        "${SMAVE_PHASE7_DIR}/model.ir"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --validation "${SMAVE_PHASE7_DIR}/validation.txt"
        --performance "${SMAVE_PHASE7_DIR}/performance.txt"
        --stage canary --traffic 0.1 --observed-hours 24
        --minimum-hours 24 --minimum-requests 64
        --parent "${SMAVE_PHASE7_DIR}/shadow.audit"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --output "${SMAVE_PHASE7_DIR}/canary.audit"
    COMMAND $<TARGET_FILE:smave> sign-release
        "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --model "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --audit "${SMAVE_PHASE7_DIR}/canary.audit"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --release phase7-r1 --output "${SMAVE_PHASE7_DIR}/r1.manifest"
    COMMAND $<TARGET_FILE:smave> activate-release
        "${SMAVE_PHASE7_DIR}/r1.manifest"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --model "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --audit "${SMAVE_PHASE7_DIR}/canary.audit"
        --parent "${SMAVE_PHASE7_DIR}/shadow.audit"
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --store "${SMAVE_PHASE7_DIR}/store"
    COMMAND $<TARGET_FILE:smave> sign-release
        "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --model "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --audit "${SMAVE_PHASE7_DIR}/canary.audit"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --release phase7-r2 --output "${SMAVE_PHASE7_DIR}/r2.manifest"
    COMMAND $<TARGET_FILE:smave> activate-release
        "${SMAVE_PHASE7_DIR}/r2.manifest"
        --bundle "${SMAVE_PHASE7_DIR}/runtime.bundle"
        --model "${SMAVE_PHASE7_DIR}/model.ir"
        --expert "${SMAVE_PHASE7_DIR}/operator.expert"
        --certificate "${SMAVE_PHASE7_DIR}/operator.verify"
        --audit "${SMAVE_PHASE7_DIR}/canary.audit"
        --parent "${SMAVE_PHASE7_DIR}/shadow.audit"
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --dataset-manifest "${SMAVE_PHASE7_DIR}/dataset.manifest"
        --store "${SMAVE_PHASE7_DIR}/store"
    COMMAND $<TARGET_FILE:smave> rollback-release
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --store "${SMAVE_PHASE7_DIR}/store"
    COMMAND $<TARGET_FILE:smave> release-status
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --store "${SMAVE_PHASE7_DIR}/store"
        > "${SMAVE_PHASE7_DIR}/status.txt"
    COMMAND $<TARGET_FILE:smave> solve-release
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --store "${SMAVE_PHASE7_DIR}/store"
        --scenario "${CMAKE_CURRENT_SOURCE_DIR}/examples/operator-scenarios/case-00.conf"
        --trace-dir "${SMAVE_PHASE7_DIR}/production-traces"
        > "${SMAVE_PHASE7_DIR}/production-solve.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SMAVE_PHASE7_DIR}/store" "${SMAVE_PHASE7_DIR}/tampered-store"
    COMMAND ${CMAKE_COMMAND} -E echo_append "tamper"
        >> "${SMAVE_PHASE7_DIR}/tampered-store/releases/phase7-r1/runtime.bundle"
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:smave>
        -DSTORE=${SMAVE_PHASE7_DIR}/tampered-store
        -DKEY=${SMAVE_PHASE7_DIR}/release.key
        -DOUTPUT=${SMAVE_PHASE7_DIR}/tamper-rejection.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_release_status_failure.cmake
    COMMAND $<TARGET_FILE:smave> release-status
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --store "${SMAVE_PHASE7_DIR}/store"
        > "${SMAVE_PHASE7_DIR}/post-tamper-original-status.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SMAVE_PHASE7_DIR}/store" "${SMAVE_PHASE7_DIR}/dataset-tampered-store"
    COMMAND ${CMAKE_COMMAND} -E echo_append "tamper"
        >> "${SMAVE_PHASE7_DIR}/dataset-tampered-store/releases/phase7-r1/dataset.manifest"
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:smave>
        -DSTORE=${SMAVE_PHASE7_DIR}/dataset-tampered-store
        -DKEY=${SMAVE_PHASE7_DIR}/release.key
        -DOUTPUT=${SMAVE_PHASE7_DIR}/dataset-tamper-rejection.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_release_status_failure.cmake
    COMMAND $<TARGET_FILE:smave> release-status
        --key "${SMAVE_PHASE7_DIR}/release.key"
        --store "${SMAVE_PHASE7_DIR}/store"
        > "${SMAVE_PHASE7_DIR}/post-dataset-tamper-original-status.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSHADOW_AUDIT=${SMAVE_PHASE7_DIR}/shadow.audit
        -DCANARY_AUDIT=${SMAVE_PHASE7_DIR}/canary.audit
        -DMANIFEST_ONE=${SMAVE_PHASE7_DIR}/r1.manifest
        -DMANIFEST_TWO=${SMAVE_PHASE7_DIR}/r2.manifest
        -DSTATE=${SMAVE_PHASE7_DIR}/store/state
        -DSTATUS_OUTPUT=${SMAVE_PHASE7_DIR}/status.txt
        -DSOLVE_OUTPUT=${SMAVE_PHASE7_DIR}/production-solve.txt
        -DKEY_PERMISSION_REPORT=${SMAVE_PHASE7_DIR}/key-permissions.txt
        -DTAMPER_REJECTION=${SMAVE_PHASE7_DIR}/tamper-rejection.txt
        -DORIGINAL_STATUS_AFTER_TAMPER=${SMAVE_PHASE7_DIR}/post-tamper-original-status.txt
        -DDATASET_TAMPER_REJECTION=${SMAVE_PHASE7_DIR}/dataset-tamper-rejection.txt
        -DORIGINAL_STATUS_AFTER_DATASET_TAMPER=${SMAVE_PHASE7_DIR}/post-dataset-tamper-original-status.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_phase7.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_CONTINUOUS_DIR "${CMAKE_CURRENT_BINARY_DIR}/continuous")
add_custom_target(reproduce-continuous
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_CONTINUOUS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_CONTINUOUS_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/BouncingBall.mo"
        --top BouncingBall
        --output "${SMAVE_CONTINUOUS_DIR}/bouncing-ball.ir"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_CONTINUOUS_DIR}/bouncing-ball.ir"
        --end 1.2 --max-step 0.05
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/BouncingBall.ref"
        --output "${SMAVE_CONTINUOUS_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_CONTINUOUS_DIR}/bouncing-ball.ir"
        --end 1.2 --max-step 0.05
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/BouncingBall.ref"
        --output "${SMAVE_CONTINUOUS_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_CONTINUOUS_DIR}/bouncing-ball.ir
        -DREPORT=${SMAVE_CONTINUOUS_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_CONTINUOUS_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_continuous.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_MULTIRATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/multirate")
set(SMAVE_SIMULTANEOUS_DIR "${CMAKE_CURRENT_BINARY_DIR}/simultaneous")
add_custom_target(reproduce-simultaneous
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_SIMULTANEOUS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_SIMULTANEOUS_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/SimultaneousAtomic.mo"
        --top SimultaneousAtomic
        --output "${SMAVE_SIMULTANEOUS_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_SIMULTANEOUS_DIR}/model.ir"
        --end 1.1 --max-step 0.2
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/SimultaneousAtomic.ref"
        --output "${SMAVE_SIMULTANEOUS_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_SIMULTANEOUS_DIR}/model.ir"
        --end 1.1 --max-step 0.2
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/SimultaneousAtomic.ref"
        --output "${SMAVE_SIMULTANEOUS_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_SIMULTANEOUS_DIR}/model.ir
        -DREPORT=${SMAVE_SIMULTANEOUS_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_SIMULTANEOUS_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_simultaneous.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_EVENT_ITERATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/event-iteration")
add_custom_target(reproduce-event-iteration
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_EVENT_ITERATION_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_EVENT_ITERATION_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CascadingEvents.mo"
        --top CascadingEvents
        --output "${SMAVE_EVENT_ITERATION_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_EVENT_ITERATION_DIR}/model.ir"
        --end 1.1 --max-step 0.2
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/CascadingEvents.ref"
        --output "${SMAVE_EVENT_ITERATION_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_EVENT_ITERATION_DIR}/model.ir"
        --end 1.1 --max-step 0.2
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/CascadingEvents.ref"
        --output "${SMAVE_EVENT_ITERATION_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_EVENT_ITERATION_DIR}/model.ir
        -DREPORT=${SMAVE_EVENT_ITERATION_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_EVENT_ITERATION_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_event_iteration.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_INITIAL_EVENTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/initial-events")
add_custom_target(reproduce-initial-events
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_INITIAL_EVENTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_INITIAL_EVENTS_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/InitialEvents.mo"
        --top InitialEvents
        --output "${SMAVE_INITIAL_EVENTS_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_INITIAL_EVENTS_DIR}/model.ir"
        --end 0.1 --max-step 0.01
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/InitialEvents.ref"
        --output "${SMAVE_INITIAL_EVENTS_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_INITIAL_EVENTS_DIR}/model.ir"
        --end 0.1 --max-step 0.01
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/InitialEvents.ref"
        --output "${SMAVE_INITIAL_EVENTS_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_INITIAL_EVENTS_DIR}/model.ir
        -DREPORT=${SMAVE_INITIAL_EVENTS_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_INITIAL_EVENTS_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_initial_events.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_GRAZING_DIR "${CMAKE_CURRENT_BINARY_DIR}/grazing")
add_custom_target(reproduce-grazing
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_GRAZING_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_GRAZING_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/GrazingContact.mo"
        --top GrazingContact
        --output "${SMAVE_GRAZING_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_GRAZING_DIR}/model.ir"
        --end 2 --max-step 0.5
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/GrazingContact.ref"
        --output "${SMAVE_GRAZING_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-continuous
        "${SMAVE_GRAZING_DIR}/model.ir"
        --end 2 --max-step 0.5
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/examples/GrazingContact.ref"
        --output "${SMAVE_GRAZING_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_GRAZING_DIR}/model.ir
        -DREPORT=${SMAVE_GRAZING_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_GRAZING_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_grazing.cmake
    DEPENDS smave
    USES_TERMINAL
)

add_custom_target(reproduce-multirate
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_MULTIRATE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_MULTIRATE_DIR}"
    COMMAND $<TARGET_FILE:smave> import-block-graph
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/block-graphs/multirate-accumulator.sbg"
        --output "${SMAVE_MULTIRATE_DIR}/model-group.ir"
    COMMAND $<TARGET_FILE:smave> run-model-group-multirate
        "${SMAVE_MULTIRATE_DIR}/model-group.ir"
        --end 0.4 --base-step 0.1
        --trace-dir "${SMAVE_MULTIRATE_DIR}/traces"
        --output "${SMAVE_MULTIRATE_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> run-model-group-multirate
        "${SMAVE_MULTIRATE_DIR}/model-group.ir"
        --end 0.4 --base-step 0.1
        --trace-dir "${SMAVE_MULTIRATE_DIR}/repeat-traces"
        --output "${SMAVE_MULTIRATE_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_MULTIRATE_DIR}/model-group.ir
        -DREPORT=${SMAVE_MULTIRATE_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_MULTIRATE_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_multirate.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_COUPLED_DIR "${CMAKE_CURRENT_BINARY_DIR}/coupled")
add_custom_target(reproduce-coupled
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_COUPLED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_COUPLED_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CoupledThermostat.mo"
        --top CoupledThermostat
        --output "${SMAVE_COUPLED_DIR}/continuous.ir"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-thermostat.hybrid"
        --end 2.5 --max-step 0.1
        --output "${SMAVE_COUPLED_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-thermostat.hybrid"
        --end 2.5 --max-step 0.1
        --output "${SMAVE_COUPLED_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_COUPLED_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_COUPLED_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_coupled.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_COUPLED_OFFSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/coupled-offset")
add_custom_target(reproduce-coupled-offset
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_COUPLED_OFFSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_COUPLED_OFFSET_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CoupledThermostat.mo"
        --top CoupledThermostat
        --output "${SMAVE_COUPLED_OFFSET_DIR}/continuous.ir"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_OFFSET_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-offset.hybrid"
        --end 1 --max-step 0.05
        --output "${SMAVE_COUPLED_OFFSET_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_OFFSET_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-offset.hybrid"
        --end 1 --max-step 0.05
        --output "${SMAVE_COUPLED_OFFSET_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_COUPLED_OFFSET_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_COUPLED_OFFSET_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_coupled_offset.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_COUPLED_INITIAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/coupled-initial")
add_custom_target(reproduce-coupled-initial
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_COUPLED_INITIAL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_COUPLED_INITIAL_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CoupledInitialEvent.mo"
        --top CoupledInitialEvent
        --output "${SMAVE_COUPLED_INITIAL_DIR}/continuous.ir"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_INITIAL_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-initial-event.hybrid"
        --end 0.5 --max-step 0.1
        --output "${SMAVE_COUPLED_INITIAL_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_INITIAL_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-initial-event.hybrid"
        --end 0.5 --max-step 0.1
        --output "${SMAVE_COUPLED_INITIAL_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_COUPLED_INITIAL_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_COUPLED_INITIAL_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_coupled_initial.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_COUPLED_SUPERDENSE_DIR "${CMAKE_CURRENT_BINARY_DIR}/coupled-superdense")
add_custom_target(reproduce-coupled-superdense
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_COUPLED_SUPERDENSE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_COUPLED_SUPERDENSE_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-continuous
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/CoupledSuperdense.mo"
        --top CoupledSuperdense
        --output "${SMAVE_COUPLED_SUPERDENSE_DIR}/continuous.ir"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_SUPERDENSE_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-superdense.hybrid"
        --end 1 --max-step 0.2
        --output "${SMAVE_COUPLED_SUPERDENSE_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-coupled
        "${SMAVE_COUPLED_SUPERDENSE_DIR}/continuous.ir"
        --hybrid "${CMAKE_CURRENT_SOURCE_DIR}/examples/hybrid/coupled-superdense.hybrid"
        --end 1 --max-step 0.2
        --output "${SMAVE_COUPLED_SUPERDENSE_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_COUPLED_SUPERDENSE_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_COUPLED_SUPERDENSE_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_coupled_superdense.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_FMI_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi")
add_custom_target(reproduce-fmi
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription.xml"
        "${SMAVE_FMI_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_DIR}/SMAVEBlackbox.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI_DIR}/SMAVEBlackbox.fmu"
        --output "${SMAVE_FMI_DIR}/model.fmi.ir"
        --report "${SMAVE_FMI_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI_DIR}/SMAVEBlackbox.fmu"
        --output "${SMAVE_FMI_DIR}/repeat.fmi.ir"
        --report "${SMAVE_FMI_DIR}/repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> inspect-fmi
        "${SMAVE_FMI_DIR}/model.fmi.ir"
        > "${SMAVE_FMI_DIR}/inspect.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_DIR}/SMAVEBlackbox.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_DIR}/smoke.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_DIR}/SMAVEBlackbox.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_DIR}/repeat-smoke.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_FMI_DIR}/model.fmi.ir
        -DREPEAT_IR=${SMAVE_FMI_DIR}/repeat.fmi.ir
        -DREPORT=${SMAVE_FMI_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_DIR}/repeat-report.txt
        -DINSPECT=${SMAVE_FMI_DIR}/inspect.txt
        -DSMOKE=${SMAVE_FMI_DIR}/smoke.txt
        -DREPEAT_SMOKE=${SMAVE_FMI_DIR}/repeat-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_SE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-se")
add_custom_target(reproduce-fmi-se
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_SE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_SE_DIR}/valid/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_SE_DIR}/multi/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_SE_DIR}/no-clock/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_SE_DIR}/missing-priority/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_SE_DIR}/invalid-preemption/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEScheduledExecution/modelDescription.xml"
        "${SMAVE_FMI_SE_DIR}/valid/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEScheduledExecutionMulti/modelDescription.xml"
        "${SMAVE_FMI_SE_DIR}/multi/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEScheduledExecutionNoClock/modelDescription.xml"
        "${SMAVE_FMI_SE_DIR}/no-clock/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEScheduledExecutionMissingPriority/modelDescription.xml"
        "${SMAVE_FMI_SE_DIR}/missing-priority/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEScheduledExecutionInvalidPreemption/modelDescription.xml"
        "${SMAVE_FMI_SE_DIR}/invalid-preemption/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_SE_DIR}/valid/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_SE_DIR}/multi/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_SE_DIR}/no-clock/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_SE_DIR}/missing-priority/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_SE_DIR}/invalid-preemption/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_SE_DIR}/valid"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecution.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_SE_DIR}/multi"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMulti.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_SE_DIR}/no-clock"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionNoClock.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_SE_DIR}/missing-priority"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMissingPriority.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_SE_DIR}/invalid-preemption"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionInvalidPreemption.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-se
        "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecution.fmu"
        --end 0.5 --interval 0.25 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI_SE_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-se
        "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecution.fmu"
        --end 0.5 --interval 0.25 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI_SE_DIR}/repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-se
        "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMulti.fmu"
        --end 0.5 --interval 0.25 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI_SE_DIR}/multi-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-se
        "${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMulti.fmu"
        --end 0.5 --interval 0.25 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI_SE_DIR}/multi-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DVALID_FMU=${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecution.fmu
        -DMULTI_FMU=${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMulti.fmu
        -DNO_CLOCK_FMU=${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionNoClock.fmu
        -DMISSING_PRIORITY_FMU=${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionMissingPriority.fmu
        -DINVALID_PREEMPTION_FMU=${SMAVE_FMI_SE_DIR}/SMAVEScheduledExecutionInvalidPreemption.fmu
        -DREPORT=${SMAVE_FMI_SE_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_SE_DIR}/repeat-report.txt
        -DMULTI_REPORT=${SMAVE_FMI_SE_DIR}/multi-report.txt
        -DMULTI_REPEAT_REPORT=${SMAVE_FMI_SE_DIR}/multi-repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_se.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI2_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi2")
add_custom_target(reproduce-fmi2
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI2_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI2_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2.xml"
        "${SMAVE_FMI2_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_fixture>"
        "${SMAVE_FMI2_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI2_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI2_DIR}/SMAVEFmi2.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI2_DIR}/SMAVEFmi2.fmu"
        --output "${SMAVE_FMI2_DIR}/model.fmi.ir"
        --report "${SMAVE_FMI2_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_DIR}/SMAVEFmi2.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI2_DIR}/smoke.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_DIR}/SMAVEFmi2.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI2_DIR}/repeat-smoke.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_FMI2_DIR}/model.fmi.ir
        -DREPORT=${SMAVE_FMI2_DIR}/report.txt
        -DSMOKE=${SMAVE_FMI2_DIR}/smoke.txt
        -DREPEAT_SMOKE=${SMAVE_FMI2_DIR}/repeat-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi2.cmake
    DEPENDS smave smave_fmi2_fixture
    USES_TERMINAL
)

set(SMAVE_SSP_DIR "${CMAKE_CURRENT_BINARY_DIR}/ssp")
add_custom_target(reproduce-ssp
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_SSP_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_SSP_DIR}/fmu-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi2-unit-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-unit-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-event-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-time-event-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi2-variable-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/fmi3-early-package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_SSP_DIR}/feedforward/resources"
        "${SMAVE_SSP_DIR}/mixed/resources"
        "${SMAVE_SSP_DIR}/transformed/resources"
        "${SMAVE_SSP_DIR}/invalid-transformation/resources"
        "${SMAVE_SSP_DIR}/invalid-unit-metadata/resources"
        "${SMAVE_SSP_DIR}/event/resources"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event/resources"
        "${SMAVE_SSP_DIR}/time-event/resources"
        "${SMAVE_SSP_DIR}/time-event-fixed/resources"
        "${SMAVE_SSP_DIR}/fmi3-early/resources"
        "${SMAVE_SSP_DIR}/cycle/resources"
        "${SMAVE_SSP_DIR}/invalid-connector/resources"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2.xml"
        "${SMAVE_SSP_DIR}/fmu-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_fixture>"
        "${SMAVE_SSP_DIR}/fmu-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmu-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription.xml"
        "${SMAVE_SSP_DIR}/fmi3-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/transformed/modelDescription2.xml"
        "${SMAVE_SSP_DIR}/fmi2-unit-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_fixture>"
        "${SMAVE_SSP_DIR}/fmi2-unit-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi2-unit-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component2-unit.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/transformed/modelDescription3.xml"
        "${SMAVE_SSP_DIR}/fmi3-unit-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-unit-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-unit-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3-unit.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEEventCS/modelDescription.xml"
        "${SMAVE_SSP_DIR}/fmi3-event-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-event-package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEEventCS$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-event-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3-event.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/time-event/modelDescription3.xml"
        "${SMAVE_SSP_DIR}/fmi3-time-event-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-time-event-package/binaries/${SMAVE_FMI_PLATFORM}/SMAVETimeEventCS$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-time-event-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3-time-event.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/time-event/modelDescription2.xml"
        "${SMAVE_SSP_DIR}/fmi2-variable-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_fixture>"
        "${SMAVE_SSP_DIR}/fmi2-variable-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi2-variable-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component2-variable.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/fmi3-undeclared-event/modelDescription.xml"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-undeclared-event-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3-undeclared-event.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/fmi3-early/modelDescription.xml"
        "${SMAVE_SSP_DIR}/fmi3-early-package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_SSP_DIR}/fmi3-early-package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-early-package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/component3-early.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/feedforward/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/feedforward/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/mixed/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/mixed/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/transformed/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/transformed/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/invalid-transformation/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/invalid-transformation/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/invalid-unit-metadata/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/invalid-unit-metadata/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/event/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/event/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/fmi3-undeclared-event/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/time-event/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/time-event/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/time-event-fixed/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/time-event-fixed/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/fmi3-early/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/fmi3-early/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/cycle/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/cycle/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/ssp/invalid-connector/SystemStructure.ssd"
        "${SMAVE_SSP_DIR}/invalid-connector/SystemStructure.ssd"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/feedforward/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/feedforward/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/mixed/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3.fmu"
        "${SMAVE_SSP_DIR}/mixed/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component2-unit.fmu"
        "${SMAVE_SSP_DIR}/transformed/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-unit.fmu"
        "${SMAVE_SSP_DIR}/transformed/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component2-unit.fmu"
        "${SMAVE_SSP_DIR}/invalid-transformation/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-unit.fmu"
        "${SMAVE_SSP_DIR}/invalid-transformation/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component2-unit.fmu"
        "${SMAVE_SSP_DIR}/invalid-unit-metadata/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-unit.fmu"
        "${SMAVE_SSP_DIR}/invalid-unit-metadata/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-event.fmu"
        "${SMAVE_SSP_DIR}/event/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/event/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-undeclared-event.fmu"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/fmi3-undeclared-event/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-time-event.fmu"
        "${SMAVE_SSP_DIR}/time-event/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component2-variable.fmu"
        "${SMAVE_SSP_DIR}/time-event/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-time-event.fmu"
        "${SMAVE_SSP_DIR}/time-event-fixed/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/time-event-fixed/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/fmi3-early/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component3-early.fmu"
        "${SMAVE_SSP_DIR}/fmi3-early/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/cycle/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/cycle/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/invalid-connector/resources/a.fmu"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${SMAVE_SSP_DIR}/component.fmu"
        "${SMAVE_SSP_DIR}/invalid-connector/resources/b.fmu"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/feedforward"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/feedforward.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/mixed"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/mixed.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/transformed"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/transformed.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/invalid-transformation"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/invalid-transformation.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/invalid-unit-metadata"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/invalid-unit-metadata.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/event"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/event.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-undeclared-event"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/fmi3-undeclared-event.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/time-event"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/time-event.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/time-event-fixed"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/time-event-fixed.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/fmi3-early"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/fmi3-early.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/cycle"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/cycle.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_SSP_DIR}/invalid-connector"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_SSP_DIR}/invalid-connector.ssp"
        --format=zip -- SystemStructure.ssd resources
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/feedforward.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/feedforward.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/mixed.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/mixed-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/mixed.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/mixed-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/transformed.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/transformed-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/transformed.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/transformed-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/event.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/event-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/event.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/event-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/time-event.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/time-event-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-ssp
        "${SMAVE_SSP_DIR}/time-event.ssp"
        --end 0.3 --step 0.1 --allow-native-execution
        --output "${SMAVE_SSP_DIR}/time-event-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/feedforward.ssp
        -DERROR_PATTERN=requires.explicit.--allow-native-execution
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/cycle.ssp
        -DERROR_PATTERN=rejects.algebraic.connection.loops
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/invalid-connector.ssp
        -DERROR_PATTERN=does.not.match.a.scalar.Real.FMI.input
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/fmi3-early.ssp
        -DERROR_PATTERN=rejects.invalid.or.undeclared.FMI.3.early.return
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/invalid-transformation.ssp
        -DERROR_PATTERN=unit.conversion.requires.compatible.BaseUnit.dimensions
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/invalid-unit-metadata.ssp
        -DERROR_PATTERN=SSP.Unit.does.not.match.embedded.FMU.UnitDefinitions
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/fmi3-undeclared-event.ssp
        -DERROR_PATTERN=requested.event.handling.without.hasEventMode
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DINPUT=${SMAVE_SSP_DIR}/time-event-fixed.ssp
        -DERROR_PATTERN=time-event.splitting.requires.variable.communication.steps.on.every.component
        -DALLOW_NATIVE_EXECUTION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_ssp_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_SSP_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_SSP_DIR}/repeat-report.txt
        -DMIXED_REPORT=${SMAVE_SSP_DIR}/mixed-report.txt
        -DMIXED_REPEAT_REPORT=${SMAVE_SSP_DIR}/mixed-repeat-report.txt
        -DTRANSFORMED_REPORT=${SMAVE_SSP_DIR}/transformed-report.txt
        -DTRANSFORMED_REPEAT_REPORT=${SMAVE_SSP_DIR}/transformed-repeat-report.txt
        -DEVENT_REPORT=${SMAVE_SSP_DIR}/event-report.txt
        -DEVENT_REPEAT_REPORT=${SMAVE_SSP_DIR}/event-repeat-report.txt
        -DTIME_EVENT_REPORT=${SMAVE_SSP_DIR}/time-event-report.txt
        -DTIME_EVENT_REPEAT_REPORT=${SMAVE_SSP_DIR}/time-event-repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_ssp.cmake
    DEPENDS smave smave_fmi_fixture smave_fmi2_fixture
    USES_TERMINAL
)

set(SMAVE_FMI2_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi2-event")
add_custom_target(reproduce-fmi2-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI2_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI2_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2Event.xml"
        "${SMAVE_FMI2_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_event_fixture>"
        "${SMAVE_FMI2_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_event_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI2_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI2_EVENT_DIR}/SMAVEFmi2Event.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI2_EVENT_DIR}/SMAVEFmi2Event.fmu"
        --output "${SMAVE_FMI2_EVENT_DIR}/model.fmi.ir"
        --report "${SMAVE_FMI2_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_EVENT_DIR}/SMAVEFmi2Event.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI2_EVENT_DIR}/smoke.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_EVENT_DIR}/SMAVEFmi2Event.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution --output "${SMAVE_FMI2_EVENT_DIR}/repeat-smoke.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_FMI2_EVENT_DIR}/model.fmi.ir
        -DREPORT=${SMAVE_FMI2_EVENT_DIR}/report.txt
        -DSMOKE=${SMAVE_FMI2_EVENT_DIR}/smoke.txt
        -DREPEAT_SMOKE=${SMAVE_FMI2_EVENT_DIR}/repeat-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi2_event.cmake
    DEPENDS smave smave_fmi2_event_fixture
    USES_TERMINAL
)

set(SMAVE_FMI2_ASYNC_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi2-async")
add_custom_target(reproduce-fmi2-async
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI2_ASYNC_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI2_ASYNC_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2Async.xml"
        "${SMAVE_FMI2_ASYNC_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_async_fixture>"
        "${SMAVE_FMI2_ASYNC_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_async_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI2_ASYNC_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI2_ASYNC_DIR}/SMAVEFmi2Async.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI2_ASYNC_DIR}/SMAVEFmi2Async.fmu"
        --output "${SMAVE_FMI2_ASYNC_DIR}/model.fmi.ir"
        --report "${SMAVE_FMI2_ASYNC_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_ASYNC_DIR}/SMAVEFmi2Async.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --async-timeout-ms 25
        --allow-native-execution --output "${SMAVE_FMI2_ASYNC_DIR}/smoke.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI2_ASYNC_DIR}/SMAVEFmi2Async.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --async-timeout-ms 25
        --allow-native-execution --output "${SMAVE_FMI2_ASYNC_DIR}/repeat-smoke.txt"
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:smave>
        -DFMU=${SMAVE_FMI2_ASYNC_DIR}/SMAVEFmi2Async.fmu
        -DOUTPUT=${SMAVE_FMI2_ASYNC_DIR}/invalid-timeout.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_fmi_timeout_failure.cmake
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_FMI2_ASYNC_DIR}/model.fmi.ir
        -DREPORT=${SMAVE_FMI2_ASYNC_DIR}/report.txt
        -DSMOKE=${SMAVE_FMI2_ASYNC_DIR}/smoke.txt
        -DREPEAT_SMOKE=${SMAVE_FMI2_ASYNC_DIR}/repeat-smoke.txt
        -DINVALID_TIMEOUT=${SMAVE_FMI2_ASYNC_DIR}/invalid-timeout.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi2_async.cmake
    DEPENDS smave smave_fmi2_async_fixture
    USES_TERMINAL
)

set(SMAVE_FMI2_ME_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi2-me")
add_custom_target(reproduce-fmi2-me
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI2_ME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI2_ME_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2ME.xml"
        "${SMAVE_FMI2_ME_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_me_fixture>"
        "${SMAVE_FMI2_ME_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_me_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI2_ME_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI2_ME_DIR}/SMAVEFmi2ME.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> import-fmu
        "${SMAVE_FMI2_ME_DIR}/SMAVEFmi2ME.fmu"
        --output "${SMAVE_FMI2_ME_DIR}/model.fmi.ir"
        --report "${SMAVE_FMI2_ME_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI2_ME_DIR}/SMAVEFmi2ME.fmu"
        --end 0.2 --step 0.1 --input rate=1
        --allow-native-execution --output "${SMAVE_FMI2_ME_DIR}/smoke.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI2_ME_DIR}/SMAVEFmi2ME.fmu"
        --end 0.2 --step 0.1 --input rate=1
        --allow-native-execution --output "${SMAVE_FMI2_ME_DIR}/repeat-smoke.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_FMI2_ME_DIR}/model.fmi.ir
        -DREPORT=${SMAVE_FMI2_ME_DIR}/report.txt
        -DSMOKE=${SMAVE_FMI2_ME_DIR}/smoke.txt
        -DREPEAT_SMOKE=${SMAVE_FMI2_ME_DIR}/repeat-smoke.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi2_me.cmake
    DEPENDS smave smave_fmi2_me_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me")
add_custom_target(reproduce-fmi-me
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchange/modelDescription.xml"
        "${SMAVE_FMI_ME_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchange$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_DIR}/SMAVEModelExchange.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_DIR}/SMAVEModelExchange.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_DIR}/SMAVEModelExchange.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_ME_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_ME_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-event")
add_custom_target(reproduce-fmi-me-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeEvent/modelDescription.xml"
        "${SMAVE_FMI_ME_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchangeEvent$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_EVENT_DIR}/SMAVEModelExchangeEvent.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_EVENT_DIR}/SMAVEModelExchangeEvent.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_EVENT_DIR}/SMAVEModelExchangeEvent.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_EVENT_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_ME_EVENT_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_ME_EVENT_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_event.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_MULTI_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-multi-event")
add_custom_target(reproduce-fmi-me-multi-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_MULTI_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeMultiEvent/modelDescription.xml"
        "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchangeMultiEvent$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/SMAVEModelExchangeMultiEvent.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/SMAVEModelExchangeMultiEvent.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/SMAVEModelExchangeMultiEvent.fmu"
        --end 0.3 --step 0.1 --input gain=1
        --allow-native-execution
        --output "${SMAVE_FMI_ME_MULTI_EVENT_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_ME_MULTI_EVENT_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_ME_MULTI_EVENT_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_multi_event.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_TIME_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-time-event")
add_custom_target(reproduce-fmi-me-time-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_TIME_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_TIME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeTimeEvent/modelDescription.xml"
        "${SMAVE_FMI_ME_TIME_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_TIME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchangeTimeEvent$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_TIME_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_TIME_EVENT_DIR}/SMAVEModelExchangeTimeEvent.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_TIME_EVENT_DIR}/SMAVEModelExchangeTimeEvent.fmu"
        --end 0.3 --step 0.1 --input gain=0
        --allow-native-execution
        --output "${SMAVE_FMI_ME_TIME_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_TIME_EVENT_DIR}/SMAVEModelExchangeTimeEvent.fmu"
        --end 0.3 --step 0.1 --input gain=0
        --allow-native-execution
        --output "${SMAVE_FMI_ME_TIME_EVENT_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_ME_TIME_EVENT_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_ME_TIME_EVENT_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_time_event.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_GRAZING_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-grazing")
add_custom_target(reproduce-fmi-me-grazing
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_GRAZING_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2MEGrazing.xml"
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_me_fixture>"
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_me_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEFmi2MEGrazing.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeGrazing/modelDescription.xml"
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchangeGrazing$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEModelExchangeGrazing.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEFmi2MEGrazing.fmu"
        --end 0.2 --step 0.1 --input rate=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEFmi2MEGrazing.fmu"
        --end 0.2 --step 0.1 --input rate=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_GRAZING_DIR}/fmi2-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEModelExchangeGrazing.fmu"
        --end 0.2 --step 0.1 --input gain=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_GRAZING_DIR}/SMAVEModelExchangeGrazing.fmu"
        --end 0.2 --step 0.1 --input gain=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_GRAZING_DIR}/fmi3-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DFMI2_REPORT=${SMAVE_FMI_ME_GRAZING_DIR}/fmi2-report.txt
        -DFMI2_REPEAT_REPORT=${SMAVE_FMI_ME_GRAZING_DIR}/fmi2-repeat-report.txt
        -DFMI3_REPORT=${SMAVE_FMI_ME_GRAZING_DIR}/fmi3-report.txt
        -DFMI3_REPEAT_REPORT=${SMAVE_FMI_ME_GRAZING_DIR}/fmi3-repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_grazing.cmake
    DEPENDS smave smave_fmi_fixture smave_fmi2_me_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_NOMINAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-nominal")
add_custom_target(reproduce-fmi-me-nominal
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_NOMINAL_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2MENominal.xml"
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi2_me_fixture>"
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_me_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEFmi2MENominal.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeNominal/modelDescription.xml"
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEModelExchangeNominal$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEModelExchangeNominal.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEFmi2MENominal.fmu"
        --end 0.2 --step 0.1 --input rate=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEFmi2MENominal.fmu"
        --end 0.2 --step 0.1 --input rate=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEModelExchangeNominal.fmu"
        --end 0.2 --step 0.1 --input gain=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_NOMINAL_DIR}/SMAVEModelExchangeNominal.fmu"
        --end 0.2 --step 0.1 --input gain=1 --allow-native-execution
        --output "${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DFMI2_REPORT=${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2-report.txt
        -DFMI2_REPEAT_REPORT=${SMAVE_FMI_ME_NOMINAL_DIR}/fmi2-repeat-report.txt
        -DFMI3_REPORT=${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3-report.txt
        -DFMI3_REPEAT_REPORT=${SMAVE_FMI_ME_NOMINAL_DIR}/fmi3-repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_nominal.cmake
    DEPENDS smave smave_fmi_fixture smave_fmi2_me_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_ME_SCALAR_TYPES_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-me-scalar-types")
add_custom_target(reproduce-fmi-me-scalar-types
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-dynamic/package/binaries/${SMAVE_FMI_PLATFORM}"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-clock/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEBlackbox/modelDescription2MEScalarTypes.xml"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi2_me_fixture>"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi2_me_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEFmi2MEScalarTypes.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeScalarTypes/modelDescription.xml"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeScalarTypes.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeInvalidDynamic/modelDescription.xml"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-dynamic/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-dynamic/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-dynamic/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeInvalidDynamic.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEModelExchangeInvalidClock/modelDescription.xml"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-clock/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-clock/package/binaries/${SMAVE_FMI_PLATFORM}/$<TARGET_FILE_NAME:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/invalid-clock/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeInvalidClock.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEFmi2MEScalarTypes.fmu"
        --end 0.2 --step 0.1 --input rate=1,count=4,flag=1,mode=2
        --allow-native-execution --string-input label=alpha --output "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEFmi2MEScalarTypes.fmu"
        --end 0.2 --step 0.1 --input rate=1,count=4,flag=1,mode=2
        --allow-native-execution --string-input label=alpha --output "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2-repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeScalarTypes.fmu"
        --end 0.2 --step 0.1 --input gain=1,count=7,flag=0,float32_value=1.5,int8_value=-3,uint8_value=4,int16_value=-5,uint16_value=6,uint32_value=7,int64_value=-8,uint64_value=9,mode=2,clock_value=1
        --allow-native-execution --array-input vector=1,2,3 --array-input dynamic_vector=1,2,3 --array-input int32_array=-2,0,4 --array-input boolean_array=0,1,0 --array-input float32_array=1,2,3 --string-input label=beta --string-array-input "string_array=alpha,beta=gamma" --string-array-input "string_array=" --string-array-input string_array=omega --binary-input blob=00a5ff --binary-array-input "binary_array=" --binary-array-input binary_array=00a5 --binary-array-input binary_array=ff --output "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3-report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu-me
        "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeScalarTypes.fmu"
        --end 0.2 --step 0.1 --input gain=1,count=7,flag=0,float32_value=1.5,int8_value=-3,uint8_value=4,int16_value=-5,uint16_value=6,uint32_value=7,int64_value=-8,uint64_value=9,mode=2,clock_value=1
        --allow-native-execution --array-input vector=1,2,3 --array-input dynamic_vector=1,2,3 --array-input int32_array=-2,0,4 --array-input boolean_array=0,1,0 --array-input float32_array=1,2,3 --string-input label=beta --string-array-input "string_array=alpha,beta=gamma" --string-array-input "string_array=" --string-array-input string_array=omega --binary-input blob=00a5ff --binary-array-input "binary_array=" --binary-array-input binary_array=00a5 --binary-array-input binary_array=ff --output "${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3-repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DFMI2_FMU=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEFmi2MEScalarTypes.fmu
        -DFMI3_FMU=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeScalarTypes.fmu
        -DINVALID_DYNAMIC_FMU=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeInvalidDynamic.fmu
        -DINVALID_CLOCK_FMU=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/SMAVEModelExchangeInvalidClock.fmu
        -DFMI2_REPORT=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2-report.txt
        -DFMI2_REPEAT_REPORT=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi2-repeat-report.txt
        -DFMI3_REPORT=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3-report.txt
        -DFMI3_REPEAT_REPORT=${SMAVE_FMI_ME_SCALAR_TYPES_DIR}/fmi3-repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_me_scalar_types.cmake
    DEPENDS smave smave_fmi_fixture smave_fmi2_me_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-event")
add_custom_target(reproduce-fmi-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEEventCS/modelDescription.xml"
        "${SMAVE_FMI_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEEventCS$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_EVENT_DIR}/SMAVEEventCS.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_EVENT_DIR}/SMAVEEventCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_EVENT_DIR}/SMAVEEventCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_EVENT_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_EVENT_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_EVENT_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_event.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_EARLY_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-early")
add_custom_target(reproduce-fmi-early
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_EARLY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_EARLY_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVEEarlyCS/modelDescription.xml"
        "${SMAVE_FMI_EARLY_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_EARLY_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVEEarlyCS$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_EARLY_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_EARLY_DIR}/SMAVEEarlyCS.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_EARLY_DIR}/SMAVEEarlyCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_EARLY_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_EARLY_DIR}/SMAVEEarlyCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_EARLY_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_EARLY_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_EARLY_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_early.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_FMI_TIME_EVENT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fmi-time-event")
add_custom_target(reproduce-fmi-time-event
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_FMI_TIME_EVENT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SMAVE_FMI_TIME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/fmi/SMAVETimeEventCS/modelDescription.xml"
        "${SMAVE_FMI_TIME_EVENT_DIR}/package/modelDescription.xml"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:smave_fmi_fixture>"
        "${SMAVE_FMI_TIME_EVENT_DIR}/package/binaries/${SMAVE_FMI_PLATFORM}/SMAVETimeEventCS$<TARGET_FILE_SUFFIX:smave_fmi_fixture>"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SMAVE_FMI_TIME_EVENT_DIR}/package"
        ${CMAKE_COMMAND} -E tar cf "${SMAVE_FMI_TIME_EVENT_DIR}/SMAVETimeEventCS.fmu"
        --format=zip -- modelDescription.xml binaries
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_TIME_EVENT_DIR}/SMAVETimeEventCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_TIME_EVENT_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> smoke-fmu
        "${SMAVE_FMI_TIME_EVENT_DIR}/SMAVETimeEventCS.fmu"
        --end 0.3 --step 0.1 --input gain=3,u=2
        --allow-native-execution
        --output "${SMAVE_FMI_TIME_EVENT_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_FMI_TIME_EVENT_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_FMI_TIME_EVENT_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_fmi_time_event.cmake
    DEPENDS smave smave_fmi_fixture
    USES_TERMINAL
)

set(SMAVE_DAE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae")
add_custom_target(reproduce-dae
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/IndexOneDAE.mo"
        --top IndexOneDAE --output "${SMAVE_DAE_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_DIR}/model.ir"
        --end 1.0 --max-step 0.1 --output "${SMAVE_DAE_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_DIR}/model.ir"
        --end 1.0 --max-step 0.1 --output "${SMAVE_DAE_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DREPORT=${SMAVE_DAE_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_DAE_MULTIGRID_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae-multigrid")
add_custom_target(reproduce-dae-multigrid
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_MULTIGRID_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_MULTIGRID_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/SymmetricSpdDAE.mo"
        --top SymmetricSpdDAE
        --output "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> snapshot-data
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/dae-multigrid-training"
        --dataset dae-multigrid-training
        --store "${SMAVE_DAE_MULTIGRID_DIR}/data-store"
        > "${SMAVE_DAE_MULTIGRID_DIR}/training-dataset-snapshot.txt"
    COMMAND ${CMAKE_COMMAND}
        -DSNAPSHOT=${SMAVE_DAE_MULTIGRID_DIR}/training-dataset-snapshot.txt
        -DSTORE=${SMAVE_DAE_MULTIGRID_DIR}/data-store
        -DDATASET_ID=dae-multigrid-training
        -DOUTPUT=${SMAVE_DAE_MULTIGRID_DIR}/training-dataset.manifest
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/export_dataset_manifest.cmake
    COMMAND $<TARGET_FILE:smave> train-dae-multigrid
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --dataset-store "${SMAVE_DAE_MULTIGRID_DIR}/data-store"
        --dataset-manifest "${SMAVE_DAE_MULTIGRID_DIR}/training-dataset.manifest"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
    COMMAND $<TARGET_FILE:smave> train-dae-multigrid
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --dataset-store "${SMAVE_DAE_MULTIGRID_DIR}/data-store"
        --dataset-manifest "${SMAVE_DAE_MULTIGRID_DIR}/training-dataset.manifest"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/repeat-dae-multigrid.artifact"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
        "${SMAVE_DAE_MULTIGRID_DIR}/repeat-dae-multigrid.artifact"
    COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact.multigrid"
        "${SMAVE_DAE_MULTIGRID_DIR}/repeat-dae-multigrid.artifact.multigrid"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --end 0.3 --max-step 0.1
        --multigrid "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --end 0.3 --max-step 0.1
        --multigrid "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/repeat-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --end 0.1 --max-step 0.05
        --multigrid "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/fallback-report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_MULTIGRID_DIR}/model.ir"
        --end 0.1 --max-step 0.05
        --multigrid "${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact"
        --output "${SMAVE_DAE_MULTIGRID_DIR}/repeat-fallback-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DARTIFACT=${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact
        -DHIERARCHY=${SMAVE_DAE_MULTIGRID_DIR}/dae-multigrid.artifact.multigrid
        -DREPORT=${SMAVE_DAE_MULTIGRID_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_MULTIGRID_DIR}/repeat-report.txt
        -DFALLBACK_REPORT=${SMAVE_DAE_MULTIGRID_DIR}/fallback-report.txt
        -DREPEAT_FALLBACK_REPORT=${SMAVE_DAE_MULTIGRID_DIR}/repeat-fallback-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae_multigrid.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_DAE_RANK_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae-rank")
add_custom_target(reproduce-dae-rank
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_RANK_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_RANK_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/RuntimeRankLossDAE.mo"
        --top RuntimeRankLossDAE --output "${SMAVE_DAE_RANK_DIR}/model.ir"
    COMMAND ${CMAKE_COMMAND}
        -DSMAVE=$<TARGET_FILE:smave>
        -DIR=${SMAVE_DAE_RANK_DIR}/model.ir
        -DREPORT=${SMAVE_DAE_RANK_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_RANK_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae_rank.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_DAE_INITIAL_EVENTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae-initial-events")
add_custom_target(reproduce-dae-initial-events
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_INITIAL_EVENTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_INITIAL_EVENTS_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/InitialEventDAE.mo"
        --top InitialEventDAE
        --output "${SMAVE_DAE_INITIAL_EVENTS_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_INITIAL_EVENTS_DIR}/model.ir"
        --end 0.1 --max-step 0.1
        --output "${SMAVE_DAE_INITIAL_EVENTS_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_INITIAL_EVENTS_DIR}/model.ir"
        --end 0.1 --max-step 0.1
        --output "${SMAVE_DAE_INITIAL_EVENTS_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_DAE_INITIAL_EVENTS_DIR}/model.ir
        -DREPORT=${SMAVE_DAE_INITIAL_EVENTS_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_INITIAL_EVENTS_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae_initial_events.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_DAE_EVENTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae-events")
add_custom_target(reproduce-dae-events
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_EVENTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_EVENTS_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/RuntimeEventDAE.mo"
        --top RuntimeEventDAE
        --output "${SMAVE_DAE_EVENTS_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_EVENTS_DIR}/model.ir"
        --end 1.1 --max-step 0.1
        --output "${SMAVE_DAE_EVENTS_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_EVENTS_DIR}/model.ir"
        --end 1.1 --max-step 0.1
        --output "${SMAVE_DAE_EVENTS_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_DAE_EVENTS_DIR}/model.ir
        -DREPORT=${SMAVE_DAE_EVENTS_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_EVENTS_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae_events.cmake
    DEPENDS smave
    USES_TERMINAL
)

set(SMAVE_DAE_GRAZING_DIR "${CMAKE_CURRENT_BINARY_DIR}/dae-grazing")
add_custom_target(reproduce-dae-grazing
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SMAVE_DAE_GRAZING_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SMAVE_DAE_GRAZING_DIR}"
    COMMAND $<TARGET_FILE:smave> compile-dae
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/GrazingEventDAE.mo"
        --top GrazingEventDAE
        --output "${SMAVE_DAE_GRAZING_DIR}/model.ir"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_GRAZING_DIR}/model.ir"
        --end 1.5 --max-step 0.5
        --output "${SMAVE_DAE_GRAZING_DIR}/report.txt"
    COMMAND $<TARGET_FILE:smave> simulate-dae
        "${SMAVE_DAE_GRAZING_DIR}/model.ir"
        --end 1.5 --max-step 0.5
        --output "${SMAVE_DAE_GRAZING_DIR}/repeat-report.txt"
    COMMAND ${CMAKE_COMMAND}
        -DIR=${SMAVE_DAE_GRAZING_DIR}/model.ir
        -DREPORT=${SMAVE_DAE_GRAZING_DIR}/report.txt
        -DREPEAT_REPORT=${SMAVE_DAE_GRAZING_DIR}/repeat-report.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_dae_grazing.cmake
    DEPENDS smave
    USES_TERMINAL
)

include(CTest)
if(BUILD_TESTING)
add_executable(smave_tests tests/test_smave.cpp)
target_link_libraries(smave_tests PRIVATE smave_core)
add_dependencies(smave_tests smave_fmi_fixture smave_fmi2_fixture smave_fmi2_event_fixture smave_fmi2_async_fixture smave_fmi2_me_fixture)
target_compile_definitions(smave_tests PRIVATE
    SMAVE_TEST_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/unit-work"
    SMAVE_FMI_FIXTURE_PATH="$<TARGET_FILE:smave_fmi_fixture>"
    SMAVE_FMI2_FIXTURE_PATH="$<TARGET_FILE:smave_fmi2_fixture>"
    SMAVE_FMI2_EVENT_FIXTURE_PATH="$<TARGET_FILE:smave_fmi2_event_fixture>"
    SMAVE_FMI2_ASYNC_FIXTURE_PATH="$<TARGET_FILE:smave_fmi2_async_fixture>"
    SMAVE_FMI2_ME_FIXTURE_PATH="$<TARGET_FILE:smave_fmi2_me_fixture>")
add_test(NAME smave_unit COMMAND smave_tests)
add_test(NAME smave_competition_router_unit COMMAND smave_tests)
set_tests_properties(smave_competition_router_unit PROPERTIES
    ENVIRONMENT "SMAVE_TEST_FILTER=competition-router")
add_test(NAME smave_release_store_recovery COMMAND smave_tests)
set_tests_properties(smave_release_store_recovery PROPERTIES
    ENVIRONMENT "SMAVE_TEST_FILTER=release-store-recovery")
    add_executable(smave_sparse_suite_unit tests/sparse_suite_unit.cpp)
    target_link_libraries(smave_sparse_suite_unit PRIVATE smave_core)
    target_compile_options(smave_sparse_suite_unit PRIVATE -Wall -Wextra -Wpedantic)
    add_test(
        NAME smave_sparse_suite_unit
        COMMAND smave_sparse_suite_unit
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/suitesparse
            ${CMAKE_CURRENT_BINARY_DIR}/sparse-suite-unit
            ${SMAVE_SPARSE_SUITE_EXPECTED_CASES})
    add_test(
        NAME smave_cli_e2e
        COMMAND ${CMAKE_COMMAND}
            -DSMAVE=$<TARGET_FILE:smave>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/Coupled.mo
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e.cmake
    )
endif()
