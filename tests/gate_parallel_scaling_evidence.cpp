#include "smave/compiler.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using Values = std::unordered_map<std::string, double>;
constexpr std::array<std::size_t, 5> worker_counts{1, 2, 4, 8, 10};

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

std::pair<double, double> bootstrap_interval(
    const std::vector<double>& values,
    std::uint64_t seed) {
    std::mt19937_64 generator(seed);
    std::uniform_int_distribution<std::size_t> sample(0, values.size() - 1);
    std::vector<double> medians;
    medians.reserve(10000);
    std::vector<double> draw(values.size());
    for (std::size_t iteration = 0; iteration < 10000; ++iteration) {
        for (double& value : draw) value = values[sample(generator)];
        medians.push_back(median(draw));
    }
    std::sort(medians.begin(), medians.end());
    return {medians[249], medians[9749]};
}

std::vector<Values> linear_values(
    const smave::ModelIR& model,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory) {
    std::vector<std::filesystem::path> scenario_paths;
    for (const auto& entry : std::filesystem::directory_iterator(scenario_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            scenario_paths.push_back(entry.path());
        }
    }
    std::sort(scenario_paths.begin(), scenario_paths.end());
    std::vector<Values> values;
    const smave::Runtime runtime(model);
    for (const auto& path : scenario_paths) {
        const auto scenario = smave::read_scenario(path);
        const auto outcome = runtime.solve(
            scenario, trace_directory / std::to_string(values.size()));
        if (!outcome.success) {
            throw std::runtime_error("linear gate scaling fixture solve failed");
        }
        values.push_back(outcome.values);
    }
    auto rejected = values.front();
    rejected.at(model.blocks.front().unknowns.front()) += 1.0e-2;
    values.push_back(std::move(rejected));
    return values;
}

std::vector<Values> nonlinear_values(const smave::ModelIR& model) {
    std::vector<Values> values;
    values.reserve(65);
    for (std::size_t index = 0; index < 64; ++index) {
        const double parameter = 1.5 + 0.01 * static_cast<double>(index);
        Values item{{"p", parameter}};
        for (const auto& variable : model.variables) {
            if (!item.contains(variable.name)) {
                item.emplace(variable.name, variable.start);
            }
        }
        item.insert_or_assign("x", parameter + 1.0);
        item.insert_or_assign("y", 2.0 * parameter + 1.0);
        values.push_back(std::move(item));
    }
    auto rejected = values.front();
    rejected.at("x") += 1.0e-2;
    values.push_back(std::move(rejected));
    return values;
}

std::vector<Values> expand_values(const std::vector<Values>& source) {
    std::vector<Values> expanded;
    expanded.reserve(source.size() * 32);
    for (std::size_t repetition = 0; repetition < 32; ++repetition) {
        expanded.insert(expanded.end(), source.begin(), source.end());
    }
    return expanded;
}

struct ScalingResult {
    std::string family;
    std::size_t requests{};
    std::size_t repetitions{};
    std::array<double, worker_counts.size()> median_total_us{};
    std::array<double, worker_counts.size()> paired_speedup{};
    std::array<double, worker_counts.size()> interval_lower{};
    std::array<double, worker_counts.size()> interval_upper{};
    std::array<std::vector<double>, worker_counts.size()> timing_samples;
    std::size_t decision_mismatches{};
    std::size_t residual_mismatches{};
};

ScalingResult measure(
    std::string family,
    const smave::ModelIR& model,
    const std::vector<Values>& source_values,
    std::size_t repetitions) {
    const auto values = expand_values(source_values);
    const smave::Runtime runtime(model);
    const auto& block = model.blocks.front();
    std::vector<smave::GateResult> authority(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        authority[index] = runtime.evaluate_gate_fused(block, values[index], true);
    }

    std::array<std::vector<double>, worker_counts.size()> timings;
    for (auto& worker_timings : timings) worker_timings.reserve(repetitions);
    ScalingResult result{
        .family = std::move(family),
        .requests = values.size(),
        .repetitions = repetitions,
    };

    const auto run = [&](std::size_t worker_count) {
        std::vector<smave::GateResult> observed(values.size());
        const auto started = std::chrono::steady_clock::now();
        std::vector<std::thread> workers;
        workers.reserve(worker_count);
        for (std::size_t worker = 0; worker < worker_count; ++worker) {
            workers.emplace_back([&, worker] {
                for (std::size_t index = worker; index < values.size();
                     index += worker_count) {
                    observed[index] = runtime.evaluate_gate_fused(
                        block, values[index], true);
                }
            });
        }
        for (auto& worker : workers) worker.join();
        const double elapsed = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - started).count();
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (observed[index].decision != authority[index].decision) {
                ++result.decision_mismatches;
            }
            const double scale = std::max(
                {1.0, std::abs(observed[index].residual_inf),
                 std::abs(authority[index].residual_inf)});
            if (std::abs(observed[index].residual_inf - authority[index].residual_inf) >
                1.0e-15 * scale) {
                ++result.residual_mismatches;
            }
        }
        return elapsed;
    };

    for (const auto worker_count : worker_counts) {
        static_cast<void>(run(worker_count));
    }
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        if (repetition % 2 == 0) {
            for (std::size_t index = 0; index < worker_counts.size(); ++index) {
                timings[index].push_back(run(worker_counts[index]));
            }
        } else {
            for (std::size_t index = worker_counts.size(); index-- > 0;) {
                timings[index].push_back(run(worker_counts[index]));
            }
        }
    }

    for (std::size_t index = 0; index < worker_counts.size(); ++index) {
        result.timing_samples[index] = timings[index];
        result.median_total_us[index] = median(timings[index]);
        std::vector<double> paired(repetitions);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            paired[repetition] = timings[0][repetition] / timings[index][repetition];
        }
        result.paired_speedup[index] = median(paired);
        const auto interval = bootstrap_interval(
            paired, UINT64_C(20260726) + index * 100 + result.family.size());
        result.interval_lower[index] = interval.first;
        result.interval_upper[index] = interval.second;
    }
    return result;
}

void write_raw_samples(
    const std::filesystem::path& path,
    const ScalingResult& linear,
    const ScalingResult& nonlinear) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write gate scaling raw samples");
    output << std::setprecision(17)
           << "SMAVE_GATE_PARALLEL_SCALING_SAMPLES 1\n"
           << "contract=paired-fused-original-equation-gate-worker-scaling-raw-samples\n"
           << "families=linear,nonlinear\n"
           << "workers=1,2,4,8,10\n"
           << "repetitions=30\n";
    for (const auto* result : {&linear, &nonlinear}) {
        for (std::size_t repetition = 0; repetition < result->repetitions; ++repetition) {
            for (std::size_t index = 0; index < worker_counts.size(); ++index) {
                output << result->family << ".repetition_" << repetition + 1
                       << ".worker_" << worker_counts[index] << ".total_us="
                       << result->timing_samples[index][repetition] << '\n';
            }
        }
    }
    output << "END\n";
}

void write_result(std::ostream& output, const ScalingResult& result) {
    output << result.family << ".requests_per_repetition=" << result.requests << '\n'
           << result.family << ".repetitions=" << result.repetitions << '\n';
    for (std::size_t index = 0; index < worker_counts.size(); ++index) {
        const std::string prefix = result.family + ".worker_" +
            std::to_string(worker_counts[index]);
        output << prefix << ".total_median_us=" << result.median_total_us[index] << '\n'
               << prefix << ".paired_speedup=" << result.paired_speedup[index] << '\n'
               << prefix << ".bootstrap_95_lower=" << result.interval_lower[index] << '\n'
               << prefix << ".bootstrap_95_upper=" << result.interval_upper[index] << '\n';
    }
    output << result.family << ".decision_mismatches="
           << result.decision_mismatches << '\n'
           << result.family << ".residual_mismatches="
           << result.residual_mismatches << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            throw std::invalid_argument(
                "usage: gate_parallel_scaling_evidence linear.mo scenarios "
                "nonlinear.mo output");
        }
        const auto linear_model = smave::compile_model(argv[1]);
        const auto nonlinear_model = smave::compile_model(argv[3]);
        const auto output_path = std::filesystem::path(argv[4]);
        const auto linear = measure(
            "linear", linear_model,
            linear_values(
                linear_model, argv[2], output_path.parent_path() / "traces"),
            30);
        const auto nonlinear = measure(
            "nonlinear", nonlinear_model, nonlinear_values(nonlinear_model), 30);
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        if (!output) throw std::runtime_error("cannot write gate scaling evidence");
        output << std::setprecision(17)
               << "SMAVE_GATE_PARALLEL_SCALING 1\n"
               << "contract=paired-fused-original-equation-gate-worker-scaling\n"
               << "families=linear,nonlinear\n"
               << "workers=1,2,4,8,10\n"
               << "bootstrap_resamples=10000\n";
        write_result(output, linear);
        write_result(output, nonlinear);
        const bool equivalent = linear.decision_mismatches == 0 &&
            linear.residual_mismatches == 0 &&
            nonlinear.decision_mismatches == 0 &&
            nonlinear.residual_mismatches == 0;
        output << "strict_equivalence=" << equivalent << '\n'
               << "END\n";
        write_raw_samples(output_path.parent_path() / "raw-samples.txt", linear, nonlinear);
        std::cout << "SMAVE_GATE_PARALLEL_SCALING " << equivalent << '\n';
        return equivalent ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "gate parallel scaling failure: " << error.what() << '\n';
        return 2;
    }
}
