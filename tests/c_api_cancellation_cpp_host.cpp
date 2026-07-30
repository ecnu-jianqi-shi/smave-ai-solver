#include "smave/c_api.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::array matrix{2.0, -1.0, -1.0, 2.0};
    constexpr std::array right_hand_side{1.0, 1.0};
    const smave_linear_problem_desc descriptor{
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        2, matrix.data(), nullptr, nullptr, nullptr, 0, right_hand_side.data()};
    smave_library* library{};
    smave_problem* problem{};
    smave_solver* solver{};
    smave_cancel_token* token{};
    smave_result* deadline_result{};
    smave_result* cancelled_result{};
    smave_result* completed_result{};
    smave_diagnostic_code diagnostic{SMAVE_DIAGNOSTIC_SUCCESS};
    std::array<double, 2> solution{};
    std::size_t required{};
    std::int32_t cancellation_available{};
    std::int32_t deadline_available{};
    const bool success =
        smave_library_create(nullptr, &library) == SMAVE_STATUS_OK &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_CANCELLATION, &cancellation_available) ==
            SMAVE_STATUS_OK && cancellation_available &&
        smave_library_has_capability(
            library, SMAVE_CAPABILITY_DEADLINE, &deadline_available) == SMAVE_STATUS_OK &&
        deadline_available && smave_cancel_token_create(library, &token) == SMAVE_STATUS_OK &&
        smave_linear_problem_create(library, &descriptor, &problem) == SMAVE_STATUS_OK &&
        smave_problem_finalize(problem) == SMAVE_STATUS_OK &&
        smave_solver_create(problem, nullptr, &solver) == SMAVE_STATUS_OK &&
        smave_solver_solve_with_timeout(solver, token, 0, &deadline_result) ==
            SMAVE_STATUS_DEADLINE_EXCEEDED &&
        smave_result_get_diagnostic_code(deadline_result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED &&
        std::string_view(smave_status_string(SMAVE_STATUS_DEADLINE_EXCEEDED)) ==
            "deadline exceeded" &&
        smave_cancel_token_request(token) == SMAVE_STATUS_OK &&
        smave_solver_solve_with_timeout(solver, token, 0, &cancelled_result) ==
            SMAVE_STATUS_CANCELLED &&
        smave_result_get_diagnostic_code(cancelled_result, &diagnostic) == SMAVE_STATUS_OK &&
        diagnostic == SMAVE_DIAGNOSTIC_CANCELLED &&
        std::string_view(smave_status_string(SMAVE_STATUS_CANCELLED)) == "cancelled" &&
        smave_cancel_token_reset(token) == SMAVE_STATUS_OK &&
        smave_solver_solve_cancellable(solver, token, &completed_result) == SMAVE_STATUS_OK &&
        smave_result_copy_solution(
            completed_result, solution.data(), solution.size(), &required) == SMAVE_STATUS_OK &&
        required == solution.size() && std::abs(solution[0] - 1.0) <= 1.0e-12 &&
        std::abs(solution[1] - 1.0) <= 1.0e-12;
    smave_result_destroy(deadline_result);
    smave_result_destroy(cancelled_result);
    smave_result_destroy(completed_result);
    const bool destroyed =
        smave_cancel_token_destroy(token) == SMAVE_STATUS_OK &&
        smave_solver_destroy(solver) == SMAVE_STATUS_OK &&
        smave_problem_destroy(problem) == SMAVE_STATUS_OK &&
        smave_library_destroy(library) == SMAVE_STATUS_OK;
    if (!success || !destroyed) return 1;
    std::cout << "SMAVE_CPP_CANCELLATION_HOST 1\n"
                 "CPP_HEADER_ONLY_PUBLIC_ABI 1\n"
                 "CPP_CANCELLATION_RESET_REUSE 1\n"
                 "CPP_DEADLINE_STATUS_DIAGNOSTIC 1\n"
                 "CPP_CANCELLATION_PRECEDENCE 1\n";
    return 0;
}
