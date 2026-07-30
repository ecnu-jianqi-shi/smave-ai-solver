#include "smave/c_api.h"

#include <stdio.h>
#include <string.h>

typedef struct fallback_context {
    int mode;
    size_t calls;
    int saw_fresh_initial_state;
} fallback_context;

static int residual(
    size_t dimension, const double* state, double* values, void* user_data) {
    (void)user_data;
    if (dimension != 2) return 1;
    values[0] = state[0] * state[0] + state[1] - 5.0;
    values[1] = state[0] + state[1] * state[1] - 3.0;
    return 0;
}

static int hard_residual(
    size_t dimension, const double* state, double* values, void* user_data) {
    (void)user_data;
    if (dimension != 1) return 1;
    values[0] = state[0] * state[0] - 4.0;
    return 0;
}

static int failing_jacobian(
    size_t dimension, const double* state, double* values, void* user_data) {
    (void)dimension;
    (void)state;
    (void)values;
    (void)user_data;
    return 1;
}

static int nonlinear_fallback(
    size_t dimension, double* solution, void* user_data) {
    fallback_context* context = (fallback_context*)user_data;
    if (dimension != 1 || context == NULL) return 1;
    ++context->calls;
    context->saw_fresh_initial_state = solution[0] == 0.0;
    if (context->mode == 1) return 1;
    solution[0] = context->mode == 2 ? 0.0 : 2.0;
    return 0;
}

static int check_external_fallback(
    smave_library* library, smave_solver* built_in_solver) {
    int32_t available = 0;
    fallback_context context = {0, 0, 0};
    smave_nonlinear_fallback_desc fallback = {
        sizeof(fallback), SMAVE_ABI_VERSION, nonlinear_fallback, &context};
    smave_result* result = NULL;
    if (smave_library_has_capability(
            library, SMAVE_CAPABILITY_EXTERNAL_NONLINEAR_FALLBACK, &available) !=
            SMAVE_STATUS_OK ||
        !available ||
        smave_solver_solve_nonlinear_with_fallback(
            built_in_solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 0 || smave_result_destroy(result) != SMAVE_STATUS_OK) {
        return 0;
    }

    const double initial[] = {0.0};
    smave_nonlinear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        hard_residual, failing_jacobian, NULL};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 1};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_cancel_token* token = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double solution[1] = {0.0};
    size_t required = 0;
    if (smave_nonlinear_problem_create(library, &descriptor, &problem) !=
            SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options, &solver) != SMAVE_STATUS_OK ||
        smave_cancel_token_create(library, &token) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 0;
    result = NULL;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 1 || !context.saw_fresh_initial_state ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK || !info.success ||
        !info.used_fallback || info.backend == NULL ||
        strcmp(info.backend, "caller-nonlinear-fallback-v1") != 0 ||
        smave_result_copy_solution(result, solution, 1, &required) != SMAVE_STATUS_OK ||
        required != 1 || solution[0] != 2.0 ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 2;
    result = NULL;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 1;
    result = NULL;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_CALLBACK_FAILURE ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 0;
    result = NULL;
    if (smave_cancel_token_request(token) != SMAVE_STATUS_OK ||
        smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_CANCELLED ||
        context.calls != 0 ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_CANCELLED ||
        smave_result_destroy(result) != SMAVE_STATUS_OK ||
        smave_cancel_token_reset(token) != SMAVE_STATUS_OK) return 0;

    fallback.abi_version = SMAVE_ABI_VERSION + 1;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_ABI_MISMATCH) return 0;
    fallback.abi_version = SMAVE_ABI_VERSION;
    fallback.struct_size = sizeof(fallback) - 1;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;
    fallback.struct_size = sizeof(fallback);
    fallback.solve = NULL;
    if (smave_solver_solve_nonlinear_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;

    if (smave_cancel_token_destroy(token) != SMAVE_STATUS_OK ||
        smave_solver_destroy(solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(problem) != SMAVE_STATUS_OK) return 0;
    return 1;
}

int main(void) {
    const double initial[] = {1.5, 1.5};
    smave_library* library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_nonlinear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, initial, residual, NULL, NULL};
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[2];
    size_t required = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_nonlinear_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK ||
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(result, solution, 2, &required) != SMAVE_STATUS_OK ||
        required != 2 || service_id == NULL || plan_id == NULL ||
        equation_family == NULL || info.diagnostic == NULL ||
        !check_external_fallback(library, solver)) return 1;
    printf("SMAVE_C_API_NONLINEAR_SERVICE 1\n"
           "service_id=\"%s\"\n"
           "success=%d\n"
           "used_fallback=%d\n"
           "backend=\"%s\"\n"
           "plan_id=\"%s\"\n"
           "equation_family=\"%s\"\n"
           "residual_inf=%.17g\n"
           "backward_error=%.17g\n"
           "solution=%.17g,%.17g\n"
           "diagnostic=\"%s\"\n"
           "EXTERNAL_NONLINEAR_FALLBACK_CAPABILITY 1\n"
           "EXTERNAL_NONLINEAR_FALLBACK_AFTER_BUILTINS 1\n"
           "EXTERNAL_NONLINEAR_FALLBACK_ORIGINAL_GATE 1\n"
           "EXTERNAL_NONLINEAR_FALLBACK_CALLBACK_FAILURE 1\n"
           "EXTERNAL_NONLINEAR_FALLBACK_CANCEL_PRECHECK 1\n"
           "EXTERNAL_NONLINEAR_FALLBACK_NEGATIVE_CONTRACTS 1\n"
           "END\n",
           service_id, info.success, info.used_fallback, info.backend, plan_id,
           equation_family, info.residual_inf, info.backward_error,
           solution[0], solution[1], info.diagnostic);
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return smave_library_destroy(library) == SMAVE_STATUS_OK ? 0 : 1;
}
