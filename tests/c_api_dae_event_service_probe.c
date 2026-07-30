#include "smave/c_api.h"

#include <math.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct EventContext {
    int mode;
} EventContext;

static int reset_order = 0;
static int high_seen = 0;
static _Atomic size_t allocations = 0;
static _Atomic size_t deallocations = 0;

enum {
    MODE_VALID_HIGH = 1,
    MODE_VALID_LOW = 2,
    MODE_INCONSISTENT = 3,
    MODE_ALGEBRAIC_DERIVATIVE = 4,
    MODE_STUCK = 5,
    MODE_CALLBACK_FAILURE = 6,
    MODE_STATELESS = 7
};

static void* tracked_allocate(size_t size, void* user_data) {
    (void)user_data;
    void* memory = malloc(size);
    if (memory != NULL) atomic_fetch_add(&allocations, 1);
    return memory;
}

static void tracked_deallocate(void* memory, void* user_data) {
    (void)user_data;
    if (memory != NULL) atomic_fetch_add(&deallocations, 1);
    free(memory);
}

static int32_t residual(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* output,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 2) return 1;
    output[0] = derivative[0] + state[1];
    output[1] = state[1] - state[0];
    return 0;
}

static int32_t jacobian(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double derivative_scale,
    double* output,
    void* user_data) {
    (void)time;
    (void)state;
    (void)derivative;
    (void)user_data;
    if (dimension != 2) return 1;
    output[0] = derivative_scale;
    output[1] = 1.0;
    output[2] = -1.0;
    output[3] = 1.0;
    return 0;
}

static int32_t guard(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* value,
    void* user_data) {
    (void)time;
    (void)derivative;
    (void)user_data;
    if (dimension != 2) return 1;
    *value = state[0] - 0.75;
    return 0;
}

static int32_t reset(
    size_t dimension,
    double time,
    const double* pre_state,
    const double* pre_derivative,
    double* post_state,
    double* post_derivative,
    void* user_data) {
    (void)time;
    (void)pre_derivative;
    EventContext* context = (EventContext*)user_data;
    if (dimension != 2 || context == NULL) return 1;
    if (context->mode == MODE_CALLBACK_FAILURE) return 1;
    if (context->mode == MODE_VALID_LOW) {
        if (!high_seen || fabs(pre_state[0] - 1.0) > 1.0e-9) return 1;
        reset_order = reset_order * 10 + 2;
    } else if (context->mode == MODE_VALID_HIGH) {
        high_seen = 1;
        reset_order = reset_order * 10 + 1;
    }
    post_state[0] = context->mode == MODE_STUCK ? 0.75 : 1.0;
    post_state[1] = context->mode == MODE_INCONSISTENT ? 0.9 : post_state[0];
    post_derivative[0] = -post_state[1];
    post_derivative[1] = context->mode == MODE_ALGEBRAIC_DERIVATIVE ? 1.0 : 0.0;
    return 0;
}

static int solve_events(
    smave_library* library,
    smave_dae_event_desc* events,
    size_t event_count,
    int expect_success,
    size_t expected_events,
    smave_diagnostic_code expected_diagnostic,
    double* event_time,
    smave_result** kept_result) {
    const uint8_t differential_mask[] = {1, 0};
    const double initial_state[] = {1.0, 1.0};
    const double initial_derivative[] = {-1.0, 0.0};
    smave_event_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative, 0.0, 0.5, 0.5,
        residual, jacobian, NULL, events, event_count};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 1000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_dae_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0.0, 0.0, 0, 0, 0, 0.0, 0, 0, 0.0, 0.0};
    smave_diagnostic_code diagnostic_code = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    const smave_status solve_status =
        smave_event_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK
        ? smave_solver_solve(solver, &result)
        : SMAVE_STATUS_INTERNAL_ERROR;
    int ok = expect_success
        ? solve_status == SMAVE_STATUS_OK &&
            smave_result_get_diagnostic_code(result, &diagnostic_code) == SMAVE_STATUS_OK &&
            diagnostic_code == expected_diagnostic &&
            smave_result_get_dae_info(result, &info) == SMAVE_STATUS_OK &&
            info.event_count == expected_events &&
            info.maximum_residual_inf <= 1.0e-8 && fabs(info.final_time - 0.5) < 1.0e-12
        : solve_status == SMAVE_STATUS_SOLVE_FAILED &&
            smave_result_get_diagnostic_code(result, &diagnostic_code) == SMAVE_STATUS_OK &&
            diagnostic_code == expected_diagnostic;
    if (ok && expect_success && event_time != NULL) *event_time = info.last_event_time;
    if (ok && kept_result != NULL) {
        *kept_result = result;
        result = NULL;
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return ok;
}

typedef struct ThreadContext {
    smave_solver* solver;
    double expected_event_time;
    double expected_solution[2];
    int success;
} ThreadContext;

static void* solve_thread(void* raw_context) {
    ThreadContext* context = (ThreadContext*)raw_context;
    context->success = 1;
    for (int iteration = 0; iteration < 16; ++iteration) {
        smave_result* result = NULL;
        smave_dae_result_info info = {
            sizeof(info), SMAVE_ABI_VERSION, 0.0, 0.0, 0, 0, 0, 0.0, 0, 0, 0.0, 0.0};
        double solution[2] = {0.0, 0.0};
        size_t required = 0;
        if (smave_solver_solve(context->solver, &result) != SMAVE_STATUS_OK ||
            smave_result_get_dae_info(result, &info) != SMAVE_STATUS_OK ||
            smave_result_copy_solution(result, solution, 2, &required) != SMAVE_STATUS_OK ||
            required != 2 || info.event_count != 1 ||
            info.last_event_time != context->expected_event_time ||
            solution[0] != context->expected_solution[0] ||
            solution[1] != context->expected_solution[1] ||
            info.maximum_residual_inf > 1.0e-8 ||
            smave_result_destroy(result) != SMAVE_STATUS_OK) {
            smave_result_destroy(result);
            context->success = 0;
            return NULL;
        }
    }
    return NULL;
}

static int check_concurrent_event_solve(smave_library* library) {
    const uint8_t differential_mask[] = {1, 0};
    const double initial_state[] = {1.0, 1.0};
    const double initial_derivative[] = {-1.0, 0.0};
    EventContext event_context = {MODE_STATELESS};
    smave_dae_event_desc event = {
        sizeof(event), SMAVE_ABI_VERSION, -1, 0, guard, reset, &event_context};
    smave_event_dae_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 2, differential_mask,
        initial_state, initial_derivative, 0.0, 0.5, 0.5,
        residual, jacobian, NULL, &event, 1};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 1000};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    if (smave_event_dae_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options, &solver) != SMAVE_STATUS_OK) {
        smave_solver_destroy(solver);
        smave_problem_destroy(problem);
        return 0;
    }
    smave_result* baseline = NULL;
    smave_dae_result_info baseline_info = {
        sizeof(baseline_info), SMAVE_ABI_VERSION,
        0.0, 0.0, 0, 0, 0, 0.0, 0, 0, 0.0, 0.0};
    double baseline_solution[2] = {0.0, 0.0};
    size_t required = 0;
    if (smave_solver_solve(solver, &baseline) != SMAVE_STATUS_OK ||
        smave_result_get_dae_info(baseline, &baseline_info) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(baseline, baseline_solution, 2, &required) !=
            SMAVE_STATUS_OK ||
        required != 2 || baseline_info.event_count != 1 ||
        !(baseline_info.last_event_time > 0.3 && baseline_info.last_event_time < 0.4) ||
        smave_result_destroy(baseline) != SMAVE_STATUS_OK) {
        smave_result_destroy(baseline);
        smave_solver_destroy(solver);
        smave_problem_destroy(problem);
        return 0;
    }
    enum { THREAD_COUNT = 8 };
    pthread_t threads[THREAD_COUNT];
    ThreadContext contexts[THREAD_COUNT];
    int created = 0;
    for (; created < THREAD_COUNT; ++created) {
        contexts[created].solver = solver;
        contexts[created].expected_event_time = baseline_info.last_event_time;
        contexts[created].expected_solution[0] = baseline_solution[0];
        contexts[created].expected_solution[1] = baseline_solution[1];
        contexts[created].success = 0;
        if (pthread_create(&threads[created], NULL, solve_thread, &contexts[created]) != 0) break;
    }
    int ok = created == THREAD_COUNT;
    for (int index = 0; index < created; ++index) {
        if (pthread_join(threads[index], NULL) != 0 || !contexts[index].success) ok = 0;
    }
    if (smave_solver_destroy(solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(problem) != SMAVE_STATUS_OK) ok = 0;
    return ok;
}

int main(void) {
    smave_library* library = NULL;
    smave_library_options library_options = {
        sizeof(library_options), SMAVE_ABI_VERSION,
        tracked_allocate, tracked_deallocate, NULL};
    if (smave_library_create(&library_options, &library) != SMAVE_STATUS_OK) return 1;

    EventContext high = {MODE_VALID_HIGH};
    EventContext low = {MODE_VALID_LOW};
    smave_dae_event_desc simultaneous[] = {
        {sizeof(simultaneous[0]), SMAVE_ABI_VERSION, -1, 1, guard, reset, &low},
        {sizeof(simultaneous[1]), SMAVE_ABI_VERSION, -1, 10, guard, reset, &high}};
    double event_time = 0.0;
    smave_result* successful_result = NULL;
    if (!solve_events(
            library, simultaneous, 2, 1, 2, SMAVE_DIAGNOSTIC_SUCCESS,
            &event_time, &successful_result) ||
        reset_order != 12 || !high_seen ||
        !(event_time > 0.3 && event_time < 0.4)) return 1;

    struct LegacyEnvelope {
        struct {
            uint32_t struct_size;
            uint32_t abi_version;
            double final_time;
            double maximum_residual_inf;
            size_t accepted_steps;
            size_t rejected_steps;
        } prefix;
        uint64_t sentinel;
    } legacy = {{offsetof(smave_dae_result_info, event_count), SMAVE_ABI_VERSION, 0, 0, 0, 0},
                UINT64_C(0x4d41564544414531)};
    if (smave_result_get_dae_info(
            successful_result, (smave_dae_result_info*)&legacy.prefix) != SMAVE_STATUS_OK ||
        legacy.sentinel != UINT64_C(0x4d41564544414531)) return 1;
    smave_result_destroy(successful_result);

    reset_order = 0;
    high_seen = 0;
    EventContext wrong_direction = {MODE_VALID_HIGH};
    smave_dae_event_desc rising = {
        sizeof(rising), SMAVE_ABI_VERSION, 1, 0, guard, reset, &wrong_direction};
    if (!solve_events(
            library, &rising, 1, 1, 0, SMAVE_DIAGNOSTIC_SUCCESS, NULL, NULL) ||
        reset_order != 0) {
        return 1;
    }

    const int rejection_modes[] = {
        MODE_INCONSISTENT,
        MODE_ALGEBRAIC_DERIVATIVE,
        MODE_STUCK,
        MODE_CALLBACK_FAILURE};
    const smave_diagnostic_code rejection_codes[] = {
        SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED,
        SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED,
        SMAVE_DIAGNOSTIC_EVENT_GUARD_NOT_RELEASED,
        SMAVE_DIAGNOSTIC_EVENT_REINIT_CALLBACK_FAILURE};
    for (size_t index = 0; index < sizeof(rejection_modes) / sizeof(rejection_modes[0]); ++index) {
        EventContext context = {rejection_modes[index]};
        smave_dae_event_desc event = {
            sizeof(event), SMAVE_ABI_VERSION, -1, 0, guard, reset, &context};
        if (!solve_events(
                library, &event, 1, 0, 0, rejection_codes[index], NULL, NULL)) return 1;
    }

    if (!check_concurrent_event_solve(library)) return 1;

    if (smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    if (atomic_load(&allocations) != atomic_load(&deallocations)) return 1;
    printf("SMAVE_C_API_DAE_EVENT_SERVICE 1\n");
    printf("DAE_EVENT_IMPLICIT_ROOT 1\n");
    printf("DAE_EVENT_PRIORITY_ORDER 1\n");
    printf("DAE_EVENT_ATOMIC_REINIT 1\n");
    printf("DAE_EVENT_WRONG_DIRECTION_IGNORED 1\n");
    printf("DAE_EVENT_INCONSISTENT_REINIT_REJECTED 1\n");
    printf("DAE_EVENT_ALGEBRAIC_DERIVATIVE_REJECTED 1\n");
    printf("DAE_EVENT_STUCK_REINIT_REJECTED 1\n");
    printf("DAE_EVENT_CALLBACK_FAILURE_REJECTED 1\n");
    printf("DAE_EVENT_STABLE_DIAGNOSTIC_CODES 1\n");
    printf("DAE_LEGACY_RESULT_PREFIX_COMPATIBLE 1\n");
    printf("DAE_EVENT_CONCURRENT_SHARED_SOLVER 1\n");
    printf("DAE_EVENT_CONCURRENT_BITWISE_DETERMINISTIC 1\n");
    printf("DAE_EVENT_ALLOCATOR_BALANCED 1\n");
    printf("dae_event_allocations=%zu\n", atomic_load(&allocations));
    printf("event_count=2\nlast_event_time=%.17g\nEND\n", event_time);
    return 0;
}
