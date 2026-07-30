#include "smave/c_api.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::array matrix{
        2.0, -1.0, 0.0,
        -1.0, 2.0, -1.0,
        0.0, -1.0, 2.0};
    constexpr std::array offset{-1.0, 0.25, -0.5};
    smave_complementarity_desc descriptor{
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR, 0,
        3, matrix.data(), nullptr, nullptr, nullptr, 0, offset.data(), nullptr};
    smave_library* library{};
    smave_problem* problem{};
    smave_solver* solver{};
    smave_result* result{};
    smave_result_info info{};
    info.struct_size = sizeof(info);
    info.abi_version = SMAVE_ABI_VERSION;
    smave_complementarity_result_info complementarity_info{};
    complementarity_info.struct_size = sizeof(complementarity_info);
    complementarity_info.abi_version = SMAVE_ABI_VERSION;
    std::array<double, 3> solution{};
    std::size_t required{};
    const char* service{};
    const char* plan{};
    const char* family{};
    const bool success =
        smave_library_create(nullptr, &library) == SMAVE_STATUS_OK &&
        smave_complementarity_problem_create(library, &descriptor, &problem) ==
            SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, nullptr, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve(solver, &result) == SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        smave_result_get_provenance(result, &service, &plan, &family) == SMAVE_STATUS_OK &&
        smave_result_get_complementarity_info(result, &complementarity_info) ==
            SMAVE_STATUS_OK &&
        smave_result_copy_solution(
            result, solution.data(), solution.size(), &required) == SMAVE_STATUS_OK &&
        required == solution.size() && service != nullptr &&
        std::string_view(service) == "smave.verified-complementarity-solve.v1" &&
        plan != nullptr && plan[0] != '\0' && family != nullptr &&
        std::string_view(family) == "strongly-monotone-linear-complementarity" &&
        std::abs(solution[0] - 0.75) <= 1.0e-7 &&
        std::abs(solution[1] - 0.5) <= 1.0e-7 &&
        std::abs(solution[2] - 0.5) <= 1.0e-7 &&
        complementarity_info.complementarity_violation <= 1.0e-8;
    smave_result_destroy(result);
    smave_solver_destroy(solver);
    smave_problem_destroy(problem);
    const bool destroyed = smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!success || !destroyed) return 1;
    std::cout << "SMAVE_CPP_COMPLEMENTARITY_HOST 1\n"
                 "CPP_HEADER_ONLY_PUBLIC_ABI 1\n"
                 "CPP_COMPLEMENTARITY_SOLVE 1\n";
    return 0;
}
