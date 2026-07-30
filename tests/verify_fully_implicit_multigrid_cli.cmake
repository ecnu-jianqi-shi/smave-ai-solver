file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(
    COMMAND "${EVIDENCE}" "${WORK}/fixture"
    RESULT_VARIABLE evidence_result)
if(NOT evidence_result EQUAL 0)
    message(FATAL_ERROR "fully implicit multigrid fixture generation failed")
endif()

set(source "${WORK}/fixture/FullyImplicitLearnedMultigrid.mo")
set(scenarios "${WORK}/fixture/scenarios")
set(ir "${WORK}/model.implicit-dae")
set(artifact "${WORK}/implicit-dae-mg.artifact")
set(assessment "${WORK}/assessment.txt")
set(report "${WORK}/report.txt")

execute_process(
    COMMAND "${SMAVE}" compile-implicit-dae "${source}"
        --top FullyImplicitLearnedMultigrid --output "${ir}"
    RESULT_VARIABLE compile_result)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "fully implicit multigrid CLI compile failed")
endif()

execute_process(
    COMMAND "${SMAVE}" train-implicit-dae-multigrid "${ir}"
        --scenarios "${scenarios}" --output "${artifact}"
    RESULT_VARIABLE train_result
    OUTPUT_VARIABLE train_output)
if(NOT train_result EQUAL 0 OR
   NOT train_output MATCHES "family=fully-implicit-first-order-step")
    message(FATAL_ERROR "fully implicit multigrid CLI train failed: ${train_output}")
endif()

execute_process(
    COMMAND "${SMAVE}" assess-implicit-dae "${ir}"
        --multigrid "${artifact}" --output "${assessment}"
    RESULT_VARIABLE assess_result)
if(NOT assess_result EQUAL 0)
    message(FATAL_ERROR "fully implicit multigrid CLI assess failed")
endif()

execute_process(
    COMMAND "${SMAVE}" simulate-implicit-dae "${ir}"
        --end 0.2 --max-step 0.1 --multigrid "${artifact}" --output "${report}"
    RESULT_VARIABLE simulate_result
    OUTPUT_VARIABLE simulate_output)
if(NOT simulate_result EQUAL 0 OR
   NOT simulate_output MATCHES "learned_preconditioned_steps: 2" OR
   NOT simulate_output MATCHES "learned_rejections: 0")
    message(FATAL_ERROR "fully implicit multigrid CLI simulate failed: ${simulate_output}")
endif()

file(READ "${assessment}" assessment_text)
file(READ "${report}" report_text)
string(REGEX MATCH "PLAN_ID \"([^\"]+)\"" assessment_plan_match "${assessment_text}")
set(assessment_plan_id "${CMAKE_MATCH_1}")
string(REGEX MATCH "PLAN_ID \"([^\"]+)\"" report_plan_match "${report_text}")
set(report_plan_id "${CMAKE_MATCH_1}")
if(assessment_plan_id STREQUAL "" OR
   NOT assessment_plan_id STREQUAL report_plan_id)
    message(FATAL_ERROR "learned assessment/execution plan mismatch")
endif()
foreach(required
    "EXPERT \"fully-implicit-learned-multigrid-pcg-cpu-v1\""
    "verified-learned-multigrid-preconditioner"
    "pcg-learned-multigrid-linear-solver")
    if(NOT assessment_text MATCHES "${required}")
        message(FATAL_ERROR "learned assessment missing: ${required}")
    endif()
endforeach()
foreach(required
    "SUCCESS 1"
    "SOLVER_BACKEND \"fully-implicit-learned-multigrid-pcg-cpu-v1\""
    "LEARNED_PRECONDITIONED_STEPS 2"
    "LEARNED_REJECTIONS 0"
    "DENSE_STEP_FALLBACKS 0")
    if(NOT report_text MATCHES "${required}")
        message(FATAL_ERROR "learned execution report missing: ${required}")
    endif()
endforeach()

message(STATUS "Fully implicit learned multigrid CLI plan and execution passed")
