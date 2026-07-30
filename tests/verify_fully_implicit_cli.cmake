file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(
    COMMAND "${EVIDENCE}" "${WORK}/source"
    RESULT_VARIABLE evidence_result
)
if(NOT evidence_result EQUAL 0)
    message(FATAL_ERROR "fully implicit source fixture generation failed")
endif()

set(source "${WORK}/source/LargeFullyImplicitDae33.mo")
set(ir "${WORK}/model.implicit-dae")
set(assessment "${WORK}/assessment.txt")
set(report "${WORK}/report.txt")

execute_process(
    COMMAND "${SMAVE}" compile-implicit-dae "${source}"
        --top LargeFullyImplicitDae33 --output "${ir}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
)
if(NOT compile_result EQUAL 0 OR
   NOT compile_output MATCHES "compiled fully implicit DAE")
    message(FATAL_ERROR "compile-implicit-dae failed: ${compile_output}")
endif()

execute_process(
    COMMAND "${SMAVE}" assess-implicit-dae "${ir}" --output "${assessment}"
    RESULT_VARIABLE assess_result
    OUTPUT_VARIABLE assess_output
)
if(NOT assess_result EQUAL 0 OR
   NOT assess_output MATCHES "dae-fully-implicit-first-order-smooth")
    message(FATAL_ERROR "assess-implicit-dae failed: ${assess_output}")
endif()

file(READ "${assessment}" assessment_text)
string(REGEX MATCH "PLAN_ID \"([^\"]+)\"" assessment_plan_match "${assessment_text}")
set(assessment_plan_id "${CMAKE_MATCH_1}")
foreach(required
    "FAMILY \"dae-fully-implicit-first-order-smooth\""
    "PLAN_STEPS 1"
    "EXPERT \"fully-implicit-csr-newton-krylov-cpu-v1\""
    "BACKEND_CHAIN 0 0 \"fixed-state-derivative-algebraic-initializer\""
    "BACKEND_CHAIN 0 7 \"original-dae-residual-gate\""
    "TERMINAL_FALLBACK \"fully-implicit-dense-newton-cpu-v1\"")
    if(NOT assessment_text MATCHES "${required}")
        message(FATAL_ERROR "fully implicit assessment missing: ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${SMAVE}" simulate-implicit-dae "${ir}"
        --end 0.02 --max-step 0.01 --output "${report}"
    RESULT_VARIABLE simulate_result
    OUTPUT_VARIABLE simulate_output
)
if(NOT simulate_result EQUAL 0 OR
   NOT simulate_output MATCHES "success: 1" OR
   NOT simulate_output MATCHES "fully-implicit-csr-newton-krylov-cpu-v1")
    message(FATAL_ERROR "simulate-implicit-dae failed: ${simulate_output}")
endif()

file(READ "${report}" report_text)
string(REGEX MATCH "PLAN_ID \"([^\"]+)\"" report_plan_match "${report_text}")
set(report_plan_id "${CMAKE_MATCH_1}")
if(assessment_plan_id STREQUAL "" OR
   NOT assessment_plan_id STREQUAL report_plan_id)
    message(FATAL_ERROR
        "assessment/execution plan mismatch: ${assessment_plan_id} != ${report_plan_id}")
endif()
foreach(required
    "SUCCESS 1"
    "SOLVER_BACKEND \"fully-implicit-csr-newton-krylov-cpu-v1\""
    "TERMINAL_FALLBACK \"fully-implicit-dense-newton-cpu-v1\""
    "BACKEND_CHAIN 0 \"fixed-state-derivative-algebraic-initializer\""
    "BACKEND_CHAIN 7 \"original-dae-residual-gate\""
    "DENSE_INITIALIZATION_FALLBACKS 0"
    "DENSE_STEP_FALLBACKS 0")
    if(NOT report_text MATCHES "${required}")
        message(FATAL_ERROR "fully implicit execution report missing: ${required}")
    endif()
endforeach()

message(STATUS "Fully implicit CLI assessment, plan, execution, and fallback passed")
