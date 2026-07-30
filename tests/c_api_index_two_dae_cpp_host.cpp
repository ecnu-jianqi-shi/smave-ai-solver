#include "smave/c_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

int residual(
    std::size_t dimension,
    double,
    const double* state,
    const double* derivative,
    double* values,
    void*) {
    if (dimension != 3) return 1;
    values[0] = derivative[0] - state[1] - state[2];
    values[1] = derivative[1] + state[0];
    values[2] = state[0];
    return 0;
}

int jacobian(
    std::size_t dimension,
    double,
    const double*,
    const double*,
    double derivative_scale,
    double* values,
    void*) {
    if (dimension != 3) return 1;
    const std::array matrix{
        derivative_scale, -1.0, -1.0,
        1.0, derivative_scale, 0.0,
        1.0, 0.0, 0.0};
    std::copy(matrix.begin(), matrix.end(), values);
    return 0;
}

}

int main() {
    constexpr std::array<std::uint8_t, 3> differential_mask{1, 1, 0};
    constexpr std::array initial_state{0.0, 1.0, -1.0};
    constexpr std::array initial_derivative{0.0, 0.0, 0.0};
    const smave_dae_problem_desc descriptor{
        sizeof(descriptor), SMAVE_ABI_VERSION, initial_state.size(),
        differential_mask.data(), initial_state.data(), initial_derivative.data(),
        0.0, 0.2, 0.1, residual, jacobian, nullptr};
    smave_library* library{};
    smave_problem* problem{};
    smave_solver* solver{};
    smave_result* result{};
    smave_dae_result_info dae_info{};
    dae_info.struct_size = sizeof(dae_info);
    dae_info.abi_version = SMAVE_ABI_VERSION;
    std::array<double, 3> solution{};
    std::size_t required{};
    const char* service{};
    const char* plan{};
    const char* family{};
    std::int32_t available{};
    const bool success =
        smave_library_create(nullptr, &library) == SMAVE_STATUS_OK &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_INDEX_TWO_DAE, &available) == SMAVE_STATUS_OK &&
        available &&
        smave_dae_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, nullptr, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_dae_info(result, &dae_info) == SMAVE_STATUS_OK &&
        smave_result_get_provenance(result, &service, &plan, &family) == SMAVE_STATUS_OK &&
        smave_result_copy_solution(
            result, solution.data(), solution.size(), &required) == SMAVE_STATUS_OK &&
        required == solution.size() && dae_info.differentiation_index == 2 &&
        dae_info.hidden_rank_checks == 3 && service != nullptr &&
        std::string_view(service) == "smave.verified-fully-implicit-dae-solve.v1" &&
        plan != nullptr && plan[0] != '\0' && family != nullptr &&
        std::string_view(family) == "dae-fully-implicit-hessenberg-index2" &&
        std::abs(solution[0]) <= 1.0e-12 &&
        std::abs(solution[1] - 1.0) <= 1.0e-12 &&
        std::abs(solution[2] + 1.0) <= 1.0e-12;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    const bool destroyed = smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!success || !destroyed) return 1;
    std::cout << "SMAVE_CPP_INDEX_TWO_DAE_HOST 1\n"
                 "CPP_HEADER_ONLY_PUBLIC_ABI 1\n"
                 "CPP_INDEX_TWO_DAE_SOLVE 1\n";
    return 0;
}
