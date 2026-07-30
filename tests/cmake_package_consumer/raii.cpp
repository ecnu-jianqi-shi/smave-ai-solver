#include "smave/cpp_api.hpp"

#include <array>
#include <iostream>

int main() {
    constexpr std::array matrix{1.0};
    constexpr std::array right_hand_side{2.0};
    const smave_linear_problem_desc descriptor{
        sizeof(descriptor), SMAVE_ABI_VERSION, SMAVE_MATRIX_DENSE_ROW_MAJOR,
        SMAVE_LINEAR_SYMMETRIC | SMAVE_LINEAR_POSITIVE_DEFINITE,
        1, matrix.data(), nullptr, nullptr, nullptr, 0, right_hand_side.data()};
    smave::sdk::Library library;
    auto problem = library.linear(descriptor);
    problem.finalize();
    auto outcome = problem.solver().solve();
    const auto solution = outcome.result.solution();
    if (outcome.status != SMAVE_STATUS_OK || solution.size() != 1 ||
        solution[0] != 2.0) return 1;
    std::cout << "SMAVE_CMAKE_RAII_CONSUMER 1\n"
                 "SMAVE_IMPORTED_CPP_TARGET 1\n"
                 "SMAVE_CPP20_INTERFACE 1\n";
    return 0;
}
