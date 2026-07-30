#include "smave/linear.hpp"
#include "smave/routing.hpp"
#include "smave/solve_service.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t peak_rss_bytes() {
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
#else
    return 0;
#endif
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

smave::LinearSystem poisson(std::size_t width) {
    const auto size = width * width;
    smave::LinearSystem system;
    system.unknowns.resize(size);
    system.right_hand_side.resize(size);
    std::vector<std::vector<std::size_t>> rows(size);
    for (std::size_t row = 0; row < width; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const auto index = row * width + column;
            system.unknowns[index] = "x" + std::to_string(index + 1);
            system.right_hand_side[index] = 1.0 + 0.001 * static_cast<double>(index);
            rows[index].push_back(index);
            if (column > 0) rows[index].push_back(index - 1);
            if (column + 1 < width) rows[index].push_back(index + 1);
            if (row > 0) rows[index].push_back(index - width);
            if (row + 1 < width) rows[index].push_back(index + width);
            std::sort(rows[index].begin(), rows[index].end());
        }
    }
    system.sparsity = smave::SparsityPattern::from_rows(size, rows);
    system.sparse_values.resize(system.sparsity.nonzeros());
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            system.sparse_values[offset] =
                system.sparsity.column_indices[offset] == row ? 4.0 : -1.0;
        }
    }
    system.symmetric = true;
    system.positive_definite = true;
    system.diagonal_condition_estimate = 1.0;
    return system;
}

double residual_inf(
    const smave::LinearSystem& system,
    const std::vector<double>& solution) {
    const auto product = system.multiply(solution);
    double maximum{};
    for (std::size_t index = 0; index < product.size(); ++index) {
        maximum = std::max(
            maximum, std::abs(product[index] - system.right_hand_side[index]));
    }
    return maximum;
}

struct Measurement {
    std::size_t width{};
    std::size_t unknowns{};
    std::size_t nonzeros{};
    std::size_t levels{};
    std::size_t storage_bytes{};
    std::size_t dense_bytes{};
    double amg_median_us{};
    double ic0_median_us{};
    double amg_residual_inf{};
    double ic0_residual_inf{};
    double amg_mean_iterations{};
    double ic0_mean_iterations{};
    double amg_speedup{};
};

Measurement measure(std::size_t width) {
    const auto system = poisson(width);
    constexpr std::size_t repetitions = 10;
    std::vector<double> amg_times;
    std::vector<double> ic0_times;
    std::size_t amg_iterations{};
    std::size_t ic0_iterations{};
    smave::AggregationAmgPcgResult last_amg;
    smave::KrylovResult last_ic0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        const auto run_amg = [&] {
            const auto started = std::chrono::steady_clock::now();
            last_amg = smave::aggregation_amg_pcg_solve(system);
            amg_times.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count());
            require(last_amg.solved, "AMG-PCG failed on admitted Poisson system");
            amg_iterations += static_cast<std::size_t>(last_amg.iterations);
        };
        const auto run_ic0 = [&] {
            const auto started = std::chrono::steady_clock::now();
            last_ic0 = smave::preconditioned_conjugate_gradient(
                system,
                std::vector<double>(system.size()),
                smave::incomplete_cholesky_zero_preconditioner(
                    system, system.sparsity),
                1.0e-12,
                1.0e-10,
                2000);
            ic0_times.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count());
            require(last_ic0.converged, "IC0-PCG failed on Poisson system");
            ic0_iterations += static_cast<std::size_t>(last_ic0.iterations);
        };
        if (repetition % 2 == 0) {
            run_amg();
            run_ic0();
        } else {
            run_ic0();
            run_amg();
        }
    }
    Measurement measurement{
        .width = width,
        .unknowns = system.size(),
        .nonzeros = system.nonzeros(),
        .levels = last_amg.levels,
        .storage_bytes = last_amg.storage_bytes,
        .dense_bytes = system.size() * system.size() * sizeof(double),
        .amg_median_us = median(amg_times),
        .ic0_median_us = median(ic0_times),
        .amg_residual_inf = residual_inf(system, last_amg.solution),
        .ic0_residual_inf = residual_inf(system, last_ic0.solution),
        .amg_mean_iterations = static_cast<double>(amg_iterations) / repetitions,
        .ic0_mean_iterations = static_cast<double>(ic0_iterations) / repetitions,
        .amg_speedup = median(ic0_times) / median(amg_times),
    };
    require(measurement.levels >= 2 && measurement.storage_bytes > 0 &&
                measurement.storage_bytes < measurement.dense_bytes &&
                measurement.amg_residual_inf <= 1.0e-7 &&
                measurement.ic0_residual_inf <= 1.0e-7,
            "AMG scale evidence failed storage or residual contract");
    return measurement;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: smave_amg_backend_evidence OUTPUT\n";
            return 2;
        }
        const auto rss_before = peak_rss_bytes();
        std::vector<Measurement> measurements;
        for (const auto width : {16U, 32U, 64U}) measurements.push_back(measure(width));
        const auto rss_after = peak_rss_bytes();

        auto non_spd = poisson(16);
        non_spd.positive_definite = false;
        const auto non_spd_result = smave::aggregation_amg_pcg_solve(non_spd);
        auto irregular = poisson(16);
        irregular.sparsity.column_indices[1] = irregular.size() - 1;
        const auto irregular_result = smave::aggregation_amg_pcg_solve(irregular);
        require(!non_spd_result.eligible && !non_spd_result.solved &&
                    !irregular_result.eligible && !irregular_result.solved,
                "AMG capability rejection failed");

        const auto service = smave::verified_linear_solve(poisson(32));
        require(service.success && !service.used_fallback &&
                    service.backend == "pcg-aggregation-amg-cpu-v1" &&
                    service.residual_inf <= 1.0e-7,
                "verified linear service did not select admitted AMG backend");

        const smave::SparseLinearProfile admitted_profile{
            .fingerprint = "amg-admitted",
            .rows = 1024,
            .columns = 1024,
            .nonzeros = 4992,
            .structurally_symmetric = true,
            .numerically_symmetric = true,
            .numerically_positive_definite = true,
            .diagonal_condition_estimate = 1.0,
            .regular_grid = true,
            .grid_dimension = 2,
        };
        const auto admitted_plan = smave::route_sparse_linear_system(admitted_profile);
        const bool router_admitted = std::any_of(
            admitted_plan.steps.begin(), admitted_plan.steps.end(), [](const auto& step) {
                return step.expert_version == "pcg-aggregation-amg-cpu-v1";
            });
        auto rejected_profile = admitted_profile;
        rejected_profile.regular_grid = false;
        const auto rejected_plan = smave::route_sparse_linear_system(rejected_profile);
        const bool router_rejected = std::none_of(
            rejected_plan.steps.begin(), rejected_plan.steps.end(), [](const auto& step) {
                return step.expert_version == "pcg-aggregation-amg-cpu-v1";
            });
        require(router_admitted && router_rejected, "AMG Router capability gate failed");

        const auto& largest = measurements.back();

        std::ofstream output(argv[1]);
        if (!output) throw std::runtime_error("cannot write AMG evidence");
        output << std::setprecision(17)
               << "SMAVE_AGGREGATION_AMG_BACKEND 1\n"
               << "backend=pcg-aggregation-amg-cpu-v1\n"
               << "contract=square-five-point-numerically-spd-csr\n"
               << "repetitions_per_scale=10\n"
               << "scales=3\n"
               << "router_admitted=1\n"
               << "router_irregular_rejected=1\n"
               << "non_spd_rejected=1\n"
               << "irregular_topology_rejected=1\n"
               << "verified_linear_service_backend=" << service.backend << '\n'
               << "verified_linear_service_success=1\n"
               << "verified_linear_service_fallback=0\n"
               << "rss_before_bytes=" << rss_before << '\n'
               << "rss_after_bytes=" << rss_after << '\n'
               << "largest_unknowns=" << largest.unknowns << '\n'
               << "largest_nonzeros=" << largest.nonzeros << '\n'
               << "largest_levels=" << largest.levels << '\n'
               << "largest_storage_bytes=" << largest.storage_bytes << '\n'
               << "largest_dense_bytes=" << largest.dense_bytes << '\n'
               << "largest_amg_median_us=" << largest.amg_median_us << '\n'
               << "largest_ic0_median_us=" << largest.ic0_median_us << '\n'
               << "largest_amg_speedup=" << largest.amg_speedup << '\n'
               << "largest_amg_mean_iterations=" << largest.amg_mean_iterations << '\n'
               << "largest_ic0_mean_iterations=" << largest.ic0_mean_iterations << '\n'
               << "largest_amg_residual_inf=" << largest.amg_residual_inf << '\n';
        for (const auto& measurement : measurements) {
            output << "SCALE width=" << measurement.width
                   << " unknowns=" << measurement.unknowns
                   << " nonzeros=" << measurement.nonzeros
                   << " levels=" << measurement.levels
                   << " storage_bytes=" << measurement.storage_bytes
                   << " dense_bytes=" << measurement.dense_bytes
                   << " amg_median_us=" << measurement.amg_median_us
                   << " ic0_median_us=" << measurement.ic0_median_us
                   << " amg_mean_iterations=" << measurement.amg_mean_iterations
                   << " ic0_mean_iterations=" << measurement.ic0_mean_iterations
                   << " amg_speedup=" << measurement.amg_speedup
                   << " amg_residual_inf=" << measurement.amg_residual_inf
                   << " ic0_residual_inf=" << measurement.ic0_residual_inf << '\n';
        }
        output << "END\n";
        std::cout << "aggregation AMG backend evidence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "aggregation AMG evidence failure: " << error.what() << '\n';
        return 1;
    }
}
