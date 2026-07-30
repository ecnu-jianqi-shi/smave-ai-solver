#include "smave/c_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int residual(
    size_t dimension, double time, const double* state, const double* derivative,
    double* values, void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 2) return 1;
    values[0] = derivative[0] + state[1];
    values[1] = state[1] - state[0];
    return 0;
}

static int jacobian(
    size_t dimension, double time, const double* state, const double* derivative,
    double derivative_scale, double* values, void* user_data) {
    (void)time;
    (void)state;
    (void)derivative;
    if (dimension != 2) return 1;
    values[0] = user_data == NULL ? derivative_scale : 0.0;
    values[1] = user_data == NULL ? 1.0 : 0.0;
    values[2] = user_data == NULL ? -1.0 : 0.0;
    values[3] = user_data == NULL ? 1.0 : 0.0;
    return 0;
}

static int solve_case(smave_library* library, void* bad_jacobian, int expect_fallback) {
    const double initial_state[] = {1.0, 1.0};
    const double initial_derivative[] = {-1.0, 0.0};
    const uint8_t differential_mask[] = {1, 0};
    smave_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative,
        0.0, 1.0, 0.1, residual, jacobian, bad_jacobian};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 1000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_dae_result_info dae_info = {
        sizeof(dae_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[2];
    size_t required = 0;
    int ok = smave_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK &&
        smave_result_get_provenance(result, &service_id, &plan_id, &equation_family) ==
            SMAVE_STATUS_OK &&
        smave_result_get_dae_info(result, &dae_info) == SMAVE_STATUS_OK &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && info.success && info.used_fallback == expect_fallback &&
        service_id != NULL && strcmp(service_id, "smave.verified-fully-implicit-dae-solve.v1") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' && equation_family != NULL &&
        strcmp(equation_family, "dae-fully-implicit-first-order-smooth") == 0 &&
        dae_info.accepted_steps == 10 && dae_info.rejected_steps == 0 &&
        fabs(dae_info.final_time - 1.0) < 1.0e-12 &&
        dae_info.maximum_residual_inf <= 1.0e-8 &&
        fabs(solution[0] - pow(1.0 / 1.1, 10.0)) < 1.0e-8 &&
        fabs(solution[1] - solution[0]) < 1.0e-10;
    if (ok && !expect_fallback) {
        printf("SMAVE_C_API_DAE_SERVICE 1\nservice_id=\"%s\"\nbackend=\"%s\"\n"
               "plan_id=\"%s\"\nequation_family=\"%s\"\nfinal_time=%.17g\n"
               "maximum_residual_inf=%.17g\naccepted_steps=%zu\nsolution=%.17g,%.17g\n",
               service_id, info.backend, plan_id, equation_family, dae_info.final_time,
               dae_info.maximum_residual_inf, dae_info.accepted_steps,
               solution[0], solution[1]);
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

int main(void) {
    smave_library* library = NULL;
    int32_t available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(library, SMAVE_CAPABILITY_DAE, &available) !=
            SMAVE_STATUS_OK || !available ||
        !solve_case(library, NULL, 0) || !solve_case(library, (void*)1, 1)) return 1;

    const double initial_state[] = {1.0, 1.0};
    const double inconsistent_derivative[] = {0.0, 0.0};
    const uint8_t differential_mask[] = {1, 0};
    smave_dae_problem_desc inconsistent = {
        sizeof(inconsistent), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, inconsistent_derivative,
        0.0, 0.1, 0.1, residual, jacobian, NULL};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 100};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    int rejected = smave_dae_problem_create(library, &inconsistent, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    if (!rejected || smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("DAE_FALLBACK 1\nDAE_INCONSISTENT_INITIAL_REJECTED 1\nEND\n");
    return 0;
}
