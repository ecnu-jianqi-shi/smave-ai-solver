#include "smave/linear.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <vector>

namespace {

using Dgesv = void (*)(const int*, const int*, double*, const int*, int*,
                       double*, const int*, int*);

std::atomic<std::size_t> calls{};
std::atomic<std::size_t> solved{};
std::atomic<std::size_t> fallbacks{};
std::atomic<std::size_t> unknowns{};

Dgesv original_dgesv() {
    static auto function = reinterpret_cast<Dgesv>(dlsym(RTLD_NEXT, "dgesv_"));
    return function;
}

int solve_lapack_semantics(std::vector<std::vector<double>> matrix,
                           const std::vector<double>& rhs,
                           std::vector<double>& solution) {
    const std::size_t size = matrix.size();
    solution = rhs;
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) >
                std::abs(matrix[pivot][column])) pivot = row;
        }
        if (!std::isfinite(matrix[pivot][column])) return -1;
        if (matrix[pivot][column] == 0.0)
            return static_cast<int>(column + 1);
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(solution[pivot], solution[column]);
        }
        for (std::size_t row = column + 1; row < size; ++row) {
            const double multiplier = matrix[row][column] / matrix[column][column];
            matrix[row][column] = multiplier;
            for (std::size_t entry = column + 1; entry < size; ++entry)
                matrix[row][entry] -= multiplier * matrix[column][entry];
            solution[row] -= multiplier * solution[column];
        }
    }
    for (std::size_t reverse = size; reverse-- > 0;) {
        for (std::size_t column = reverse + 1; column < size; ++column)
            solution[reverse] -= matrix[reverse][column] * solution[column];
        solution[reverse] /= matrix[reverse][reverse];
    }
    return std::all_of(solution.begin(), solution.end(),
               [](double value) { return std::isfinite(value); })
        ? 0
        : -1;
}

struct ReportWriter {
    ~ReportWriter() {
        const char* path = std::getenv("SMAVE_OMC_LAPACK_REPORT");
        if (!path) return;
        std::ofstream output(path);
        output << "SMAVE_OPENMODELICA_LAPACK 1\n"
               << "CALLS " << calls.load() << "\n"
               << "SOLVED " << solved.load() << "\n"
               << "FALLBACKS " << fallbacks.load() << "\n"
               << "TOTAL_UNKNOWNS " << unknowns.load() << "\nEND\n";
    }
} report_writer;

}  // namespace

extern "C" void dgesv_(const int* order, const int* right_hand_sides,
                         double* matrix, const int* leading_dimension,
                         int* pivots, double* right_hand_side,
                         const int* rhs_leading_dimension, int* info) {
    ++calls;
    auto fallback = original_dgesv();
    if (!order || !right_hand_sides || !leading_dimension ||
        !rhs_leading_dimension || !info || *order < 0 ||
        *right_hand_sides < 0 || *leading_dimension < std::max(1, *order) ||
        *rhs_leading_dimension < std::max(1, *order) || *order > 4096) {
        ++fallbacks;
        if (fallback) fallback(order, right_hand_sides, matrix, leading_dimension,
                               pivots, right_hand_side, rhs_leading_dimension, info);
        else if (info) *info = -1;
        return;
    }
    if (*order == 0 || *right_hand_sides == 0) {
        *info = 0;
        ++solved;
        return;
    }
    if (!matrix || !right_hand_side) {
        ++fallbacks;
        if (fallback) fallback(order, right_hand_sides, matrix, leading_dimension,
                               pivots, right_hand_side, rhs_leading_dimension, info);
        else *info = -1;
        return;
    }
    smave::LinearSystem system;
    system.matrix.assign(*order, std::vector<double>(*order));
    for (int row = 0; row < *order; ++row) {
        for (int column = 0; column < *order; ++column)
            system.matrix[row][column] = matrix[column * *leading_dimension + row];
    }
    std::vector<std::vector<double>> solutions(
        *right_hand_sides, std::vector<double>(*order));
    bool success = true;
    int singular_pivot = 0;
    for (int rhs = 0; rhs < *right_hand_sides; ++rhs) {
        system.right_hand_side.assign(
            right_hand_side + rhs * *rhs_leading_dimension,
            right_hand_side + rhs * *rhs_leading_dimension + *order);
        success = smave::dense_direct_solve(system, solutions[rhs]);
        if (!success) {
            const int lapack_status = solve_lapack_semantics(
                system.matrix, system.right_hand_side, solutions[rhs]);
            success = lapack_status == 0;
            if (lapack_status > 0) singular_pivot = lapack_status;
        }
        if (!success) break;
    }
    if (singular_pivot > 0) {
        *info = singular_pivot;
        ++solved;
        return;
    }
    if (!success) {
        ++fallbacks;
        if (fallback) fallback(order, right_hand_sides, matrix, leading_dimension,
                               pivots, right_hand_side, rhs_leading_dimension, info);
        else *info = 1;
        return;
    }
    for (int rhs = 0; rhs < *right_hand_sides; ++rhs)
        std::copy(solutions[rhs].begin(), solutions[rhs].end(),
                  right_hand_side + rhs * *rhs_leading_dimension);
    if (pivots)
        for (int index = 0; index < *order; ++index) pivots[index] = index + 1;
    *info = 0;
    ++solved;
    unknowns += static_cast<std::size_t>(*order) * *right_hand_sides;
}
