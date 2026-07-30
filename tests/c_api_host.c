#include "smave/c_api.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

static atomic_size_t allocations;
static atomic_size_t deallocations;

static int has_diagnostic_code(
    const smave_result* result, smave_diagnostic_code expected) {
    smave_diagnostic_code actual = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    return smave_result_get_diagnostic_code(result, &actual) == SMAVE_STATUS_OK &&
        actual == expected;
}

static void* tracked_allocate(size_t size, void* user_data) {
    (void)user_data;
    atomic_fetch_add(&allocations, 1);
    return malloc(size);
}

static void tracked_deallocate(void* memory, void* user_data) {
    (void)user_data;
    atomic_fetch_add(&deallocations, 1);
    free(memory);
}

typedef struct solve_thread_context {
    const smave_solver* solver;
    int success;
} solve_thread_context;

static void* solve_thread(void* argument) {
    solve_thread_context* context = (solve_thread_context*)argument;
    size_t iteration;
    context->success = 1;
    for (iteration = 0; iteration < 32; ++iteration) {
        smave_result* result = NULL;
        smave_result_info info = {
            sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
        double values[3];
        size_t required = 0;
        if (smave_solver_solve(context->solver, &result) != SMAVE_STATUS_OK ||
            smave_result_get_info(result, &info) != SMAVE_STATUS_OK || !info.success ||
            !has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS) ||
            smave_result_copy_solution(result, values, 3, &required) != SMAVE_STATUS_OK ||
            required != 3 || fabs(values[0] - 1.0) > 1.0e-10 ||
            fabs(values[1] - 1.0) > 1.0e-10 || fabs(values[2] - 1.0) > 1.0e-10) {
            context->success = 0;
        }
        smave_result_destroy(result);
        if (!context->success) break;
    }
    return NULL;
}

static int check_concurrent_solve(smave_library* library,
                                  smave_linear_problem_desc descriptor) {
    enum { thread_count = 8 };
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    pthread_t threads[thread_count];
    solve_thread_context contexts[thread_count];
    int index;
    if (smave_linear_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK) return 0;
    for (index = 0; index < thread_count; ++index) {
        contexts[index].solver = solver;
        contexts[index].success = 0;
        if (pthread_create(&threads[index], NULL, solve_thread, &contexts[index]) != 0) return 0;
    }
    for (index = 0; index < thread_count; ++index) {
        if (pthread_join(threads[index], NULL) != 0 || !contexts[index].success) return 0;
    }
    return smave_solver_destroy(solver) == SMAVE_STATUS_OK &&
        smave_problem_destroy(problem) == SMAVE_STATUS_OK;
}

static int check_singular_rejection(smave_library* library) {
    const double matrix[] = {1, 2, 2, 4};
    const double right[] = {3, 6};
    smave_linear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        0, 2, matrix, NULL, NULL, NULL, 0, right};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    int success = smave_linear_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        result != NULL && smave_result_get_info(result, &info) == SMAVE_STATUS_OK &&
        !info.success && info.diagnostic != NULL &&
        has_diagnostic_code(result, SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED);
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int nonlinear_residual(
    size_t dimension, const double* state, double* residual, void* user_data) {
    (void)user_data;
    if (dimension != 2) return 1;
    residual[0] = state[0] * state[0] + state[1] - 5.0;
    residual[1] = state[0] + state[1] * state[1] - 3.0;
    return 0;
}

static int bad_nonlinear_jacobian(
    size_t dimension, const double* state, double* jacobian, void* user_data) {
    size_t index;
    (void)state;
    (void)user_data;
    for (index = 0; index < dimension * dimension; ++index) jacobian[index] = 0.0;
    return 0;
}

static int ode_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1 || state == NULL || derivative == NULL) return 1;
    derivative[0] = -state[0];
    return 0;
}

static int check_ode(smave_library* library) {
    const double initial[] = {1.0};
    smave_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 1.0, 0.1, ode_rhs, NULL};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 100000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    double solution[1];
    size_t required = 0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    smave_ode_result_info ode_info = {
        sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    int success = smave_ode_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS) &&
        !info.used_fallback && info.residual_inf <= 1.0 &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        service_id != NULL &&
        strcmp(service_id, "smave.verified-explicit-ode-solve.v1") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' && equation_family != NULL &&
        strcmp(equation_family, "explicit-ode-smooth") == 0 &&
        smave_result_get_ode_info(result, &ode_info) == SMAVE_STATUS_OK &&
        fabs(ode_info.final_time - 1.0) <= 1.0e-12 &&
        ode_info.maximum_scaled_local_error <= 1.0 && ode_info.accepted_steps > 0 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        fabs(solution[0] - 0.36787944117144233) <= 1.0e-7;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    descriptor.end_time = descriptor.start_time;
    problem = NULL;
    success = success &&
        smave_ode_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_INVALID_ARGUMENT && problem == NULL;
    return success;
}

static int event_guard(
    size_t dimension, double time, const double* state, double* guard,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    *guard = state[0] - 0.5;
    return 0;
}

static int event_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)pre_state;
    (void)user_data;
    if (dimension != 1) return 1;
    post_state[0] = 0.0;
    return 0;
}

static int event_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = 1.0;
    return 0;
}

static int check_event_ode(smave_library* library) {
    const double initial[] = {0.0};
    const smave_event_desc event = {
        sizeof(event), SMAVE_ABI_VERSION, 1, 0, event_guard, event_reset, NULL};
    smave_event_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 0.9, 0.2, event_rhs, NULL, &event, 1};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_ode_result_info ode_info = {
        sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[1];
    size_t required = 0;
    int success = smave_event_ode_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS) &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        strcmp(service_id, "smave.verified-explicit-ode-solve.v1") == 0 &&
        strcmp(equation_family, "explicit-ode-with-events") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' &&
        smave_result_get_ode_info(result, &ode_info) == SMAVE_STATUS_OK &&
        ode_info.event_count == 1 && fabs(ode_info.last_event_time - 0.5) < 1.0e-8 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        fabs(solution[0] - 0.4) < 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int dae_residual(
    size_t dimension, double time, const double* state, const double* derivative,
    double* residual, void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1 || state == NULL || derivative == NULL || residual == NULL) return 1;
    residual[0] = derivative[0] + state[0];
    return 0;
}

static int dae_jacobian(
    size_t dimension, double time, const double* state, const double* derivative,
    double derivative_scale, double* jacobian, void* user_data) {
    (void)time;
    (void)state;
    (void)derivative;
    (void)user_data;
    if (dimension != 1 || jacobian == NULL) return 1;
    jacobian[0] = 1.0 + derivative_scale;
    return 0;
}

static int check_dae(smave_library* library) {
    const double initial_state[] = {1.0};
    const double initial_derivative[] = {-1.0};
    const uint8_t differential_mask[] = {1};
    smave_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, differential_mask,
        initial_state, initial_derivative,
        0.0, 1.0, 0.1, dae_residual, dae_jacobian, NULL};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 1000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_dae_result_info dae_info = {
        sizeof(dae_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    smave_ode_result_info ode_info = {
        sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[1];
    size_t required = 0;
    int success = smave_dae_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS) &&
        !info.used_fallback && info.residual_inf <= 1.0e-8 &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        service_id != NULL &&
        strcmp(service_id, "smave.verified-fully-implicit-dae-solve.v1") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' && equation_family != NULL &&
        strcmp(equation_family, "dae-fully-implicit-first-order-smooth") == 0 &&
        smave_result_get_dae_info(result, &dae_info) == SMAVE_STATUS_OK &&
        smave_result_get_ode_info(result, &ode_info) == SMAVE_STATUS_UNSUPPORTED &&
        fabs(dae_info.final_time - 1.0) <= 1.0e-12 &&
        dae_info.accepted_steps == 10 && dae_info.rejected_steps == 0 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        fabs(solution[0] - pow(1.0 / 1.1, 10.0)) <= 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int check_nonlinear_fallback(smave_library* library) {
    const double initial[] = {1.5, 1.5};
    smave_nonlinear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, initial,
        nonlinear_residual, bad_nonlinear_jacobian, NULL};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    double solution[2];
    size_t required = 0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    smave_ode_result_info ode_info = {
        sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    int success = smave_nonlinear_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS) &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        service_id != NULL && strcmp(service_id, "smave.verified-nonlinear-solve.v1") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' &&
        equation_family != NULL &&
        strcmp(equation_family, "nonlinear-algebraic-smooth") == 0 &&
        smave_result_get_ode_info(result, &ode_info) == SMAVE_STATUS_UNSUPPORTED &&
        info.used_fallback && info.residual_inf <= 1.0e-9 &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        fabs(solution[0] - 2.0) <= 1.0e-8 && fabs(solution[1] - 1.0) <= 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int require_status(smave_status actual, smave_status expected, const char* message) {
    if (actual == expected) return 1;
    fprintf(stderr, "%s: expected %s, got %s\n", message,
            smave_status_string(expected), smave_status_string(actual));
    return 0;
}

static int solve_and_check(smave_library* library, smave_linear_problem_desc descriptor) {
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    double solution[3] = {0, 0, 0};
    size_t required = 0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    if (!require_status(smave_linear_problem_create(library, &descriptor, &problem),
                        SMAVE_STATUS_OK, "create problem")) return 0;
    if (!require_status(smave_solver_create(problem, NULL, &solver),
                        SMAVE_STATUS_INVALID_STATE, "reject unfinalized problem")) return 0;
    if (!require_status(smave_problem_finalize(problem), SMAVE_STATUS_OK,
                        "finalize problem")) return 0;
    if (!require_status(smave_solver_create(problem, NULL, &solver), SMAVE_STATUS_OK,
                        "create solver")) return 0;
    if (!require_status(smave_problem_destroy(problem), SMAVE_STATUS_INVALID_STATE,
                        "reject live-child problem destroy")) return 0;
    {
        const smave_status solve_status = smave_solver_solve(solver, &result);
        if (solve_status != SMAVE_STATUS_OK) {
            if (result != NULL && smave_result_get_info(result, &info) == SMAVE_STATUS_OK) {
                fprintf(stderr, "solve diagnostic: %s\n", info.diagnostic);
            }
            smave_result_destroy(result);
            return require_status(solve_status, SMAVE_STATUS_OK, "solve") && 0;
        }
    }
    if (!require_status(smave_result_get_info(result, &info), SMAVE_STATUS_OK,
                        "result info")) return 0;
    if (!has_diagnostic_code(result, SMAVE_DIAGNOSTIC_SUCCESS)) return 0;
    if (!info.success || info.dimension != 3 || info.backend == NULL ||
        info.diagnostic == NULL ||
        strstr(info.diagnostic, "smave.verified-linear-solve.v1: ") != info.diagnostic ||
        info.backward_error > 1.0e-10) {
        fprintf(stderr, "invalid result metadata\n");
        return 0;
    }
    if (!require_status(smave_result_get_provenance(
                            result, &service_id, &plan_id, &equation_family),
                        SMAVE_STATUS_OK, "result provenance") ||
        service_id == NULL || strcmp(service_id, "smave.verified-linear-solve.v1") != 0 ||
        plan_id == NULL || plan_id[0] == '\0' ||
        equation_family == NULL || equation_family[0] == '\0') {
        return 0;
    }
    {
        smave_ode_result_info ode_info = {
            sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
        if (!require_status(smave_result_get_ode_info(result, &ode_info),
                            SMAVE_STATUS_UNSUPPORTED, "reject linear ODE metadata")) {
            return 0;
        }
    }
    if (!require_status(smave_result_copy_solution(result, solution, 2, &required),
                        SMAVE_STATUS_BUFFER_TOO_SMALL, "small result buffer")) return 0;
    if (required != 3 ||
        !require_status(smave_result_copy_solution(result, solution, 3, &required),
                        SMAVE_STATUS_OK, "copy solution")) return 0;
    if (fabs(solution[0] - 1.0) > 1.0e-10 || fabs(solution[1] - 1.0) > 1.0e-10 ||
        fabs(solution[2] - 1.0) > 1.0e-10) {
        fprintf(stderr, "wrong solution %.17g %.17g %.17g\n",
                solution[0], solution[1], solution[2]);
        return 0;
    }
    return require_status(smave_result_destroy(result), SMAVE_STATUS_OK, "destroy result") &&
        require_status(smave_solver_destroy(solver), SMAVE_STATUS_OK, "destroy solver") &&
        require_status(smave_problem_destroy(problem), SMAVE_STATUS_OK, "destroy problem");
}

int main(void) {
    smave_library_options options = {
        sizeof(options), SMAVE_ABI_VERSION, tracked_allocate, tracked_deallocate, NULL};
    smave_library* library = NULL;
    int32_t available = 0;
    const double dense[] = {4, -1, 0, -1, 4, -1, 0, -1, 3};
    const double right[] = {3, 2, 2};
    const size_t row_offsets[] = {0, 2, 5, 7};
    const size_t columns[] = {0, 1, 0, 1, 2, 1, 2};
    const double sparse[] = {4, -1, -1, 4, -1, -1, 3};
    smave_linear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        3, dense, NULL, NULL, NULL, 0, right};

    if (smave_abi_version() != SMAVE_ABI_VERSION || strlen(smave_version_string()) == 0) return 1;
    if (!require_status(smave_library_create(&options, &library), SMAVE_STATUS_OK,
                        "create library")) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_LINEAR_DENSE, &available),
                        SMAVE_STATUS_OK, "dense capability") || !available) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_DAE, &available),
                        SMAVE_STATUS_OK, "DAE capability") || !available) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_NONLINEAR, &available),
                        SMAVE_STATUS_OK, "nonlinear capability") || !available) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_ODE, &available),
                        SMAVE_STATUS_OK, "ODE capability") || !available) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_EVENTS, &available),
                        SMAVE_STATUS_OK, "event capability") || !available) return 1;
    if (!require_status(smave_library_has_capability(
                            library, SMAVE_CAPABILITY_COMPLEMENTARITY, &available),
                        SMAVE_STATUS_OK, "complementarity capability") || !available) return 1;
    if (!solve_and_check(library, descriptor)) return 1;
    if (!check_concurrent_solve(library, descriptor)) {
        fprintf(stderr, "concurrent solve verification failed\n");
        return 1;
    }
    descriptor.storage = SMAVE_MATRIX_CSR;
    descriptor.dense_values = NULL;
    descriptor.row_offsets = row_offsets;
    descriptor.column_indices = columns;
    descriptor.sparse_values = sparse;
    descriptor.nonzeros = 7;
    if (!solve_and_check(library, descriptor)) return 1;
    if (!check_singular_rejection(library)) {
        fprintf(stderr, "singular rejection verification failed\n");
        return 1;
    }
    if (!check_nonlinear_fallback(library)) {
        fprintf(stderr, "nonlinear fallback verification failed\n");
        return 1;
    }
    if (!check_ode(library)) {
        fprintf(stderr, "ODE solve verification failed\n");
        return 1;
    }
    if (!check_event_ode(library)) {
        fprintf(stderr, "event ODE solve verification failed\n");
        return 1;
    }
    if (!check_dae(library)) {
        fprintf(stderr, "DAE solve verification failed\n");
        return 1;
    }

    descriptor.abi_version = SMAVE_ABI_VERSION + 1;
    {
        smave_problem* invalid = NULL;
        if (!require_status(smave_linear_problem_create(library, &descriptor, &invalid),
                            SMAVE_STATUS_ABI_MISMATCH, "reject ABI mismatch")) return 1;
    }
    if (!require_status(smave_library_destroy(library), SMAVE_STATUS_OK,
                        "destroy library")) return 1;
    if (atomic_load(&allocations) != atomic_load(&deallocations)) {
        fprintf(stderr, "allocator imbalance: %zu allocations, %zu deallocations\n",
                atomic_load(&allocations), atomic_load(&deallocations));
        return 1;
    }
    printf("SMAVE_C_API_HOST 6\nSUCCESS 1\nDENSE 1\nCSR 1\nNONLINEAR 1\nODE 1\nDAE 1\nEVENTS 1\nCOMPLEMENTARITY_CAPABILITY 1\n"
           "NONLINEAR_FALLBACK 1\nNONLINEAR_SHARED_SERVICE 1\nDAE_SHARED_SERVICE 1\nEVENT_SHARED_SERVICE 1\nCONCURRENT 1\n"
           "SINGULAR_REJECTED 1\nSTABLE_DIAGNOSTIC_CODES 1\n"
           "ABI_MISMATCH_REJECTED 1\nALLOCATOR_BALANCED 1\n"
           "ALLOCATIONS %zu\n", atomic_load(&allocations));
    return 0;
}
