#include "smave/c_api.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static atomic_size_t callback_count;

static int valid_fallback(size_t dimension, double* solution, void* user_data) {
    (void)user_data;
    atomic_fetch_add(&callback_count, 1);
    if (dimension != 2) return 1;
    solution[0] = 1.0;
    solution[1] = 1.0;
    return 0;
}

static int rejected_fallback(size_t dimension, double* solution, void* user_data) {
    (void)user_data;
    atomic_fetch_add(&callback_count, 1);
    if (dimension != 2) return 1;
    solution[0] = 0.0;
    solution[1] = 0.0;
    return 0;
}

static int failed_fallback(size_t dimension, double* solution, void* user_data) {
    (void)dimension;
    (void)solution;
    (void)user_data;
    atomic_fetch_add(&callback_count, 1);
    return 1;
}

static int nonlinear_residual(
    size_t dimension,
    const double* state,
    double* residual,
    void* user_data) {
    (void)user_data;
    if (dimension != 1) return 1;
    residual[0] = state[0] - 1.0;
    return 0;
}

int main(void) {
    const double matrix[] = {1.0, 1.0, 2.0, 2.0};
    const double right[] = {2.0, 4.0};
    const double regular_matrix[] = {2.0, 0.0, 0.0, 3.0};
    const double regular_right[] = {2.0, 3.0};
    const double initial[] = {0.0};
    const smave_linear_problem_desc linear_desc = {
        sizeof(linear_desc), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        0, 2, matrix, NULL, NULL, NULL, 0, right};
    const smave_nonlinear_problem_desc nonlinear_desc = {
        sizeof(nonlinear_desc), SMAVE_ABI_VERSION, 1, initial,
        nonlinear_residual, NULL, NULL};
    const smave_linear_problem_desc regular_desc = {
        sizeof(regular_desc), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        2, regular_matrix, NULL, NULL, NULL, 0, regular_right};
    const smave_linear_fallback_desc valid = {
        sizeof(valid), SMAVE_ABI_VERSION, valid_fallback, NULL};
    const smave_linear_fallback_desc rejected = {
        sizeof(rejected), SMAVE_ABI_VERSION, rejected_fallback, NULL};
    const smave_linear_fallback_desc failed = {
        sizeof(failed), SMAVE_ABI_VERSION, failed_fallback, NULL};
    smave_linear_fallback_desc bad_abi = valid;
    bad_abi.abi_version = SMAVE_ABI_VERSION + 1;
    smave_linear_fallback_desc bad_size = valid;
    bad_size.struct_size = sizeof(bad_size) - 1;
    smave_library* library = NULL;
    smave_problem* linear_problem = NULL;
    smave_problem* nonlinear_problem = NULL;
    smave_problem* regular_problem = NULL;
    smave_solver* linear_solver = NULL;
    smave_solver* nonlinear_solver = NULL;
    smave_solver* regular_solver = NULL;
    smave_cancel_token* token = NULL;
    int32_t available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_EXTERNAL_LINEAR_FALLBACK, &available) !=
            SMAVE_STATUS_OK || !available ||
        smave_linear_problem_create(library, &linear_desc, &linear_problem) !=
            SMAVE_STATUS_OK ||
        smave_nonlinear_problem_create(library, &nonlinear_desc, &nonlinear_problem) !=
            SMAVE_STATUS_OK ||
        smave_linear_problem_create(library, &regular_desc, &regular_problem) !=
            SMAVE_STATUS_OK ||
        smave_problem_finalize(linear_problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(nonlinear_problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(regular_problem) != SMAVE_STATUS_OK ||
        smave_solver_create(linear_problem, NULL, &linear_solver) != SMAVE_STATUS_OK ||
        smave_solver_create(nonlinear_problem, NULL, &nonlinear_solver) != SMAVE_STATUS_OK ||
        smave_solver_create(regular_problem, NULL, &regular_solver) != SMAVE_STATUS_OK ||
        smave_cancel_token_create(library, &token) != SMAVE_STATUS_OK) return 1;

    smave_result* result = NULL;
    if (smave_solver_solve(linear_solver, &result) != SMAVE_STATUS_SOLVE_FAILED ||
        result == NULL) return 1;
    smave_result_destroy(result);

    result = NULL;
    const size_t before_primary = atomic_load(&callback_count);
    smave_result_info primary_info = {
        sizeof(primary_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    if (smave_solver_solve_linear_with_fallback(
            regular_solver, &rejected, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK || result == NULL ||
        atomic_load(&callback_count) != before_primary ||
        smave_result_get_info(result, &primary_info) != SMAVE_STATUS_OK ||
        !primary_info.success || primary_info.backend == NULL ||
        strcmp(primary_info.backend, "caller-linear-fallback-v1") == 0) return 1;
    smave_result_destroy(result);

    result = NULL;
    const size_t before_valid = atomic_load(&callback_count);
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &valid, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK || result == NULL ||
        atomic_load(&callback_count) != before_valid + 1) return 1;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    double solution[2] = {0.0, 0.0};
    size_t required = 0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    const int accepted =
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        info.used_fallback && info.backend != NULL &&
        strcmp(info.backend, "caller-linear-fallback-v1") == 0 &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_SUCCESS &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && solution[0] == 1.0 && solution[1] == 1.0 &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        service_id != NULL && strcmp(service_id, "smave.verified-linear-solve.v1") == 0 &&
        plan_id != NULL && strstr(plan_id, "caller-linear-fallback-v1") != NULL &&
        equation_family != NULL;
    smave_result_destroy(result);
    if (!accepted) return 1;

    result = NULL;
    diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &rejected, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED || result == NULL ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED) return 1;
    smave_result_destroy(result);

    result = NULL;
    diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &failed, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED || result == NULL ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_CALLBACK_FAILURE) return 1;
    smave_result_destroy(result);

    const size_t before_cancel = atomic_load(&callback_count);
    if (smave_cancel_token_request(token) != SMAVE_STATUS_OK) return 1;
    result = NULL;
    diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &valid, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_CANCELLED || result == NULL ||
        atomic_load(&callback_count) != before_cancel ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_CANCELLED) return 1;
    smave_result_destroy(result);
    if (smave_cancel_token_reset(token) != SMAVE_STATUS_OK) return 1;

    result = (smave_result*)(uintptr_t)1;
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &bad_abi, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_ABI_MISMATCH || result != NULL) return 1;
    result = (smave_result*)(uintptr_t)1;
    if (smave_solver_solve_linear_with_fallback(
            linear_solver, &bad_size, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_INVALID_ARGUMENT || result != NULL) return 1;
    result = (smave_result*)(uintptr_t)1;
    if (smave_solver_solve_linear_with_fallback(
            nonlinear_solver, &valid, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_UNSUPPORTED || result != NULL) return 1;

    if (smave_cancel_token_destroy(token) != SMAVE_STATUS_OK ||
        smave_solver_destroy(regular_solver) != SMAVE_STATUS_OK ||
        smave_solver_destroy(nonlinear_solver) != SMAVE_STATUS_OK ||
        smave_solver_destroy(linear_solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(regular_problem) != SMAVE_STATUS_OK ||
        smave_problem_destroy(nonlinear_problem) != SMAVE_STATUS_OK ||
        smave_problem_destroy(linear_problem) != SMAVE_STATUS_OK ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;

    printf("SMAVE_C_API_EXTERNAL_LINEAR_FALLBACK 1\n"
           "EXTERNAL_LINEAR_FALLBACK_CAPABILITY 1\n"
           "EXTERNAL_LINEAR_FALLBACK_AFTER_BUILTINS 1\n"
           "EXTERNAL_LINEAR_FALLBACK_ORIGINAL_GATE 1\n"
           "EXTERNAL_LINEAR_FALLBACK_CALLBACK_FAILURE 1\n"
           "EXTERNAL_LINEAR_FALLBACK_CANCEL_PRECHECK 1\n"
           "EXTERNAL_LINEAR_FALLBACK_NEGATIVE_CONTRACTS 1\n"
           "EXTERNAL_LINEAR_FALLBACK_SHARED_SERVICE 1\nEND\n");
    return 0;
}
