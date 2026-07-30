#include "smave/c_api.h"

#include <stdio.h>
#include <stdint.h>

typedef struct legacy_ode_result_info {
    uint32_t struct_size;
    uint32_t abi_version;
    double final_time;
    double maximum_scaled_local_error;
    size_t accepted_steps;
    size_t rejected_steps;
} legacy_ode_result_info;

typedef struct extended_legacy_ode_result_info {
    legacy_ode_result_info base;
    uint64_t sentinel;
} extended_legacy_ode_result_info;

static int rhs(
    size_t dimension, double time, const double* state, double* derivative,
    void* user_data) {
    (void)time;
    (void)user_data;
    if (dimension != 1) return 1;
    derivative[0] = -state[0];
    return 0;
}

int main(void) {
    const double initial[] = {1.0};
    smave_library* library = NULL;
    smave_problem* problem = NULL;
    smave_solver* solver = NULL;
    smave_result* result = NULL;
    smave_ode_problem_desc descriptor = {
        sizeof(descriptor), SMAVE_ABI_VERSION, 1, initial,
        0.0, 1.0, 0.1, rhs, NULL};
    smave_solver_options options = {
        sizeof(options), SMAVE_ABI_VERSION, 1.0e-10, 1.0e-8, 100000};
    smave_result_info info = {
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, NULL, NULL};
    smave_ode_result_info ode_info = {
        sizeof(ode_info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, 0};
    extended_legacy_ode_result_info legacy_info = {
        {sizeof(legacy_info.base), SMAVE_ABI_VERSION, 0, 0, 0, 0},
        UINT64_C(0x1122334455667788)};
    const char* service_id = NULL;
    const char* plan_id = NULL;
    const char* equation_family = NULL;
    double solution[1];
    size_t required = 0;
    if (smave_library_create(NULL, &library) != SMAVE_STATUS_OK ||
        smave_ode_problem_create(library, &descriptor, &problem) != SMAVE_STATUS_OK ||
        smave_problem_finalize(problem) != SMAVE_STATUS_OK ||
        smave_solver_create(problem, &options, &solver) != SMAVE_STATUS_OK ||
        smave_solver_solve(solver, &result) != SMAVE_STATUS_OK ||
        smave_result_get_info(result, &info) != SMAVE_STATUS_OK ||
        smave_result_get_provenance(
            result, &service_id, &plan_id, &equation_family) != SMAVE_STATUS_OK ||
        smave_result_get_ode_info(result, &ode_info) != SMAVE_STATUS_OK ||
        smave_result_get_ode_info(
            result, (smave_ode_result_info*)&legacy_info.base) != SMAVE_STATUS_OK ||
        legacy_info.sentinel != UINT64_C(0x1122334455667788) ||
        smave_result_copy_solution(result, solution, 1, &required) != SMAVE_STATUS_OK ||
        required != 1 || service_id == NULL || plan_id == NULL ||
        equation_family == NULL || info.diagnostic == NULL) return 1;
    printf("SMAVE_C_API_ODE_SERVICE 1\n"
           "service_id=\"%s\"\n"
           "success=%d\n"
           "used_fallback=%d\n"
           "backend=\"%s\"\n"
           "plan_id=\"%s\"\n"
           "equation_family=\"%s\"\n"
           "residual_inf=%.17g\n"
           "backward_error=%.17g\n"
           "final_time=%.17g\n"
           "maximum_scaled_local_error=%.17g\n"
           "accepted_steps=%zu\n"
           "rejected_steps=%zu\n"
           "solution=%.17g\n"
           "diagnostic=\"%s\"\n"
           "legacy_ode_result_prefix_compatible=1\n"
           "END\n",
           service_id, info.success, info.used_fallback, info.backend, plan_id,
           equation_family, info.residual_inf, info.backward_error,
           ode_info.final_time, ode_info.maximum_scaled_local_error,
           ode_info.accepted_steps, ode_info.rejected_steps,
           solution[0], info.diagnostic);
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    return smave_library_destroy(library) == SMAVE_STATUS_OK ? 0 : 1;
}
