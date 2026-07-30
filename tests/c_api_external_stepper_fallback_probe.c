#include "smave/c_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct stepper_context {
    int mode;
    size_t calls;
    int saw_fresh_buffers;
    int saw_differential_mask;
    smave_cancel_token* request_token;
} stepper_context;

static int ode_rhs(
    size_t dimension, double time, const double* state,
    double* derivative, void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = state[0];
    return 0;
}

static int ode_stepper(
    size_t dimension,
    double from_time,
    const double* previous_state,
    double to_time,
    double* quarter_state,
    double* midpoint_state,
    double* three_quarter_state,
    double* next_state,
    void* user_data) {
    stepper_context* context = (stepper_context*)user_data;
    if (dimension != 1 || context == NULL) return 1;
    ++context->calls;
    context->saw_fresh_buffers =
        quarter_state[0] == previous_state[0] &&
        midpoint_state[0] == previous_state[0] &&
        three_quarter_state[0] == previous_state[0] &&
        next_state[0] == previous_state[0];
    if (context->mode == 1) return 1;
    const double step = to_time - from_time;
    if (context->mode == 2) {
        quarter_state[0] = previous_state[0];
        midpoint_state[0] = previous_state[0];
        three_quarter_state[0] = previous_state[0];
        next_state[0] = previous_state[0];
    } else {
        quarter_state[0] = previous_state[0] * exp(0.25 * step);
        midpoint_state[0] = previous_state[0] * exp(0.5 * step);
        three_quarter_state[0] = previous_state[0] * exp(0.75 * step);
        next_state[0] = previous_state[0] * exp(step);
    }
    if (context->mode == 3 && context->request_token != NULL) {
        return smave_cancel_token_request(context->request_token) == SMAVE_STATUS_OK
            ? 0 : 1;
    }
    return 0;
}

static int dae_residual(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* residual,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    residual[0] = derivative[0] - state[0] * state[0];
    return 0;
}

static int failing_dae_jacobian(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double derivative_scale,
    double* jacobian,
    void* user_data) {
    (void)dimension;
    (void)time;
    (void)state;
    (void)derivative;
    (void)derivative_scale;
    (void)jacobian;
    (void)user_data;
    return 1;
}

static int dae_stepper(
    size_t dimension,
    const uint8_t* differential_mask,
    double from_time,
    const double* previous_state,
    const double* previous_derivative,
    double to_time,
    double* next_state,
    double* next_derivative,
    void* user_data) {
    stepper_context* context = (stepper_context*)user_data;
    if (dimension != 1 || differential_mask == NULL || context == NULL) return 1;
    ++context->calls;
    context->saw_fresh_buffers =
        next_state[0] == previous_state[0] &&
        next_derivative[0] == previous_derivative[0];
    context->saw_differential_mask = differential_mask[0] == 1;
    if (context->mode == 1) return 1;
    const double step = to_time - from_time;
    if (context->mode == 2) {
        next_state[0] = previous_state[0];
        next_derivative[0] = previous_derivative[0];
    } else {
        const double discriminant = 1.0 - 4.0 * step * previous_state[0];
        if (!(discriminant >= 0.0)) return 1;
        next_state[0] = (1.0 - sqrt(discriminant)) / (2.0 * step);
        next_derivative[0] = (next_state[0] - previous_state[0]) / step;
    }
    if (context->mode == 3 && context->request_token != NULL) {
        return smave_cancel_token_request(context->request_token) == SMAVE_STATUS_OK
            ? 0 : 1;
    }
    return 0;
}

static int diagnostic_is(
    smave_result* result, smave_diagnostic_code expected) {
    smave_diagnostic_code actual = SMAVE_DIAGNOSTIC_SUCCESS;
    return result != NULL &&
        smave_result_get_diagnostic_code(result, &actual) == SMAVE_STATUS_OK &&
        actual == expected;
}

static int check_ode(
    smave_library* library,
    smave_cancel_token* token,
    smave_solver** returned_solver,
    smave_problem** returned_problem,
    smave_ode_dense_step_fallback_desc* returned_fallback) {
    static const double initial[] = {1.0};
    smave_ode_problem_desc problem_desc = {
        sizeof(problem_desc), SMAVE_ABI_VERSION, 1, initial,
        0.0, 1.0, 1.0, ode_rhs, NULL};
    smave_solver_options strict_options = {
        sizeof(strict_options), SMAVE_ABI_VERSION, 3.0e-5, 0.0, 1};
    smave_solver_options loose_options = {
        sizeof(loose_options), SMAVE_ABI_VERSION, 1.0, 0.0, 100};
    stepper_context context = {0, 0, 0, 0, token};
    smave_ode_dense_step_fallback_desc fallback = {
        sizeof(fallback), SMAVE_ABI_VERSION, ode_stepper, &context};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_solver* loose_solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    double solution[1] = {0.0};
    size_t required = 0;
    if (smave_ode_problem_create(library, &problem_desc, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &strict_options, &solver) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &loose_options, &loose_solver) != SMAVE_STATUS_OK) {
        return 0;
    }

    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 1 || !context.saw_fresh_buffers ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK ||
        !info.success || !info.used_fallback || info.backend == NULL ||
        strcmp(info.backend, "caller-ode-dense-stepper-fallback-v1") != 0 ||
        smave_result_copy_solution(result, solution, 1, &required) != SMAVE_STATUS_OK ||
        required != 1 || fabs(solution[0] - exp(1.0)) > 1.0e-12 ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 0;
    result = NULL;
    if (smave_solver_solve_ode_with_fallback(
            loose_solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 0 || smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 2;
    result = NULL;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 1;
    result = NULL;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_CALLBACK_FAILURE) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 0;
    result = NULL;
    if (smave_cancel_token_request(token) != SMAVE_STATUS_OK ||
        smave_solver_solve_ode_with_fallback(
            solver, &fallback, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_CANCELLED ||
        context.calls != 0 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_CANCELLED) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK ||
        smave_cancel_token_reset(token) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 3;
    result = NULL;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_CANCELLED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_CANCELLED) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK ||
        smave_cancel_token_reset(token) != SMAVE_STATUS_OK) return 0;

    context.mode = 0;
    fallback.abi_version = SMAVE_ABI_VERSION + 1;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_ABI_MISMATCH) return 0;
    fallback.abi_version = SMAVE_ABI_VERSION;
    fallback.struct_size = sizeof(fallback) - 1;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;
    fallback.struct_size = sizeof(fallback);
    fallback.step = NULL;
    if (smave_solver_solve_ode_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;
    fallback.step = ode_stepper;

    if (smave_solver_destroy(loose_solver) != SMAVE_STATUS_OK) return 0;
    *returned_solver = solver;
    *returned_fallback = fallback;
    returned_fallback->user_data = NULL;
    *returned_problem = problem;
    return smave_problem_destroy(problem) == SMAVE_STATUS_INVALID_STATE;
}

static int check_dae(
    smave_library* library,
    smave_cancel_token* token,
    smave_solver* ode_solver,
    const smave_ode_dense_step_fallback_desc* ode_fallback) {
    static const uint8_t mask[] = {1};
    static const double initial_state[] = {1.0};
    static const double initial_derivative[] = {1.0};
    smave_dae_problem_desc problem_desc = {
        sizeof(problem_desc), SMAVE_ABI_VERSION, 1, mask,
        initial_state, initial_derivative, 0.0, 0.1, 0.1,
        dae_residual, failing_dae_jacobian, NULL};
    smave_solver_options strict_options = {
        sizeof(strict_options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-10, 1};
    smave_solver_options loose_options = {
        sizeof(loose_options), SMAVE_ABI_VERSION, 1.0e-8, 1.0e-8, 100};
    stepper_context context = {0, 0, 0, 0, token};
    smave_dae_step_fallback_desc fallback = {
        sizeof(fallback), SMAVE_ABI_VERSION, dae_stepper, &context};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_solver* loose_solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    if (smave_dae_problem_create(library, &problem_desc, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &strict_options, &solver) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &loose_options, &loose_solver) != SMAVE_STATUS_OK) {
        return 0;
    }

    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 1 || !context.saw_fresh_buffers ||
        !context.saw_differential_mask ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK ||
        !info.success || !info.used_fallback || info.backend == NULL ||
        strcmp(info.backend, "caller-dae-stepper-fallback-v1") != 0 ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 0;
    result = NULL;
    if (smave_solver_solve_dae_with_fallback(
            loose_solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_OK ||
        context.calls != 0 || smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 2;
    result = NULL;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 1;
    result = NULL;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_SOLVE_FAILED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_CALLBACK_FAILURE) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK) return 0;

    context.calls = 0;
    context.mode = 3;
    result = NULL;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, token, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_CANCELLED ||
        context.calls != 1 ||
        !diagnostic_is(result, SMAVE_DIAGNOSTIC_CANCELLED) ||
        smave_result_destroy(result) != SMAVE_STATUS_OK ||
        smave_cancel_token_reset(token) != SMAVE_STATUS_OK) return 0;

    if (smave_solver_solve_ode_with_fallback(
            solver, ode_fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_UNSUPPORTED ||
        smave_solver_solve_dae_with_fallback(
            ode_solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
            SMAVE_STATUS_UNSUPPORTED) return 0;

    fallback.abi_version = SMAVE_ABI_VERSION + 1;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_ABI_MISMATCH) return 0;
    fallback.abi_version = SMAVE_ABI_VERSION;
    fallback.struct_size = sizeof(fallback) - 1;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;
    fallback.struct_size = sizeof(fallback);
    fallback.step = NULL;
    if (smave_solver_solve_dae_with_fallback(
            solver, &fallback, NULL, SMAVE_TIMEOUT_INFINITE, &result) !=
        SMAVE_STATUS_INVALID_ARGUMENT) return 0;

    return smave_solver_destroy(loose_solver) == SMAVE_STATUS_OK &&
        smave_solver_destroy(solver) == SMAVE_STATUS_OK &&
        smave_problem_destroy(problem) == SMAVE_STATUS_OK;
}

int main(void) {
    smave_library* library = NULL;
    smave_cancel_token* token = NULL;
    smave_solver* ode_solver = NULL;
    smave_problem* ode_problem = NULL;
    smave_ode_dense_step_fallback_desc ode_fallback;
    int32_t ode_available = 0;
    int32_t dae_available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_cancel_token_create(library, &token) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_EXTERNAL_ODE_STEPPER_FALLBACK,
            &ode_available) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_EXTERNAL_DAE_STEPPER_FALLBACK,
            &dae_available) != SMAVE_STATUS_OK ||
        !ode_available || !dae_available ||
        !check_ode(library, token, &ode_solver, &ode_problem, &ode_fallback) ||
        !check_dae(library, token, ode_solver, &ode_fallback) ||
        smave_solver_destroy(ode_solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(ode_problem) != SMAVE_STATUS_OK ||
        smave_cancel_token_destroy(token) != SMAVE_STATUS_OK ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) {
        return 1;
    }
    printf("SMAVE_C_API_EXTERNAL_STEPPER_FALLBACK 1\n"
           "EXTERNAL_ODE_STEPPER_CAPABILITY 1\n"
           "EXTERNAL_ODE_STEPPER_AFTER_BUILTINS 1\n"
           "EXTERNAL_ODE_STEPPER_DENSE_OUTPUT_GATE 1\n"
           "EXTERNAL_DAE_STEPPER_CAPABILITY 1\n"
           "EXTERNAL_DAE_STEPPER_AFTER_BUILTINS 1\n"
           "EXTERNAL_DAE_STEPPER_KINEMATIC_RESIDUAL_GATE 1\n"
           "EXTERNAL_STEPPER_FRESH_BUFFERS 1\n"
           "EXTERNAL_STEPPER_CALLBACK_FAILURE 1\n"
           "EXTERNAL_STEPPER_CONTROL_BOUNDARIES 1\n"
           "EXTERNAL_STEPPER_NEGATIVE_CONTRACTS 1\n"
           "END\n");
    return 0;
}
