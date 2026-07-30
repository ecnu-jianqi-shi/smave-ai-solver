#include "smave/c_api.h"

#include <math.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct ModeContext {
    double slope;
    double algebraic_factor;
    int impossible;
} ModeContext;

typedef struct TransitionContext {
    double guard_offset;
    double post_x;
    double post_z;
    int fail;
} TransitionContext;

typedef struct ThreadContext {
    smave_solver* solver;
    uint64_t expected_x;
    uint64_t expected_z;
    uint64_t expected_time;
    size_t expected_event_count;
    size_t expected_mode;
    size_t expected_projection_count;
    int ok;
} ThreadContext;

typedef struct StableDaeContext {
    double guard_offset;
    double expected_stable_x;
    double expected_current_x;
    int action;
} StableDaeContext;

static uint64_t bits(double value) {
    uint64_t result = 0;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static int32_t mode_residual(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* residual,
    void* user_data) {
    (void)time;
    const ModeContext* context = (const ModeContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    residual[0] = derivative[0] - context->slope;
    residual[1] = context->impossible
        ? 1.0 : state[1] - context->algebraic_factor * state[0];
    return 0;
}

static int32_t mode_jacobian(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double derivative_scale,
    double* jacobian,
    void* user_data) {
    (void)time;
    (void)state;
    (void)derivative;
    const ModeContext* context = (const ModeContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    jacobian[0] = derivative_scale;
    jacobian[1] = 0.0;
    jacobian[2] = -context->algebraic_factor;
    jacobian[3] = 1.0;
    return 0;
}

static int32_t transition_guard(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* guard,
    void* user_data) {
    (void)time;
    (void)derivative;
    const TransitionContext* context = (const TransitionContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    *guard = state[0] - context->guard_offset;
    return 0;
}

static int32_t transition_reset(
    size_t dimension,
    double time,
    const double* pre_state,
    const double* pre_derivative,
    double* post_state,
    double* post_derivative,
    void* user_data) {
    (void)time;
    (void)pre_state;
    (void)pre_derivative;
    const TransitionContext* context = (const TransitionContext*)user_data;
    if (dimension != 2 || context == NULL || context->fail) return 1;
    post_state[0] = context->post_x;
    post_state[1] = context->post_z;
    post_derivative[0] = 0.0;
    post_derivative[1] = 0.0;
    return 0;
}

static int32_t stable_dae_guard(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* guard,
    void* user_data) {
    (void)time;
    (void)derivative;
    const StableDaeContext* context = (const StableDaeContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    *guard = state[0] - context->guard_offset;
    return 0;
}

static int32_t stable_dae_reset(
    size_t dimension,
    double time,
    const double* stable_pre_state,
    const double* stable_pre_derivative,
    const double* current_state,
    const double* current_derivative,
    double* proposed_state,
    double* proposed_derivative,
    void* user_data) {
    (void)time;
    const StableDaeContext* context = (const StableDaeContext*)user_data;
    if (dimension != 2 || context == NULL ||
        fabs(stable_pre_state[0] - context->expected_stable_x) > 1.0e-7 ||
        fabs(stable_pre_derivative[0] - 1.0) > 1.0e-7 ||
        fabs(current_state[0] - context->expected_current_x) > 1.0e-7) return 1;
    proposed_state[0] = current_state[0];
    proposed_state[1] = current_state[1];
    proposed_derivative[0] = current_derivative[0];
    proposed_derivative[1] = current_derivative[1];
    if (context->action == 0) proposed_derivative[0] = 9.0;
    if (context->action == 1) proposed_state[1] = 9.0;
    if (context->action == 2) {
        proposed_state[0] = 0.25;
        proposed_state[1] = 9.0;
    }
    return 0;
}

static int create_problem(
    smave_library* library,
    int impossible,
    smave_problem** problem) {
    static const uint8_t differential_mask[] = {1, 0};
    static const double initial_state[] = {0.0, 0.0};
    static const double initial_derivative[] = {1.0, 0.0};
    static ModeContext mode_contexts[] = {
        {1.0, 1.0, 0}, {0.0, 2.0, 0}, {0.0, 3.0, 0}};
    static ModeContext impossible_mode_contexts[] = {
        {1.0, 1.0, 0}, {0.0, 2.0, 0}, {0.0, 3.0, 1}};
    static TransitionContext transitions_context[] = {
        {0.5, 1.0, 9.0, 0}, {0.9, 0.25, 9.0, 0}};
    ModeContext* selected_modes = impossible
        ? impossible_mode_contexts : mode_contexts;
    smave_hybrid_dae_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &selected_modes[0]},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &selected_modes[1]},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &selected_modes[2]}};
    smave_hybrid_dae_transition_desc transitions[] = {
        {offsetof(smave_hybrid_dae_transition_desc, stable_reset),
         SMAVE_ABI_VERSION, 0, 1, 1, 10,
         transition_guard, transition_reset, &transitions_context[0],
         stable_dae_reset, NULL, NULL},
        {offsetof(smave_hybrid_dae_transition_desc, stable_reset),
         SMAVE_ABI_VERSION, 1, 2, 1, 10,
         transition_guard, transition_reset, &transitions_context[1],
         stable_dae_reset, NULL, NULL}};
    smave_hybrid_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative, 0, 0.0, 1.0, 0.2,
        modes, 3, transitions, 2};
    return smave_hybrid_dae_problem_create(library, &descriptor, problem) ==
        SMAVE_STATUS_OK;
}

static int query_success(
    smave_result* result,
    double* x,
    double* z,
    smave_hybrid_dae_result_info* hybrid_info) {
    double solution[2] = {0.0, 0.0};
    size_t required = 0;
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    if (smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_SUCCESS ||
        smave_result_get_hybrid_dae_info(result, hybrid_info) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(result, solution, 2, &required) != SMAVE_STATUS_OK ||
        required != 2) return 0;
    *x = solution[0];
    *z = solution[1];
    return 1;
}

static void* solve_thread(void* argument) {
    ThreadContext* context = (ThreadContext*)argument;
    context->ok = 1;
    for (int iteration = 0; iteration < 16; ++iteration) {
        smave_result* result = NULL;
        smave_hybrid_dae_result_info info = {
            sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
        double x = 0.0;
        double z = 0.0;
        if (smave_solver_solve(context->solver, &result) != SMAVE_STATUS_OK ||
            !query_success(result, &x, &z, &info) ||
            bits(x) != context->expected_x || bits(z) != context->expected_z ||
            bits(info.last_event_time) != context->expected_time ||
            info.event_count != context->expected_event_count ||
            info.final_mode != context->expected_mode ||
            info.consistency_projection_count !=
                context->expected_projection_count) context->ok = 0;
        smave_result_destroy(result);
        if (!context->ok) break;
    }
    return NULL;
}

static int solve_stable_dae_batch(smave_library* library) {
    const uint8_t differential_mask[] = {1, 0};
    const uint8_t no_write[] = {0, 0};
    const uint8_t write_x_derivative[] = {1, 0};
    const uint8_t write_z_state[] = {0, 1};
    const uint8_t write_both_state[] = {1, 1};
    const double initial_state[] = {0.0, 0.0};
    const double initial_derivative[] = {1.0, 0.0};
    ModeContext mode_contexts[] = {
        {1.0, 1.0, 0}, {0.0, 2.0, 0}, {0.0, 3.0, 0}};
    smave_hybrid_dae_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[0]},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[1]},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[2]}};
    StableDaeContext derivative_reset = {0.5, 0.5, 0.5, 0};
    StableDaeContext algebraic_reset = {0.5, 0.5, 0.5, 1};
    StableDaeContext cascade_reset = {0.4, 0.5, 0.5, 2};
    smave_hybrid_dae_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 20,
         stable_dae_guard, NULL, &derivative_reset,
         stable_dae_reset, no_write, write_x_derivative},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 0, 1, 1, 10,
         stable_dae_guard, NULL, &algebraic_reset,
         stable_dae_reset, write_z_state, no_write},
        {sizeof(transitions[2]), SMAVE_ABI_VERSION, 1, 2, 1, 10,
         stable_dae_guard, NULL, &cascade_reset,
         stable_dae_reset, write_both_state, no_write}};
    smave_hybrid_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative, 0, 0.0, 1.0, 0.2,
        modes, 3, transitions, 3};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_dae_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
    double solution[2] = {0.0, 0.0};
    size_t required = 0;
    int ok = smave_hybrid_dae_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_hybrid_dae_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 3 && info.final_mode == 2 &&
        info.consistency_projection_count == 2 &&
        fabs(info.last_event_time - 0.5) < 1.0e-7 &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && fabs(solution[0] - 0.25) < 1.0e-9 &&
        fabs(solution[1] - 0.75) < 1.0e-9;
    pthread_t threads[8];
    ThreadContext contexts[8];
    if (ok) {
        for (int index = 0; index < 8; ++index) {
            contexts[index] = (ThreadContext){
                solver,
                bits(solution[0]),
                bits(solution[1]),
                bits(info.last_event_time),
                3,
                2,
                2,
                0};
            if (pthread_create(&threads[index], NULL, solve_thread, &contexts[index]) != 0) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            for (int index = 0; index < 8; ++index) {
                if (pthread_join(threads[index], NULL) != 0 || !contexts[index].ok) {
                    ok = 0;
                }
            }
        }
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_stable_dae_conflict(smave_library* library, int conflict_kind) {
    const uint8_t differential_mask[] = {1, 0};
    const uint8_t no_write[] = {0, 0};
    const uint8_t write_x_derivative[] = {1, 0};
    const uint8_t write_z_state[] = {0, 1};
    const double initial_state[] = {0.0, 0.0};
    const double initial_derivative[] = {1.0, 0.0};
    ModeContext mode_contexts[] = {
        {1.0, 1.0, 0}, {0.0, 2.0, 0}, {0.0, 3.0, 0}};
    smave_hybrid_dae_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[0]},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[1]},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, mode_residual, mode_jacobian,
         &mode_contexts[2]}};
    StableDaeContext first = {0.5, 0.5, 0.5, conflict_kind == 0 ? 1 : 0};
    StableDaeContext second = {0.5, 0.5, 0.5, conflict_kind == 1 ? 0 : 1};
    const uint8_t* first_state_mask = conflict_kind == 0 ? write_z_state : no_write;
    const uint8_t* first_derivative_mask =
        conflict_kind == 0 ? no_write : write_x_derivative;
    const uint8_t* second_state_mask =
        conflict_kind == 1 ? no_write : write_z_state;
    const uint8_t* second_derivative_mask =
        conflict_kind == 1 ? write_x_derivative : no_write;
    smave_hybrid_dae_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 20,
         stable_dae_guard, NULL, &first,
         stable_dae_reset, first_state_mask, first_derivative_mask},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 0,
         conflict_kind == 2 ? 2u : 1u, 1, 10,
         stable_dae_guard, NULL, &second,
         stable_dae_reset, second_state_mask, second_derivative_mask}};
    smave_hybrid_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative, 0, 0.0, 1.0, 0.2,
        modes, 3, transitions, 2};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_dae_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double solution[2] = {-1.0, -1.0};
    size_t required = 0;
    int ok = smave_hybrid_dae_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT &&
        smave_result_get_hybrid_dae_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 0 && info.final_mode == 0 &&
        info.consistency_projection_count == 0 && info.final_time < 0.5 &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && fabs(solution[0] - info.final_time) < 1.0e-12 &&
        fabs(solution[1] - solution[0]) < 1.0e-12;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

int main(void) {
    smave_library* library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    int32_t available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_HYBRID_DAE, &available) != SMAVE_STATUS_OK ||
        !available || !create_problem(library, 0, &problem) ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK) return 1;

    smave_hybrid_dae_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
    double x = 0.0;
    double z = 0.0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* family = NULL;
    if (!query_success(result, &x, &z, &info) ||
        smave_result_get_provenance(
            result, &service_id, &plan_id, &family) != SMAVE_STATUS_OK ||
        strcmp(service_id, "smave.verified-fully-implicit-hybrid-dae-solve.v1") != 0 ||
        strcmp(family, "dae-fully-implicit-hybrid-multimode") != 0 ||
        info.event_count != 2 || info.final_mode != 2 ||
        info.consistency_projection_count != 2 ||
        fabs(info.last_event_time - 0.5) > 1.0e-7 ||
        fabs(x - 0.25) > 1.0e-9 || fabs(z - 0.75) > 1.0e-9) return 1;
    const uint64_t expected_x = bits(x);
    const uint64_t expected_z = bits(z);
    const uint64_t expected_time = bits(info.last_event_time);
    smave_result_destroy(result);
    result = NULL;

    pthread_t threads[8];
    ThreadContext contexts[8];
    for (int index = 0; index < 8; ++index) {
        contexts[index] = (ThreadContext){
            solver, expected_x, expected_z, expected_time, 2, 2, 2, 0};
        if (pthread_create(&threads[index], NULL, solve_thread, &contexts[index]) != 0) {
            return 1;
        }
    }
    for (int index = 0; index < 8; ++index) {
        if (pthread_join(threads[index], NULL) != 0 || !contexts[index].ok) return 1;
    }
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);

    if (!create_problem(library, 1, &problem) ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_SOLVE_FAILED) return 1;
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double rollback[2] = {-1.0, -1.0};
    size_t required = 0;
    info = (smave_hybrid_dae_result_info){
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
    if (smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED ||
        smave_result_get_hybrid_dae_info(result, &info) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(result, rollback, 2, &required) != SMAVE_STATUS_OK ||
        required != 2 || info.event_count != 0 || info.final_mode != 0 ||
        info.consistency_projection_count != 0 ||
        info.final_time >= 0.5 || fabs(rollback[0] - info.final_time) > 1.0e-12 ||
        fabs(rollback[1] - rollback[0]) > 1.0e-12) return 1;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);

    if (!solve_stable_dae_batch(library) ||
        !solve_stable_dae_conflict(library, 0) ||
        !solve_stable_dae_conflict(library, 1) ||
        !solve_stable_dae_conflict(library, 2)) return 1;
    if (smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;

    printf("SMAVE_C_API_HYBRID_DAE_SERVICE 1\n");
    printf("service_id=\"smave.verified-fully-implicit-hybrid-dae-solve.v1\"\n");
    printf("equation_family=\"dae-fully-implicit-hybrid-multimode\"\n");
    printf("HYBRID_DAE_MODE_RESIDUALS 1\n");
    printf("HYBRID_DAE_TARGET_CONSISTENCY_GATE 1\n");
    printf("HYBRID_DAE_AUTOMATIC_CONSISTENCY_PROJECTION 1\n");
    printf("HYBRID_DAE_UNPROJECTABLE_REJECTED 1\n");
    printf("HYBRID_DAE_SUPERDENSE_CASCADE 1\n");
    printf("HYBRID_DAE_TRANSACTION_ROLLBACK 1\n");
    printf("HYBRID_DAE_PROJECTION_METADATA_ATOMIC 1\n");
    printf("HYBRID_DAE_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1\n");
    printf("HYBRID_DAE_STABLE_PRE_ACROSS_MICROSTEPS 1\n");
    printf("HYBRID_DAE_STATE_DERIVATIVE_WRITESET_MERGE 1\n");
    printf("HYBRID_DAE_WRITESET_CONFLICTS_REJECTED 1\n");
    printf("HYBRID_DAE_TARGET_MODE_CONFLICT_REJECTED 1\n");
    printf("HYBRID_DAE_STABLE_SHARED_SOLVER_DETERMINISTIC 1\n");
    printf("HYBRID_DAE_CONCURRENT_BITWISE_DETERMINISTIC 1\nEND\n");
    return 0;
}
