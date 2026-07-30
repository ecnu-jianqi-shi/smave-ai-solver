#include "smave/c_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct extended_complementarity_desc {
    smave_complementarity_desc base;
    unsigned long long future_field;
} extended_complementarity_desc;

typedef struct extended_complementarity_result_info {
    smave_complementarity_result_info base;
    unsigned long long future_field;
} extended_complementarity_result_info;

static int solve_problem(
    smave_library* library,
    const smave_complementarity_desc* descriptor,
    int maximum_iterations,
    int expected_fallback) {
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, maximum_iterations};
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    extended_complementarity_result_info extended_info = {
        {sizeof(extended_info), SMAVE_ABI_VERSION, 0, 0, 0, 0},
        0x8877665544332211ULL};
    smave_diagnostic_code diagnostic_code = SMAVE_DIAGNOSTIC_INVALID_CONTRACT;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[3] = {0, 0, 0};
    double gap[3] = {0, 0, 0};
    size_t solution_required = 0;
    size_t gap_required = 0;
    int success =
        smave_complementarity_problem_create(library, descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, &options, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        info.used_fallback == expected_fallback &&
        smave_result_get_diagnostic_code(result, &diagnostic_code) == SMAVE_STATUS_OK &&
        diagnostic_code == SMAVE_DIAGNOSTIC_SUCCESS &&
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) == SMAVE_STATUS_OK &&
        service_id != NULL &&
        strcmp(service_id, "smave.verified-complementarity-solve.v1") == 0 &&
        plan_id != NULL && plan_id[0] != '\0' && equation_family != NULL &&
        strcmp(equation_family, "strongly-monotone-linear-complementarity") == 0 &&
        smave_result_get_complementarity_info(result, &extended_info.base) ==
            SMAVE_STATUS_OK &&
        extended_info.future_field == 0x8877665544332211ULL &&
        smave_result_copy_solution(
            result, solution, 3, &solution_required) == SMAVE_STATUS_OK &&
        smave_result_copy_complementarity_gap(
            result, gap, 3, &gap_required) == SMAVE_STATUS_OK &&
        solution_required == 3 && gap_required == 3 &&
        fabs(solution[0] - 0.75) <= 1.0e-7 &&
        fabs(solution[1] - 0.5) <= 1.0e-7 &&
        fabs(solution[2] - 0.5) <= 1.0e-7 &&
        fabs(gap[0]) <= 1.0e-7 && fabs(gap[1]) <= 1.0e-7 &&
        fabs(gap[2]) <= 1.0e-7 &&
        extended_info.base.primal_violation <= 1.0e-8 &&
        extended_info.base.dual_violation <= 1.0e-8 &&
        extended_info.base.complementarity_violation <= 1.0e-8 &&
        extended_info.base.attempts >= 1 && info.backend != NULL &&
        info.diagnostic != NULL &&
        strstr(info.diagnostic, "smave.verified-complementarity-solve.v1: ") ==
            info.diagnostic;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return success;
}

int main(void) {
    double dense_matrix[] = {
        2.0, -1.0, 0.0,
        -1.0, 2.0, -1.0,
        0.0, -1.0, 2.0};
    double offset[] = {-1.0, 0.25, -0.5};
    const size_t row_offsets[] = {0, 2, 5, 7};
    const size_t column_indices[] = {0, 1, 0, 1, 2, 1, 2};
    const double sparse_values[] = {2.0, -1.0, -1.0, 2.0, -1.0, -1.0, 2.0};
    smave_complementarity_desc dense = {
        sizeof(dense), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR, 0,
        3, dense_matrix, NULL, NULL, NULL, 0, offset, NULL};
    smave_complementarity_desc sparse = {
        sizeof(sparse), SMAVE_ABI_VERSION, SMAVE_MATRIX_CSR, 0,
        3, NULL, row_offsets, column_indices, sparse_values, 7, offset, NULL};
    smave_library* library = NULL;
    int32_t capability = 0;
    smave_problem* copied_problem = NULL;
    smave_problem* rejected_problem = NULL;
    double nonmonotone_matrix[] = {1.0, 2.0, 2.0, 1.0};
    double nonmonotone_offset[] = {-1.0, -1.0};
    smave_complementarity_desc nonmonotone = {
        sizeof(nonmonotone), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR, 0,
        2, nonmonotone_matrix, NULL, NULL, NULL, 0, nonmonotone_offset, NULL};
    const size_t invalid_rows[] = {0, 2, 3};
    const size_t invalid_columns[] = {1, 0, 1};
    const double invalid_values[] = {-1.0, 2.0, 2.0};
    smave_complementarity_desc invalid_csr = {
        sizeof(invalid_csr), SMAVE_ABI_VERSION, SMAVE_MATRIX_CSR, 0,
        2, NULL, invalid_rows, invalid_columns, invalid_values, 3,
        nonmonotone_offset, NULL};
    extended_complementarity_desc extended = {
        {sizeof(extended), SMAVE_ABI_VERSION, SMAVE_MATRIX_CSR, 0,
         3, NULL, row_offsets, column_indices, sparse_values, 7, offset, NULL},
        0x1122334455667788ULL};

    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_COMPLEMENTARITY, &capability) != SMAVE_STATUS_OK ||
        capability != 1 || !solve_problem(library, &dense, 4000, 0) ||
        !solve_problem(library, &sparse, 4000, 0) ||
        !solve_problem(library, &dense, 1, 1) ||
        !solve_problem(library, &extended.base, 4000, 0) ||
        extended.future_field != 0x1122334455667788ULL) {
        return 1;
    }

    if (smave_complementarity_problem_create(
            library, &dense, &copied_problem) != SMAVE_STATUS_OK) return 1;
    dense_matrix[0] = NAN;
    offset[0] = NAN;
    if (smave_problem_finalize(copied_problem) != SMAVE_STATUS_OK) return 1;
    {
        smave_solver* solver = NULL;
        smave_result* result = NULL;
        if (smave_solver_create(copied_problem, NULL, &solver) != SMAVE_STATUS_OK ||
            smave_solver_solve(solver, &result) != SMAVE_STATUS_OK) return 1;
        smave_result_destroy(result);
        smave_solver_destroy(solver);
    }
    smave_problem_destroy(copied_problem);

    if (smave_complementarity_problem_create(
            library, &nonmonotone, &rejected_problem) != SMAVE_STATUS_UNSUPPORTED ||
        rejected_problem != NULL ||
        smave_complementarity_problem_create(
            library, &invalid_csr, &rejected_problem) != SMAVE_STATUS_INVALID_ARGUMENT ||
        rejected_problem != NULL) return 1;
    dense.abi_version = SMAVE_ABI_VERSION + 1;
    if (smave_complementarity_problem_create(
            library, &dense, &rejected_problem) != SMAVE_STATUS_ABI_MISMATCH ||
        rejected_problem != NULL) return 1;

    if (smave_library_destroy(library) != SMAVE_STATUS_OK) return 1;
    printf("SMAVE_C_API_COMPLEMENTARITY_SERVICE 1\n"
           "COMPLEMENTARITY_CAPABILITY 1\n"
           "COMPLEMENTARITY_DENSE_CSR_EQUIVALENT 1\n"
           "COMPLEMENTARITY_ORIGINAL_GAP_GATE 1\n"
           "COMPLEMENTARITY_INEQUALITY_GATE 1\n"
           "COMPLEMENTARITY_PRODUCT_GATE 1\n"
           "COMPLEMENTARITY_ACTIVE_SET_FALLBACK 1\n"
           "COMPLEMENTARITY_INPUTS_COPIED 1\n"
           "COMPLEMENTARITY_NONMONOTONE_REJECTED 1\n"
           "COMPLEMENTARITY_INVALID_CSR_REJECTED 1\n"
           "COMPLEMENTARITY_ABI_MISMATCH_REJECTED 1\n"
           "COMPLEMENTARITY_EXTENDED_STRUCT_TAIL_PRESERVED 1\n");
    return 0;
}
