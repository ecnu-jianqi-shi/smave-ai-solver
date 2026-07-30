#include "smave/c_api.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct TransitionContext {
    double threshold;
    double reset_value;
    int fail;
} TransitionContext;

typedef struct StableTransitionContext {
    double guard_offset;
    double expected_stable_x;
    double expected_current_x;
    size_t write_index;
    double value;
    int use_stable_plus_current;
} StableTransitionContext;

static int32_t increasing_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = 1.0;
    return 0;
}

static int32_t decreasing_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = -1.0;
    return 0;
}

static int32_t zero_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = 0.0;
    return 0;
}

static int32_t two_state_increasing_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 2) return 1;
    derivative[0] = 1.0;
    derivative[1] = 0.0;
    return 0;
}

static int32_t two_state_zero_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 2) return 1;
    derivative[0] = 0.0;
    derivative[1] = 0.0;
    return 0;
}

static int32_t transition_guard(
    size_t dimension, double time, const double* state, double* guard,
    void* user_data) {
    (void)time;
    TransitionContext* context = (TransitionContext*)user_data;
    if (dimension != 1 || context == NULL) return 1;
    *guard = state[0] - context->threshold;
    return 0;
}

static int32_t transition_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)pre_state;
    TransitionContext* context = (TransitionContext*)user_data;
    if (dimension != 1 || context == NULL || context->fail) return 1;
    post_state[0] = context->reset_value;
    return 0;
}

static int32_t stable_transition_guard(
    size_t dimension, double time, const double* state, double* guard,
    void* user_data) {
    (void)time;
    const StableTransitionContext* context =
        (const StableTransitionContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    *guard = state[0] - context->guard_offset;
    return 0;
}

static int32_t stable_transition_reset(
    size_t dimension,
    double time,
    const double* stable_pre_state,
    const double* current_state,
    double* proposed_state,
    void* user_data) {
    (void)time;
    const StableTransitionContext* context =
        (const StableTransitionContext*)user_data;
    if (dimension != 2 || context == NULL ||
        fabs(stable_pre_state[0] - context->expected_stable_x) > 1.0e-7 ||
        fabs(current_state[0] - context->expected_current_x) > 1.0e-7 ||
        context->write_index >= dimension) return 1;
    proposed_state[0] = current_state[0];
    proposed_state[1] = current_state[1];
    proposed_state[context->write_index] = context->use_stable_plus_current
        ? stable_pre_state[0] + current_state[0] : context->value;
    return 0;
}

static int solve_valid(smave_library* library) {
    const double initial_state[] = {0.0};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, decreasing_rhs, NULL}};
    TransitionContext rising = {0.5, 1.0, 0};
    TransitionContext falling = {0.75, 0.0, 0};
    const size_t legacy_transition_size =
        offsetof(smave_hybrid_transition_desc, stable_reset);
    const smave_hybrid_transition_desc transitions[] = {
        {legacy_transition_size, SMAVE_ABI_VERSION, 0, 1, 1, 10,
         transition_guard, transition_reset, &rising,
         stable_transition_reset, NULL},
        {legacy_transition_size, SMAVE_ABI_VERSION, 1, 0, -1, 10,
         transition_guard, transition_reset, &falling,
         stable_transition_reset, NULL}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial_state, 0,
        0.0, 1.0, 0.2, modes, 2, transitions, 2};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 10000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_hybrid_result_info hybrid_info = {
        sizeof(hybrid_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[1] = {0.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_SUCCESS &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        strcmp(service_id, "smave.verified-explicit-hybrid-solve.v1") == 0 &&
        strcmp(equation_family, "explicit-hybrid-multimode") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' &&
        smave_result_get_hybrid_info(result, &hybrid_info) == SMAVE_STATUS_OK &&
        hybrid_info.event_count == 2 && hybrid_info.final_mode == 0 &&
        fabs(hybrid_info.last_event_time - 0.75) < 1.0e-7 &&
        fabs(hybrid_info.final_time - 1.0) < 1.0e-12 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && fabs(solution[0] - 0.25) < 1.0e-6;
    if (ok) {
        printf("SMAVE_C_API_HYBRID_SERVICE 1\nservice_id=\"%s\"\n"
               "equation_family=\"%s\"\nplan_id=\"%s\"\n"
               "event_count=%zu\nlast_event_time=%.17g\nfinal_mode=%zu\nsolution=%.17g\n",
               service_id, equation_family, plan_id, hybrid_info.event_count,
               hybrid_info.last_event_time, hybrid_info.final_mode, solution[0]);
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_conflict(smave_library* library) {
    const double initial_state[] = {0.0};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, decreasing_rhs, NULL}};
    TransitionContext first = {0.5, 1.0, 0};
    TransitionContext second = {0.5, 2.0, 0};
    const smave_hybrid_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 10,
         transition_guard, transition_reset, &first, NULL, NULL},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 0, 1, 1, 1,
         transition_guard, transition_reset, &second, NULL, NULL}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial_state, 0,
        0.0, 1.0, 0.2, modes, 2, transitions, 2};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_result_info hybrid_info = {
        sizeof(hybrid_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double solution[1] = {-1.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_NUMERICAL_FAILURE &&
        smave_result_get_hybrid_info(result, &hybrid_info) == SMAVE_STATUS_OK &&
        hybrid_info.event_count == 0 && hybrid_info.final_mode == 0 &&
        hybrid_info.final_time < 0.5 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && fabs(solution[0] - hybrid_info.final_time) < 1.0e-12;
    if (!ok && result != NULL) {
        smave_result_info info = {
            sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
        smave_result_get_info(result, &info);
        smave_result_get_diagnostic_code(result, &diagnostic);
        smave_result_get_hybrid_info(result, &hybrid_info);
        smave_result_copy_solution(result, solution, 1, &required);
        fprintf(stderr,
                "hybrid conflict mismatch code=%d time=%.17g events=%zu mode=%zu "
                "solution=%.17g diagnostic=%s\n",
                (int)diagnostic, hybrid_info.final_time, hybrid_info.event_count,
                hybrid_info.final_mode, solution[0],
                info.diagnostic != NULL ? info.diagnostic : "<null>");
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_superdense_cascade(smave_library* library) {
    const double initial_state[] = {0.0};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, zero_rhs, NULL},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, zero_rhs, NULL}};
    TransitionContext first = {0.5, 1.0, 0};
    TransitionContext second = {0.9, 0.25, 0};
    const smave_hybrid_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 10,
         transition_guard, transition_reset, &first, NULL, NULL},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 1, 2, 1, 10,
         transition_guard, transition_reset, &second, NULL, NULL}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial_state, 0,
        0.0, 1.0, 0.2, modes, 3, transitions, 2};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    double solution[1] = {0.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_hybrid_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 2 && info.final_mode == 2 &&
        fabs(info.last_event_time - 0.5) < 1.0e-7 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && fabs(solution[0] - 0.25) < 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_superdense_cycle(smave_library* library) {
    const double initial_state[] = {0.0};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, zero_rhs, NULL}};
    TransitionContext first = {0.5, 1.0, 0};
    TransitionContext second = {0.9, 1.0, 0};
    const smave_hybrid_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 10,
         transition_guard, transition_reset, &first, NULL, NULL},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 1, 0, 1, 10,
         transition_guard, transition_reset, &second, NULL, NULL}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial_state, 0,
        0.0, 1.0, 0.2, modes, 2, transitions, 2};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double solution[1] = {-1.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_NUMERICAL_FAILURE &&
        smave_result_get_hybrid_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 0 && info.final_mode == 0 && info.final_time < 0.5 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && fabs(solution[0] - info.final_time) < 1.0e-12;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_stable_pre_batch(smave_library* library) {
    const double initial_state[] = {0.0, 0.0};
    const uint8_t write_x[] = {1, 0};
    const uint8_t write_y[] = {0, 1};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, two_state_increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, two_state_zero_rhs, NULL},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, two_state_zero_rhs, NULL}};
    StableTransitionContext write_first = {0.5, 0.5, 0.5, 0, 1.5, 0};
    StableTransitionContext write_second = {0.5, 0.5, 0.5, 1, 2.5, 0};
    StableTransitionContext cascade = {1.0, 0.5, 1.5, 1, 0.0, 1};
    const smave_hybrid_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 20,
         stable_transition_guard, NULL, &write_first,
         stable_transition_reset, write_x},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 0, 1, 1, 10,
         stable_transition_guard, NULL, &write_second,
         stable_transition_reset, write_y},
        {sizeof(transitions[2]), SMAVE_ABI_VERSION, 1, 2, 1, 10,
         stable_transition_guard, NULL, &cascade,
         stable_transition_reset, write_y}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, initial_state, 0,
        0.0, 1.0, 0.2, modes, 3, transitions, 3};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    double solution[2] = {0.0, 0.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_hybrid_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 3 && info.final_mode == 2 &&
        fabs(info.last_event_time - 0.5) < 1.0e-7 &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && fabs(solution[0] - 1.5) < 1.0e-9 &&
        fabs(solution[1] - 2.0) < 1.0e-9;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

static int solve_write_conflict(smave_library* library, int target_conflict) {
    const double initial_state[] = {0.0, 0.0};
    const uint8_t write_x[] = {1, 0};
    const uint8_t write_y[] = {0, 1};
    const smave_hybrid_mode_desc modes[] = {
        {sizeof(modes[0]), SMAVE_ABI_VERSION, two_state_increasing_rhs, NULL},
        {sizeof(modes[1]), SMAVE_ABI_VERSION, two_state_zero_rhs, NULL},
        {sizeof(modes[2]), SMAVE_ABI_VERSION, two_state_zero_rhs, NULL}};
    StableTransitionContext first = {0.5, 0.5, 0.5, 0, 1.0, 0};
    StableTransitionContext second = {
        0.5, 0.5, 0.5, target_conflict ? 1u : 0u, 2.0, 0};
    const smave_hybrid_transition_desc transitions[] = {
        {sizeof(transitions[0]), SMAVE_ABI_VERSION, 0, 1, 1, 20,
         stable_transition_guard, NULL, &first,
         stable_transition_reset, write_x},
        {sizeof(transitions[1]), SMAVE_ABI_VERSION, 0,
         target_conflict ? 2u : 1u, 1, 10,
         stable_transition_guard, NULL, &second,
         stable_transition_reset, target_conflict ? write_y : write_x}};
    smave_hybrid_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, initial_state, 0,
        0.0, 1.0, 0.2, modes, 3, transitions, 2};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_hybrid_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0, 0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double solution[2] = {-1.0, -1.0};
    size_t required = 0;
    int ok = smave_hybrid_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED &&
        smave_result_get_diagnostic_code(result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT &&
        smave_result_get_hybrid_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 0 && info.final_mode == 0 && info.final_time < 0.5 &&
        smave_result_copy_solution(result, solution, 2, &required) == SMAVE_STATUS_OK &&
        required == 2 && fabs(solution[0] - info.final_time) < 1.0e-12 &&
        solution[1] == 0.0;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

int main(void) {
    smave_library* library = NULL;
    int32_t available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(library, SMAVE_CAPABILITY_HYBRID, &available) !=
            SMAVE_STATUS_OK || !available ||
        !solve_valid(library) || !solve_conflict(library) ||
        !solve_superdense_cascade(library) || !solve_superdense_cycle(library) ||
        !solve_stable_pre_batch(library) || !solve_write_conflict(library, 0) ||
        !solve_write_conflict(library, 1) ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("HYBRID_MODE_SPECIFIC_RHS 1\n");
    printf("HYBRID_TWO_MODE_SWITCHES 1\n");
    printf("HYBRID_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1\n");
    printf("HYBRID_PRIORITY_TRANSACTION_ROLLBACK 1\n");
    printf("HYBRID_SUPERDENSE_CASCADE 1\n");
    printf("HYBRID_SUPERDENSE_CYCLE_ROLLBACK 1\n");
    printf("HYBRID_STABLE_PRE_ACROSS_MICROSTEPS 1\n");
    printf("HYBRID_DISJOINT_WRITESET_ATOMIC_MERGE 1\n");
    printf("HYBRID_OVERLAPPING_WRITESET_REJECTED 1\n");
    printf("HYBRID_TARGET_MODE_CONFLICT_REJECTED 1\n");
    printf("HYBRID_STABLE_DIAGNOSTIC_CODES 1\nEND\n");
    return 0;
}
