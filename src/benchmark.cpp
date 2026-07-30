#include "smave/benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

std::vector<std::filesystem::path> scenario_files(
    const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::invalid_argument("benchmark scenario path is not a directory");
    }
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    if (result.empty()) throw std::invalid_argument("benchmark suite has no .conf files");
    return result;
}

double quantile(const std::vector<double>& sorted, double probability) {
    if (sorted.empty()) return 0.0;
    const double position = probability * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - fraction) + sorted[upper] * fraction;
}

DistributionSummary summarize(std::vector<double> values) {
    if (values.empty()) return {};
    std::sort(values.begin(), values.end());
    return DistributionSummary{
        .minimum = values.front(),
        .mean = std::accumulate(values.begin(), values.end(), 0.0) /
            static_cast<double>(values.size()),
        .median = quantile(values, 0.50),
        .p90 = quantile(values, 0.90),
        .p99 = quantile(values, 0.99),
        .maximum = values.back(),
    };
}

struct BootstrapInterval {
    double estimate{};
    double p01{};
    double win_rate{};
    double lower{};
    double upper{};
};

BootstrapInterval paired_median_speedup_interval(
    const std::vector<double>& baseline,
    const std::vector<double>& accelerated,
    std::size_t bootstrap_samples) {
    if (baseline.size() != accelerated.size() || baseline.empty() ||
        bootstrap_samples == 0) {
        throw std::invalid_argument("paired benchmark samples are invalid");
    }
    std::vector<double> ratios;
    ratios.reserve(baseline.size());
    for (std::size_t index = 0; index < baseline.size(); ++index) {
        if (!(baseline[index] > 0.0) || !(accelerated[index] > 0.0)) {
            throw std::invalid_argument("paired benchmark wall time must be positive");
        }
        ratios.push_back(baseline[index] / accelerated[index]);
    }
    auto sorted_ratios = ratios;
    std::sort(sorted_ratios.begin(), sorted_ratios.end());
    BootstrapInterval interval;
    interval.estimate = quantile(sorted_ratios, 0.5);
    interval.p01 = quantile(sorted_ratios, 0.01);
    interval.win_rate = static_cast<double>(std::count_if(
        ratios.begin(), ratios.end(), [](double ratio) { return ratio > 1.0; })) /
        static_cast<double>(ratios.size());
    std::mt19937_64 generator(0x534d415645ULL);
    std::uniform_int_distribution<std::size_t> sample_index(0, ratios.size() - 1);
    std::vector<double> bootstrap_estimates;
    bootstrap_estimates.reserve(bootstrap_samples);
    std::vector<double> resampled(ratios.size());
    for (std::size_t sample = 0; sample < bootstrap_samples; ++sample) {
        for (double& value : resampled) value = ratios[sample_index(generator)];
        std::sort(resampled.begin(), resampled.end());
        bootstrap_estimates.push_back(quantile(resampled, 0.5));
    }
    std::sort(bootstrap_estimates.begin(), bootstrap_estimates.end());
    interval.lower = quantile(bootstrap_estimates, 0.025);
    interval.upper = quantile(bootstrap_estimates, 0.975);
    return interval;
}

struct TimedOutcome {
    SolveOutcome outcome;
    double wall_us{};
};

void discard_trace(
    const std::filesystem::path& trace_directory,
    const SolveOutcome& outcome) {
    if (outcome.trace_id.empty()) return;
    std::error_code error;
    std::filesystem::remove(
        trace_directory / (outcome.trace_id + ".trace"), error);
}

TimedOutcome timed_solve(
    const Runtime& runtime,
    const std::unordered_map<std::string, double>& scenario,
    const std::filesystem::path& trace_directory,
    bool retain_trace) {
    const auto started = std::chrono::steady_clock::now();
    auto outcome = runtime.solve(scenario, trace_directory);
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - started).count();
    if (!retain_trace) discard_trace(trace_directory, outcome);
    return {std::move(outcome), elapsed};
}

double iterations(const SolveOutcome& outcome) {
    double result = 0.0;
    for (const auto& block : outcome.blocks) {
        result += block.expert_iterations + block.fallback_iterations;
    }
    return result;
}

double mixed_qoi_error(
    const SolveOutcome& baseline,
    const SolveOutcome& accelerated) {
    double maximum = 0.0;
    for (const auto& [name, reference] : baseline.values) {
        const auto candidate = accelerated.values.find(name);
        if (candidate == accelerated.values.end()) return std::numeric_limits<double>::infinity();
        const double denominator = 1.0e-10 + 1.0e-4 * std::abs(reference);
        maximum = std::max(maximum, std::abs(candidate->second - reference) / denominator);
    }
    return maximum;
}

std::string performance_hash(const PerformanceReport& report) {
    std::ostringstream contract;
    contract << std::setprecision(17) << report.schema_version << '|';
    if (report.schema_version >= 2) {
        contract << report.dataset_id << '|' << report.dataset_version << '|'
                 << report.dataset_manifest_hash << '|';
    }
    contract << report.scenarios << '|' << report.repetitions << '|' << report.samples << '|';
    contract << report.baseline_wall_us.median << '|' << report.baseline_wall_us.p90 << '|'
             << report.baseline_wall_us.p99 << '|' << report.accelerated_wall_us.median << '|'
             << report.accelerated_wall_us.p90 << '|' << report.accelerated_wall_us.p99 << '|'
             << report.baseline_iterations.median << '|' << report.baseline_iterations.mean << '|'
             << report.accelerated_iterations.median << '|'
             << report.accelerated_iterations.mean << '|';
    contract << report.median_speedup << '|' << report.p99_speedup << '|'
             << report.paired_median_speedup << '|' << report.paired_p01_speedup << '|'
             << report.paired_win_rate << '|' << report.paired_speedup_ci95_lower << '|'
             << report.paired_speedup_ci95_upper << '|' << report.bootstrap_samples << '|'
             << report.maximum_mixed_qoi_error << '|' << report.baseline_failures << '|'
             << report.accelerated_failures << '|' << report.gate_mismatches << '|'
             << report.same_accuracy << '|' << report.p99_not_regressed;
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : contract.str()) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string field_value(const std::string& line, std::string_view key) {
    if (!line.starts_with(key)) throw std::invalid_argument("invalid performance field");
    return line.substr(key.size());
}

template <typename Value>
Value numeric_field(const std::string& line, std::string_view key) {
    std::istringstream input(field_value(line, key));
    Value result{};
    if (!(input >> result) || !input.eof()) {
        throw std::invalid_argument("invalid performance numeric field");
    }
    return result;
}

bool boolean_field(const std::string& line, std::string_view key) {
    const auto text = field_value(line, key);
    if (text == "1") return true;
    if (text == "0") return false;
    throw std::invalid_argument("invalid performance boolean field");
}

}  // namespace

void PerformanceReport::seal() { report_hash = performance_hash(*this); }

void PerformanceReport::validate() const {
    if ((schema_version != 1 && schema_version != 2) || scenarios == 0 ||
        repetitions == 0 || samples != scenarios * repetitions || bootstrap_samples == 0 ||
        paired_win_rate < 0.0 || paired_win_rate > 1.0 ||
        paired_speedup_ci95_upper < paired_speedup_ci95_lower) {
        throw std::invalid_argument("invalid performance report");
    }
    const bool has_dataset = !dataset_id.empty() || !dataset_version.empty() ||
        !dataset_manifest_hash.empty();
    if (schema_version == 1 && has_dataset) {
        throw std::invalid_argument("performance v1 cannot contain dataset lineage");
    }
    if (schema_version == 2 &&
        (dataset_id.empty() || dataset_version.empty() || dataset_manifest_hash.empty())) {
        throw std::invalid_argument("performance v2 requires complete dataset lineage");
    }
    if (schema_version == 1) {
        if (!report_hash.empty()) {
            throw std::invalid_argument("performance v1 cannot contain a report hash");
        }
    } else if (report_hash != performance_hash(*this)) {
        throw std::invalid_argument("performance report integrity check failed");
    }
}

PerformanceReport benchmark_runtimes(
    const Runtime& baseline,
    const Runtime& accelerated,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t repetitions,
    std::size_t warmup_repetitions,
    std::size_t bootstrap_samples) {
    if (repetitions == 0 || bootstrap_samples == 0) {
        throw std::invalid_argument("benchmark repetitions and bootstrap samples must be positive");
    }
    const auto paths = scenario_files(scenario_directory);
    PerformanceReport report;
    report.scenarios = paths.size();
    report.repetitions = repetitions;
    std::vector<double> baseline_times;
    std::vector<double> accelerated_times;
    std::vector<double> baseline_iterations;
    std::vector<double> accelerated_iterations;
    for (const auto& path : paths) {
        const auto scenario = read_scenario(path);
        for (std::size_t warmup = 0; warmup < warmup_repetitions; ++warmup) {
            auto baseline_warmup = baseline.solve(scenario, std::filesystem::path{});
            auto accelerated_warmup = accelerated.solve(scenario, std::filesystem::path{});
        }
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            TimedOutcome baseline_result;
            TimedOutcome accelerated_result;
            const bool retain = repetition == 0;
            if (repetition % 2 == 0) {
                baseline_result = timed_solve(
                    baseline, scenario,
                    retain ? (trace_directory / "baseline") : std::filesystem::path{}, retain);
                accelerated_result = timed_solve(
                    accelerated, scenario,
                    retain ? (trace_directory / "accelerated") : std::filesystem::path{}, retain);
            } else {
                accelerated_result = timed_solve(
                    accelerated, scenario, std::filesystem::path{}, false);
                baseline_result = timed_solve(
                    baseline, scenario, std::filesystem::path{}, false);
            }
            baseline_times.push_back(baseline_result.wall_us);
            accelerated_times.push_back(accelerated_result.wall_us);
            baseline_iterations.push_back(iterations(baseline_result.outcome));
            accelerated_iterations.push_back(iterations(accelerated_result.outcome));
            if (!baseline_result.outcome.success) ++report.baseline_failures;
            if (!accelerated_result.outcome.success) ++report.accelerated_failures;
            const double error = mixed_qoi_error(
                baseline_result.outcome, accelerated_result.outcome);
            report.maximum_mixed_qoi_error = std::max(
                report.maximum_mixed_qoi_error, error);
            if (error > 1.0) ++report.gate_mismatches;
        }
    }
    report.samples = baseline_times.size();
    report.baseline_wall_us = summarize(baseline_times);
    report.accelerated_wall_us = summarize(accelerated_times);
    report.baseline_iterations = summarize(baseline_iterations);
    report.accelerated_iterations = summarize(accelerated_iterations);
    report.median_speedup = report.accelerated_wall_us.median > 0.0
        ? report.baseline_wall_us.median / report.accelerated_wall_us.median
        : 0.0;
    report.p99_speedup = report.accelerated_wall_us.p99 > 0.0
        ? report.baseline_wall_us.p99 / report.accelerated_wall_us.p99
        : 0.0;
    report.bootstrap_samples = bootstrap_samples;
    const auto paired_speedup = paired_median_speedup_interval(
        baseline_times, accelerated_times, report.bootstrap_samples);
    report.paired_median_speedup = paired_speedup.estimate;
    report.paired_p01_speedup = paired_speedup.p01;
    report.paired_win_rate = paired_speedup.win_rate;
    report.paired_speedup_ci95_lower = paired_speedup.lower;
    report.paired_speedup_ci95_upper = paired_speedup.upper;
    report.same_accuracy = report.baseline_failures == 0 &&
        report.accelerated_failures == 0 && report.gate_mismatches == 0 &&
        report.maximum_mixed_qoi_error <= 1.0;
    report.p99_not_regressed = report.accelerated_wall_us.p99 <=
        report.baseline_wall_us.p99;
    return report;
}

void write_performance_report(
    const PerformanceReport& report,
    const std::filesystem::path& path) {
    report.validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write performance report");
    output << std::setprecision(17)
           << "SMAVE_PERFORMANCE " << report.schema_version << '\n';
    if (report.schema_version >= 2) {
        output << "dataset_id=" << report.dataset_id << '\n'
               << "dataset_version=" << report.dataset_version << '\n'
               << "dataset_manifest_hash=" << report.dataset_manifest_hash << '\n';
    }
    output << "scenarios=" << report.scenarios << '\n'
           << "repetitions=" << report.repetitions << '\n'
           << "samples=" << report.samples << '\n'
           << "baseline_median_us=" << report.baseline_wall_us.median << '\n'
           << "baseline_p90_us=" << report.baseline_wall_us.p90 << '\n'
           << "baseline_p99_us=" << report.baseline_wall_us.p99 << '\n'
           << "accelerated_median_us=" << report.accelerated_wall_us.median << '\n'
           << "accelerated_p90_us=" << report.accelerated_wall_us.p90 << '\n'
           << "accelerated_p99_us=" << report.accelerated_wall_us.p99 << '\n'
           << "baseline_median_iterations=" << report.baseline_iterations.median << '\n'
           << "baseline_mean_iterations=" << report.baseline_iterations.mean << '\n'
           << "accelerated_median_iterations=" << report.accelerated_iterations.median << '\n'
           << "accelerated_mean_iterations=" << report.accelerated_iterations.mean << '\n'
           << "median_speedup=" << report.median_speedup << '\n'
           << "p99_speedup=" << report.p99_speedup << '\n'
           << "paired_median_speedup=" << report.paired_median_speedup << '\n'
           << "paired_p01_speedup=" << report.paired_p01_speedup << '\n'
           << "paired_win_rate=" << report.paired_win_rate << '\n'
           << "paired_speedup_ci95_lower=" << report.paired_speedup_ci95_lower << '\n'
           << "paired_speedup_ci95_upper=" << report.paired_speedup_ci95_upper << '\n'
           << "bootstrap_samples=" << report.bootstrap_samples << '\n'
           << "maximum_mixed_qoi_error=" << report.maximum_mixed_qoi_error << '\n'
           << "baseline_failures=" << report.baseline_failures << '\n'
           << "accelerated_failures=" << report.accelerated_failures << '\n'
           << "gate_mismatches=" << report.gate_mismatches << '\n'
           << "same_accuracy=" << report.same_accuracy << '\n'
           << "p99_not_regressed=" << report.p99_not_regressed << '\n';
    if (report.schema_version >= 2) {
        output << "report_hash=" << report.report_hash << '\n';
    }
    output
           << "END\n";
}

PerformanceReport read_performance_report(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read performance report");
    std::string magic;
    int version{};
    if (!(input >> magic >> version) || magic != "SMAVE_PERFORMANCE" ||
        (version != 1 && version != 2)) {
        throw std::invalid_argument("unsupported performance report");
    }
    std::string line;
    std::getline(input, line);
    PerformanceReport report;
    report.schema_version = version;
    bool ended = false;
    while (std::getline(input, line)) {
        if (line == "END") { ended = true; break; }
        if (line.starts_with("dataset_id=")) report.dataset_id = field_value(line, "dataset_id=");
        else if (line.starts_with("dataset_version=")) report.dataset_version = field_value(line, "dataset_version=");
        else if (line.starts_with("dataset_manifest_hash=")) report.dataset_manifest_hash = field_value(line, "dataset_manifest_hash=");
        else if (line.starts_with("scenarios=")) report.scenarios = numeric_field<std::size_t>(line, "scenarios=");
        else if (line.starts_with("repetitions=")) report.repetitions = numeric_field<std::size_t>(line, "repetitions=");
        else if (line.starts_with("samples=")) report.samples = numeric_field<std::size_t>(line, "samples=");
        else if (line.starts_with("baseline_median_us=")) report.baseline_wall_us.median = numeric_field<double>(line, "baseline_median_us=");
        else if (line.starts_with("baseline_p90_us=")) report.baseline_wall_us.p90 = numeric_field<double>(line, "baseline_p90_us=");
        else if (line.starts_with("baseline_p99_us=")) report.baseline_wall_us.p99 = numeric_field<double>(line, "baseline_p99_us=");
        else if (line.starts_with("accelerated_median_us=")) report.accelerated_wall_us.median = numeric_field<double>(line, "accelerated_median_us=");
        else if (line.starts_with("accelerated_p90_us=")) report.accelerated_wall_us.p90 = numeric_field<double>(line, "accelerated_p90_us=");
        else if (line.starts_with("accelerated_p99_us=")) report.accelerated_wall_us.p99 = numeric_field<double>(line, "accelerated_p99_us=");
        else if (line.starts_with("baseline_median_iterations=")) report.baseline_iterations.median = numeric_field<double>(line, "baseline_median_iterations=");
        else if (line.starts_with("baseline_mean_iterations=")) report.baseline_iterations.mean = numeric_field<double>(line, "baseline_mean_iterations=");
        else if (line.starts_with("accelerated_median_iterations=")) report.accelerated_iterations.median = numeric_field<double>(line, "accelerated_median_iterations=");
        else if (line.starts_with("accelerated_mean_iterations=")) report.accelerated_iterations.mean = numeric_field<double>(line, "accelerated_mean_iterations=");
        else if (line.starts_with("median_speedup=")) report.median_speedup = numeric_field<double>(line, "median_speedup=");
        else if (line.starts_with("p99_speedup=")) report.p99_speedup = numeric_field<double>(line, "p99_speedup=");
        else if (line.starts_with("paired_median_speedup=")) report.paired_median_speedup = numeric_field<double>(line, "paired_median_speedup=");
        else if (line.starts_with("paired_p01_speedup=")) report.paired_p01_speedup = numeric_field<double>(line, "paired_p01_speedup=");
        else if (line.starts_with("paired_win_rate=")) report.paired_win_rate = numeric_field<double>(line, "paired_win_rate=");
        else if (line.starts_with("paired_speedup_ci95_lower=")) report.paired_speedup_ci95_lower = numeric_field<double>(line, "paired_speedup_ci95_lower=");
        else if (line.starts_with("paired_speedup_ci95_upper=")) report.paired_speedup_ci95_upper = numeric_field<double>(line, "paired_speedup_ci95_upper=");
        else if (line.starts_with("bootstrap_samples=")) report.bootstrap_samples = numeric_field<std::size_t>(line, "bootstrap_samples=");
        else if (line.starts_with("maximum_mixed_qoi_error=")) report.maximum_mixed_qoi_error = numeric_field<double>(line, "maximum_mixed_qoi_error=");
        else if (line.starts_with("baseline_failures=")) report.baseline_failures = numeric_field<std::size_t>(line, "baseline_failures=");
        else if (line.starts_with("accelerated_failures=")) report.accelerated_failures = numeric_field<std::size_t>(line, "accelerated_failures=");
        else if (line.starts_with("gate_mismatches=")) report.gate_mismatches = numeric_field<std::size_t>(line, "gate_mismatches=");
        else if (line.starts_with("same_accuracy=")) report.same_accuracy = boolean_field(line, "same_accuracy=");
        else if (line.starts_with("p99_not_regressed=")) report.p99_not_regressed = boolean_field(line, "p99_not_regressed=");
        else if (line.starts_with("report_hash=")) report.report_hash = field_value(line, "report_hash=");
        else throw std::invalid_argument("unknown performance report field");
    }
    if (!ended) throw std::invalid_argument("truncated performance report");
    input >> std::ws;
    if (!input.eof()) throw std::invalid_argument("trailing performance report content");
    report.validate();
    return report;
}

}  // namespace smave
