#include "smave/c_api.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const double matrix[] = {4, -1, 0, -1, 4, -1, 0, -1, 3};
    const double right[] = {3, 2, 2};
    smave_library* library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_linear_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        3, matrix, NULL, NULL, NULL, 0, right};
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    double solution[3];
    size_t required = 0;
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_linear_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, NULL, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK ||
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) != SMAVE_STATUS_OK ||
        smave_result_copy_solution(result, solution, 3, &required) != SMAVE_STATUS_OK ||
        required != 3 || service_id == NULL || plan_id == NULL ||
        equation_family == NULL || info.diagnostic == NULL ||
        strstr(info.diagnostic, "smave.verified-linear-solve.v1: ") != info.diagnostic) {
        return 1;
    }
    printf("SMAVE_C_API_LINEAR_SERVICE 1\n"
           "service_id=\"%s\"\n"
           "success=%d\n"
           "used_fallback=%d\n"
           "backend=\"%s\"\n"
           "plan_id=\"%s\"\n"
           "equation_family=\"%s\"\n"
           "residual_inf=%.17g\n"
           "backward_error=%.17g\n"
           "solution=%.17g,%.17g,%.17g\n"
           "diagnostic=\"%s\"\n"
           "END\n",
           service_id, info.success, info.used_fallback, info.backend,
           plan_id, equation_family, info.residual_inf,
           info.backward_error, solution[0], solution[1], solution[2], info.diagnostic);
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return smave_library_destroy(library) == SMAVE_STATUS_OK ? 0 : 1;
}
