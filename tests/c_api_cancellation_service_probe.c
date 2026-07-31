#include "smave/c_api.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static atomic_size_t allocations;
static atomic_size_t deallocations;
static atomic_size_t callback_count;

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

static int slow_evaluate(
    size_t input_count,
    const double* inputs,
    size_t output_count,
    double* outputs,
    double time,
    void* user_data) {
    (void)input_count;
    (void)inputs;
    (void)user_data;
    if (output_count != 1) return 1;
    const struct timespec pause = {0, 1000000};
    nanosleep(&pause, NULL);
    outputs[0] = time;
    atomic_fetch_add(&callback_count, 1);
    return 0;
}

static int original_gate(
    size_t input_count,
    const double* inputs,
    size_t output_count,
    const double* outputs,
    double time,
    double* residual,
    void* user_data) {
    (void)input_count;
    (void)inputs;
    (void)user_data;
    if (output_count != 1) return 1;
    *residual = outputs[0] >= time ? outputs[0] - time : time - outputs[0];
    return 0;
}

typedef struct solve_context {
    const smave_solver* solver;
    const smave_cancel_token* token;
    smave_result* result;
    smave_status status;
} solve_context;

static void* run_solve(void* argument) {
    solve_context* context = (solve_context*)argument;
    context->status = smave_solver_solve_cancellable(
        context->solver, context->token, &context->result);
    return NULL;
}

int main(void) {
    const smave_library_options library_options = {
        sizeof(library_options), SMAVE_ABI_VERSION,
        tracked_allocate, tracked_deallocate, NULL};
    const smave_block_node_desc node = {
        sizeof(node), SMAVE_ABI_VERSION, SMAVE_BLOCK_CALLBACK, 0,
        0, 1, 0.001, 0.0, 0.0, -1.0, NULL,
        slow_evaluate, NULL, original_gate, NULL};
    const smave_block_graph_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, &node, 1, NULL, 0, 0.02, 0.001};
    const smave_solver_options solver_options = {
        sizeof(solver_options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 2000};
    smave_library* library = NULL;
    smave_library* foreign_library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_cancel_token* token = NULL;
    smave_cancel_token* foreign_token = NULL;
    int32_t cancellation_available = 0;
    int32_t deadline_available = 0;
    pthread_t thread;
    solve_context context = {0};
    const int created =
        smave_library_create(&library_options, &library) == SMAVE_STATUS_OK &&
        smave_library_create(NULL, &foreign_library) == SMAVE_STATUS_OK &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_CANCELLATION, &cancellation_available) ==
            SMAVE_STATUS_OK && cancellation_available &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_DEADLINE, &deadline_available) ==
            SMAVE_STATUS_OK && deadline_available &&
        smave_block_graph_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &solver_options, &solver) == SMAVE_STATUS_OK &&
        smave_cancel_token_create(library, &token) == SMAVE_STATUS_OK &&
        smave_cancel_token_create(foreign_library, &foreign_token) == SMAVE_STATUS_OK;
    if (!created) { fprintf(stderr, "DIAG cancellation: setup failed at line 110\n"); return 1; }

    smave_result* foreign_result = NULL;
    if (smave_solver_solve_cancellable(solver, foreign_token, &foreign_result) !=
            SMAVE_STATUS_INVALID_ARGUMENT || foreign_result != NULL) { fprintf(stderr, "DIAG cancellation: foreign token rejection failed at line 114\n"); return 1; }

    context.solver = solver;
    context.token = token;
    if (pthread_create(&thread, NULL, run_solve, &context) != 0) { fprintf(stderr, "DIAG cancellation: pthread_create failed at line 118\n"); return 1; }
    while (atomic_load(&callback_count) < 4) {
        const struct timespec pause = {0, 100000};
        nanosleep(&pause, NULL);
    }
    const int active_lifecycle_rejected =
        smave_cancel_token_reset(token) == SMAVE_STATUS_INVALID_STATE &&
        smave_cancel_token_destroy(token) == SMAVE_STATUS_INVALID_STATE;
    if (smave_cancel_token_request(token) != SMAVE_STATUS_OK ||
        pthread_join(thread, NULL) != 0) { fprintf(stderr, "DIAG cancellation: cancel request/join failed at line 127\n"); return 1; }

    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_block_graph_result_info graph_info = {0};
    graph_info.struct_size = sizeof(graph_info);
    graph_info.abi_version = SMAVE_ABI_VERSION;
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    double output = -2.0;
    size_t required = 0;
    const int cancelled =
        context.status == SMAVE_STATUS_CANCELLED && context.result != NULL &&
        smave_result_get_info(context.result, &info) == SMAVE_STATUS_OK && !info.success &&
        smave_result_get_diagnostic_code(context.result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_CANCELLED &&
        smave_result_get_block_graph_info(context.result, &graph_info) == SMAVE_STATUS_OK &&
        graph_info.ticks > 0 && graph_info.ticks < 21 &&
        smave_result_copy_solution(context.result, &output, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && output == graph_info.final_time;
    smave_result_destroy(context.result);
    if (!cancelled || !active_lifecycle_rejected) { fprintf(stderr, "DIAG cancellation: cancelled=%d active_lifecycle_rejected=%d at line 147\n", cancelled, active_lifecycle_rejected); return 1; }

    smave_result* pre_cancelled_result = NULL;
    const int sticky =
        smave_solver_solve_cancellable(solver, token, &pre_cancelled_result) ==
            SMAVE_STATUS_CANCELLED && pre_cancelled_result != NULL;
    smave_result_destroy(pre_cancelled_result);
    if (!sticky || smave_cancel_token_reset(token) != SMAVE_STATUS_OK) { fprintf(stderr, "DIAG cancellation: sticky=%d at line 154\n", sticky); return 1; }

    smave_result* deadline_result = NULL;
    smave_block_graph_result_info deadline_info = {0};
    deadline_info.struct_size = sizeof(deadline_info);
    deadline_info.abi_version = SMAVE_ABI_VERSION;
    diagnostic = SMAVE_DIAGNOSTIC_SUCCESS;
    output = -2.0;
    required = 0;
    const smave_status deadline_status =
        smave_solver_solve_with_timeout(solver, token, 5000000, &deadline_result);
    int deadline_expired = 0;
    if (deadline_status != SMAVE_STATUS_DEADLINE_EXCEEDED) {
        fprintf(stderr, "DIAG cancellation: deadline status=%d (expected %d), result=%p\n",
                (int)deadline_status, (int)SMAVE_STATUS_DEADLINE_EXCEEDED, (void*)deadline_result);
    } else if (deadline_result == NULL) {
        fprintf(stderr, "DIAG cancellation: deadline result null\n");
    } else if (smave_result_get_info(deadline_result, &info) != SMAVE_STATUS_OK || info.success) {
        fprintf(stderr, "DIAG cancellation: deadline info ok=%d success=%d\n",
                smave_result_get_info(deadline_result, &info) == SMAVE_STATUS_OK, info.success);
    } else if (smave_result_get_diagnostic_code(deadline_result, &diagnostic) != SMAVE_STATUS_OK ||
               diagnostic != SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED) {
        fprintf(stderr, "DIAG cancellation: deadline diag=%d\n", (int)diagnostic);
    } else if (smave_result_get_block_graph_info(deadline_result, &deadline_info) != SMAVE_STATUS_OK) {
        fprintf(stderr, "DIAG cancellation: deadline graph info failed\n");
    } else if (deadline_info.ticks >= 21) {
        fprintf(stderr, "DIAG cancellation: deadline ticks=%zu (expected <21)\n", deadline_info.ticks);
    } else if (smave_result_copy_solution(deadline_result, &output, 1, &required) != SMAVE_STATUS_OK ||
               required != 1 || output != deadline_info.final_time) {
        fprintf(stderr, "DIAG cancellation: deadline copy req=%zu output=%.17g final_time=%.17g\n",
                required, output, deadline_info.final_time);
    } else {
        deadline_expired = 1;
    }
    smave_result_destroy(deadline_result);
    if (!deadline_expired) { fprintf(stderr, "DIAG cancellation: deadline_expired=0 at line 174\n"); return 1; }

    const size_t callbacks_before_reuse = atomic_load(&callback_count);
    smave_result* completed_result = NULL;
    smave_block_graph_result_info completed_info = {0};
    completed_info.struct_size = sizeof(completed_info);
    completed_info.abi_version = SMAVE_ABI_VERSION;
    const int reused =
        smave_solver_solve_with_timeout(
            solver, token, SMAVE_TIMEOUT_INFINITE, &completed_result) ==
            SMAVE_STATUS_OK &&
        smave_result_get_block_graph_info(completed_result, &completed_info) == SMAVE_STATUS_OK &&
        completed_info.ticks == 21 && completed_info.final_time == 0.02 &&
        atomic_load(&callback_count) >= callbacks_before_reuse + 21;
    smave_result_destroy(completed_result);
    if (!reused) { fprintf(stderr, "DIAG cancellation: reused=0 at line 189\n"); return 1; }

    if (smave_cancel_token_destroy(token) != SMAVE_STATUS_OK ||
        smave_solver_destroy(solver) != SMAVE_STATUS_OK ||
        smave_problem_destroy(problem) != SMAVE_STATUS_OK ||
        smave_cancel_token_destroy(foreign_token) != SMAVE_STATUS_OK ||
        smave_library_destroy(foreign_library) != SMAVE_STATUS_OK ||
        smave_library_destroy(library) != SMAVE_STATUS_OK ||
        atomic_load(&allocations) != atomic_load(&deallocations)) { fprintf(stderr, "DIAG cancellation: alloc=%zu dealloc=%zu at line 197\n", atomic_load(&allocations), atomic_load(&deallocations)); return 1; }

    printf("SMAVE_C_API_CANCELLATION_SERVICE 1\n"
           "CANCELLATION_CAPABILITY 1\n"
           "CANCELLATION_CROSS_THREAD_REQUEST 1\n"
           "CANCELLATION_STABLE_STATUS_DIAGNOSTIC 1\n"
           "CANCELLATION_ATOMIC_COMMIT_BOUNDARY 1\n"
           "CANCELLATION_TOKEN_STICKY_RESET_REUSE 1\n"
           "CANCELLATION_ACTIVE_LIFECYCLE_REJECTED 1\n"
           "CANCELLATION_FOREIGN_LIBRARY_REJECTED 1\n"
           "DEADLINE_CAPABILITY 1\n"
           "DEADLINE_STABLE_STATUS_DIAGNOSTIC 1\n"
           "DEADLINE_ATOMIC_COMMIT_BOUNDARY 1\n"
           "DEADLINE_TOKEN_UNCHANGED_UNLIMITED_REUSE 1\n"
           "CANCELLATION_ALLOCATOR_BALANCED 1\nEND\n");
    return 0;
}
