#include "smave/c_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct callback_state {
    int primary_calls;
    int fallback_calls;
    int gate_calls;
} callback_state;

static int bad_primary(
    size_t input_count, const double* inputs, size_t output_count, double* outputs,
    double time, void* user_data) {
    callback_state* state = (callback_state*)user_data;
    (void)time;
    ++state->primary_calls;
    if (input_count != 1 || output_count != 1) return 1;
    outputs[0] = inputs[0] * 3.0 + 0.25;
    return 0;
}

static int exact_fallback(
    size_t input_count, const double* inputs, size_t output_count, double* outputs,
    double time, void* user_data) {
    callback_state* state = (callback_state*)user_data;
    (void)time;
    ++state->fallback_calls;
    if (input_count != 1 || output_count != 1) return 1;
    outputs[0] = inputs[0] * 3.0;
    return 0;
}

static int original_gate(
    size_t input_count, const double* inputs, size_t output_count, const double* outputs,
    double time, double* residual, void* user_data) {
    callback_state* state = (callback_state*)user_data;
    (void)time;
    ++state->gate_calls;
    if (input_count != 1 || output_count != 1) return 1;
    *residual = fabs(outputs[0] - inputs[0] * 3.0);
    return 0;
}

typedef struct extended_block_graph_desc {
    smave_block_graph_desc base;
    unsigned long long future_field;
} extended_block_graph_desc;

typedef struct extended_block_graph_info {
    smave_block_graph_result_info base;
    unsigned long long future_field;
} extended_block_graph_info;

static int run_graph(smave_library* library, callback_state* callback) {
    double sum_weights[] = {1.0, 1.0};
    smave_block_node_desc nodes[] = {
        {sizeof(nodes[0]), SMAVE_ABI_VERSION, SMAVE_BLOCK_CONSTANT, 0, 0, 1,
         0.1, 0.0, 1.0, 0.0, NULL, NULL, NULL, NULL, NULL},
        {sizeof(nodes[1]), SMAVE_ABI_VERSION, SMAVE_BLOCK_UNIT_DELAY, 0, 1, 1,
         0.1, 0.0, 0.0, 0.0, NULL, NULL, NULL, NULL, NULL},
        {sizeof(nodes[2]), SMAVE_ABI_VERSION, SMAVE_BLOCK_SUM, 0, 2, 1,
         0.1, 0.0, 0.0, 0.0, sum_weights, NULL, NULL, NULL, NULL},
        {sizeof(nodes[3]), SMAVE_ABI_VERSION, SMAVE_BLOCK_CALLBACK, 0, 1, 1,
         0.2, 0.1, 0.0, -3.0, NULL, bad_primary, exact_fallback, original_gate, callback},
        {sizeof(nodes[4]), SMAVE_ABI_VERSION, SMAVE_BLOCK_UNIT_DELAY, 0, 1, 1,
         0.2, 0.1, 0.0, -6.0, NULL, NULL, NULL, NULL, NULL}};
    smave_block_connection_desc connections[] = {
        {sizeof(connections[0]), SMAVE_ABI_VERSION, 0, 0, 2, 0},
        {sizeof(connections[1]), SMAVE_ABI_VERSION, 1, 0, 2, 1},
        {sizeof(connections[2]), SMAVE_ABI_VERSION, 2, 0, 1, 0},
        {sizeof(connections[3]), SMAVE_ABI_VERSION, 2, 0, 3, 0},
        {sizeof(connections[4]), SMAVE_ABI_VERSION, 3, 0, 4, 0}};
    extended_block_graph_desc descriptor = {
        {sizeof(descriptor), SMAVE_ABI_VERSION, nodes, 5, connections, 5, 0.4, 0.1},
        0x1122334455667788ULL};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_result_info info = {sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    extended_block_graph_info graph_info = {0};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 100};
    double outputs[5] = {0};
    size_t offsets[6] = {0};
    size_t order[5] = {0};
    size_t required = 0;
    const char* service = NULL;
    const char* plan = NULL;
    const char* family = NULL;
    graph_info.base.struct_size = sizeof(graph_info);
    graph_info.base.abi_version = SMAVE_ABI_VERSION;
    graph_info.future_field = 0x8877665544332211ULL;
    int success =
        smave_block_graph_problem_create(library, &descriptor.base, &problem) == SMAVE_STATUS_OK;
    nodes[0].parameter = NAN;
    connections[0].source_node = 99;
    sum_weights[0] = NAN;
    success = success && descriptor.future_field == 0x1122334455667788ULL &&
        callback->primary_calls == 0 && callback->fallback_calls == 0 && callback->gate_calls == 0 &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        info.used_fallback && info.dimension == 5 &&
        smave_result_get_block_graph_info(result, &graph_info.base) == SMAVE_STATUS_OK &&
        graph_info.future_field == 0x8877665544332211ULL &&
        graph_info.base.final_time == 0.4 && graph_info.base.ticks == 5 &&
        graph_info.base.node_executions == 19 && graph_info.base.fallback_count == 2 &&
        graph_info.base.node_count == 5 &&
        graph_info.base.maximum_original_gate_residual == 0.0 &&
        graph_info.base.fixed_point_components == 0 &&
        graph_info.base.fixed_point_iterations == 0 &&
        smave_result_get_provenance(result, &service, &plan, &family) == SMAVE_STATUS_OK &&
        strcmp(service, "smave.verified-block-graph-solve.v1") == 0 &&
        strcmp(family, "scalar-multiphysics-block-graph") == 0 && plan[0] != '\0' &&
        smave_result_copy_solution(result, outputs, 5, &required) == SMAVE_STATUS_OK &&
        required == 5 && outputs[0] == 1.0 && outputs[1] == 4.0 && outputs[2] == 5.0 &&
        outputs[3] == 12.0 && outputs[4] == 6.0 &&
        smave_result_copy_block_output_offsets(result, offsets, 6, &required) == SMAVE_STATUS_OK &&
        required == 6 && offsets[0] == 0 && offsets[5] == 5 &&
        smave_result_copy_block_commit_order(result, order, 5, &required) == SMAVE_STATUS_OK &&
        required == 5 && order[0] == 0 && order[1] == 1 && order[2] == 2 &&
        order[3] == 3 && order[4] == 4 && callback->primary_calls == 2 &&
        callback->fallback_calls == 2 && callback->gate_calls == 4;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

static int feedback_graphs(smave_library* library) {
    double convergent_weights[] = {0.5, 1.0};
    smave_block_node_desc nodes[] = {
        {sizeof(nodes[0]), SMAVE_ABI_VERSION, SMAVE_BLOCK_CONSTANT, 0, 0, 1,
         0.1, 0.0, 1.0, 0.0, NULL, NULL, NULL, NULL, NULL},
        {sizeof(nodes[1]), SMAVE_ABI_VERSION, SMAVE_BLOCK_SUM, 0, 2, 1,
         0.1, 0.0, 0.0, 0.0, convergent_weights, NULL, NULL, NULL, NULL},
        {sizeof(nodes[2]), SMAVE_ABI_VERSION, SMAVE_BLOCK_GAIN, 0, 1, 1,
         0.1, 0.0, 0.5, 0.0, NULL, NULL, NULL, NULL, NULL}};
    smave_block_connection_desc cycle[] = {
        {sizeof(cycle[0]), SMAVE_ABI_VERSION, 2, 0, 1, 0},
        {sizeof(cycle[1]), SMAVE_ABI_VERSION, 0, 0, 1, 1},
        {sizeof(cycle[2]), SMAVE_ABI_VERSION, 1, 0, 2, 0}};
    smave_block_graph_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, nodes, 3, cycle, 3, 0.0, 0.1};
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 200};
    smave_block_graph_result_info info = {0};
    smave_diagnostic_code diagnostic = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    double outputs[3] = {0};
    size_t required = 0;
    info.struct_size = sizeof(info);
    info.abi_version = SMAVE_ABI_VERSION;
    if (smave_block_graph_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK ||
        smave_result_get_block_graph_info(result, &info) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(result, outputs, 3, &required) != SMAVE_STATUS_OK ||
        required != 3 || fabs(outputs[0] - 1.0) > 1.0e-10 ||
        fabs(outputs[1] - 4.0 / 3.0) > 1.0e-8 ||
        fabs(outputs[2] - 2.0 / 3.0) > 1.0e-8 ||
        info.fixed_point_components != 1 || info.fixed_point_iterations == 0 ||
        info.maximum_fixed_point_residual > 2.0e-10) return 0;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    result = NULL;
    solver = NULL;
    problem = NULL;

    convergent_weights[0] = 1.0;
    nodes[2].parameter = 1.0;
    options.maximum_iterations = 8;
    if (smave_block_graph_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_SOLVE_FAILED || result == NULL ||
        smave_result_get_diagnostic_code(result, &diagnostic) != SMAVE_STATUS_OK ||
        diagnostic != SMAVE_DIAGNOSTIC_ITERATION_LIMIT) return 0;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    result = NULL;
    solver = NULL;
    problem = NULL;

    nodes[2].sample_time = 0.2;
    if (smave_block_graph_problem_create(library, &descriptor, &problem) !=
            SMAVE_STATUS_INVALID_ARGUMENT || problem != NULL) return 0;
    nodes[2].sample_time = 0.1;
    cycle[1].target_node = 8;
    if (smave_block_graph_problem_create(library, &descriptor, &problem) !=
            SMAVE_STATUS_INVALID_ARGUMENT || problem != NULL) return 0;
    cycle[1].target_node = 1;
    nodes[0].abi_version = SMAVE_ABI_VERSION + 1;
    if (smave_block_graph_problem_create(library, &descriptor, &problem) !=
            SMAVE_STATUS_ABI_MISMATCH || problem != NULL) return 0;
    return 1;
}

int main(void) {
    smave_library* library = NULL;
    callback_state callback = {0, 0, 0};
    int32_t capability = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_MULTIPHYSICS, &capability) != SMAVE_STATUS_OK ||
        capability != 1 || !run_graph(library, &callback) || !feedback_graphs(library) ||
        smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("SMAVE_C_API_BLOCK_GRAPH_SERVICE 1\n"
           "BLOCK_GRAPH_CAPABILITY 1\n"
           "BLOCK_GRAPH_MULTIRATE_ZERO_ORDER_HOLD 1\n"
           "BLOCK_GRAPH_DETERMINISTIC_COMMIT_ORDER 1\n"
           "BLOCK_GRAPH_CALLBACK_ORIGINAL_GATE 1\n"
           "BLOCK_GRAPH_LOCAL_FALLBACK 1\n"
           "BLOCK_GRAPH_INPUTS_COPIED 1\n"
           "BLOCK_GRAPH_ALGEBRAIC_FIXED_POINT 1\n"
           "BLOCK_GRAPH_DIVERGENT_FEEDBACK_REJECTED 1\n"
           "BLOCK_GRAPH_MIXED_RATE_FEEDBACK_REJECTED 1\n"
           "BLOCK_GRAPH_INVALID_CONNECTION_REJECTED 1\n"
           "BLOCK_GRAPH_ABI_MISMATCH_REJECTED 1\n"
           "BLOCK_GRAPH_EXTENDED_STRUCT_TAIL_PRESERVED 1\n");
    return 0;
}
