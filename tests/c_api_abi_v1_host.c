#include "smave/c_api.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct extended_library_options {
    smave_library_options base;
    uint64_t future_field;
} extended_library_options;

typedef struct extended_linear_problem_desc {
    smave_linear_problem_desc base;
    uint64_t future_field;
} extended_linear_problem_desc;

typedef struct extended_solver_options {
    smave_solver_options base;
    uint64_t future_field;
} extended_solver_options;

typedef struct extended_result_info {
    smave_result_info base;
    uint64_t future_field;
} extended_result_info;

int main(void) {
    const double matrix[] = {4, -1, 0, -1, 4, -1, 0, -1, 3};
    const double right[] = {3, 2, 2};
    smave_library* library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    double solution[3] = {0, 0, 0};
    size_t required = 0;
    extended_library_options library_options = {
        {sizeof(library_options), SMAVE_ABI_VERSION, NULL, NULL, NULL},
        UINT64_C(0x1122334455667788)};
    extended_linear_problem_desc descriptor = {
        {sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
         SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
         3, matrix, NULL, NULL, NULL, 0, right},
        UINT64_C(0x2233445566778899)};
    extended_solver_options options = {
        {sizeof(options), SMAVE_ABI_VERSION, 1.0e-12, 1.0e-10, 100},
        UINT64_C(0x33445566778899aa)};
    extended_result_info info = {
        {sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL},
        UINT64_C(0x445566778899aabb)};

    if (smave_abi_version() != SMAVE_ABI_VERSION ||
        smave_library_create(&library_options.base, &library) != SMAVE_STATUS_OK ||
        library_options.future_field != UINT64_C(0x1122334455667788) ||
        smave_linear_problem_create(library, &descriptor.base, &problem) != SMAVE_STATUS_OK ||
        descriptor.future_field != UINT64_C(0x2233445566778899) ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options.base, &solver) != SMAVE_STATUS_OK ||
        options.future_field != UINT64_C(0x33445566778899aa) ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK ||
        smave_result_get_info(result, &info.base) != SMAVE_STATUS_OK ||
        info.future_field != UINT64_C(0x445566778899aabb) || !info.base.success ||
        info.base.dimension != 3 || info.base.backend == NULL ||
        info.base.diagnostic == NULL ||
        strstr(info.base.diagnostic, "smave.verified-linear-solve.v1: ") !=
            info.base.diagnostic ||
        smave_result_copy_solution(result, solution, 3, &required) != SMAVE_STATUS_OK ||
        required != 3 || fabs(solution[0] - 1.0) > 1.0e-12 ||
        fabs(solution[1] - 1.0) > 1.0e-12 || fabs(solution[2] - 1.0) > 1.0e-12) {
        return 1;
    }
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    if (smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;

    library_options.base.abi_version = SMAVE_ABI_VERSION + 1;
    library = NULL;
    if (smave_library_create(&library_options.base, &library) != SMAVE_STATUS_ABI_MISMATCH ||
        library != NULL) return 1;

    printf("SMAVE_C_API_ABI_V1_HOST 1\n"
           "FROZEN_V1_HEADER 1\n"
           "CURRENT_LIBRARY 1\n"
           "EXTENDED_STRUCT_INPUT 1\n"
           "EXTENDED_STRUCT_OUTPUT 1\n"
           "UNKNOWN_TAIL_PRESERVED 1\n"
           "ABI_MISMATCH_REJECTED 1\n"
           "SOLUTION 1,1,1\n");
    return 0;
}
