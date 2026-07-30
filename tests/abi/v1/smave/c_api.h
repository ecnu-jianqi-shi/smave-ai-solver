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
#define SMAVE_LINEAR_SYMMETRIC 1u
#define SMAVE_LINEAR_POSITIVE_DEFINITE 2u

typedef struct smave_library smave_library;
typedef struct smave_problem smave_problem;
typedef struct smave_solver smave_solver;
typedef struct smave_result smave_result;

typedef enum smave_status {
    SMAVE_STATUS_OK = 0,
    SMAVE_STATUS_INVALID_ARGUMENT = 1,
    SMAVE_STATUS_ABI_MISMATCH = 2,
    SMAVE_STATUS_INVALID_STATE = 3,
    SMAVE_STATUS_UNSUPPORTED = 4,
    SMAVE_STATUS_SOLVE_FAILED = 5,
    SMAVE_STATUS_BUFFER_TOO_SMALL = 6,
    SMAVE_STATUS_INTERNAL_ERROR = 7
} smave_status;

typedef enum smave_capability {
    SMAVE_CAPABILITY_LINEAR_DENSE = 1,
    SMAVE_CAPABILITY_LINEAR_CSR = 2,
    SMAVE_CAPABILITY_NONLINEAR = 3,
    SMAVE_CAPABILITY_ODE = 4,
    SMAVE_CAPABILITY_DAE = 5,
    SMAVE_CAPABILITY_EVENTS = 6,
    SMAVE_CAPABILITY_COMPLEMENTARITY = 7,
    SMAVE_CAPABILITY_MULTIPHYSICS = 8
} smave_capability;

typedef enum smave_matrix_storage {
    SMAVE_MATRIX_DENSE_ROW_MAJOR = 1,
    SMAVE_MATRIX_CSR = 2
} smave_matrix_storage;

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

typedef struct smave_solver_options {
    uint32_t struct_size;
    uint32_t abi_version;
    double absolute_tolerance;
    double relative_tolerance;
    int32_t maximum_iterations;
} smave_solver_options;

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

typedef struct smave_nonlinear_problem_desc {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t dimension;
    const double* initial_state;
    smave_nonlinear_residual_fn residual;
    smave_nonlinear_jacobian_fn jacobian;
    void* user_data;
} smave_nonlinear_problem_desc;

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
SMAVE_API smave_status smave_linear_problem_create(
    smave_library* library,
    const smave_linear_problem_desc* descriptor,
    smave_problem** problem);
SMAVE_API smave_status smave_nonlinear_problem_create(
    smave_library* library,
    const smave_nonlinear_problem_desc* descriptor,
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
SMAVE_API smave_status smave_result_get_info(
    const smave_result* result,
    smave_result_info* info);
SMAVE_API smave_status smave_result_copy_solution(
    const smave_result* result,
    double* values,
    size_t capacity,
    size_t* required);
SMAVE_API smave_status smave_result_destroy(smave_result* result);

#ifdef __cplusplus
}
#endif

#endif
