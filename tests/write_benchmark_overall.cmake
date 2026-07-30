if(NOT DEFINED BUILD_ROOT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "BUILD_ROOT and OUTPUT are required")
endif()

function(require_summary relative marker out)
    set(path "${BUILD_ROOT}/${relative}/summary.txt")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing benchmark summary: ${path}")
    endif()
    file(READ "${path}" text)
    if(NOT text MATCHES "^${marker}")
        message(FATAL_ERROR "invalid benchmark summary: ${path}")
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

require_summary("benchmark-suitesparse-routed" "SMAVE_SUITESPARSE_BENCHMARK 1" sparse)
require_summary("benchmark-petsc-ts" "SMAVE_PETSC_TS_BENCHMARK 1" petsc)
require_summary("benchmark-multiphysics-msl" "SMAVE_OPENMODELICA_MSL_BENCHMARK 1" msl)
require_summary("benchmark-cops" "SMAVE_COPS_BENCHMARK 1" cops)
set(COPS_JULIA_IPOPT_SOLVED 0)
set(COPS_JULIA_FAILED 0)
set(COPS_JULIA_TIMED_OUT 0)
set(COPS_KKT_AGREEMENTS 0)
set(COPS_KKT_PERFORMANCE_COMPARISONS 0)
set(COPS_FULL_NLP_ATTEMPTED 0)
set(COPS_FULL_NLP_BOTH_SOLVED 0)
set(COPS_FULL_NLP_AGREEMENTS 0)
set(COPS_FULL_NLP_PERFORMANCE_COMPARISONS 0)
set(COPS_FULL_NLP_TIMED_OUT 0)
set(COPS_FULL_NLP_KKT_SOLVES 0)
set(COPS_FULL_NLP_EXTERNAL_FALLBACK_SOLVES 0)
set(COPS_FULL_NLP_FALLBACK_ONLY_CASES 0)
set(COPS_FULL_NLP_RESOURCE_GATED_CASES 0)
set(cops_julia_path "${BUILD_ROOT}/benchmark-cops-julia/summary.txt")
if(EXISTS "${cops_julia_path}")
    file(READ "${cops_julia_path}" cops_julia)
    if(NOT cops_julia MATCHES "^SMAVE_COPS_JULIA_BASELINE 1")
        message(FATAL_ERROR "invalid COPS Julia summary: ${cops_julia_path}")
    endif()
    foreach(pair IN ITEMS
        "SOLVED;COPS_JULIA_IPOPT_SOLVED"
        "FAILED;COPS_JULIA_FAILED"
        "TIMED_OUT;COPS_JULIA_TIMED_OUT")
        list(GET pair 0 key)
        list(GET pair 1 output_name)
        string(REGEX MATCH "${key} ([0-9]+)" match "${cops_julia}")
        if(NOT match)
            message(FATAL_ERROR "missing ${key} in COPS Julia summary")
        endif()
        set(${output_name} "${CMAKE_MATCH_1}")
    endforeach()
endif()
set(cops_full_nlp_path "${BUILD_ROOT}/benchmark-cops-julia/madnlp-summary.txt")
if(EXISTS "${cops_full_nlp_path}")
    file(READ "${cops_full_nlp_path}" cops_full_nlp)
    if(NOT cops_full_nlp MATCHES "^SMAVE_COPS_MADNLP_COMPARISON 12")
        message(FATAL_ERROR "invalid COPS full-NLP comparison: ${cops_full_nlp_path}")
    endif()
    foreach(pair IN ITEMS
        "ATTEMPTED;COPS_FULL_NLP_ATTEMPTED"
        "BOTH_SOLVED;COPS_FULL_NLP_BOTH_SOLVED"
        "CORRECTNESS_AGREEMENTS;COPS_FULL_NLP_AGREEMENTS"
        "PERFORMANCE_COMPARISONS;COPS_FULL_NLP_PERFORMANCE_COMPARISONS"
        "TIMED_OUT;COPS_FULL_NLP_TIMED_OUT"
        "SMAVE_KKT_SOLVES;COPS_FULL_NLP_KKT_SOLVES"
        "EXTERNAL_FALLBACK_SOLVES;COPS_FULL_NLP_EXTERNAL_FALLBACK_SOLVES"
        "FALLBACK_ONLY_CASES;COPS_FULL_NLP_FALLBACK_ONLY_CASES"
        "RESOURCE_GATED_CASES;COPS_FULL_NLP_RESOURCE_GATED_CASES")
        list(GET pair 0 key)
        list(GET pair 1 output_name)
        string(REGEX MATCH "${key} ([0-9]+)" match "${cops_full_nlp}")
        if(NOT match)
            message(FATAL_ERROR "missing ${key} in COPS full-NLP comparison")
        endif()
        set(${output_name} "${CMAKE_MATCH_1}")
    endforeach()
endif()
set(cops_kkt_path "${BUILD_ROOT}/benchmark-cops-julia/comparison.txt")
if(EXISTS "${cops_kkt_path}")
    file(READ "${cops_kkt_path}" cops_kkt)
    if(NOT cops_kkt MATCHES "^SMAVE_COPS_KKT_COMPARISON 1")
        message(FATAL_ERROR "invalid COPS KKT comparison: ${cops_kkt_path}")
    endif()
    foreach(pair IN ITEMS
        "AGREEMENTS;COPS_KKT_AGREEMENTS"
        "PERFORMANCE_COMPARISONS;COPS_KKT_PERFORMANCE_COMPARISONS")
        list(GET pair 0 key)
        list(GET pair 1 output_name)
        string(REGEX MATCH "${key} ([0-9]+)" match "${cops_kkt}")
        if(NOT match)
            message(FATAL_ERROR "missing ${key} in COPS KKT comparison")
        endif()
        set(${output_name} "${CMAKE_MATCH_1}")
    endforeach()
endif()
set(advection_path "${BUILD_ROOT}/benchmark-pdebench/advection-summary.txt")
if(EXISTS "${advection_path}")
    file(READ "${advection_path}" advection)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${advection}")
    set(PDEBENCH_ADVECTION_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${advection}")
    set(PDEBENCH_ADVECTION_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${advection}")
    set(PDEBENCH_ADVECTION_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_ADVECTION_SOLVES 0)
    set(PDEBENCH_ADVECTION_AGREEMENT 0)
    set(PDEBENCH_ADVECTION_SPEEDUP 0)
endif()
set(darcy_path "${BUILD_ROOT}/benchmark-pdebench/darcy-summary.txt")
if(EXISTS "${darcy_path}")
    file(READ "${darcy_path}" darcy)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${darcy}")
    set(PDEBENCH_DARCY_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${darcy}")
    set(PDEBENCH_DARCY_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${darcy}")
    set(PDEBENCH_DARCY_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_DARCY_SOLVES 0)
    set(PDEBENCH_DARCY_AGREEMENT 0)
    set(PDEBENCH_DARCY_SPEEDUP 0)
endif()
set(burgers_path "${BUILD_ROOT}/benchmark-pdebench/burgers-summary.txt")
if(EXISTS "${burgers_path}")
    file(READ "${burgers_path}" burgers)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${burgers}")
    set(PDEBENCH_BURGERS_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${burgers}")
    set(PDEBENCH_BURGERS_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${burgers}")
    set(PDEBENCH_BURGERS_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_BURGERS_SOLVES 0)
    set(PDEBENCH_BURGERS_AGREEMENT 0)
    set(PDEBENCH_BURGERS_SPEEDUP 0)
endif()
set(diffusion_sorption_path
    "${BUILD_ROOT}/benchmark-pdebench/diffusion-sorption-summary.txt")
if(EXISTS "${diffusion_sorption_path}")
    file(READ "${diffusion_sorption_path}" diffusion_sorption)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${diffusion_sorption}")
    set(PDEBENCH_DIFFUSION_SORPTION_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${diffusion_sorption}")
    set(PDEBENCH_DIFFUSION_SORPTION_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${diffusion_sorption}")
    set(PDEBENCH_DIFFUSION_SORPTION_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_DIFFUSION_SORPTION_SOLVES 0)
    set(PDEBENCH_DIFFUSION_SORPTION_AGREEMENT 0)
    set(PDEBENCH_DIFFUSION_SORPTION_SPEEDUP 0)
endif()
set(shallow_water_path "${BUILD_ROOT}/benchmark-pdebench/shallow-water-summary.txt")
if(EXISTS "${shallow_water_path}")
    file(READ "${shallow_water_path}" shallow_water)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${shallow_water}")
    set(PDEBENCH_SHALLOW_WATER_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${shallow_water}")
    set(PDEBENCH_SHALLOW_WATER_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${shallow_water}")
    set(PDEBENCH_SHALLOW_WATER_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_SHALLOW_WATER_SOLVES 0)
    set(PDEBENCH_SHALLOW_WATER_AGREEMENT 0)
    set(PDEBENCH_SHALLOW_WATER_SPEEDUP 0)
endif()
set(ns_incompressible_path
    "${BUILD_ROOT}/benchmark-pdebench/ns-incompressible-summary.txt")
if(EXISTS "${ns_incompressible_path}")
    file(READ "${ns_incompressible_path}" ns_incompressible)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${ns_incompressible}")
    set(PDEBENCH_NS_INCOMPRESSIBLE_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${ns_incompressible}")
    set(PDEBENCH_NS_INCOMPRESSIBLE_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${ns_incompressible}")
    set(PDEBENCH_NS_INCOMPRESSIBLE_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_NS_INCOMPRESSIBLE_SOLVES 0)
    set(PDEBENCH_NS_INCOMPRESSIBLE_AGREEMENT 0)
    set(PDEBENCH_NS_INCOMPRESSIBLE_SPEEDUP 0)
endif()
set(cfd_1d_path "${BUILD_ROOT}/benchmark-pdebench/cfd-1d-summary.txt")
if(EXISTS "${cfd_1d_path}")
    file(READ "${cfd_1d_path}" cfd_1d)
    string(REGEX MATCH "SOLVES ([0-9]+)" match "${cfd_1d}")
    set(PDEBENCH_CFD_1D_SOLVES "${CMAKE_MATCH_1}")
    string(REGEX MATCH "CROSS_SOLVER_AGREEMENT ([0-9]+)" match "${cfd_1d}")
    set(PDEBENCH_CFD_1D_AGREEMENT "${CMAKE_MATCH_1}")
    string(REGEX MATCH "SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)" match "${cfd_1d}")
    set(PDEBENCH_CFD_1D_SPEEDUP "${CMAKE_MATCH_1}")
else()
    set(PDEBENCH_CFD_1D_SOLVES 0)
    set(PDEBENCH_CFD_1D_AGREEMENT 0)
    set(PDEBENCH_CFD_1D_SPEEDUP 0)
endif()
math(EXPR PDEBENCH_SMAVE_COMPARISONS
    "${PDEBENCH_ADVECTION_AGREEMENT}+${PDEBENCH_DARCY_AGREEMENT}+${PDEBENCH_BURGERS_AGREEMENT}+${PDEBENCH_DIFFUSION_SORPTION_AGREEMENT}+${PDEBENCH_SHALLOW_WATER_AGREEMENT}+${PDEBENCH_NS_INCOMPRESSIBLE_AGREEMENT}+${PDEBENCH_CFD_1D_AGREEMENT}")
set(PDEBENCH_100X_ACHIEVED 0)
foreach(prefix IN ITEMS
    ADVECTION DARCY BURGERS DIFFUSION_SORPTION SHALLOW_WATER
    NS_INCOMPRESSIBLE CFD_1D)
    if(PDEBENCH_${prefix}_AGREEMENT EQUAL 1 AND
       PDEBENCH_${prefix}_SPEEDUP GREATER_EQUAL 100.0)
        math(EXPR PDEBENCH_100X_ACHIEVED "${PDEBENCH_100X_ACHIEVED}+1")
    endif()
endforeach()
if(PDEBENCH_100X_ACHIEVED EQUAL 7)
    set(PDEBENCH_ALL_100X 1)
else()
    set(PDEBENCH_ALL_100X 0)
endif()
set(petsc_comparison_path "${BUILD_ROOT}/benchmark-petsc-ts/comparison.txt")
if(EXISTS "${petsc_comparison_path}")
    file(READ "${petsc_comparison_path}" petsc_comparison)
    string(REGEX MATCH "AGREEMENTS ([0-9]+)" match "${petsc_comparison}")
    set(PETSC_TS_SMAVE_TRAJECTORY_COMPARISONS "${CMAKE_MATCH_1}")
    string(REGEX MATCHALL "SMAVE_VS_PETSC_SPEEDUP [0-9.eE+-]+"
        petsc_speedup_fields "${petsc_comparison}")
    list(LENGTH petsc_speedup_fields PETSC_TS_PERFORMANCE_COMPARISONS)
else()
    set(PETSC_TS_SMAVE_TRAJECTORY_COMPARISONS 0)
    set(PETSC_TS_PERFORMANCE_COMPARISONS 0)
endif()
set(petsc_classification_path "${BUILD_ROOT}/benchmark-petsc-ts/classification.txt")
if(EXISTS "${petsc_classification_path}")
    file(READ "${petsc_classification_path}" petsc_classification)
    foreach(field IN ITEMS
        "EQUATION_CASES;PETSC_TS_EQUATION_CASES"
        "FRAMEWORK_SELF_TESTS;PETSC_TS_FRAMEWORK_SELF_TESTS"
        "EQUATION_COMPARISONS_COMPLETE;PETSC_TS_EQUATION_COMPARISONS_COMPLETE"
        "EQUATION_COMPARISONS_PENDING;PETSC_TS_EQUATION_COMPARISONS_PENDING")
        list(GET field 0 source_key)
        list(GET field 1 output_key)
        string(REGEX MATCH "${source_key} ([0-9]+)" match "${petsc_classification}")
        if(NOT match)
            message(FATAL_ERROR "missing ${source_key} in PETSc classification")
        endif()
        set(${output_key} "${CMAKE_MATCH_1}")
    endforeach()
else()
    set(PETSC_TS_EQUATION_CASES 0)
    set(PETSC_TS_FRAMEWORK_SELF_TESTS 0)
    set(PETSC_TS_EQUATION_COMPARISONS_COMPLETE 0)
    set(PETSC_TS_EQUATION_COMPARISONS_PENDING 0)
endif()
set(OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS 0)
set(OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS 0)
set(OPENMODELICA_MSL_LINEAR_SOLVER_CASES 0)
set(OPENMODELICA_MSL_NO_APPLICABLE_LINEAR_SYSTEM_CASES 0)
set(OPENMODELICA_MSL_LINEAR_CALLS 0)
set(OPENMODELICA_MSL_FALLBACKS 0)
set(msl_comparison_path
    "${BUILD_ROOT}/benchmark-multiphysics-msl/comparison.txt")
if(EXISTS "${msl_comparison_path}")
    file(READ "${msl_comparison_path}" msl_comparison)
    if(NOT msl_comparison MATCHES "^SMAVE_OPENMODELICA_MSL_COMPARISON 1")
        message(FATAL_ERROR "invalid MSL comparison: ${msl_comparison_path}")
    endif()
    file(STRINGS "${msl_comparison_path}" msl_case_lines REGEX "^CASE ")
    foreach(case_line IN LISTS msl_case_lines)
        string(REGEX MATCH "AGREEMENT ([0-9]+)" match "${case_line}")
        if(CMAKE_MATCH_1 EQUAL 1)
            math(EXPR OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS
                "${OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS}+1")
        endif()
        string(REGEX MATCH "CALLS ([0-9]+)" match "${case_line}")
        set(msl_calls "${CMAKE_MATCH_1}")
        math(EXPR OPENMODELICA_MSL_LINEAR_CALLS
            "${OPENMODELICA_MSL_LINEAR_CALLS}+${msl_calls}")
        if(msl_calls GREATER 0)
            math(EXPR OPENMODELICA_MSL_LINEAR_SOLVER_CASES
                "${OPENMODELICA_MSL_LINEAR_SOLVER_CASES}+1")
        else()
            math(EXPR OPENMODELICA_MSL_NO_APPLICABLE_LINEAR_SYSTEM_CASES
                "${OPENMODELICA_MSL_NO_APPLICABLE_LINEAR_SYSTEM_CASES}+1")
        endif()
        string(REGEX MATCH "FALLBACKS ([0-9]+)" match "${case_line}")
        math(EXPR OPENMODELICA_MSL_FALLBACKS
            "${OPENMODELICA_MSL_FALLBACKS}+${CMAKE_MATCH_1}")
        if(case_line MATCHES "SMAVE_VS_OPENMODELICA_SPEEDUP [0-9.eE+-]+")
            math(EXPR OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS
                "${OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS}+1")
        endif()
    endforeach()
endif()
foreach(pair IN ITEMS
    "sparse;SMAVE_CONVERGED;SUITESPARSE_SMAVE_CONVERGED"
    "sparse;CORRECTNESS_AGREEMENTS;SUITESPARSE_CROSS_SOLVER_AGREEMENTS"
    "sparse;PERFORMANCE_COMPARISONS;SUITESPARSE_PERFORMANCE_COMPARISONS"
    "sparse;NO_COMMON_SUCCESS;SUITESPARSE_NO_COMMON_SUCCESS"
    "sparse;INVALID_ASSETS;SUITESPARSE_INVALID_ASSETS"
    "petsc;PASSED;PETSC_TS_BASELINE_PASSED"
    "msl;PASSED;OPENMODELICA_MSL_BASELINE_PASSED"
    "cops;EXECUTED;COPS_IPOPT_SOLVED"
    "cops;BLOCKED_LICENSE;COPS_BLOCKED_LICENSE")
    list(GET pair 0 source_name)
    list(GET pair 1 key)
    list(GET pair 2 output_name)
    string(REGEX MATCH "${key} ([0-9]+)" match "${${source_name}}")
    if(NOT match)
        message(FATAL_ERROR "missing ${key} in ${source_name} summary")
    endif()
    set(${output_name} "${CMAKE_MATCH_1}")
endforeach()
math(EXPR SUITESPARSE_SMAVE_FAILED
    "39-${SUITESPARSE_SMAVE_CONVERGED}-${SUITESPARSE_INVALID_ASSETS}")
set(pdebench_verified 0)
file(STRINGS "${SOURCE_ROOT}/benchmark/pdebench/files.tsv" pdebench_rows)
foreach(row IN LISTS pdebench_rows)
    if(row MATCHES "^#" OR row STREQUAL "")
        continue()
    endif()
    string(REPLACE "\t" ";" fields "${row}")
    list(GET fields 0 relative_path)
    list(GET fields 2 expected_size)
    list(GET fields 3 expected_md5)
    set(path "${SOURCE_ROOT}/benchmark/pdebench/${relative_path}")
    if(EXISTS "${path}")
        file(SIZE "${path}" actual_size)
        if(actual_size EQUAL expected_size)
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E md5sum "${path}"
                OUTPUT_VARIABLE md5_output
                RESULT_VARIABLE md5_status
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(md5_status EQUAL 0 AND md5_output MATCHES "^${expected_md5} ")
                math(EXPR pdebench_verified "${pdebench_verified}+1")
            endif()
        endif()
    endif()
endforeach()
set(OVERALL_COMPLETE 0)
set(OVERALL_REASON
    "One or more benchmark suites, authoritative assets, or applicable comparisons remain incomplete")
if(SUITESPARSE_INVALID_ASSETS EQUAL 0 AND
   sparse MATCHES "SYSTEM_CASES 39" AND sparse MATCHES "CHECKPOINTS 39" AND
   sparse MATCHES "COMPLETE 1" AND
   PETSC_TS_EQUATION_COMPARISONS_COMPLETE EQUAL PETSC_TS_EQUATION_CASES AND
   PETSC_TS_EQUATION_COMPARISONS_PENDING EQUAL 0 AND
   OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS EQUAL 7 AND
   OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS EQUAL 7 AND
   COPS_JULIA_IPOPT_SOLVED EQUAL 68 AND COPS_KKT_AGREEMENTS EQUAL 68 AND
   COPS_KKT_PERFORMANCE_COMPARISONS EQUAL 68 AND
   COPS_FULL_NLP_ATTEMPTED EQUAL 68 AND
   COPS_FULL_NLP_PERFORMANCE_COMPARISONS GREATER 0 AND
   pdebench_verified EQUAL 7 AND PDEBENCH_SMAVE_COMPARISONS EQUAL 7)
    set(OVERALL_COMPLETE 1)
    set(OVERALL_REASON
        "All benchmark cases and authoritative assets were executed; applicable same-input performance comparisons are reported, while failures, timeouts, fallback-only cases, and no-common-success cases remain explicitly classified")
endif()
file(MAKE_DIRECTORY "${OUTPUT}")
string(TIMESTAMP report_date "%Y-%m-%d")
file(WRITE "${OUTPUT}/summary.txt"
    "SMAVE_BENCHMARK_OVERALL 1\n"
    "DATE ${report_date}\n"
    "SUITESPARSE_SYSTEMS 39\n"
    "SUITESPARSE_ATTEMPTED 39\n"
    "SUITESPARSE_SMAVE_CONVERGED ${SUITESPARSE_SMAVE_CONVERGED}\n"
    "SUITESPARSE_CROSS_SOLVER_AGREEMENTS ${SUITESPARSE_CROSS_SOLVER_AGREEMENTS}\n"
    "SUITESPARSE_PERFORMANCE_COMPARISONS ${SUITESPARSE_PERFORMANCE_COMPARISONS}\n"
    "SUITESPARSE_NO_COMMON_SUCCESS ${SUITESPARSE_NO_COMMON_SUCCESS}\n"
    "SUITESPARSE_SMAVE_FAILED ${SUITESPARSE_SMAVE_FAILED}\n"
    "SUITESPARSE_INVALID_ASSETS ${SUITESPARSE_INVALID_ASSETS}\n"
    "PETSC_TS_BASELINE_PASSED ${PETSC_TS_BASELINE_PASSED}\n"
    "PETSC_TS_SMAVE_TRAJECTORY_COMPARISONS ${PETSC_TS_SMAVE_TRAJECTORY_COMPARISONS}\n"
    "PETSC_TS_SMAVE_PERFORMANCE_COMPARISONS ${PETSC_TS_PERFORMANCE_COMPARISONS}\n"
    "PETSC_TS_EQUATION_CASES ${PETSC_TS_EQUATION_CASES}\n"
    "PETSC_TS_FRAMEWORK_SELF_TESTS ${PETSC_TS_FRAMEWORK_SELF_TESTS}\n"
    "PETSC_TS_EQUATION_COMPARISONS_COMPLETE ${PETSC_TS_EQUATION_COMPARISONS_COMPLETE}\n"
    "PETSC_TS_EQUATION_COMPARISONS_PENDING ${PETSC_TS_EQUATION_COMPARISONS_PENDING}\n"
    "OPENMODELICA_MSL_BASELINE_PASSED ${OPENMODELICA_MSL_BASELINE_PASSED}\n"
    "OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS ${OPENMODELICA_MSL_SMAVE_TRAJECTORY_COMPARISONS}\n"
    "OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS ${OPENMODELICA_MSL_END_TO_END_PERFORMANCE_COMPARISONS}\n"
    "OPENMODELICA_MSL_LINEAR_SOLVER_CASES ${OPENMODELICA_MSL_LINEAR_SOLVER_CASES}\n"
    "OPENMODELICA_MSL_NO_APPLICABLE_LINEAR_SYSTEM_CASES ${OPENMODELICA_MSL_NO_APPLICABLE_LINEAR_SYSTEM_CASES}\n"
    "OPENMODELICA_MSL_LINEAR_CALLS ${OPENMODELICA_MSL_LINEAR_CALLS}\n"
    "OPENMODELICA_MSL_FALLBACKS ${OPENMODELICA_MSL_FALLBACKS}\n"
    "COPS_INSTANCES 68\n"
    "COPS_IPOPT_SOLVED ${COPS_IPOPT_SOLVED}\n"
    "COPS_BLOCKED_LICENSE ${COPS_BLOCKED_LICENSE}\n"
    "COPS_JULIA_IPOPT_SOLVED ${COPS_JULIA_IPOPT_SOLVED}\n"
    "COPS_JULIA_FAILED ${COPS_JULIA_FAILED}\n"
    "COPS_JULIA_TIMED_OUT ${COPS_JULIA_TIMED_OUT}\n"
    "COPS_KKT_SMAVE_AGREEMENTS ${COPS_KKT_AGREEMENTS}\n"
    "COPS_KKT_PERFORMANCE_COMPARISONS ${COPS_KKT_PERFORMANCE_COMPARISONS}\n"
    "COPS_FULL_NLP_ATTEMPTED ${COPS_FULL_NLP_ATTEMPTED}\n"
    "COPS_FULL_NLP_BOTH_SOLVED ${COPS_FULL_NLP_BOTH_SOLVED}\n"
    "COPS_FULL_NLP_SMAVE_COMPARISONS ${COPS_FULL_NLP_AGREEMENTS}\n"
    "COPS_FULL_NLP_PERFORMANCE_COMPARISONS ${COPS_FULL_NLP_PERFORMANCE_COMPARISONS}\n"
    "COPS_FULL_NLP_TIMED_OUT ${COPS_FULL_NLP_TIMED_OUT}\n"
    "COPS_FULL_NLP_KKT_SOLVES ${COPS_FULL_NLP_KKT_SOLVES}\n"
    "COPS_FULL_NLP_EXTERNAL_FALLBACK_SOLVES ${COPS_FULL_NLP_EXTERNAL_FALLBACK_SOLVES}\n"
    "COPS_FULL_NLP_FALLBACK_ONLY_CASES ${COPS_FULL_NLP_FALLBACK_ONLY_CASES}\n"
    "COPS_FULL_NLP_RESOURCE_GATED_CASES ${COPS_FULL_NLP_RESOURCE_GATED_CASES}\n"
    "PDEBENCH_EXPECTED_FILES 7\n"
    "PDEBENCH_FULLY_VERIFIED_FILES ${pdebench_verified}\n"
    "PDEBENCH_ADVECTION_SAME_INPUT_SOLVES ${PDEBENCH_ADVECTION_SOLVES}\n"
    "PDEBENCH_ADVECTION_CROSS_SOLVER_AGREEMENT ${PDEBENCH_ADVECTION_AGREEMENT}\n"
    "PDEBENCH_ADVECTION_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_ADVECTION_SPEEDUP}\n"
    "PDEBENCH_DARCY_SAME_INPUT_SOLVES ${PDEBENCH_DARCY_SOLVES}\n"
    "PDEBENCH_DARCY_CROSS_SOLVER_AGREEMENT ${PDEBENCH_DARCY_AGREEMENT}\n"
    "PDEBENCH_DARCY_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_DARCY_SPEEDUP}\n"
    "PDEBENCH_BURGERS_SAME_INPUT_SOLVES ${PDEBENCH_BURGERS_SOLVES}\n"
    "PDEBENCH_BURGERS_CROSS_SOLVER_AGREEMENT ${PDEBENCH_BURGERS_AGREEMENT}\n"
    "PDEBENCH_BURGERS_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_BURGERS_SPEEDUP}\n"
    "PDEBENCH_DIFFUSION_SORPTION_SAME_INPUT_SOLVES ${PDEBENCH_DIFFUSION_SORPTION_SOLVES}\n"
    "PDEBENCH_DIFFUSION_SORPTION_CROSS_SOLVER_AGREEMENT ${PDEBENCH_DIFFUSION_SORPTION_AGREEMENT}\n"
    "PDEBENCH_DIFFUSION_SORPTION_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_DIFFUSION_SORPTION_SPEEDUP}\n"
    "PDEBENCH_SHALLOW_WATER_SAME_INPUT_SOLVES ${PDEBENCH_SHALLOW_WATER_SOLVES}\n"
    "PDEBENCH_SHALLOW_WATER_CROSS_SOLVER_AGREEMENT ${PDEBENCH_SHALLOW_WATER_AGREEMENT}\n"
    "PDEBENCH_SHALLOW_WATER_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_SHALLOW_WATER_SPEEDUP}\n"
    "PDEBENCH_NS_INCOMPRESSIBLE_SAME_INPUT_SOLVES ${PDEBENCH_NS_INCOMPRESSIBLE_SOLVES}\n"
    "PDEBENCH_NS_INCOMPRESSIBLE_CROSS_SOLVER_AGREEMENT ${PDEBENCH_NS_INCOMPRESSIBLE_AGREEMENT}\n"
    "PDEBENCH_NS_INCOMPRESSIBLE_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_NS_INCOMPRESSIBLE_SPEEDUP}\n"
    "PDEBENCH_CFD_1D_SAME_INPUT_SOLVES ${PDEBENCH_CFD_1D_SOLVES}\n"
    "PDEBENCH_CFD_1D_CROSS_SOLVER_AGREEMENT ${PDEBENCH_CFD_1D_AGREEMENT}\n"
    "PDEBENCH_CFD_1D_SMAVE_VS_CLASSICAL_SPEEDUP ${PDEBENCH_CFD_1D_SPEEDUP}\n"
    "PDEBENCH_SMAVE_COMPARISONS ${PDEBENCH_SMAVE_COMPARISONS}\n"
    "PDEBENCH_100X_TARGET_CASES 7\n"
    "PDEBENCH_100X_ACHIEVED_CASES ${PDEBENCH_100X_ACHIEVED}\n"
    "PDEBENCH_ALL_100X ${PDEBENCH_ALL_100X}\n"
    "OVERALL_SMAVE_VS_TRADITIONAL_COMPLETE ${OVERALL_COMPLETE}\n"
    "REASON \"${OVERALL_REASON}\"\n"
    "END\n")
message(STATUS "Wrote benchmark overall report to ${OUTPUT}/summary.txt")
