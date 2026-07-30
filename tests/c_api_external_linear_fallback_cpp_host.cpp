#include "smave/c_api.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

int fallback(size_t dimension, double* solution, void*) {
    if (dimension != 2) return 1;
    solution[0] = 2.0;
    solution[1] = 0.0;
    return 0;
}

}

int main() {
    constexpr std::array matrix{1.0, 1.0, 2.0, 2.0};
    constexpr std::array right_hand_side{2.0, 4.0};
    const smave_linear_problem_desc problem_desc{
        sizeof(problem_desc), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        0, 2, matrix.data(), nullptr, nullptr, nullptr, 0, right_hand_side.data()};
    const smave_linear_fallback_desc fallback_desc{
        sizeof(fallback_desc), SMAVE_ABI_VERSION, fallback, nullptr};
    smave_library* library{};
    smave_problem* problem{};
    smave_solver* solver{};
    smave_result* result{};
    smave_result_info info{
        sizeof(info), SMAVE_ABI_VERSION, 0, 0, 0, 0, 0, nullptr, nullptr};
    std::array<double, 2> solution{};
    std::size_t required{};
    std::int32_t available{};
    const bool solved =
        smave_library_create(nullptr, &library) == SMAVE_STATUS_OK &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_EXTERNAL_LINEAR_FALLBACK, &available) ==
            SMAVE_STATUS_OK && available &&
        smave_linear_problem_create(library, &problem_desc, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, nullptr, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve_linear_with_fallback(
            solver, &fallback_desc, nullptr, SMAVE_TIMEOUT_INFINITE, &result) ==
            SMAVE_STATUS_OK &&
        smave_result_get_info(result, &info) == SMAVE_STATUS_OK && info.success &&
        info.used_fallback && info.backend != nullptr &&
        std::strcmp(info.backend, "caller-linear-fallback-v1") == 0 &&
        smave_result_copy_solution(result, solution.data(), solution.size(), &required) ==
            SMAVE_STATUS_OK && required == solution.size() && solution[0] == 2.0 &&
        solution[1] == 0.0;
    smave_result_destroy(result);
    const bool destroyed =
        smave_solver_destroy(solver) == SMAVE_STATUS_OK &&
        smave_problem_destroy(problem) == SMAVE_STATUS_OK &&
        smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!solved || !destroyed) return 1;
    std::cout << "SMAVE_CPP_EXTERNAL_LINEAR_FALLBACK_HOST 1\n"
                 "CPP_EXTERNAL_LINEAR_FALLBACK_PUBLIC_ABI 1\n"
                 "CPP_EXTERNAL_LINEAR_FALLBACK_VERIFIED 1\n";
    return 0;
}
