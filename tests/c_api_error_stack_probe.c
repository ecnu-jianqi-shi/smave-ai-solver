#include "smave/c_api.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct worker_context {
    smave_library* library;
    int ok;
} worker_context;

static int residual(
    size_t dimension,
    const double* state,
    double* output,
    void* user_data) {
    (void)user_data;
    if (dimension != 1 || state == NULL || output == NULL) return 1;
    output[0] = state[0] - 1.0;
    return 0;
}

static int get_error(
    smave_library* library,
    size_t newest_index,
    smave_error_info* info) {
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(*info);
    info->abi_version = SMAVE_ABI_VERSION;
    return smave_library_get_error(library, newest_index, info) == SMAVE_STATUS_OK;
}

static void* worker_main(void* opaque) {
    worker_context* context = (worker_context*)opaque;
    size_t count = 99;
    if (smave_library_get_error_count(context->library, &count) != SMAVE_STATUS_OK ||
        count != 0) return NULL;

    const double initial_state[] = {0.0};
    const smave_nonlinear_problem_desc invalid = {
        sizeof(invalid), SMAVE_ABI_VERSION, 0, initial_state, residual, NULL, NULL};
    smave_problem* problem = NULL;
    if (smave_nonlinear_problem_create(context->library, &invalid, &problem) !=
            SMAVE_STATUS_INVALID_ARGUMENT ||
        problem != NULL) return NULL;
    if (smave_library_get_error_count(context->library, &count) != SMAVE_STATUS_OK ||
        count != 1) return NULL;

    smave_error_info info;
    if (!get_error(context->library, 0, &info) ||
        info.status != SMAVE_STATUS_INVALID_ARGUMENT || info.trace_id == 0 ||
        strcmp(info.operation, "smave_nonlinear_problem_create") != 0 ||
        strstr(info.message, "descriptor") == NULL) return NULL;
    context->ok = 1;
    return NULL;
}

int main(void) {
    smave_library* library = NULL;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK || library == NULL) return 1;

    int32_t available = 0;
    size_t count = 99;
    if (smave_library_has_capability(
            library, SMAVE_CAPABILITY_ERROR_STACK, &available) != SMAVE_STATUS_OK ||
        !available || smave_library_clear_errors(library) != SMAVE_STATUS_OK ||
        smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 0) {
        return 1;
    }

    const double initial_state[] = {0.0};
    const smave_nonlinear_problem_desc invalid_nonlinear = {
        sizeof(invalid_nonlinear), SMAVE_ABI_VERSION, 0,
        initial_state, residual, NULL, NULL};
    smave_problem* problem = NULL;
    if (smave_nonlinear_problem_create(library, &invalid_nonlinear, &problem) !=
            SMAVE_STATUS_INVALID_ARGUMENT ||
        problem != NULL) return 1;

    smave_error_info first;
    if (!get_error(library, 0, &first) || first.trace_id == 0 ||
        first.status != SMAVE_STATUS_INVALID_ARGUMENT ||
        strcmp(first.operation, "smave_nonlinear_problem_create") != 0 ||
        strcmp(first.message, "descriptor shape or residual callback is invalid") != 0) {
        return 1;
    }
    const uint64_t first_trace_id = first.trace_id;

    const smave_ode_problem_desc invalid_ode = {
        sizeof(invalid_ode), SMAVE_ABI_VERSION + 1, 1, initial_state,
        0.0, 1.0, 0.1, NULL, NULL};
    if (smave_ode_problem_create(library, &invalid_ode, &problem) !=
            SMAVE_STATUS_ABI_MISMATCH ||
        problem != NULL) return 1;
    if (smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 2) {
        return 1;
    }

    smave_error_info newest;
    smave_error_info older;
    if (!get_error(library, 0, &newest) || !get_error(library, 1, &older) ||
        newest.trace_id <= first_trace_id || newest.status != SMAVE_STATUS_ABI_MISMATCH ||
        strcmp(newest.operation, "smave_ode_problem_create") != 0 ||
        older.trace_id != first_trace_id ||
        strcmp(older.operation, "smave_nonlinear_problem_create") != 0) return 1;

    smave_error_info invalid_info = {
        sizeof(invalid_info), SMAVE_ABI_VERSION + 1, 0, SMAVE_STATUS_OK, NULL, NULL};
    if (smave_library_get_error(library, 0, &invalid_info) != SMAVE_STATUS_ABI_MISMATCH) {
        return 1;
    }
    invalid_info.struct_size = sizeof(invalid_info) - 1;
    invalid_info.abi_version = SMAVE_ABI_VERSION;
    if (smave_library_get_error(library, 0, &invalid_info) !=
            SMAVE_STATUS_INVALID_ARGUMENT ||
        smave_library_get_error(library, 2, &invalid_info) !=
            SMAVE_STATUS_INVALID_ARGUMENT ||
        smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 2) {
        return 1;
    }

    worker_context context = {library, 0};
    pthread_t worker;
    if (pthread_create(&worker, NULL, worker_main, &context) != 0 ||
        pthread_join(worker, NULL) != 0 || !context.ok ||
        smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 2) {
        return 1;
    }

    const smave_nonlinear_problem_desc valid = {
        sizeof(valid), SMAVE_ABI_VERSION, 1, initial_state, residual, NULL, NULL};
    smave_solver* solver = NULL;
    if (smave_nonlinear_problem_create(library, &valid, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(problem) != SMAVE_STATUS_INVALID_STATE) return 1;

    smave_error_info lifecycle;
    if (!get_error(library, 0, &lifecycle) ||
        lifecycle.status != SMAVE_STATUS_INVALID_STATE ||
        strcmp(lifecycle.operation, "smave_problem_destroy") != 0 ||
        strstr(lifecycle.message, "live solver") == NULL ||
        smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 3) {
        return 1;
    }
    if (smave_solver_destroy(solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(problem) != SMAVE_STATUS_OK) return 1;

    if (smave_library_clear_errors(library) != SMAVE_STATUS_OK) return 1;
    for (size_t index = 0; index < 10; ++index) {
        if (smave_library_has_capability(
                library, SMAVE_CAPABILITY_LINEAR_DENSE, NULL) !=
            SMAVE_STATUS_INVALID_ARGUMENT) return 1;
    }
    smave_error_info bounded_newest;
    smave_error_info bounded_oldest;
    if (smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 8 ||
        !get_error(library, 0, &bounded_newest) ||
        !get_error(library, 7, &bounded_oldest) ||
        bounded_newest.trace_id - bounded_oldest.trace_id != 7 ||
        strcmp(bounded_newest.operation, "smave_library_has_capability") != 0 ||
        smave_library_clear_errors(library) != SMAVE_STATUS_OK ||
        smave_library_get_error_count(library, &count) != SMAVE_STATUS_OK || count != 0 ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;

    printf("SMAVE_C_API_ERROR_STACK 1\n"
           "ERROR_STACK_CAPABILITY 1\n"
           "ERROR_STACK_NEWEST_FIRST 1\n"
           "ERROR_STACK_THREAD_LOCAL 1\n"
           "ERROR_STACK_LIFECYCLE 1\n"
           "ERROR_STACK_ABI_VALIDATION 1\n"
           "ERROR_STACK_BOUNDED 1\n"
           "ERROR_STACK_CLEAR 1\n");
    return 0;
}
