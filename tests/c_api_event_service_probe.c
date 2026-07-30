#include "smave/c_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = 1.0;
    return 0;
}

static int guard(
    size_t dimension, double time, const double* state, double* value,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    *value = state[0] - 0.5;
    return 0;
}

static int high_priority_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)pre_state;
    (void)user_data;
    if (dimension != 1) return 1;
    post_state[0] = -0.25;
    return 0;
}

static int low_priority_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    post_state[0] = 2.0 * pre_state[0];
    return 0;
}

static int stuck_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)pre_state;
    (void)user_data;
    if (dimension != 1) return 1;
    post_state[0] = 0.5;
    return 0;
}

static int falling_rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)state;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = -1.0;
    return 0;
}

static int falling_guard(
    size_t dimension, double time, const double* state, double* value,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    *value = state[0] - 0.5;
    return 0;
}

static int falling_reset(
    size_t dimension, double time, const double* pre_state, double* post_state,
    void* user_data) {
    (void)time;
    (void)pre_state;
    (void)user_data;
    if (dimension != 1) return 1;
    post_state[0] = 0.75;
    return 0;
}

static int solve_valid(smave_library* library) {
    const double initial[] = {0.0};
    const smave_event_desc events[] = {
        {sizeof(events[0]), SMAVE_ABI_VERSION, 1, 1, guard, low_priority_reset, NULL},
        {sizeof(events[1]), SMAVE_ABI_VERSION, 1, 10, guard, high_priority_reset, NULL},
    };
    smave_event_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 1.0, 0.2, rhs, NULL, events, 2};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 10000};
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
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        strcmp(service_id, "smave.verified-explicit-ode-solve.v1") == 0 &&
        strcmp(equation_family, "explicit-ode-with-events") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' &&
        smave_result_get_ode_info(result, &ode_info) == SMAVE_STATUS_OK &&
        ode_info.event_count == 2 && fabs(ode_info.last_event_time - 0.5) < 1.0e-8 &&
        fabs(ode_info.final_time - 1.0) < 1.0e-12 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        required == 1 && fabs(solution[0]) < 1.0e-8;
    if (success) {
        printf("SMAVE_C_API_EVENT_SERVICE 1\nservice_id=\"%s\"\n"
               "equation_family=\"%s\"\nbackend=\"%s\"\nplan_id=\"%s\"\n"
               "event_count=%zu\nlast_event_time=%.17g\nsolution=%.17g\n",
               service_id, equation_family, info.backend, plan_id,
               ode_info.event_count, ode_info.last_event_time, solution[0]);
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int reject_stuck_reset(smave_library* library) {
    const double initial[] = {0.0};
    const smave_event_desc event = {
        sizeof(event), SMAVE_ABI_VERSION, 1, 0, guard, stuck_reset, NULL};
    smave_event_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 1.0, 0.2, rhs, NULL, &event, 1};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    int rejected = smave_event_ode_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_SOLVE_FAILED;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return rejected;
}

static int solve_falling_direction(smave_library* library) {
    const double initial[] = {1.0};
    const smave_event_desc event = {
        sizeof(event), SMAVE_ABI_VERSION, -1, 0,
        falling_guard, falling_reset, NULL};
    smave_event_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 0.6, 0.2, falling_rhs, NULL, &event, 1};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_ode_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    double solution[1];
    size_t required = 0;
    int success = smave_event_ode_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, NULL, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_ode_info(result, &info) == SMAVE_STATUS_OK &&
        info.event_count == 1 && fabs(info.last_event_time - 0.5) < 1.0e-8 &&
        smave_result_copy_solution(result, solution, 1, &required) == SMAVE_STATUS_OK &&
        fabs(solution[0] - 0.65) < 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

int main(void) {
    smave_library* library = NULL;
    int32_t available = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(library, SMAVE_CAPABILITY_EVENTS, &available) !=
            SMAVE_STATUS_OK || !available ||
        !solve_valid(library) || !solve_falling_direction(library) ||
        !reject_stuck_reset(library) ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("EVENT_DIRECTION 1\nEVENT_RISING_DIRECTION 1\nEVENT_FALLING_DIRECTION 1\nEVENT_PRIORITY_ORDER 1\n"
           "EVENT_ATOMIC_RESET 1\nEVENT_STUCK_RESET_REJECTED 1\nEND\n");
    return 0;
}
