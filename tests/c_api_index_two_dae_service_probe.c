#include "smave/c_api.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum model_kind {
    MODEL_CANONICAL = 0,
    MODEL_HIDDEN_RANK_DEFICIENT = 1
};

static int index_two_residual(
    size_t dimension, double time, const double* state, const double* derivative,
    double* residual, void* user_data) {
    (void)time;
    if (dimension != 3) return 1;
    const enum model_kind kind = (enum model_kind)(intptr_t)user_data;
    residual[0] = derivative[0] - state[1] -
        (kind == MODEL_CANONICAL ? state[2] : 0.0);
    residual[1] = derivative[1] + state[0];
    residual[2] = state[0];
    return 0;
}

static int index_two_jacobian(
    size_t dimension, double time, const double* state, const double* derivative,
    double derivative_scale, double* jacobian, void* user_data) {
    (void)time;
    (void)state;
    (void)derivative;
    if (dimension != 3) return 1;
    const enum model_kind kind = (enum model_kind)(intptr_t)user_data;
    jacobian[0] = derivative_scale;
    jacobian[1] = -1.0;
    jacobian[2] = kind == MODEL_CANONICAL ? -1.0 : 0.0;
    jacobian[3] = 1.0;
    jacobian[4] = derivative_scale;
    jacobian[5] = 0.0;
    jacobian[6] = 1.0;
    jacobian[7] = 0.0;
    jacobian[8] = 0.0;
    return 0;
}

static void destroy_solve_objects(
    smave_result* result, smave_solver* solver, smave_problem* problem) {
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
}

static int solve_positive(smave_library* library) {
    const uint8_t differential_mask[] = {1, 1, 0};
    const double initial_state[] = {0.0, 1.0, -1.0};
    const double initial_derivative[] = {0.0, 0.0, 0.0};
    const smave_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 3, differential_mask,
        initial_state, initial_derivative, 0.0, 0.3, 0.1,
        index_two_residual, index_two_jacobian, (void*)(intptr_t)MODEL_CANONICAL};
    const smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 100};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_dae_result_info dae_info = {0};
    dae_info.struct_size = sizeof(dae_info);
    dae_info.abi_version = SMAVE_ABI_VERSION;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[3] = {0.0, 0.0, 0.0};
    size_t required = 0;
    const int success =
        smave_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        smave_result_get_dae_info(result, &dae_info) == SMAVE_STATUS_OK &&
        smave_result_copy_solution(result, solution, 3, &required) == SMAVE_STATUS_OK &&
        required == 3 && service_id != NULL &&
        strcmp(service_id, "smave.verified-fully-implicit-dae-solve.v1") == 0 &&
        plan_id != NULL && strstr(plan_id, "hessenberg-index2-hidden-rank-v1") != NULL &&
        equation_family != NULL &&
        strcmp(equation_family, "dae-fully-implicit-hessenberg-index2") == 0 &&
        dae_info.differentiation_index == 2 && dae_info.hidden_rank_checks == 4 &&
        dae_info.minimum_hidden_rank_margin > 0.99 &&
        dae_info.maximum_hidden_residual_inf <= 1.0e-12 &&
        dae_info.accepted_steps == 3 && dae_info.rejected_steps == 0 &&
        fabs(dae_info.final_time - 0.3) <= 1.0e-12 &&
        fabs(solution[0]) <= 1.0e-12 && fabs(solution[1] - 1.0) <= 1.0e-12 &&
        fabs(solution[2] + 1.0) <= 1.0e-12;
    if (success) {
        printf("SMAVE_C_API_INDEX_TWO_DAE_SERVICE 1\n"
               "service_id=\"%s\"\nplan_id=\"%s\"\n"
               "equation_family=\"%s\"\ndifferentiation_index=%zu\n"
               "hidden_rank_checks=%zu\nminimum_hidden_rank_margin=%.17g\n"
               "maximum_hidden_residual_inf=%.17g\nsolution=%.17g,%.17g,%.17g\n",
               service_id, plan_id, equation_family, dae_info.differentiation_index,
               dae_info.hidden_rank_checks, dae_info.minimum_hidden_rank_margin,
               dae_info.maximum_hidden_residual_inf,
               solution[0], solution[1], solution[2]);
    }
    destroy_solve_objects(result, solver, problem);
    return success;
}

static int solve_rejected_case(
    smave_library* library,
    enum model_kind kind,
    const double initial_state[3],
    const double initial_derivative[3],
    smave_diagnostic_code expected_diagnostic,
    size_t expected_index) {
    const uint8_t differential_mask[] = {1, 1, 0};
    const smave_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 3, differential_mask,
        initial_state, initial_derivative, 0.0, 0.1, 0.1,
        index_two_residual, index_two_jacobian, (void*)(intptr_t)kind};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    smave_dae_result_info dae_info = {0};
    dae_info.struct_size = sizeof(dae_info);
    dae_info.abi_version = SMAVE_ABI_VERSION;
    const int rejected =
        smave_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == expected_diagnostic &&
        smave_result_get_dae_info(result, &dae_info) == SMAVE_STATUS_OK &&
        dae_info.differentiation_index == expected_index;
    destroy_solve_objects(result, solver, problem);
    return rejected;
}

static int result_abi_checks(smave_library* library) {
    const uint8_t differential_mask[] = {1, 1, 0};
    const double initial_state[] = {0.0, 1.0, -1.0};
    const double initial_derivative[] = {0.0, 0.0, 0.0};
    const smave_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 3, differential_mask,
        initial_state, initial_derivative, 0.0, 0.1, 0.1,
        index_two_residual, index_two_jacobian, (void*)(intptr_t)MODEL_CANONICAL};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    struct extended_result {
        smave_dae_result_info info;
        uint64_t tail;
    } extended = {{0}, UINT64_C(0x7395a4e621bc08df)};
    struct event_prefix_result {
        struct {
            uint32_t struct_size;
            uint32_t abi_version;
            double final_time;
            double maximum_residual_inf;
            size_t accepted_steps;
            size_t rejected_steps;
            size_t event_count;
            double last_event_time;
        } info;
        uint64_t tail;
    } event_prefix = {{
        offsetof(smave_dae_result_info, differentiation_index),
        SMAVE_ABI_VERSION, 0.0, 0.0, 0, 0, 99, -1.0},
        UINT64_C(0x3ec4527ad91106bf)};
    extended.info.struct_size = sizeof(extended);
    extended.info.abi_version = SMAVE_ABI_VERSION;
    smave_dae_result_info mismatch = {0};
    mismatch.struct_size = sizeof(mismatch);
    mismatch.abi_version = SMAVE_ABI_VERSION + 1;
    const int valid =
        smave_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_dae_info(result, &mismatch) == SMAVE_STATUS_ABI_MISMATCH &&
        smave_result_get_dae_info(
            result, (smave_dae_result_info*)&event_prefix.info) == SMAVE_STATUS_OK &&
        event_prefix.info.event_count == 0 && event_prefix.info.last_event_time == 0.0 &&
        event_prefix.tail == UINT64_C(0x3ec4527ad91106bf) &&
        smave_result_get_dae_info(result, &extended.info) == SMAVE_STATUS_OK &&
        extended.info.differentiation_index == 2 &&
        extended.tail == UINT64_C(0x7395a4e621bc08df);
    destroy_solve_objects(result, solver, problem);
    return valid;
}

int main(void) {
    smave_library* library = NULL;
    int32_t available = 0;
    int32_t dae_available = 0;
    const double hidden_inconsistent_state[] = {0.0, 1.0, 0.0};
    const double hidden_inconsistent_derivative[] = {1.0, 0.0, 0.0};
    const double rank_deficient_state[] = {0.0, 0.0, 0.0};
    const double rank_deficient_derivative[] = {0.0, 0.0, 0.0};
    const int success =
        smave_library_create(NULL, &library) == SMAVE_STATUS_OK &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_INDEX_TWO_DAE, &available) == SMAVE_STATUS_OK &&
        available &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_DAE, &dae_available) == SMAVE_STATUS_OK &&
        dae_available && solve_positive(library) &&
        solve_rejected_case(
            library, MODEL_CANONICAL, hidden_inconsistent_state,
            hidden_inconsistent_derivative, SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED, 2) &&
        solve_rejected_case(
            library, MODEL_HIDDEN_RANK_DEFICIENT, rank_deficient_state,
            rank_deficient_derivative, SMAVE_DIAGNOSTIC_INVALID_CONTRACT, 1) &&
        result_abi_checks(library);
    const int destroyed = smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!success || !destroyed) return 1;
    printf("INDEX_TWO_DAE_CAPABILITY 1\n"
           "INDEX_TWO_DAE_HIDDEN_CONSISTENCY_GATE 1\n"
           "INDEX_TWO_DAE_HIDDEN_RANK_GATE 1\n"
           "INDEX_TWO_DAE_RESULT_ABI_MISMATCH 1\n"
           "INDEX_TWO_DAE_EVENT_RESULT_PREFIX_PRESERVED 1\n"
           "INDEX_TWO_DAE_EXTENDED_RESULT_TAIL_PRESERVED 1\nEND\n");
    return 0;
}
