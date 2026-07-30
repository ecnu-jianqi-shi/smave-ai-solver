#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace smave::test {

enum class BenchmarkSolverOrder {
    classical_first,
    smave_first,
};

inline BenchmarkSolverOrder benchmark_solver_order() {
    const char* value = std::getenv("SMAVE_BENCHMARK_SOLVER_ORDER");
    if (value == nullptr || std::string_view(value) == "classical-first") {
        return BenchmarkSolverOrder::classical_first;
    }
    if (std::string_view(value) == "smave-first") {
        return BenchmarkSolverOrder::smave_first;
    }
    throw std::invalid_argument(
        "SMAVE_BENCHMARK_SOLVER_ORDER must be classical-first or smave-first");
}

inline std::string_view benchmark_solver_order_name(BenchmarkSolverOrder order) {
    return order == BenchmarkSolverOrder::classical_first
        ? "classical-first"
        : "smave-first";
}

template <typename ClassicalFunction, typename SmaveFunction>
void run_in_benchmark_solver_order(
    BenchmarkSolverOrder order,
    ClassicalFunction&& run_classical,
    SmaveFunction&& run_smave) {
    if (order == BenchmarkSolverOrder::smave_first) {
        std::forward<SmaveFunction>(run_smave)();
        std::forward<ClassicalFunction>(run_classical)();
    } else {
        std::forward<ClassicalFunction>(run_classical)();
        std::forward<SmaveFunction>(run_smave)();
    }
}

}  // namespace smave::test
