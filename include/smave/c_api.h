#ifndef SMAVE_C_API_H
#define SMAVE_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(SMAVE_C_API_BUILD)
#    define SMAVE_API __declspec(dllexport)
#  else
#    define SMAVE_API __declspec(dllimport)
#  endif
#else
#  define SMAVE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SMAVE_ABI_VERSION 1u
#define SMAVE_TIMEOUT_INFINITE UINT64_MAX
#define SMAVE_LINEAR_SYMMETRIC 1u
#define SMAVE_LINEAR_POSITIVE_DEFINITE 2u

typedef struct smave_library smave_library;
typedef struct smave_problem smave_problem;
typedef struct smave_solver smave_solver;
typedef struct smave_result smave_result;
typedef struct smave_cancel_token smave_cancel_token;

typedef enum smave_status {
    SMAVE_STATUS_OK = 0,
    SMAVE_STATUS_INVALID_ARGUMENT = 1,
    SMAVE_STATUS_ABI_MISMATCH = 2,
    SMAVE_STATUS_INVALID_STATE = 3,
    SMAVE_STATUS_UNSUPPORTED = 4,
    SMAVE_STATUS_SOLVE_FAILED = 5,
    SMAVE_STATUS_BUFFER_TOO_SMALL = 6,
    SMAVE_STATUS_INTERNAL_ERROR = 7,
    SMAVE_STATUS_CANCELLED = 8,
    SMAVE_STATUS_DEADLINE_EXCEEDED = 9
} smave_status;

typedef enum smave_capability {
    SMAVE_CAPABILITY_LINEAR_DENSE = 1,
    SMAVE_CAPABILITY_LINEAR_CSR = 2,
    SMAVE_CAPABILITY_NONLINEAR = 3,
    SMAVE_CAPABILITY_ODE = 4,
    SMAVE_CAPABILITY_DAE = 5,
    SMAVE_CAPABILITY_EVENTS = 6,
    SMAVE_CAPABILITY_COMPLEMENTARITY = 7,
    SMAVE_CAPABILITY_MULTIPHYSICS = 8,
    SMAVE_CAPABILITY_HYBRID = 9,
    SMAVE_CAPABILITY_HYBRID_DAE = 10,
    SMAVE_CAPABILITY_INDEX_TWO_DAE = 11,
    SMAVE_CAPABILITY_CANCELLATION = 12,
    SMAVE_CAPABILITY_DEADLINE = 13,
    SMAVE_CAPABILITY_EXTERNAL_LINEAR_FALLBACK = 14,
    SMAVE_CAPABILITY_EXTERNAL_NONLINEAR_FALLBACK = 15,
    SMAVE_CAPABILITY_EXTERNAL_ODE_STEPPER_FALLBACK = 16,
    SMAVE_CAPABILITY_EXTERNAL_DAE_STEPPER_FALLBACK = 17,
    SMAVE_CAPABILITY_ERROR_STACK = 18
} smave_capability;

typedef enum smave_matrix_storage {
    SMAVE_MATRIX_DENSE_ROW_MAJOR = 1,
    SMAVE_MATRIX_CSR = 2
} smave_matrix_storage;

typedef enum smave_block_node_kind {
    SMAVE_BLOCK_CONSTANT = 1,
    SMAVE_BLOCK_GAIN = 2,
    SMAVE_BLOCK_SUM = 3,
    SMAVE_BLOCK_UNIT_DELAY = 4,
    SMAVE_BLOCK_SWITCH_GT = 5,
    SMAVE_BLOCK_SWITCH_GE = 6,
    SMAVE_BLOCK_SWITCH_NE_ZERO = 7,
    SMAVE_BLOCK_CALLBACK = 8
} smave_block_node_kind;

typedef enum smave_diagnostic_code {
    SMAVE_DIAGNOSTIC_SUCCESS = 0,
    SMAVE_DIAGNOSTIC_INVALID_CONTRACT = 1,
    SMAVE_DIAGNOSTIC_CALLBACK_FAILURE = 2,
    SMAVE_DIAGNOSTIC_NUMERICAL_FAILURE = 3,
    SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED = 4,
    SMAVE_DIAGNOSTIC_ITERATION_LIMIT = 5,
    SMAVE_DIAGNOSTIC_EVENT_REINIT_CALLBACK_FAILURE = 6,
    SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED = 7,
    SMAVE_DIAGNOSTIC_EVENT_GUARD_NOT_RELEASED = 8,
    SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT = 9,
    SMAVE_DIAGNOSTIC_CANCELLED = 10,
    SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED = 11
} smave_diagnostic_code;

typedef struct smave_error_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t trace_id;
    smave_status status;
    const char* operation;
    const char* message;
} smave_error_info;

typedef void* (*smave_allocate_fn)(size_t size, void* user_data);
typedef void (*smave_deallocate_fn)(void* memory, void* user_data);

typedef struct smave_library_options {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_allocate_fn allocate;
    smave_deallocate_fn deallocate;
    void* allocator_user_data;
} smave_library_options;

typedef struct smave_linear_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t storage;
    uint32_t flags;
    size_t dimension;
    const double* dense_values;
    const size_t* row_offsets;
    const size_t* column_indices;
    const double* sparse_values;
    size_t nonzeros;
    const double* right_hand_side;
} smave_linear_problem_desc;

typedef struct smave_complementarity_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t storage;
    uint32_t flags;
    size_t dimension;
    const double* dense_matrix;
    const size_t* row_offsets;
    const size_t* column_indices;
    const double* sparse_values;
    size_t nonzeros;
    const double* offset;
    const double* initial_state;
} smave_complementarity_desc;

typedef int32_t (*smave_block_evaluate_fn)(
    size_t input_count,
    const double* inputs,
    size_t output_count,
    double* outputs,
    double time,
    void* user_data);

typedef int32_t (*smave_block_gate_fn)(
    size_t input_count,
    const double* inputs,
    size_t output_count,
    const double* outputs,
    double time,
    double* original_gate_residual,
    void* user_data);

typedef struct smave_block_node_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t kind;
    uint32_t reserved;
    size_t input_count;
    size_t output_count;
    double sample_time;
    double sample_offset;
    double parameter;
    double initial_output;
    const double* sum_weights;
    smave_block_evaluate_fn evaluate;
    smave_block_evaluate_fn local_fallback;
    smave_block_gate_fn gate;
    void* user_data;
} smave_block_node_desc;

typedef struct smave_block_connection_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t source_node;
    size_t source_port;
    size_t target_node;
    size_t target_port;
} smave_block_connection_desc;

typedef struct smave_block_graph_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    const smave_block_node_desc* nodes;
    size_t node_count;
    const smave_block_connection_desc* connections;
    size_t connection_count;
    double end_time;
    double base_step;
} smave_block_graph_desc;

typedef struct smave_solver_options {
    uint32_t struct_size;
    uint32_t abi_version;
    double absolute_tolerance;
    double relative_tolerance;
    int32_t maximum_iterations;
} smave_solver_options;

typedef int32_t (*smave_linear_fallback_fn)(
    size_t dimension,
    double* solution,
    void* user_data);

typedef struct smave_linear_fallback_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_linear_fallback_fn solve;
    void* user_data;
} smave_linear_fallback_desc;

typedef int32_t (*smave_nonlinear_fallback_fn)(
    size_t dimension,
    double* solution,
    void* user_data);

typedef struct smave_nonlinear_fallback_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_nonlinear_fallback_fn solve;
    void* user_data;
} smave_nonlinear_fallback_desc;

typedef int32_t (*smave_ode_dense_step_fallback_fn)(
    size_t dimension,
    double from_time,
    const double* previous_state,
    double to_time,
    double* quarter_state,
    double* midpoint_state,
    double* three_quarter_state,
    double* next_state,
    void* user_data);

typedef struct smave_ode_dense_step_fallback_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_ode_dense_step_fallback_fn step;
    void* user_data;
} smave_ode_dense_step_fallback_desc;

typedef int32_t (*smave_dae_step_fallback_fn)(
    size_t dimension,
    const uint8_t* differential_mask,
    double from_time,
    const double* previous_state,
    const double* previous_derivative,
    double to_time,
    double* next_state,
    double* next_derivative,
    void* user_data);

typedef struct smave_dae_step_fallback_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_dae_step_fallback_fn step;
    void* user_data;
} smave_dae_step_fallback_desc;

typedef int32_t (*smave_nonlinear_residual_fn)(
    size_t dimension,
    const double* state,
    double* residual,
    void* user_data);

typedef int32_t (*smave_nonlinear_jacobian_fn)(
    size_t dimension,
    const double* state,
    double* jacobian_row_major,
    void* user_data);

typedef int32_t (*smave_ode_rhs_fn)(
    size_t dimension,
    double time,
    const double* state,
    double* derivative,
    void* user_data);

typedef int32_t (*smave_event_guard_fn)(
    size_t dimension,
    double time,
    const double* state,
    double* guard,
    void* user_data);

typedef int32_t (*smave_event_reset_fn)(
    size_t dimension,
    double time,
    const double* pre_state,
    double* post_state,
    void* user_data);

typedef int32_t (*smave_hybrid_stable_reset_fn)(
    size_t dimension,
    double time,
    const double* stable_pre_state,
    const double* current_state,
    double* proposed_state,
    void* user_data);

typedef int32_t (*smave_dae_residual_fn)(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* residual,
    void* user_data);

typedef int32_t (*smave_dae_jacobian_fn)(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double derivative_scale,
    double* jacobian_row_major,
    void* user_data);

typedef int32_t (*smave_dae_event_guard_fn)(
    size_t dimension,
    double time,
    const double* state,
    const double* derivative,
    double* guard,
    void* user_data);

typedef int32_t (*smave_dae_event_reset_fn)(
    size_t dimension,
    double time,
    const double* pre_state,
    const double* pre_derivative,
    double* post_state,
    double* post_derivative,
    void* user_data);

typedef int32_t (*smave_hybrid_dae_stable_reset_fn)(
    size_t dimension,
    double time,
    const double* stable_pre_state,
    const double* stable_pre_derivative,
    const double* current_state,
    const double* current_derivative,
    double* proposed_state,
    double* proposed_derivative,
    void* user_data);

typedef struct smave_nonlinear_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const double* initial_state;
    smave_nonlinear_residual_fn residual;
    smave_nonlinear_jacobian_fn jacobian;
    void* user_data;
} smave_nonlinear_problem_desc;

typedef struct smave_ode_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const double* initial_state;
    double start_time;
    double end_time;
    double maximum_step;
    smave_ode_rhs_fn right_hand_side;
    void* user_data;
} smave_ode_problem_desc;

typedef struct smave_event_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t direction;
    int32_t priority;
    smave_event_guard_fn guard;
    smave_event_reset_fn reset;
    void* user_data;
} smave_event_desc;

typedef struct smave_event_ode_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const double* initial_state;
    double start_time;
    double end_time;
    double maximum_step;
    smave_ode_rhs_fn right_hand_side;
    void* right_hand_side_user_data;
    const smave_event_desc* events;
    size_t event_count;
} smave_event_ode_problem_desc;

typedef struct smave_dae_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const uint8_t* differential_mask;
    const double* initial_state;
    const double* initial_derivative;
    double start_time;
    double end_time;
    double maximum_step;
    smave_dae_residual_fn residual;
    smave_dae_jacobian_fn jacobian;
    void* user_data;
} smave_dae_problem_desc;

typedef struct smave_dae_event_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t direction;
    int32_t priority;
    smave_dae_event_guard_fn guard;
    smave_dae_event_reset_fn reset;
    void* user_data;
} smave_dae_event_desc;

typedef struct smave_event_dae_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const uint8_t* differential_mask;
    const double* initial_state;
    const double* initial_derivative;
    double start_time;
    double end_time;
    double maximum_step;
    smave_dae_residual_fn residual;
    smave_dae_jacobian_fn jacobian;
    void* residual_user_data;
    const smave_dae_event_desc* events;
    size_t event_count;
} smave_event_dae_problem_desc;

typedef struct smave_hybrid_mode_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_ode_rhs_fn right_hand_side;
    void* user_data;
} smave_hybrid_mode_desc;

typedef struct smave_hybrid_transition_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t source_mode;
    size_t target_mode;
    int32_t direction;
    int32_t priority;
    smave_event_guard_fn guard;
    smave_event_reset_fn reset;
    void* user_data;
    smave_hybrid_stable_reset_fn stable_reset;
    const uint8_t* write_mask;
} smave_hybrid_transition_desc;

typedef struct smave_hybrid_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const double* initial_state;
    size_t initial_mode;
    double start_time;
    double end_time;
    double maximum_step;
    const smave_hybrid_mode_desc* modes;
    size_t mode_count;
    const smave_hybrid_transition_desc* transitions;
    size_t transition_count;
} smave_hybrid_problem_desc;

typedef struct smave_hybrid_dae_mode_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    smave_dae_residual_fn residual;
    smave_dae_jacobian_fn jacobian;
    void* user_data;
} smave_hybrid_dae_mode_desc;

typedef struct smave_hybrid_dae_transition_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t source_mode;
    size_t target_mode;
    int32_t direction;
    int32_t priority;
    smave_dae_event_guard_fn guard;
    smave_dae_event_reset_fn reset;
    void* user_data;
    smave_hybrid_dae_stable_reset_fn stable_reset;
    const uint8_t* state_write_mask;
    const uint8_t* derivative_write_mask;
} smave_hybrid_dae_transition_desc;

typedef struct smave_hybrid_dae_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const uint8_t* differential_mask;
    const double* initial_state;
    const double* initial_derivative;
    size_t initial_mode;
    double start_time;
    double end_time;
    double maximum_step;
    const smave_hybrid_dae_mode_desc* modes;
    size_t mode_count;
    const smave_hybrid_dae_transition_desc* transitions;
    size_t transition_count;
} smave_hybrid_dae_problem_desc;

typedef struct smave_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t success;
    int32_t used_fallback;
    size_t dimension;
    double residual_inf;
    double backward_error;
    const char* backend;
    const char* diagnostic;
} smave_result_info;

typedef struct smave_ode_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_scaled_local_error;
    size_t accepted_steps;
    size_t rejected_steps;
    size_t event_count;
    double last_event_time;
} smave_ode_result_info;

typedef struct smave_dae_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_residual_inf;
    size_t accepted_steps;
    size_t rejected_steps;
    size_t event_count;
    double last_event_time;
    size_t differentiation_index;
    size_t hidden_rank_checks;
    double minimum_hidden_rank_margin;
    double maximum_hidden_residual_inf;
} smave_dae_result_info;

typedef struct smave_hybrid_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_scaled_local_error;
    size_t accepted_steps;
    size_t rejected_steps;
    size_t event_count;
    double last_event_time;
    size_t final_mode;
} smave_hybrid_result_info;

typedef struct smave_hybrid_dae_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_residual_inf;
    size_t accepted_steps;
    size_t rejected_steps;
    size_t event_count;
    double last_event_time;
    size_t final_mode;
    size_t consistency_projection_count;
} smave_hybrid_dae_result_info;

typedef struct smave_complementarity_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double primal_violation;
    double dual_violation;
    double complementarity_violation;
    size_t attempts;
} smave_complementarity_result_info;

typedef struct smave_block_graph_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_connection_error;
    double maximum_original_gate_residual;
    size_t ticks;
    size_t node_executions;
    size_t fallback_count;
    size_t node_count;
    double maximum_fixed_point_residual;
    size_t fixed_point_components;
    size_t fixed_point_iterations;
} smave_block_graph_result_info;

SMAVE_API uint32_t smave_abi_version(void);
SMAVE_API const char* smave_version_string(void);
SMAVE_API const char* smave_status_string(smave_status status);

SMAVE_API smave_status smave_library_create(
    const smave_library_options* options,
    smave_library** library);
SMAVE_API smave_status smave_library_destroy(smave_library* library);
SMAVE_API smave_status smave_library_has_capability(
    const smave_library* library,
    smave_capability capability,
    int32_t* available);
SMAVE_API smave_status smave_library_get_error_count(
    const smave_library* library,
    size_t* count);
SMAVE_API smave_status smave_library_get_error(
    const smave_library* library,
    size_t newest_index,
    smave_error_info* info);
SMAVE_API smave_status smave_library_clear_errors(smave_library* library);

SMAVE_API smave_status smave_linear_problem_create(
    smave_library* library,
    const smave_linear_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_complementarity_problem_create(
    smave_library* library,
    const smave_complementarity_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_block_graph_problem_create(
    smave_library* library,
    const smave_block_graph_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_nonlinear_problem_create(
    smave_library* library,
    const smave_nonlinear_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_ode_problem_create(
    smave_library* library,
    const smave_ode_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_event_ode_problem_create(
    smave_library* library,
    const smave_event_ode_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_dae_problem_create(
    smave_library* library,
    const smave_dae_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_event_dae_problem_create(
    smave_library* library,
    const smave_event_dae_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_hybrid_problem_create(
    smave_library* library,
    const smave_hybrid_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_hybrid_dae_problem_create(
    smave_library* library,
    const smave_hybrid_dae_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_problem_finalize(smave_problem* problem);
SMAVE_API smave_status smave_problem_destroy(smave_problem* problem);

SMAVE_API smave_status smave_solver_create(
    smave_problem* problem,
    const smave_solver_options* options,
    smave_solver** solver);
SMAVE_API smave_status smave_solver_destroy(smave_solver* solver);
SMAVE_API smave_status smave_solver_solve(
    const smave_solver* solver,
    smave_result** result);
SMAVE_API smave_status smave_cancel_token_create(
    smave_library* library,
    smave_cancel_token** token);
SMAVE_API smave_status smave_cancel_token_request(smave_cancel_token* token);
SMAVE_API smave_status smave_cancel_token_reset(smave_cancel_token* token);
SMAVE_API smave_status smave_cancel_token_destroy(smave_cancel_token* token);
SMAVE_API smave_status smave_solver_solve_cancellable(
    const smave_solver* solver,
    const smave_cancel_token* token,
    smave_result** result);
SMAVE_API smave_status smave_solver_solve_with_timeout(
    const smave_solver* solver,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);
SMAVE_API smave_status smave_solver_solve_linear_with_fallback(
    const smave_solver* solver,
    const smave_linear_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);
SMAVE_API smave_status smave_solver_solve_nonlinear_with_fallback(
    const smave_solver* solver,
    const smave_nonlinear_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);
SMAVE_API smave_status smave_solver_solve_ode_with_fallback(
    const smave_solver* solver,
    const smave_ode_dense_step_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);
SMAVE_API smave_status smave_solver_solve_dae_with_fallback(
    const smave_solver* solver,
    const smave_dae_step_fallback_desc* fallback,
    const smave_cancel_token* token,
    uint64_t timeout_nanoseconds,
    smave_result** result);

SMAVE_API smave_status smave_result_get_info(
    const smave_result* result,
    smave_result_info* info);
SMAVE_API smave_status smave_result_get_provenance(
    const smave_result* result,
    const char** service_id,
    const char** plan_id,
    const char** equation_family);
SMAVE_API smave_status smave_result_get_diagnostic_code(
    const smave_result* result,
    smave_diagnostic_code* diagnostic_code);
SMAVE_API smave_status smave_result_get_ode_info(
    const smave_result* result,
    smave_ode_result_info* info);
SMAVE_API smave_status smave_result_get_dae_info(
    const smave_result* result,
    smave_dae_result_info* info);
SMAVE_API smave_status smave_result_get_hybrid_info(
    const smave_result* result,
    smave_hybrid_result_info* info);
SMAVE_API smave_status smave_result_get_hybrid_dae_info(
    const smave_result* result,
    smave_hybrid_dae_result_info* info);
SMAVE_API smave_status smave_result_get_complementarity_info(
    const smave_result* result,
    smave_complementarity_result_info* info);
SMAVE_API smave_status smave_result_get_block_graph_info(
    const smave_result* result,
    smave_block_graph_result_info* info);
SMAVE_API smave_status smave_result_copy_solution(
    const smave_result* result,
    double* values,
    size_t capacity,
    size_t* required);
SMAVE_API smave_status smave_result_copy_complementarity_gap(
    const smave_result* result,
    double* values,
    size_t capacity,
    size_t* required);
SMAVE_API smave_status smave_result_copy_block_output_offsets(
    const smave_result* result,
    size_t* values,
    size_t capacity,
    size_t* required);
SMAVE_API smave_status smave_result_copy_block_commit_order(
    const smave_result* result,
    size_t* values,
    size_t capacity,
    size_t* required);
SMAVE_API smave_status smave_result_destroy(smave_result* result);

#ifdef __cplusplus
}
#endif

#endif
