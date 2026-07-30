#include "smave/competition.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

double quantile(std::vector<double> values, double probability) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = probability * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

std::vector<std::filesystem::path> scenarios(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    if (result.empty()) throw std::invalid_argument("competition suite has no scenarios");
    return result;
}

template <typename Value>
Value value_after(const std::string& token, const std::string& key) {
    if (!token.starts_with(key)) throw std::invalid_argument("invalid competition field");
    std::istringstream input(token.substr(key.size()));
    Value value{};
    if (!(input >> value) || !input.eof()) {
        throw std::invalid_argument("invalid competition value for " + key);
    }
    return value;
}

std::string competition_hash(const CompetitionReport& report) {
    std::ostringstream contract;
    contract << std::setprecision(17)
             << report.block_fingerprint << '|' << report.winner;
    if (report.schema_version >= 4) {
        contract << '|' << report.dataset_id
                 << '|' << report.dataset_version
                 << '|' << report.dataset_manifest_hash;
    }
    for (const auto& entry : report.entries) {
        contract << '|' << entry.expert_version
                 << '|' << entry.attempts
                 << '|' << entry.passes
                 << '|' << entry.fallbacks
                 << '|' << entry.failures
                 << '|' << entry.erroneous_accepts
                 << '|' << entry.empirical_pass_rate
                 << '|' << entry.predicted_pass_rate
                 << '|' << entry.calibration_error
                 << '|' << entry.median_wall_us
                 << '|' << entry.p90_wall_us
                 << '|' << entry.p99_wall_us
                 << '|' << entry.median_iterations;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : contract.str()) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

}  // namespace

void CompetitionReport::seal() {
    report_hash = competition_hash(*this);
}

void CompetitionReport::validate() const {
    if ((schema_version != 3 && schema_version != 4) || block_fingerprint.empty() ||
        entries.empty() || winner.empty()) {
        throw std::invalid_argument("incomplete competition report");
    }
    const bool has_dataset = !dataset_id.empty() || !dataset_version.empty() ||
        !dataset_manifest_hash.empty();
    if (schema_version == 3 && has_dataset) {
        throw std::invalid_argument("competition v3 cannot contain dataset lineage");
    }
    if (schema_version == 4 &&
        (dataset_id.empty() || dataset_version.empty() || dataset_manifest_hash.empty())) {
        throw std::invalid_argument("competition v4 requires complete dataset lineage");
    }
    if (report_hash != competition_hash(*this)) {
        throw std::invalid_argument("competition report integrity check failed");
    }
}

CompetitionReport compete_experts(
    const ModelIR& model,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    Tolerance tolerance,
    std::size_t repetitions) {
    if (model.blocks.size() != 1) {
        throw std::invalid_argument("competition MVP requires exactly one block");
    }
    if (repetitions == 0) {
        throw std::invalid_argument("competition repetitions must be positive");
    }
    const auto paths = scenarios(scenario_directory);
    const auto& block = model.blocks.front();
    auto candidates = CompileRouter{}.lookup(block, registry, bundle);
    candidates.push_back(CandidateExpert{
        .expert_version = bundle.terminal_fallback,
        .permission = Permission::direct,
        .builtin = true,
    });
    CompetitionReport report;
    report.block_fingerprint = block.fingerprint;
    struct Accumulator {
        CompetitionEntry entry;
        std::vector<double> walls;
        std::vector<double> iterations;
        double predicted_sum = 0.0;
    };
    std::vector<Accumulator> accumulators(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        accumulators[index].entry.expert_version = candidates[index].expert_version;
    }
    for (const auto& path : paths) {
        const auto scenario = read_scenario(path);
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            for (std::size_t offset = 0; offset < candidates.size(); ++offset) {
                const std::size_t index = (repetition + offset) % candidates.size();
                const auto& candidate = candidates[index];
                auto& accumulator = accumulators[index];
                const bool terminal_baseline =
                    candidate.expert_version == bundle.terminal_fallback;
                BlockContext context;
                context.values = scenario;
                RoutingConfig routing;
                routing.top_k = 1;
                routing.expert_allowlist.insert(candidate.expert_version);
                if (terminal_baseline) {
                    accumulator.predicted_sum += 1.0;
                } else {
                    const auto plan = RuntimeRouter(routing).route(
                        block, context, candidates, registry, bundle);
                    accumulator.predicted_sum += plan.steps.empty()
                        ? 0.0
                        : plan.steps.front().pass_probability;
                }
                const auto started = std::chrono::steady_clock::now();
                const Runtime runtime(model, registry, bundle, tolerance, routing);
                const auto outcome = runtime.solve(
                    scenario,
                    trace_directory / candidate.expert_version /
                        std::to_string(repetition));
                accumulator.walls.push_back(std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - started).count());
                ++accumulator.entry.attempts;
                if (!outcome.success) ++accumulator.entry.failures;
                if (!outcome.blocks.empty()) {
                    const auto& block_outcome = outcome.blocks.front();
                    accumulator.iterations.push_back(
                        block_outcome.expert_iterations + block_outcome.fallback_iterations);
                    if (terminal_baseline) {
                        if (outcome.success) ++accumulator.entry.passes;
                    } else if (block_outcome.path == SolvePath::full_fallback) {
                        ++accumulator.entry.fallbacks;
                    } else {
                        ++accumulator.entry.passes;
                    }
                    if (block_outcome.path != SolvePath::full_fallback &&
                        block_outcome.gate.decision != GateDecision::direct_accept) {
                        ++accumulator.entry.erroneous_accepts;
                    }
                }
            }
        }
    }
    for (auto& accumulator : accumulators) {
        auto& entry = accumulator.entry;
        entry.empirical_pass_rate = entry.attempts
            ? static_cast<double>(entry.passes) / static_cast<double>(entry.attempts)
            : 0.0;
        entry.predicted_pass_rate = entry.attempts
            ? accumulator.predicted_sum / static_cast<double>(entry.attempts)
            : 0.0;
        entry.calibration_error = std::abs(
            entry.predicted_pass_rate - entry.empirical_pass_rate);
        entry.median_wall_us = quantile(accumulator.walls, 0.50);
        entry.p90_wall_us = quantile(accumulator.walls, 0.90);
        entry.p99_wall_us = quantile(accumulator.walls, 0.99);
        entry.median_iterations = quantile(accumulator.iterations, 0.50);
        report.entries.push_back(std::move(entry));
    }
    constexpr double equivalence_relative_band = 0.02;
    constexpr double equivalence_absolute_band_us = 100.0;
    const auto verified = [](const CompetitionEntry& entry) {
        return entry.attempts > 0 && entry.failures == 0 &&
            entry.passes == entry.attempts && entry.erroneous_accepts == 0;
    };
    double fastest_verified = std::numeric_limits<double>::infinity();
    for (const auto& entry : report.entries) {
        if (verified(entry)) {
            fastest_verified = std::min(fastest_verified, entry.median_wall_us);
        }
    }
    const double equivalent_limit = fastest_verified + std::max(
        fastest_verified * equivalence_relative_band,
        equivalence_absolute_band_us);
    const CompetitionEntry* winner = nullptr;
    for (const auto& entry : report.entries) {
        if (!verified(entry) || entry.median_wall_us > equivalent_limit) continue;
        if (winner == nullptr ||
            entry.median_iterations < winner->median_iterations ||
            (entry.median_iterations == winner->median_iterations &&
             entry.calibration_error < winner->calibration_error) ||
            (entry.median_iterations == winner->median_iterations &&
             entry.calibration_error == winner->calibration_error &&
             entry.expert_version < winner->expert_version)) {
            winner = &entry;
        }
    }
    if (winner != nullptr) {
        report.winner = winner->expert_version;
    }
    report.seal();
    return report;
}

CompetitionReport read_competition_report(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read competition report");
    std::string magic;
    int version{};
    if (!(input >> magic >> version) || magic != "SMAVE_COMPETITION" ||
        (version != 3 && version != 4)) {
        throw std::invalid_argument("unsupported competition report");
    }
    CompetitionReport report;
    report.schema_version = version;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        if (line == "END") break;
        if (line.starts_with("block_fingerprint=")) {
            report.block_fingerprint = line.substr(std::string("block_fingerprint=").size());
            continue;
        }
        if (line.starts_with("dataset_id=")) {
            report.dataset_id = line.substr(std::string("dataset_id=").size());
            continue;
        }
        if (line.starts_with("dataset_version=")) {
            report.dataset_version = line.substr(std::string("dataset_version=").size());
            continue;
        }
        if (line.starts_with("dataset_manifest_hash=")) {
            report.dataset_manifest_hash =
                line.substr(std::string("dataset_manifest_hash=").size());
            continue;
        }
        if (line.starts_with("winner=")) {
            report.winner = line.substr(std::string("winner=").size());
            continue;
        }
        if (line.starts_with("report_hash=")) {
            report.report_hash = line.substr(std::string("report_hash=").size());
            continue;
        }
        if (!line.starts_with("EXPERT ")) throw std::invalid_argument("invalid competition row");
        std::istringstream row(line.substr(7));
        CompetitionEntry entry;
        if (!(row >> std::quoted(entry.expert_version))) {
            throw std::invalid_argument("invalid competition expert");
        }
        std::string token;
        row >> token; entry.attempts = value_after<std::size_t>(token, "attempts=");
        row >> token; entry.passes = value_after<std::size_t>(token, "passes=");
        row >> token; entry.fallbacks = value_after<std::size_t>(token, "fallbacks=");
        row >> token; entry.failures = value_after<std::size_t>(token, "failures=");
        row >> token; entry.erroneous_accepts = value_after<std::size_t>(
            token, "erroneous_accepts=");
        row >> token; entry.empirical_pass_rate = value_after<double>(token, "empirical_pass_rate=");
        row >> token; entry.predicted_pass_rate = value_after<double>(token, "predicted_pass_rate=");
        row >> token; entry.calibration_error = value_after<double>(token, "calibration_error=");
        row >> token; entry.median_wall_us = value_after<double>(token, "median_wall_us=");
        row >> token; entry.p90_wall_us = value_after<double>(token, "p90_wall_us=");
        row >> token; entry.p99_wall_us = value_after<double>(token, "p99_wall_us=");
        row >> token; entry.median_iterations = value_after<double>(token, "median_iterations=");
        if (row >> token) throw std::invalid_argument("unexpected competition field");
        report.entries.push_back(std::move(entry));
    }
    report.validate();
    return report;
}

void apply_competition_profile(
    const CompetitionReport& report,
    RoutingConfig& routing) {
    report.validate();
    if (report.block_fingerprint.empty()) {
        throw std::invalid_argument("competition profile has no block fingerprint");
    }
    const auto winner = std::find_if(
        report.entries.begin(), report.entries.end(),
        [&](const CompetitionEntry& entry) {
            return entry.expert_version == report.winner;
        });
    if (winner == report.entries.end() || winner->attempts == 0 ||
        winner->failures != 0 || winner->fallbacks != 0 ||
        winner->passes != winner->attempts || winner->erroneous_accepts != 0) {
        throw std::invalid_argument("competition profile winner is not fully gate-verified");
    }
    for (const auto& entry : report.entries) {
        if (entry.expert_version.empty() || entry.attempts == 0 ||
            entry.passes > entry.attempts || entry.fallbacks > entry.attempts ||
            entry.failures > entry.attempts ||
            entry.erroneous_accepts > entry.attempts ||
            !std::isfinite(entry.empirical_pass_rate) ||
            !std::isfinite(entry.predicted_pass_rate) ||
            !std::isfinite(entry.calibration_error) ||
            !std::isfinite(entry.median_wall_us) ||
            !std::isfinite(entry.median_iterations) ||
            entry.empirical_pass_rate < 0.0 || entry.empirical_pass_rate > 1.0 ||
            entry.predicted_pass_rate < 0.0 || entry.predicted_pass_rate > 1.0 ||
            entry.calibration_error < 0.0 || entry.calibration_error > 1.0 ||
            entry.median_wall_us < 0.0 || entry.median_iterations < 0.0 ||
            entry.median_iterations > static_cast<double>(
                std::numeric_limits<int>::max())) {
            throw std::invalid_argument("competition profile contains invalid metrics");
        }
    }
    routing.calibration_block_fingerprint = report.block_fingerprint;
    routing.calibration_winner = report.winner;
    routing.calibrations.clear();
    routing.expert_allowlist.clear();
    if (!report.winner.empty()) {
        routing.expert_allowlist.insert(report.winner);
    }
    for (const auto& entry : report.entries) {
        routing.calibrations.emplace(entry.expert_version, RouteCalibration{
            .attempts = entry.attempts,
            .passes = entry.passes,
            .fallbacks = entry.fallbacks,
            .failures = entry.failures,
            .pass_probability = entry.empirical_pass_rate,
            .calibration_error = entry.calibration_error,
            .median_wall_us = entry.median_wall_us,
            .work_iterations = static_cast<int>(
                std::ceil(entry.median_iterations)),
        });
    }
}

void write_competition_report(
    const CompetitionReport& report,
    const std::filesystem::path& path) {
    report.validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write competition report");
    output << std::setprecision(17)
           << "SMAVE_COMPETITION " << report.schema_version << '\n';
    if (report.schema_version >= 4) {
        output << "dataset_id=" << report.dataset_id << '\n'
               << "dataset_version=" << report.dataset_version << '\n'
               << "dataset_manifest_hash=" << report.dataset_manifest_hash << '\n';
    }
    output << "block_fingerprint=" << report.block_fingerprint << '\n'
           << "winner=" << report.winner << '\n'
           << "report_hash=" << report.report_hash << '\n';
    for (const auto& entry : report.entries) {
        output << "EXPERT " << std::quoted(entry.expert_version)
               << " attempts=" << entry.attempts
               << " passes=" << entry.passes
               << " fallbacks=" << entry.fallbacks
               << " failures=" << entry.failures
               << " erroneous_accepts=" << entry.erroneous_accepts
               << " empirical_pass_rate=" << entry.empirical_pass_rate
               << " predicted_pass_rate=" << entry.predicted_pass_rate
               << " calibration_error=" << entry.calibration_error
               << " median_wall_us=" << entry.median_wall_us
               << " p90_wall_us=" << entry.p90_wall_us
               << " p99_wall_us=" << entry.p99_wall_us
               << " median_iterations=" << entry.median_iterations << '\n';
    }
    output << "END\n";
}

}  // namespace smave
