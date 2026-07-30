#include "smave/operator.hpp"

#include "smave/device.hpp"
#include "smave/expression.hpp"
#include "smave/linear.hpp"
#include "smave/runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

namespace smave {
namespace {

std::uint64_t process_peak_rss_bytes() {
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

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

struct DiagonalCorrectionResult {
    std::unordered_map<std::string, double> values;
    GateResult gate;
    std::size_t iterations{};
};

struct HintsScheduleResult {
    std::unordered_map<std::string, double> values;
    GateResult gate;
    std::size_t iterations{};
    std::size_t jacobi_iterations{};
    std::size_t learned_corrections{};
    bool valid{true};
};

struct CorrectionBudgetStatistics {
    std::size_t budget{};
    std::vector<double> complete_times;
    std::size_t accepts{};
    std::size_t fallbacks{};
    std::size_t failures{};
    std::size_t correction_iterations{};
    double maximum_accepted_residual{};
};

DiagonalCorrectionResult diagonal_residual_correction(
    const ModelIR& model,
    const BlockIR& block,
    const std::vector<Expression>& residuals,
    const std::unordered_map<std::string, double>& context,
    const std::unordered_map<std::string, double>& candidate,
    const Runtime& runtime,
    std::size_t maximum_iterations) {
    DiagonalCorrectionResult result;
    result.values = context;
    for (const auto& variable : model.variables) {
        result.values.try_emplace(variable.name, variable.start);
    }
    for (const auto& __entry : candidate) {

        const auto& name = __entry.first;

        const auto& value = __entry.second;
        result.values.insert_or_assign(name, value);
    }
    result.gate = runtime.evaluate_gate_fused(block, result.values, true);
    for (std::size_t iteration = 0;
         iteration < maximum_iterations &&
         (iteration == 0 || result.gate.decision != GateDecision::direct_accept);
         ++iteration) {
        auto next = result.values;
        bool valid = true;
        for (std::size_t row = 0; row < block.unknowns.size(); ++row) {
            const double residual = residuals[row].evaluate(result.values);
            const std::unordered_map<std::string, double> direction{
                {block.unknowns[row], 1.0}};
            const auto diagonal = residuals[row].directional_derivative(
                result.values, direction);
            if (!diagonal.has_value() || !std::isfinite(residual) ||
                !std::isfinite(*diagonal) || std::abs(*diagonal) < 1.0e-14) {
                valid = false;
                break;
            }
            next[block.unknowns[row]] -= 0.8 * residual / *diagonal;
        }
        if (!valid) break;
        result.values = std::move(next);
        result.iterations = iteration + 1;
        result.gate = runtime.evaluate_gate_fused(block, result.values, true);
    }
    return result;
}

std::optional<std::size_t> decimal_suffix(std::string_view name) {
    const auto first_digit = std::find_if(
        name.begin(), name.end(), [](unsigned char character) {
            return character >= '0' && character <= '9';
        });
    if (first_digit == name.end() ||
        !std::all_of(first_digit, name.end(), [](unsigned char character) {
            return character >= '0' && character <= '9';
        })) {
        return std::nullopt;
    }
    std::size_t value{};
    for (auto current = first_digit; current != name.end(); ++current) {
        value = value * 10 + static_cast<std::size_t>(*current - '0');
    }
    return value;
}

std::vector<std::size_t> hints_feature_rows(
    const LatentOperatorArtifact& artifact,
    const BlockIR& block) {
    if (!block.linear || artifact.features.size() != artifact.outputs.size() ||
        artifact.outputs != block.unknowns) {
        return {};
    }
    std::vector<std::size_t> rows;
    rows.reserve(artifact.features.size());
    for (const auto& feature : artifact.features) {
        const auto feature_suffix = decimal_suffix(feature);
        if (!feature_suffix.has_value()) return {};
        const auto output = std::find_if(
            artifact.outputs.begin(), artifact.outputs.end(),
            [&](const std::string& name) {
                return decimal_suffix(name) == feature_suffix;
            });
        if (output == artifact.outputs.end()) return {};
        rows.push_back(static_cast<std::size_t>(
            std::distance(artifact.outputs.begin(), output)));
    }
    if (std::unordered_set<std::size_t>(rows.begin(), rows.end()).size() != rows.size()) {
        return {};
    }
    return rows;
}

std::vector<double> zero_anchored_latent_correction(
    const LatentOperatorArtifact& artifact,
    const std::vector<double>& feature_values) {
    if (feature_values.size() != artifact.features.size()) {
        throw std::invalid_argument("HINTS feature width does not match latent operator");
    }
    std::vector<double> latent(artifact.state_basis.size());
    for (std::size_t component = 0; component < latent.size(); ++component) {
        for (std::size_t feature = 0; feature < feature_values.size(); ++feature) {
            latent[component] += artifact.latent_coefficients[component][feature + 1] *
                feature_values[feature];
        }
    }
    std::vector<double> correction(artifact.outputs.size());
    for (std::size_t output = 0; output < correction.size(); ++output) {
        for (std::size_t component = 0; component < latent.size(); ++component) {
            correction[output] += artifact.state_basis[component][output] * latent[component];
        }
    }
    return correction;
}

HintsScheduleResult run_hints_schedule(
    const ModelIR& model,
    const BlockIR& block,
    const std::unordered_map<std::string, double>& context,
    const LatentOperatorArtifact& artifact,
    const std::vector<std::size_t>& feature_rows,
    const LinearSystem& system,
    const std::vector<double>& diagonal,
    const Runtime& runtime,
    std::size_t maximum_iterations,
    std::size_t numerical_to_learned_ratio,
    double jacobi_weight) {
    if (feature_rows.size() != artifact.features.size() ||
        system.size() != block.unknowns.size() ||
        diagonal.size() != block.unknowns.size() ||
        artifact.outputs != block.unknowns || numerical_to_learned_ratio == 0) {
        throw std::invalid_argument("HINTS schedule contract mismatch");
    }
    HintsScheduleResult result;
    result.values = context;
    for (const auto& variable : model.variables) {
        result.values.try_emplace(variable.name, variable.start);
    }
    std::vector<double> state(block.unknowns.size());
    for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
        state[index] = result.values.at(block.unknowns[index]);
    }
    auto residual = system.multiply(state);
    for (std::size_t row = 0; row < residual.size(); ++row) {
        residual[row] -= system.right_hand_side[row];
    }
    result.gate = runtime.evaluate_gate_with_residuals(
        block, result.values, residual, true);
    for (std::size_t iteration = 0;
         iteration < maximum_iterations &&
         result.gate.decision != GateDecision::direct_accept;
         ++iteration) {
        if ((iteration + 1) % numerical_to_learned_ratio == 0) {
            std::vector<double> residual_rhs(artifact.features.size());
            for (std::size_t feature = 0; feature < feature_rows.size(); ++feature) {
                residual_rhs[feature] = -residual[feature_rows[feature]];
            }
            const auto correction =
                zero_anchored_latent_correction(artifact, residual_rhs);
            for (std::size_t output = 0; output < correction.size(); ++output) {
                if (!std::isfinite(correction[output])) {
                    result.valid = false;
                    break;
                }
                state[output] += correction[output];
            }
            if (!result.valid) break;
            ++result.learned_corrections;
        } else {
            auto next = state;
            for (std::size_t row = 0; row < block.unknowns.size(); ++row) {
                if (!std::isfinite(residual[row]) || !std::isfinite(diagonal[row]) ||
                    std::abs(diagonal[row]) < 1.0e-14) {
                    result.valid = false;
                    break;
                }
                next[row] -= jacobi_weight * residual[row] / diagonal[row];
            }
            if (!result.valid) break;
            state = std::move(next);
            ++result.jacobi_iterations;
        }
        for (std::size_t index = 0; index < block.unknowns.size(); ++index) {
            result.values[block.unknowns[index]] = state[index];
        }
        result.iterations = iteration + 1;
        residual = system.multiply(state);
        for (std::size_t row = 0; row < residual.size(); ++row) {
            residual[row] -= system.right_hand_side[row];
        }
        result.gate = runtime.evaluate_gate_with_residuals(
            block, result.values, residual, true);
    }
    return result;
}

std::string contract(const LatentOperatorArtifact& artifact, bool include_measurement = true) {
    std::ostringstream output;
    output << std::setprecision(17)
           << artifact.schema_version << '|' << artifact.expert_version << '|'
           << artifact.model_source_hash << '|' << artifact.block_fingerprint << '|';
    if (artifact.schema_version == "smave.latent-operator.v2") {
        output << artifact.training_dataset_id << '|'
               << artifact.training_dataset_version << '|'
               << artifact.training_dataset_manifest_hash << '|';
    }
    output << artifact.training_samples << '|' << artifact.retained_energy << '|'
           << artifact.training_rmse << '|';
    if (include_measurement) output << artifact.training_wall_us;
    output << '|'
           << artifact.output_permission;
    for (const auto& name : artifact.features) output << "|f:" << name;
    for (const auto& name : artifact.outputs) output << "|o:" << name;
    for (const auto& name : artifact.qoi_outputs) output << "|q:" << name;
    for (std::size_t index = 0; index < artifact.feature_minimum.size(); ++index) {
        output << "|d:" << artifact.feature_minimum[index] << ':'
               << artifact.feature_maximum[index];
    }
    for (const double value : artifact.state_mean) output << "|m:" << value;
    for (const auto& row : artifact.state_basis) {
        for (const double value : row) output << "|b:" << value;
    }
    for (const auto& row : artifact.latent_coefficients) {
        for (const double value : row) output << "|c:" << value;
    }
    return output.str();
}

bool solve_system(
    std::vector<std::vector<double>> matrix,
    std::vector<double> right,
    std::vector<double>& solution) {
    const std::size_t size = matrix.size();
    if (right.size() != size) return false;
    for (std::size_t row = 0; row < size; ++row) {
        if (matrix[row].size() != size) return false;
        std::size_t pivot = row;
        for (std::size_t candidate = row + 1; candidate < size; ++candidate) {
            if (std::abs(matrix[candidate][row]) > std::abs(matrix[pivot][row])) {
                pivot = candidate;
            }
        }
        if (std::abs(matrix[pivot][row]) < 1.0e-14) return false;
        std::swap(matrix[pivot], matrix[row]);
        std::swap(right[pivot], right[row]);
        const double diagonal = matrix[row][row];
        for (std::size_t column = row; column < size; ++column) {
            matrix[row][column] /= diagonal;
        }
        right[row] /= diagonal;
        for (std::size_t other = 0; other < size; ++other) {
            if (other == row) continue;
            const double factor = matrix[other][row];
            for (std::size_t column = row; column < size; ++column) {
                matrix[other][column] -= factor * matrix[row][column];
            }
            right[other] -= factor * right[row];
        }
    }
    solution = std::move(right);
    return std::all_of(solution.begin(), solution.end(), [](double value) {
        return std::isfinite(value);
    });
}

double dot(const std::vector<double>& left, const std::vector<double>& right) {
    return std::inner_product(left.begin(), left.end(), right.begin(), 0.0);
}

double norm(const std::vector<double>& vector) {
    return std::sqrt(std::max(0.0, dot(vector, vector)));
}

std::vector<std::filesystem::path> scenario_files(
    const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::invalid_argument("operator scenario suite is not a directory");
    }
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    if (result.empty()) throw std::invalid_argument("operator suite has no scenarios");
    return result;
}

const BlockIR& block_by_id(const ModelIR& model, const std::string& block_id) {
    const auto block = std::find_if(
        model.blocks.begin(), model.blocks.end(),
        [&](const BlockIR& item) { return item.id == block_id; });
    if (block == model.blocks.end()) throw std::invalid_argument("unknown operator block");
    return *block;
}

std::vector<std::string> parameter_features(
    const ModelIR& model, const BlockIR& block) {
    std::vector<std::string> features;
    for (const auto& context : block.contexts) {
        const auto variable = std::find_if(
            model.variables.begin(), model.variables.end(),
            [&](const VariableIR& item) { return item.name == context; });
        if (variable != model.variables.end() && variable->kind == "parameter") {
            features.push_back(context);
        }
    }
    std::sort(features.begin(), features.end());
    return features;
}

std::vector<std::vector<double>> pod_basis(
    const std::vector<std::vector<double>>& centered,
    std::size_t maximum_rank,
    double& retained_energy) {
    const std::size_t samples = centered.size();
    const std::size_t dimension = centered.front().size();
    std::vector<std::vector<double>> covariance(
        dimension, std::vector<double>(dimension));
    for (const auto& sample : centered) {
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t column = 0; column < dimension; ++column) {
                covariance[row][column] += sample[row] * sample[column];
            }
        }
    }
    double total_energy = 0.0;
    for (std::size_t index = 0; index < dimension; ++index) {
        total_energy += covariance[index][index];
    }
    std::vector<std::vector<double>> basis;
    double captured = 0.0;
    const std::size_t rank = std::min({maximum_rank, samples, dimension});
    for (std::size_t component = 0; component < rank; ++component) {
        std::vector<double> vector(dimension);
        for (std::size_t index = 0; index < dimension; ++index) {
            vector[index] = std::sin(
                static_cast<double>((component + 1) * (index + 1)) + 0.5);
        }
        const double seed_norm = norm(vector);
        for (double& value : vector) value /= seed_norm;
        for (int iteration = 0; iteration < 100; ++iteration) {
            std::vector<double> next(dimension);
            for (std::size_t row = 0; row < dimension; ++row) {
                next[row] = dot(covariance[row], vector);
            }
            for (const auto& previous : basis) {
                const double projection = dot(next, previous);
                for (std::size_t index = 0; index < dimension; ++index) {
                    next[index] -= projection * previous[index];
                }
            }
            const double length = norm(next);
            if (length < 1.0e-12) break;
            for (double& value : next) value /= length;
            vector = std::move(next);
        }
        std::vector<double> product(dimension);
        for (std::size_t row = 0; row < dimension; ++row) {
            product[row] = dot(covariance[row], vector);
        }
        const double eigenvalue = dot(vector, product);
        if (!(eigenvalue > 1.0e-12) || !std::isfinite(eigenvalue)) break;
        basis.push_back(vector);
        captured += eigenvalue;
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t column = 0; column < dimension; ++column) {
                covariance[row][column] -= eigenvalue * vector[row] * vector[column];
            }
        }
        if (total_energy > 0.0 && captured / total_energy >= 0.999999) break;
    }
    if (basis.empty()) {
        basis.push_back(std::vector<double>(dimension));
        basis.front().front() = 1.0;
    }
    retained_energy = total_energy > 0.0 ? std::min(1.0, captured / total_energy) : 1.0;
    return basis;
}

double quantile(std::vector<double> values, double probability) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = probability * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

struct BootstrapInterval {
    double median{};
    double lower{};
    double upper{};
};

BootstrapInterval paired_median_bootstrap(
    const std::vector<double>& baseline,
    const std::vector<double>& accelerated,
    std::uint64_t seed,
    std::size_t resamples) {
    if (baseline.empty() || baseline.size() != accelerated.size() || resamples == 0) {
        return {};
    }
    std::vector<double> paired_speedups;
    paired_speedups.reserve(baseline.size());
    for (std::size_t index = 0; index < baseline.size(); ++index) {
        paired_speedups.push_back(accelerated[index] > 0.0
            ? baseline[index] / accelerated[index]
            : 0.0);
    }
    std::mt19937_64 generator(seed);
    std::uniform_int_distribution<std::size_t> sample(0, paired_speedups.size() - 1);
    std::vector<double> bootstrap_medians;
    bootstrap_medians.reserve(resamples);
    std::vector<double> draw(paired_speedups.size());
    for (std::size_t resample = 0; resample < resamples; ++resample) {
        for (double& value : draw) value = paired_speedups[sample(generator)];
        bootstrap_medians.push_back(quantile(draw, 0.5));
    }
    return {
        .median = quantile(paired_speedups, 0.5),
        .lower = quantile(bootstrap_medians, 0.025),
        .upper = quantile(bootstrap_medians, 0.975),
    };
}

double paired_amortized_speedup(
    double online_speedup,
    double paired_saving_us,
    double training_wall_us,
    std::size_t projected_queries) {
    const double speedup_delta = online_speedup - 1.0;
    if (online_speedup <= 0.0 || projected_queries == 0 ||
        std::abs(speedup_delta) <= 1.0e-15 ||
        paired_saving_us / speedup_delta <= 0.0) {
        return 0.0;
    }
    const double operator_us = paired_saving_us / speedup_delta;
    const double baseline_us = online_speedup * operator_us;
    const double projected_operator = training_wall_us +
        operator_us * static_cast<double>(projected_queries);
    return projected_operator > 0.0
        ? baseline_us * static_cast<double>(projected_queries) /
            projected_operator
        : 0.0;
}

double normalized_error(double reference, double candidate, double relative) {
    return std::abs(candidate - reference) /
        (1.0e-10 + relative * std::abs(reference));
}

std::string benchmark_contract(const OperatorBenchmarkReport& report) {
    std::ostringstream output;
    if (report.schema_version >= 2) {
        output << report.schema_version << '|' << report.dataset_id << '|'
               << report.dataset_version << '|' << report.dataset_manifest_hash << '|';
    }
    output << std::setprecision(17)
           << report.requests << '|' << report.repetitions << '|'
           << report.batches << '|' << report.average_batch << '|'
           << report.accepted << '|' << report.fallbacks << '|'
           << report.failures << '|' << report.acceptance_rate << '|'
           << report.baseline_median_us << '|' << report.operator_median_us << '|'
           << report.online_speedup << '|' << report.training_wall_us << '|'
           << report.break_even_queries << '|' << report.projected_queries << '|'
           << report.amortized_speedup << '|' << report.maximum_full_state_error << '|'
           << report.maximum_qoi_error << '|'
           << report.maximum_candidate_full_state_error << '|'
           << report.maximum_candidate_qoi_error << '|'
           << report.candidate_qoi_within_tolerance << '|'
           << report.same_accuracy << '|'
           << report.break_even_met << '|' << report.artifact_hash << '|'
           << report.certificate_hash;
    if (report.schema_version >= 3) {
        output << '|' << report.paired_speedup_ci95_lower << '|'
               << report.paired_speedup_ci95_upper << '|'
               << report.paired_median_saving_us;
    }
    return output.str();
}

}  // namespace

void OperatorBenchmarkReport::seal() {
    report_hash = digest(benchmark_contract(*this));
}

void OperatorBenchmarkReport::validate() const {
    const std::size_t samples = requests * repetitions;
    if (schema_version < 1 || schema_version > 3 || requests == 0 ||
        repetitions == 0 || batches != repetitions ||
        std::abs(average_batch - static_cast<double>(requests)) > 1.0e-12 ||
        accepted + fallbacks != samples ||
        failures > samples || acceptance_rate < 0.0 || acceptance_rate > 1.0 ||
        baseline_median_us < 0.0 || operator_median_us < 0.0 ||
        online_speedup < 0.0 || training_wall_us < 0.0 || projected_queries == 0 ||
        amortized_speedup < 0.0 || maximum_full_state_error < 0.0 ||
        maximum_qoi_error < 0.0 || maximum_candidate_full_state_error < 0.0 ||
        maximum_candidate_qoi_error < 0.0 || artifact_hash.empty() ||
        certificate_hash.empty() ||
        report_hash != digest(benchmark_contract(*this))) {
        throw std::invalid_argument("invalid operator benchmark report");
    }
    if (same_accuracy != (failures == 0 && maximum_full_state_error <= 1.0 &&
            maximum_qoi_error <= 1.0)) {
        throw std::invalid_argument("inconsistent operator accuracy result");
    }
    if (candidate_qoi_within_tolerance != (maximum_candidate_qoi_error <= 1.0)) {
        throw std::invalid_argument("inconsistent operator candidate QoI result");
    }
    const auto close = [](double left, double right) {
        return std::abs(left - right) <= 1.0e-12 *
            std::max({1.0, std::abs(left), std::abs(right)});
    };
    const double expected_acceptance = static_cast<double>(accepted) /
        static_cast<double>(samples);
    if (schema_version >= 3) {
        const bool valid_interval = paired_speedup_ci95_lower >= 0.0 &&
            paired_speedup_ci95_upper >= paired_speedup_ci95_lower &&
            online_speedup >= paired_speedup_ci95_lower &&
            online_speedup <= paired_speedup_ci95_upper &&
            std::isfinite(paired_median_saving_us);
        const std::size_t expected_break_even = paired_median_saving_us > 0.0
            ? static_cast<std::size_t>(
                  std::ceil(training_wall_us / paired_median_saving_us))
            : std::numeric_limits<std::size_t>::max();
        const double expected_amortized = paired_amortized_speedup(
            online_speedup, paired_median_saving_us, training_wall_us,
            projected_queries);
        const bool expected_gate = paired_speedup_ci95_lower > 1.0 &&
            expected_break_even <= projected_queries && expected_amortized > 1.0 &&
            expected_acceptance >= 0.95 && same_accuracy;
        if (!valid_interval || !close(acceptance_rate, expected_acceptance) ||
            break_even_queries != expected_break_even ||
            !close(amortized_speedup, expected_amortized) ||
            break_even_met != expected_gate) {
            throw std::invalid_argument("inconsistent paired operator benchmark metrics");
        }
        return;
    }
    const double expected_online = operator_median_us > 0.0
        ? baseline_median_us / operator_median_us
        : 0.0;
    const double saving = baseline_median_us - operator_median_us;
    const std::size_t expected_break_even = saving > 0.0
        ? static_cast<std::size_t>(std::ceil(training_wall_us / saving))
        : std::numeric_limits<std::size_t>::max();
    const double expected_amortized = training_wall_us +
            operator_median_us * projected_queries > 0.0
        ? baseline_median_us * projected_queries /
            (training_wall_us + operator_median_us * projected_queries)
        : 0.0;
    const bool expected_gate = expected_online > 1.0 &&
        expected_break_even <= projected_queries && expected_amortized > 1.0 &&
        expected_acceptance >= 0.95 && same_accuracy && candidate_qoi_within_tolerance;
    if (!close(acceptance_rate, expected_acceptance) ||
        !close(online_speedup, expected_online) ||
        break_even_queries != expected_break_even ||
        !close(amortized_speedup, expected_amortized) ||
        break_even_met != expected_gate) {
        throw std::invalid_argument("inconsistent operator benchmark metrics");
    }
}

void LatentOperatorArtifact::seal() {
    if (expert_version.empty()) {
        expert_version = "latent-operator-" + digest(contract(*this, false));
    }
    artifact_hash = digest(contract(*this));
}

void LatentOperatorArtifact::validate() const {
    if ((schema_version != "smave.latent-operator.v1" &&
         schema_version != "smave.latent-operator.v2") || expert_version.empty() ||
        model_source_hash.empty() || block_fingerprint.empty() || features.empty() ||
        outputs.empty() || training_samples == 0 || state_mean.size() != outputs.size() ||
        state_basis.empty() || state_basis.size() != latent_coefficients.size() ||
        feature_minimum.size() != features.size() ||
        feature_maximum.size() != features.size() ||
        output_permission != "full-state-corrected") {
        throw std::invalid_argument("invalid latent operator artifact");
    }
    const bool has_training_dataset = !training_dataset_id.empty() ||
        !training_dataset_version.empty() || !training_dataset_manifest_hash.empty();
    if (schema_version == "smave.latent-operator.v1" && has_training_dataset) {
        throw std::invalid_argument("latent operator v1 cannot contain training lineage");
    }
    if (schema_version == "smave.latent-operator.v2" &&
        (training_dataset_id.empty() || training_dataset_version.empty() ||
         training_dataset_manifest_hash.empty())) {
        throw std::invalid_argument("latent operator v2 requires complete training lineage");
    }
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (feature_minimum[index] > feature_maximum[index]) {
            throw std::invalid_argument("latent operator domain is invalid");
        }
    }
    for (const auto& vector : state_basis) {
        if (vector.size() != outputs.size() ||
            !std::all_of(vector.begin(), vector.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("latent operator basis shape is invalid");
        }
    }
    for (const auto& row : latent_coefficients) {
        if (row.size() != features.size() + 1 ||
            !std::all_of(row.begin(), row.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::invalid_argument("latent operator coefficient shape is invalid");
        }
    }
    if (!std::all_of(state_mean.begin(), state_mean.end(), [](double value) {
            return std::isfinite(value);
        })) {
        throw std::invalid_argument("latent operator state mean is invalid");
    }
    if (!std::isfinite(retained_energy) || retained_energy < 0.0 || retained_energy > 1.0 ||
        !std::isfinite(training_rmse) || training_rmse < 0.0 ||
        !std::isfinite(training_wall_us) || training_wall_us < 0.0 ||
        artifact_hash != digest(contract(*this))) {
        throw std::invalid_argument("latent operator integrity check failed");
    }
    for (const auto& qoi : qoi_outputs) {
        if (std::find(outputs.begin(), outputs.end(), qoi) == outputs.end()) {
            throw std::invalid_argument("latent operator QoI is not a full-state output");
        }
    }
}

void LatentOperatorArtifact::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write latent operator artifact");
    output << std::setprecision(17)
           << "SMAVE_LATENT_OPERATOR "
           << (schema_version == "smave.latent-operator.v2" ? 2 : 1) << '\n'
           << "VERSION " << std::quoted(expert_version) << '\n'
           << "MODEL " << std::quoted(model_source_hash) << '\n'
           << "BLOCK " << std::quoted(block_fingerprint) << '\n'
           << (schema_version == "smave.latent-operator.v2"
               ? "TRAINING_DATASET_ID \"" + training_dataset_id + "\"\n"
                 "TRAINING_DATASET_VERSION \"" + training_dataset_version + "\"\n"
                 "TRAINING_DATASET_MANIFEST_HASH \"" +
                    training_dataset_manifest_hash + "\"\n"
               : "")
           << "TRAINING " << training_samples << ' ' << retained_energy << ' '
           << training_rmse << ' ' << training_wall_us << '\n'
           << "PERMISSION " << std::quoted(output_permission) << '\n'
           << "FEATURES " << features.size();
    for (const auto& name : features) output << ' ' << std::quoted(name);
    output << "\nDOMAIN";
    for (std::size_t index = 0; index < features.size(); ++index) {
        output << ' ' << feature_minimum[index] << ' ' << feature_maximum[index];
    }
    output << "\nOUTPUTS " << outputs.size();
    for (const auto& name : outputs) output << ' ' << std::quoted(name);
    output << "\nQOI " << qoi_outputs.size();
    for (const auto& name : qoi_outputs) output << ' ' << std::quoted(name);
    output << "\nMEAN";
    for (const double value : state_mean) output << ' ' << value;
    output << "\nBASIS " << state_basis.size() << ' ' << outputs.size() << '\n';
    for (const auto& row : state_basis) {
        for (const double value : row) output << value << ' ';
        output << '\n';
    }
    output << "COEFFICIENTS " << latent_coefficients.size() << ' '
           << features.size() + 1 << '\n';
    for (const auto& row : latent_coefficients) {
        for (const double value : row) output << value << ' ';
        output << '\n';
    }
    output << "HASH " << std::quoted(artifact_hash) << "\nEND\n";
}

LatentOperatorArtifact LatentOperatorArtifact::read(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read latent operator artifact");
    auto tag = [&](std::string_view expected) {
        std::string actual;
        input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error("invalid latent operator artifact");
        }
    };
    LatentOperatorArtifact artifact;
    tag("SMAVE_LATENT_OPERATOR"); int version{}; input >> version;
    if (version != 1 && version != 2) {
        throw std::runtime_error("unsupported latent operator schema");
    }
    artifact.schema_version = version == 2
        ? "smave.latent-operator.v2"
        : "smave.latent-operator.v1";
    tag("VERSION"); input >> std::quoted(artifact.expert_version);
    tag("MODEL"); input >> std::quoted(artifact.model_source_hash);
    tag("BLOCK"); input >> std::quoted(artifact.block_fingerprint);
    if (version >= 2) {
        tag("TRAINING_DATASET_ID"); input >> std::quoted(artifact.training_dataset_id);
        tag("TRAINING_DATASET_VERSION");
        input >> std::quoted(artifact.training_dataset_version);
        tag("TRAINING_DATASET_MANIFEST_HASH");
        input >> std::quoted(artifact.training_dataset_manifest_hash);
    }
    tag("TRAINING"); input >> artifact.training_samples >> artifact.retained_energy
                            >> artifact.training_rmse >> artifact.training_wall_us;
    tag("PERMISSION"); input >> std::quoted(artifact.output_permission);
    tag("FEATURES"); std::size_t features{}; input >> features;
    artifact.features.resize(features);
    for (auto& name : artifact.features) input >> std::quoted(name);
    tag("DOMAIN");
    artifact.feature_minimum.resize(features);
    artifact.feature_maximum.resize(features);
    for (std::size_t index = 0; index < features; ++index) {
        input >> artifact.feature_minimum[index] >> artifact.feature_maximum[index];
    }
    tag("OUTPUTS"); std::size_t outputs{}; input >> outputs;
    artifact.outputs.resize(outputs);
    for (auto& name : artifact.outputs) input >> std::quoted(name);
    tag("QOI"); std::size_t qoi{}; input >> qoi;
    artifact.qoi_outputs.resize(qoi);
    for (auto& name : artifact.qoi_outputs) input >> std::quoted(name);
    tag("MEAN"); artifact.state_mean.resize(outputs);
    for (double& value : artifact.state_mean) input >> value;
    tag("BASIS"); std::size_t rank{}, columns{}; input >> rank >> columns;
    if (columns != outputs) throw std::runtime_error("latent operator basis mismatch");
    artifact.state_basis.assign(rank, std::vector<double>(columns));
    for (auto& row : artifact.state_basis) for (double& value : row) input >> value;
    tag("COEFFICIENTS"); std::size_t rows{}, width{}; input >> rows >> width;
    artifact.latent_coefficients.assign(rows, std::vector<double>(width));
    for (auto& row : artifact.latent_coefficients) for (double& value : row) input >> value;
    tag("HASH"); input >> std::quoted(artifact.artifact_hash);
    tag("END");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("trailing latent operator content");
    artifact.validate();
    return artifact;
}

LatentOperatorExpert::LatentOperatorExpert(
    LatentOperatorArtifact artifact,
    std::optional<VerificationCertificate> certificate)
    : artifact_(std::move(artifact)), certificate_(std::move(certificate)) {
    artifact_.validate();
    affine_weights_.assign(
        artifact_.outputs.size() * artifact_.features.size(), 0.0F);
    affine_bias_.resize(artifact_.outputs.size());
    for (std::size_t output = 0; output < artifact_.outputs.size(); ++output) {
        double bias = artifact_.state_mean[output];
        for (std::size_t component = 0;
             component < artifact_.state_basis.size(); ++component) {
            const auto basis = artifact_.state_basis[component][output];
            bias += basis * artifact_.latent_coefficients[component][0];
            for (std::size_t feature = 0;
                 feature < artifact_.features.size(); ++feature) {
                affine_weights_[output * artifact_.features.size() + feature] +=
                    static_cast<float>(basis *
                        artifact_.latent_coefficients[component][feature + 1]);
            }
        }
        affine_bias_[output] = static_cast<float>(bias);
    }
    if (certificate_) {
        certificate_->validate();
        if (certificate_->expert_version != artifact_.expert_version ||
            certificate_->artifact_hash != artifact_.artifact_hash ||
            certificate_->block_fingerprint != artifact_.block_fingerprint ||
            certificate_->training_dataset_id != artifact_.training_dataset_id ||
            certificate_->training_dataset_version != artifact_.training_dataset_version ||
            certificate_->training_dataset_manifest_hash !=
                artifact_.training_dataset_manifest_hash) {
            throw std::invalid_argument("verification certificate does not match operator");
        }
    }
}

std::string LatentOperatorExpert::version() const { return artifact_.expert_version; }

Capability LatentOperatorExpert::match(const BlockIR& block) const {
    const bool compatible = block.smooth && !block.event_related &&
        block.fingerprint == artifact_.block_fingerprint &&
        block.unknowns == artifact_.outputs;
    return Capability{
        .linear = compatible && block.linear,
        .nonlinear = compatible && !block.linear,
        .event_related = false,
        .preconditioner = false,
        .backend_roles = {BackendRole::operator_candidate},
        .devices = {"cpu", "metal-gpu", "coreml-neural-engine"},
        .evidence_level = EvidenceLevel::e2,
        .maximum_permission = Permission::corrected,
    };
}

double LatentOperatorExpert::ood_score(const BlockContext& context) const {
    if (certificate_ && !certificate_->contains(context.values)) {
        return std::numeric_limits<double>::infinity();
    }
    double score = 0.0;
    for (std::size_t index = 0; index < artifact_.features.size(); ++index) {
        const auto value = context.values.find(artifact_.features[index]);
        if (value == context.values.end()) return std::numeric_limits<double>::infinity();
        const double width = std::max(
            artifact_.feature_maximum[index] - artifact_.feature_minimum[index], 1.0e-12);
        if (value->second < artifact_.feature_minimum[index]) {
            score = std::max(
                score, (artifact_.feature_minimum[index] - value->second) / width);
        } else if (value->second > artifact_.feature_maximum[index]) {
            score = std::max(
                score, (value->second - artifact_.feature_maximum[index]) / width);
        }
    }
    return score;
}

Estimate LatentOperatorExpert::estimate(
    const BlockIR&, const BlockContext& context) const {
    const double ood = ood_score(context);
    return Estimate{
        .pass_probability = ood == 0.0 ? 0.99 : 0.0,
        .expected_setup_time_us = 0.1,
        .expected_solve_time_us = static_cast<double>(
            artifact_.state_basis.size() *
            (artifact_.features.size() + artifact_.outputs.size())) * 0.005,
        .expected_correction_time_us = 1.0,
        .failure_cost_us = 100.0,
        .risk_score = 0.0001 + ood,
        .ood_score = ood,
    };
}

ExpertResult LatentOperatorExpert::solve(
    const BlockIR&, const BlockContext& context, const SolveBudget&) const {
    ExpertResult result;
    if (ood_score(context) != 0.0) {
        result.status = "ood";
        return result;
    }
    std::vector<double> latent(artifact_.state_basis.size());
    for (std::size_t component = 0; component < latent.size(); ++component) {
        latent[component] = artifact_.latent_coefficients[component][0];
        for (std::size_t feature = 0; feature < artifact_.features.size(); ++feature) {
            latent[component] += artifact_.latent_coefficients[component][feature + 1] *
                context.values.at(artifact_.features[feature]);
        }
    }
    for (std::size_t output = 0; output < artifact_.outputs.size(); ++output) {
        double value = artifact_.state_mean[output];
        for (std::size_t component = 0; component < latent.size(); ++component) {
            value += artifact_.state_basis[component][output] * latent[component];
        }
        result.candidate[artifact_.outputs[output]] = value;
    }
    result.status = "candidate";
    result.uncertainty = artifact_.training_rmse;
    result.telemetry["latent_rank"] = static_cast<double>(artifact_.state_basis.size());
    result.telemetry["retained_energy"] = artifact_.retained_energy;
    return result;
}

std::vector<ExpertResult> LatentOperatorExpert::solve_batch(
    const BlockIR&,
    const std::vector<BlockContext>& contexts) const {
    std::vector<ExpertResult> results(contexts.size());
    std::vector<std::vector<double>> latent(
        contexts.size(), std::vector<double>(artifact_.state_basis.size()));
    for (std::size_t item = 0; item < contexts.size(); ++item) {
        if (ood_score(contexts[item]) != 0.0) {
            results[item].status = "ood";
            continue;
        }
        for (std::size_t component = 0; component < artifact_.state_basis.size(); ++component) {
            latent[item][component] = artifact_.latent_coefficients[component][0];
        }
    }
    for (std::size_t feature = 0; feature < artifact_.features.size(); ++feature) {
        for (std::size_t component = 0; component < artifact_.state_basis.size(); ++component) {
            const double weight = artifact_.latent_coefficients[component][feature + 1];
            for (std::size_t item = 0; item < contexts.size(); ++item) {
                if (results[item].status == "ood") continue;
                latent[item][component] += weight *
                    contexts[item].values.at(artifact_.features[feature]);
            }
        }
    }
    for (std::size_t output = 0; output < artifact_.outputs.size(); ++output) {
        for (std::size_t item = 0; item < contexts.size(); ++item) {
            if (results[item].status == "ood") continue;
            double value = artifact_.state_mean[output];
            for (std::size_t component = 0; component < artifact_.state_basis.size(); ++component) {
                value += artifact_.state_basis[component][output] * latent[item][component];
            }
            results[item].candidate[artifact_.outputs[output]] = value;
        }
    }
    for (auto& result : results) {
        if (result.status == "ood") continue;
        result.status = "candidate";
        result.uncertainty = artifact_.training_rmse;
        result.telemetry["latent_rank"] = static_cast<double>(artifact_.state_basis.size());
        result.telemetry["batch_size"] = static_cast<double>(contexts.size());
    }
    return results;
}

std::vector<ExpertResult> LatentOperatorExpert::solve_batch_on_device(
    const std::string& device,
    const BlockIR& block,
    const std::vector<BlockContext>& contexts,
    DeviceExecutionResult* execution) const {
    if (device == "cpu") {
        if (execution != nullptr) {
            *execution = DeviceExecutionResult{};
            execution->backend = "latent-operator-affine-batch-cpu-v1";
            execution->available = true;
            execution->executed = true;
            execution->verified = true;
            execution->reason = "CPU latent operator batch selected";
        }
        return solve_batch(block, contexts);
    }
    std::vector<ExpertResult> results(contexts.size());
    if (!match(block).linear || contexts.empty() ||
        (device != "metal-gpu" && device != "coreml-neural-engine")) {
        if (execution != nullptr) {
            *execution = DeviceExecutionResult{};
            execution->reason = "latent operator device batch contract mismatch";
        }
        return results;
    }
    std::vector<float> inputs;
    inputs.reserve(contexts.size() * artifact_.features.size());
    std::vector<std::size_t> device_items;
    device_items.reserve(contexts.size());
    for (std::size_t item = 0; item < contexts.size(); ++item) {
        if (ood_score(contexts[item]) != 0.0) {
            results[item].status = "ood";
            continue;
        }
        device_items.push_back(item);
        for (const auto& feature : artifact_.features) {
            inputs.push_back(static_cast<float>(contexts[item].values.at(feature)));
        }
    }
    if (device_items.empty()) {
        if (execution != nullptr) {
            *execution = DeviceExecutionResult{};
            execution->reason = "latent operator device batch has no in-domain context";
        }
        return results;
    }
    DeviceExecutionResult device_result;
    if (device == "metal-gpu") {
        device_result = metal_gpu_affine_batch(
            inputs, device_items.size(), artifact_.features.size(),
            affine_weights_, artifact_.outputs.size(), affine_bias_,
            2.0e-5, 2.0e-5);
    } else {
        const auto working_directory = std::filesystem::temp_directory_path() /
            ("smave-coreml-latent-operator-" + artifact_.artifact_hash);
        device_result = coreml_neural_engine_affine_tensor_batch(
            inputs, device_items.size(), artifact_.features.size(),
            affine_weights_, artifact_.outputs.size(), affine_bias_,
            working_directory, 2.0e-2, 1.0e-3);
    }
    if (execution != nullptr) *execution = device_result;
    if (!device_result.executed || !device_result.verified ||
        device_result.output.size() != device_items.size() * artifact_.outputs.size()) {
        return results;
    }
    for (std::size_t device_item = 0; device_item < device_items.size(); ++device_item) {
        const auto item = device_items[device_item];
        for (std::size_t output = 0; output < artifact_.outputs.size(); ++output) {
            results[item].candidate[artifact_.outputs[output]] =
                device_result.output[device_item * artifact_.outputs.size() + output];
        }
        results[item].status = "candidate";
        results[item].uncertainty = artifact_.training_rmse;
        results[item].telemetry["latent_rank"] =
            static_cast<double>(artifact_.state_basis.size());
        results[item].telemetry["batch_size"] =
            static_cast<double>(device_items.size());
        results[item].telemetry["device_execution"] = 1.0;
    }
    return results;
}

bool LatentOperatorExpert::operator_batch_is_resident(
    const std::string& device,
    std::size_t batch) const {
    if (device != "coreml-neural-engine" || batch == 0) return false;
    const auto working_directory = std::filesystem::temp_directory_path() /
        ("smave-coreml-latent-operator-" + artifact_.artifact_hash);
    return coreml_neural_engine_affine_tensor_batch_is_resident(
        batch, artifact_.features.size(), affine_weights_,
        artifact_.outputs.size(), affine_bias_, working_directory);
}

std::size_t LatentOperatorExpert::in_domain_batch_size(
    const std::vector<BlockContext>& contexts) const {
    return static_cast<std::size_t>(std::count_if(
        contexts.begin(), contexts.end(),
        [&](const BlockContext& context) { return ood_score(context) == 0.0; }));
}

LatentOperatorArtifact train_latent_operator(
    const ModelIR& model,
    const std::string& block_id,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t maximum_rank,
    std::vector<std::string> qoi_outputs) {
    const auto started = std::chrono::steady_clock::now();
    const auto& block = block_by_id(model, block_id);
    if (block.event_related || !block.smooth || maximum_rank == 0) {
        throw std::invalid_argument(
            "latent operator requires a smooth non-event block");
    }
    LatentOperatorArtifact artifact;
    artifact.model_source_hash = model.source_hash;
    artifact.block_fingerprint = block.fingerprint;
    artifact.features = parameter_features(model, block);
    artifact.outputs = block.unknowns;
    artifact.qoi_outputs = std::move(qoi_outputs);
    if (artifact.qoi_outputs.empty()) artifact.qoi_outputs = artifact.outputs;
    if (artifact.features.empty()) throw std::invalid_argument("operator has no parameters");
    artifact.feature_minimum.assign(
        artifact.features.size(), std::numeric_limits<double>::infinity());
    artifact.feature_maximum.assign(
        artifact.features.size(), -std::numeric_limits<double>::infinity());

    Registry empty_registry;
    RuntimeBundle fallback_bundle;
    fallback_bundle.bundle_id = "operator-labels-" + model.source_hash;
    fallback_bundle.model_source_hash = model.source_hash;
    fallback_bundle.seal();
    const Runtime label_runtime(model, std::move(empty_registry), fallback_bundle);
    std::vector<std::vector<double>> design;
    std::vector<std::vector<double>> states;
    for (const auto& path : scenario_files(scenario_directory)) {
        const auto scenario = read_scenario(path);
        const auto outcome = label_runtime.solve(scenario, trace_directory / path.stem());
        if (!outcome.success) continue;
        std::vector<double> row{1.0};
        for (std::size_t index = 0; index < artifact.features.size(); ++index) {
            const double value = scenario.at(artifact.features[index]);
            row.push_back(value);
            artifact.feature_minimum[index] = std::min(
                artifact.feature_minimum[index], value);
            artifact.feature_maximum[index] = std::max(
                artifact.feature_maximum[index], value);
        }
        std::vector<double> state;
        for (const auto& output : artifact.outputs) state.push_back(outcome.values.at(output));
        design.push_back(std::move(row));
        states.push_back(std::move(state));
    }
    artifact.training_samples = states.size();
    if (states.size() < 2) throw std::invalid_argument("insufficient operator labels");
    artifact.state_mean.assign(artifact.outputs.size(), 0.0);
    for (const auto& state : states) {
        for (std::size_t index = 0; index < state.size(); ++index) {
            artifact.state_mean[index] += state[index];
        }
    }
    for (double& value : artifact.state_mean) value /= static_cast<double>(states.size());
    std::vector<std::vector<double>> centered = states;
    for (auto& state : centered) {
        for (std::size_t index = 0; index < state.size(); ++index) {
            state[index] -= artifact.state_mean[index];
        }
    }
    artifact.state_basis = pod_basis(centered, maximum_rank, artifact.retained_energy);
    std::vector<std::vector<double>> latent_targets(
        artifact.state_basis.size(), std::vector<double>(states.size()));
    for (std::size_t sample = 0; sample < states.size(); ++sample) {
        for (std::size_t component = 0; component < artifact.state_basis.size(); ++component) {
            latent_targets[component][sample] = dot(
                artifact.state_basis[component], centered[sample]);
        }
    }
    const std::size_t width = artifact.features.size() + 1;
    std::vector<std::vector<double>> gram(width, std::vector<double>(width));
    for (const auto& row : design) {
        for (std::size_t left = 0; left < width; ++left) {
            for (std::size_t right = 0; right < width; ++right) {
                gram[left][right] += row[left] * row[right];
            }
        }
    }
    for (std::size_t index = 1; index < width; ++index) gram[index][index] += 1.0e-8;
    artifact.latent_coefficients.resize(artifact.state_basis.size());
    for (std::size_t component = 0; component < artifact.state_basis.size(); ++component) {
        std::vector<double> right(width);
        for (std::size_t sample = 0; sample < design.size(); ++sample) {
            for (std::size_t feature = 0; feature < width; ++feature) {
                right[feature] += design[sample][feature] * latent_targets[component][sample];
            }
        }
        if (!solve_system(gram, right, artifact.latent_coefficients[component])) {
            throw std::runtime_error("latent operator regression is singular");
        }
    }
    double squared_error = 0.0;
    for (std::size_t sample = 0; sample < design.size(); ++sample) {
        for (std::size_t output = 0; output < artifact.outputs.size(); ++output) {
            double prediction = artifact.state_mean[output];
            for (std::size_t component = 0; component < artifact.state_basis.size(); ++component) {
                double latent = artifact.latent_coefficients[component][0];
                for (std::size_t feature = 1; feature < width; ++feature) {
                    latent += artifact.latent_coefficients[component][feature] *
                        design[sample][feature];
                }
                prediction += artifact.state_basis[component][output] * latent;
            }
            squared_error += std::pow(prediction - states[sample][output], 2);
        }
    }
    artifact.training_rmse = std::sqrt(
        squared_error / static_cast<double>(states.size() * artifact.outputs.size()));
    artifact.training_wall_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - started).count();
    artifact.seal();
    artifact.validate();
    return artifact;
}

void register_latent_operator(
    Registry& registry,
    const LatentOperatorArtifact& artifact,
    const std::string& domain_version,
    const std::string& tolerance_profile,
    const std::string& hardware_profile,
    std::optional<VerificationCertificate> certificate) {
    const std::string evidence = certificate
        ? certificate->certificate_hash
        : "operator-labels-" + std::to_string(artifact.training_samples);
    auto expert = std::make_shared<LatentOperatorExpert>(artifact, std::move(certificate));
    registry.register_expert(expert, ExpertGrant{
        .expert_version = expert->version(),
        .block_family = artifact.block_fingerprint,
        .domain_version = domain_version,
        .tolerance_profile = tolerance_profile,
        .hardware_profile = hardware_profile,
        .permission = Permission::corrected,
        .evidence_level = EvidenceLevel::e2,
        .evidence_bundle = evidence,
        .artifact_hash = artifact.artifact_hash,
        .expires_unix_seconds = 0,
    });
}

VerificationCertificate verify_latent_operator(
    const ModelIR& model,
    const LatentOperatorArtifact& artifact,
    std::size_t maximum_depth,
    const std::filesystem::path& trace_directory) {
    artifact.validate();
    auto registry = make_default_registry(model);
    register_latent_operator(registry, artifact);
    auto bundle = make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    const Runtime runtime(model, std::move(registry), bundle);
    auto certificate = verify_cells(
        artifact.expert_version,
        artifact.artifact_hash,
        artifact.block_fingerprint,
        artifact.features,
        artifact.feature_minimum,
        artifact.feature_maximum,
        [&](const std::unordered_map<std::string, double>& context) {
            const auto outcome = runtime.solve(context, trace_directory);
            ProbeResult probe;
            const bool latent_attempted = !outcome.blocks.empty() &&
                std::find(
                    outcome.blocks.front().attempted_experts.begin(),
                    outcome.blocks.front().attempted_experts.end(),
                    artifact.expert_version) !=
                    outcome.blocks.front().attempted_experts.end();
            probe.accepted = outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().path == SolvePath::corrected_accept && latent_attempted;
            if (!outcome.blocks.empty()) {
                probe.residual = outcome.blocks.front().gate.residual_inf;
                probe.risk = static_cast<double>(outcome.blocks.front().expert_iterations) / 8.0;
            }
            probe.reason = probe.accepted
                ? "latent operator corrected candidate passed runtime gate"
                : "latent operator required fallback or failed correction";
            return probe;
        },
        maximum_depth);
    if (artifact.schema_version == "smave.latent-operator.v2") {
        certificate.schema_version = "smave.verified-cells.v2";
        certificate.training_dataset_id = artifact.training_dataset_id;
        certificate.training_dataset_version = artifact.training_dataset_version;
        certificate.training_dataset_manifest_hash = artifact.training_dataset_manifest_hash;
        certificate.seal();
        certificate.validate();
    }
    return certificate;
}

OperatorBenchmarkReport benchmark_latent_operator(
    const ModelIR& model,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const LatentOperatorArtifact& artifact,
    const std::filesystem::path& scenario_directory,
    const std::filesystem::path& trace_directory,
    std::size_t repetitions,
    std::size_t projected_queries,
    Tolerance tolerance,
    std::string device) {
    if (repetitions == 0 || projected_queries == 0) {
        throw std::invalid_argument("operator benchmark counts must be positive");
    }
    if (device != "auto" && device != "cpu" && device != "metal-gpu" &&
        device != "coreml-neural-engine") {
        throw std::invalid_argument(
            "operator benchmark device must be auto, cpu, metal-gpu, or coreml-neural-engine");
    }
    const auto paths = scenario_files(scenario_directory);
    const auto rss_before_bytes = process_peak_rss_bytes();
    RoutingConfig baseline_routing;
    baseline_routing.expert_allowlist = {
        "pcg-ic0-cpu-v1", "pcg-jacobi-cpu-v1", "dense-direct-cpu-v1"};
    const auto runtime_setup_started = std::chrono::steady_clock::now();
    const Runtime baseline(model, tolerance, baseline_routing);
    registry.validate_bundle(bundle, model);
    const Runtime corrector(model, tolerance, baseline_routing);
    std::vector<Expression> diagonal_corrector_residuals;
    std::unordered_map<std::string, Expression> operator_residuals;
    diagonal_corrector_residuals.reserve(model.blocks.front().equation_ids.size());
    operator_residuals.reserve(model.blocks.front().equation_ids.size());
    for (const auto& equation_id : model.blocks.front().equation_ids) {
        const auto equation = std::find_if(
            model.equations.begin(), model.equations.end(),
            [&](const EquationIR& item) { return item.id == equation_id; });
        if (equation == model.equations.end()) {
            throw std::invalid_argument("operator benchmark block references unknown equation");
        }
        diagonal_corrector_residuals.emplace_back(equation->residual);
        operator_residuals.emplace(equation->id, Expression(equation->residual));
    }
    constexpr std::size_t hints_maximum_iterations = 400;
    constexpr std::size_t hints_numerical_to_learned_ratio = 25;
    constexpr double hints_jacobi_weight = 0.8;
    const auto hints_rows = hints_feature_rows(artifact, model.blocks.front());
    const bool hints_schedule_applicable =
        hints_rows.size() == artifact.features.size() && !hints_rows.empty();
    const Runtime online_router(model, registry, bundle, tolerance, {});
    RoutingConfig forced_operator_routing;
    forced_operator_routing.top_k = 1;
    forced_operator_routing.expert_allowlist.insert(artifact.expert_version);
    const Runtime forced_operator(
        model, registry, bundle, tolerance, forced_operator_routing);
    RoutingConfig fallback_only_routing;
    fallback_only_routing.top_k = 1;
    fallback_only_routing.calibration_block_fingerprint =
        model.blocks.front().fingerprint;
    fallback_only_routing.calibration_winner = bundle.terminal_fallback;
    const Runtime fallback_only(
        model, registry, bundle, tolerance, fallback_only_routing);
    const double runtime_setup_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - runtime_setup_started).count();
    const auto operator_setup_started = std::chrono::steady_clock::now();
    const LatentOperatorExpert operator_expert(artifact);
    const double operator_setup_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - operator_setup_started).count();
    struct ExternalBaseline {
        std::string backend;
        std::unique_ptr<Runtime> runtime;
        std::vector<double> times;
        std::size_t attempted{};
        std::size_t native_uses{};
        std::size_t fallbacks{};
        std::size_t failures{};
        double maximum_mixed_qoi_error{};
    };
    std::vector<ExternalBaseline> external_baselines;
    const auto available_candidates = CompileRouter{}.lookup(
        model.blocks.front(), registry, bundle);
    for (const std::string backend : {
             "superlu-dgssv-cpu-v1", "accelerate-sparse-qr-cpu-v1"}) {
        const bool available = std::any_of(
            available_candidates.begin(), available_candidates.end(),
            [&](const CandidateExpert& candidate) {
                return candidate.builtin && candidate.expert_version == backend;
            });
        if (!available) continue;
        RoutingConfig routing;
        routing.top_k = 1;
        routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
        routing.calibration_winner = backend;
        routing.expert_allowlist.insert(backend);
        external_baselines.push_back({
            .backend = backend,
            .runtime = std::make_unique<Runtime>(
                model, registry, bundle, tolerance, routing),
        });
    }
    const auto rss_after_setup_bytes = process_peak_rss_bytes();
    OperatorBenchmarkReport report;
    report.schema_version = 3;
    report.requests = paths.size();
    report.repetitions = repetitions;
    report.batches = repetitions;
    report.average_batch = static_cast<double>(paths.size());
    report.projected_queries = projected_queries;
    report.training_wall_us = artifact.training_wall_us;
    report.artifact_hash = artifact.artifact_hash;
    report.certificate_hash = registry.grant(artifact.expert_version).evidence_bundle;
    std::vector<double> baseline_times;
    std::vector<double> operator_times;
    std::vector<double> candidate_times;
    std::vector<double> candidate_gate_times;
    std::vector<double> fused_candidate_gate_times;
    std::vector<double> batched_candidate_gate_times;
    std::vector<double> correction_times;
    std::vector<double> diagonal_correction_times;
    std::vector<double> shared_hybrid_times;
    std::vector<double> hints_schedule_times;
    std::vector<double> router_online_times;
    std::vector<double> router_forced_times;
    constexpr std::array<std::size_t, 7> correction_budgets{0, 1, 2, 4, 8, 16, 32};
    constexpr std::size_t correction_budget_sweep_repetitions = 30;
    std::array<CorrectionBudgetStatistics, correction_budgets.size()>
        correction_budget_statistics;
    for (std::size_t index = 0; index < correction_budgets.size(); ++index) {
        correction_budget_statistics[index].budget = correction_budgets[index];
    }
    std::size_t router_online_failures{};
    std::size_t router_forced_failures{};
    std::size_t router_online_operator_uses{};
    std::size_t router_forced_operator_uses{};
    std::size_t router_result_mismatches{};
    double router_maximum_mixed_qoi_error{};
    std::size_t candidate_gate_nonreject{};
    std::size_t diagonal_correction_accepts{};
    std::size_t diagonal_correction_failures{};
    std::size_t diagonal_correction_iterations{};
    std::size_t diagonal_correction_gate_mismatches{};
    double diagonal_correction_maximum_residual{};
    std::size_t shared_hybrid_candidate_uses{};
    std::size_t shared_hybrid_accepts{};
    std::size_t shared_hybrid_fallbacks{};
    std::size_t shared_hybrid_failures{};
    std::size_t shared_hybrid_total_iterations{};
    std::size_t shared_hybrid_gate_mismatches{};
    double shared_hybrid_maximum_residual{};
    double shared_hybrid_maximum_mixed_qoi_error{};
    std::size_t hints_schedule_accepts{};
    std::size_t hints_schedule_fallbacks{};
    std::size_t hints_schedule_failures{};
    std::size_t hints_schedule_iterations{};
    std::size_t hints_schedule_jacobi_iterations{};
    std::size_t hints_schedule_learned_corrections{};
    std::size_t hints_schedule_gate_decision_mismatches{};
    std::size_t hints_schedule_gate_residual_mismatches{};
    double hints_schedule_maximum_residual{};
    double hints_schedule_maximum_gate_residual_difference{};
    double hints_schedule_maximum_mixed_qoi_error{};
    std::size_t batched_gate_mismatches{};
    std::size_t batched_gate_false_accepts{};
    std::size_t batched_gate_false_rejects{};
    std::size_t batched_gate_residual_mismatches{};
    std::size_t fused_gate_mismatches{};
    std::size_t fused_gate_false_accepts{};
    std::size_t fused_gate_false_rejects{};
    std::size_t fused_gate_residual_mismatches{};
    double candidate_gate_maximum_residual{};
    std::string selected_device;
    std::string device_backend;
    std::string device_name;
    std::size_t device_batches{};
    std::size_t device_rejections{};
    double device_upload_us{};
    double device_kernel_us{};
    double device_download_us{};
    double device_maximum_absolute_error{};
    double device_maximum_relative_error{};
    std::string device_reason;
    std::vector<std::unordered_map<std::string, double>> scenarios;
    std::vector<BlockContext> contexts;
    for (const auto& path : paths) {
        scenarios.push_back(read_scenario(path));
        contexts.push_back(BlockContext{.values = scenarios.back()});
    }
    LinearSystem hints_linear_system;
    std::vector<double> hints_diagonal;
    if (hints_schedule_applicable) {
        hints_linear_system = assemble_linear_system(
            model, model.blocks.front(), operator_residuals, scenarios.front());
        hints_diagonal.reserve(hints_linear_system.size());
        for (std::size_t row = 0; row < hints_linear_system.size(); ++row) {
            hints_diagonal.push_back(hints_linear_system.coefficient(row, row));
        }
    }
    std::filesystem::remove_all(trace_directory / "shared-hybrid-accepted");
    std::filesystem::remove_all(trace_directory / "shared-hybrid-fallback");
    std::filesystem::remove_all(trace_directory / "hints-accepted");
    std::filesystem::remove_all(trace_directory / "hints-fallback");
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        std::vector<SolveOutcome> baseline_outcomes(paths.size());
        std::vector<SolveOutcome> operator_outcomes(paths.size());
        std::vector<SolveOutcome> shared_hybrid_fallback_outcomes(paths.size());
        std::vector<std::unordered_map<std::string, double>> shared_hybrid_values(
            paths.size());
        std::vector<bool> shared_hybrid_success(paths.size());
        std::vector<SolveOutcome> hints_schedule_fallback_outcomes(paths.size());
        std::vector<std::unordered_map<std::string, double>> hints_schedule_values(
            paths.size());
        std::vector<bool> hints_schedule_success(paths.size());
        std::vector<SolveOutcome> router_online_outcomes(paths.size());
        std::vector<SolveOutcome> router_forced_outcomes(paths.size());
        std::vector<std::vector<SolveOutcome>> external_outcomes(
            external_baselines.size(), std::vector<SolveOutcome>(paths.size()));
        std::vector<ExpertResult> raw_candidates;
        double baseline_total_us{};
        double operator_total_us{};
        double candidate_total_us{};
        double candidate_gate_total_us{};
        double fused_candidate_gate_total_us{};
        double batched_candidate_gate_total_us{};
        double correction_total_us{};
        double diagonal_correction_total_us{};
        double shared_hybrid_total_us{};
        double hints_schedule_total_us{};
        double router_online_total_us{};
        double router_forced_total_us{};
        auto run_baseline = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                baseline_outcomes[item] = baseline.solve(
                    scenarios[item],
                    trace_directory / "baseline" / std::to_string(repetition) /
                        std::to_string(item));
            }
            baseline_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_operator = [&] {
            const auto started = std::chrono::steady_clock::now();
            const auto device_operations = contexts.size() *
                artifact.features.size() * artifact.outputs.size();
            const auto resident_batch = operator_expert.in_domain_batch_size(contexts);
            selected_device = device == "auto" &&
                    device_operations >= 16U * 1024U * 1024U &&
                    operator_expert.operator_batch_is_resident(
                        "coreml-neural-engine", resident_batch)
                ? "coreml-neural-engine"
                : (device == "auto" ? "cpu" : device);
            DeviceExecutionResult execution;
            raw_candidates = operator_expert.solve_batch_on_device(
                selected_device, model.blocks.front(), contexts, &execution);
            candidate_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
            if (selected_device != "cpu") {
                device_backend = execution.backend;
                device_name = execution.device_name;
                device_upload_us += execution.upload_us;
                device_kernel_us += execution.kernel_us;
                device_download_us += execution.download_us;
                device_maximum_absolute_error = std::max(
                    device_maximum_absolute_error,
                    execution.maximum_absolute_error);
                device_maximum_relative_error = std::max(
                    device_maximum_relative_error,
                    execution.maximum_relative_error);
                device_reason = execution.reason;
                if (execution.executed && execution.verified) ++device_batches;
                else ++device_rejections;
            }
            const auto correction_started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                if (raw_candidates[item].status == "candidate") {
                    operator_outcomes[item] = corrector.correct_candidate(
                        scenarios[item],
                        model.blocks.front().id,
                        raw_candidates[item].candidate,
                        artifact.expert_version,
                        trace_directory / "operator" / std::to_string(repetition) /
                            std::to_string(item));
                } else {
                    operator_outcomes[item] = corrector.solve(
                        scenarios[item],
                        trace_directory / "operator-fallback" /
                            std::to_string(repetition) / std::to_string(item));
                }
            }
            correction_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - correction_started).count();
            operator_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
            std::vector<std::unordered_map<std::string, double>> candidate_values;
            candidate_values.reserve(scenarios.size());
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                candidate_values.push_back(scenarios[item]);
                if (raw_candidates[item].status != "candidate") continue;
                for (const auto& [name, value] : raw_candidates[item].candidate) {
                    candidate_values.back().insert_or_assign(name, value);
                }
            }
            const auto diagonal_correction_started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                if (raw_candidates[item].status != "candidate") {
                    ++diagonal_correction_failures;
                    continue;
                }
                const auto corrected = diagonal_residual_correction(
                    model,
                    model.blocks.front(),
                    diagonal_corrector_residuals,
                    scenarios[item],
                    raw_candidates[item].candidate,
                    corrector,
                    32);
                diagonal_correction_iterations += corrected.iterations;
                diagonal_correction_maximum_residual = std::max(
                    diagonal_correction_maximum_residual, corrected.gate.residual_inf);
                const auto reference_gate = corrector.evaluate_gate_reference(
                    model.blocks.front(), corrected.values, true);
                if (reference_gate.decision != corrected.gate.decision ||
                    std::abs(reference_gate.residual_inf - corrected.gate.residual_inf) >
                        1.0e-15 * std::max(
                            {1.0, std::abs(reference_gate.residual_inf),
                             std::abs(corrected.gate.residual_inf)})) {
                    ++diagonal_correction_gate_mismatches;
                }
                if (corrected.gate.decision == GateDecision::direct_accept) {
                    ++diagonal_correction_accepts;
                } else {
                    ++diagonal_correction_failures;
                }
            }
            diagonal_correction_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - diagonal_correction_started).count();
            std::vector<GateResult> scalar_gates;
            scalar_gates.reserve(candidate_values.size());
            const auto gate_started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                const auto gate = corrector.evaluate_gate_reference(
                    model.blocks.front(), candidate_values[item], false);
                scalar_gates.push_back(gate);
                if (gate.decision != GateDecision::reject) ++candidate_gate_nonreject;
                candidate_gate_maximum_residual = std::max(
                    candidate_gate_maximum_residual, gate.residual_inf);
            }
            candidate_gate_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - gate_started).count();
            std::vector<GateResult> fused_gates;
            fused_gates.reserve(candidate_values.size());
            const auto fused_gate_started = std::chrono::steady_clock::now();
            for (const auto& values : candidate_values) {
                fused_gates.push_back(corrector.evaluate_gate_fused(
                    model.blocks.front(), values, false));
            }
            fused_candidate_gate_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - fused_gate_started).count();
            for (std::size_t item = 0; item < scalar_gates.size(); ++item) {
                const double residual_scale = std::max(
                    {1.0, std::abs(scalar_gates[item].residual_inf),
                     std::abs(fused_gates[item].residual_inf)});
                if (scalar_gates[item].decision != fused_gates[item].decision ||
                    std::abs(scalar_gates[item].residual_inf -
                             fused_gates[item].residual_inf) >
                        1.0e-15 * residual_scale) {
                    ++fused_gate_mismatches;
                    if (scalar_gates[item].decision == GateDecision::reject &&
                        fused_gates[item].decision != GateDecision::reject) {
                        ++fused_gate_false_accepts;
                    } else if (scalar_gates[item].decision != GateDecision::reject &&
                               fused_gates[item].decision == GateDecision::reject) {
                        ++fused_gate_false_rejects;
                    } else {
                        ++fused_gate_residual_mismatches;
                    }
                }
            }
            const auto batched_gate_started = std::chrono::steady_clock::now();
            const auto batched_gates = corrector.evaluate_gate_batch(
                model.blocks.front(), candidate_values, false);
            batched_candidate_gate_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - batched_gate_started).count();
            if (batched_gates.size() != scalar_gates.size()) {
                batched_gate_mismatches += std::max(
                    batched_gates.size(), scalar_gates.size());
            } else {
                for (std::size_t item = 0; item < scalar_gates.size(); ++item) {
                    const double residual_scale = std::max(
                        {1.0, std::abs(scalar_gates[item].residual_inf),
                         std::abs(batched_gates[item].residual_inf)});
                    if (scalar_gates[item].decision != batched_gates[item].decision ||
                        std::abs(scalar_gates[item].residual_inf -
                                 batched_gates[item].residual_inf) >
                            1.0e-12 * residual_scale) {
                        ++batched_gate_mismatches;
                        if (scalar_gates[item].decision == GateDecision::reject &&
                            batched_gates[item].decision != GateDecision::reject) {
                            ++batched_gate_false_accepts;
                        } else if (scalar_gates[item].decision != GateDecision::reject &&
                                   batched_gates[item].decision == GateDecision::reject) {
                            ++batched_gate_false_rejects;
                        } else {
                            ++batched_gate_residual_mismatches;
                        }
                    }
                }
            }
        };
        auto run_router_online = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                router_online_outcomes[item] = online_router.solve(
                    scenarios[item],
                    trace_directory / "router-online" / std::to_string(repetition) /
                        std::to_string(item));
            }
            router_online_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_shared_hybrid = [&] {
            const auto started = std::chrono::steady_clock::now();
            const auto candidates = operator_expert.solve_batch_on_device(
                "cpu", model.blocks.front(), contexts);
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                bool accepted = false;
                if (candidates[item].status == "candidate") {
                    ++shared_hybrid_candidate_uses;
                    const auto corrected = diagonal_residual_correction(
                        model,
                        model.blocks.front(),
                        diagonal_corrector_residuals,
                        scenarios[item],
                        candidates[item].candidate,
                        corrector,
                        32);
                    shared_hybrid_total_iterations += corrected.iterations;
                    shared_hybrid_maximum_residual = std::max(
                        shared_hybrid_maximum_residual, corrected.gate.residual_inf);
                    const auto reference_gate = corrector.evaluate_gate_reference(
                        model.blocks.front(), corrected.values, true);
                    if (reference_gate.decision != corrected.gate.decision ||
                        std::abs(reference_gate.residual_inf - corrected.gate.residual_inf) >
                            1.0e-15 * std::max(
                                {1.0, std::abs(reference_gate.residual_inf),
                                 std::abs(corrected.gate.residual_inf)})) {
                        ++shared_hybrid_gate_mismatches;
                    }
                    if (corrected.gate.decision == GateDecision::direct_accept) {
                        std::unordered_map<std::string, double> corrected_candidate;
                        corrected_candidate.reserve(model.blocks.front().unknowns.size());
                        for (const auto& unknown : model.blocks.front().unknowns) {
                            corrected_candidate.emplace(unknown, corrected.values.at(unknown));
                        }
                        auto committed = corrector.commit_corrected_candidate(
                            scenarios[item],
                            model.blocks.front().id,
                            corrected_candidate,
                            "shared-weighted-jacobi-hybrid-cpu-v1",
                            trace_directory / "shared-hybrid-accepted" /
                                std::to_string(repetition) / std::to_string(item));
                        accepted = committed.success && !committed.blocks.empty() &&
                            committed.blocks.front().path == SolvePath::corrected_accept;
                        if (accepted) {
                            shared_hybrid_values[item] = committed.values;
                            shared_hybrid_success[item] = true;
                            ++shared_hybrid_accepts;
                        } else {
                            shared_hybrid_fallback_outcomes[item] = std::move(committed);
                        }
                    }
                }
                if (accepted) continue;
                ++shared_hybrid_fallbacks;
                if (!shared_hybrid_fallback_outcomes[item].success) {
                    shared_hybrid_fallback_outcomes[item] = fallback_only.solve(
                        scenarios[item],
                        trace_directory / "shared-hybrid-fallback" /
                            std::to_string(repetition) / std::to_string(item));
                }
                shared_hybrid_success[item] =
                    shared_hybrid_fallback_outcomes[item].success;
                if (shared_hybrid_success[item]) {
                    shared_hybrid_values[item] =
                        shared_hybrid_fallback_outcomes[item].values;
                } else {
                    ++shared_hybrid_failures;
                }
            }
            shared_hybrid_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_hints_baseline = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                bool accepted = false;
                if (hints_schedule_applicable) {
                    update_linear_right_hand_side(
                        hints_linear_system,
                        model,
                        model.blocks.front(),
                        operator_residuals,
                        scenarios[item]);
                    const auto result = run_hints_schedule(
                        model,
                        model.blocks.front(),
                        scenarios[item],
                        artifact,
                        hints_rows,
                        hints_linear_system,
                        hints_diagonal,
                        corrector,
                        hints_maximum_iterations,
                        hints_numerical_to_learned_ratio,
                        hints_jacobi_weight);
                    hints_schedule_iterations += result.iterations;
                    hints_schedule_jacobi_iterations += result.jacobi_iterations;
                    hints_schedule_learned_corrections += result.learned_corrections;
                    hints_schedule_maximum_residual = std::max(
                        hints_schedule_maximum_residual, result.gate.residual_inf);
                    const auto reference_gate = corrector.evaluate_gate_reference(
                        model.blocks.front(), result.values, true);
                    const double gate_residual_difference = std::abs(
                        reference_gate.residual_inf - result.gate.residual_inf);
                    hints_schedule_maximum_gate_residual_difference = std::max(
                        hints_schedule_maximum_gate_residual_difference,
                        gate_residual_difference);
                    if (reference_gate.decision != result.gate.decision) {
                        ++hints_schedule_gate_decision_mismatches;
                    }
                    if (gate_residual_difference >
                        1.0e-12 * std::max(
                            {1.0, std::abs(reference_gate.residual_inf),
                             std::abs(result.gate.residual_inf)})) {
                        ++hints_schedule_gate_residual_mismatches;
                    }
                    if (result.valid &&
                        result.gate.decision == GateDecision::direct_accept) {
                        std::unordered_map<std::string, double> candidate;
                        candidate.reserve(model.blocks.front().unknowns.size());
                        for (const auto& unknown : model.blocks.front().unknowns) {
                            candidate.emplace(unknown, result.values.at(unknown));
                        }
                        auto committed = corrector.commit_corrected_candidate(
                            scenarios[item],
                            model.blocks.front().id,
                            candidate,
                            "hints-official-schedule-latent-operator-cpu-v1",
                            trace_directory / "hints-accepted" /
                                std::to_string(repetition) / std::to_string(item));
                        accepted = committed.success && !committed.blocks.empty() &&
                            committed.blocks.front().path == SolvePath::corrected_accept;
                        if (accepted) {
                            hints_schedule_values[item] = committed.values;
                            hints_schedule_success[item] = true;
                            ++hints_schedule_accepts;
                        } else {
                            hints_schedule_fallback_outcomes[item] = std::move(committed);
                        }
                    }
                }
                if (accepted) continue;
                ++hints_schedule_fallbacks;
                if (!hints_schedule_fallback_outcomes[item].success) {
                    hints_schedule_fallback_outcomes[item] = fallback_only.solve(
                        scenarios[item],
                        trace_directory / "hints-fallback" /
                            std::to_string(repetition) / std::to_string(item));
                }
                hints_schedule_success[item] =
                    hints_schedule_fallback_outcomes[item].success;
                if (hints_schedule_success[item]) {
                    hints_schedule_values[item] =
                        hints_schedule_fallback_outcomes[item].values;
                } else {
                    ++hints_schedule_failures;
                }
            }
            hints_schedule_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_router_forced = [&] {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                router_forced_outcomes[item] = forced_operator.solve(
                    scenarios[item],
                    trace_directory / "router-forced" / std::to_string(repetition) /
                        std::to_string(item));
            }
            router_forced_total_us = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - started).count();
        };
        auto run_external = [&](std::size_t external_index) {
            auto& external = external_baselines[external_index];
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                external_outcomes[external_index][item] = external.runtime->solve(
                    scenarios[item],
                    trace_directory / "external" / external.backend /
                        std::to_string(repetition) / std::to_string(item));
            }
            external.times.push_back(
                std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - started).count() /
                static_cast<double>(paths.size()));
        };
        const std::size_t measured_paths =
            (hints_schedule_applicable ? 6 : 5) + external_baselines.size();
        for (std::size_t offset = 0; offset < measured_paths; ++offset) {
            const std::size_t selected = (repetition + offset) % measured_paths;
            if (selected == 0) run_baseline();
            else if (selected == 1) run_operator();
            else if (selected == 2) run_router_online();
            else if (selected == 3) run_router_forced();
            else if (selected == 4) run_shared_hybrid();
            else if (hints_schedule_applicable && selected == 5) run_hints_baseline();
            else run_external(selected - (hints_schedule_applicable ? 6 : 5));
        }
        baseline_times.push_back(baseline_total_us / static_cast<double>(paths.size()));
        operator_times.push_back(operator_total_us / static_cast<double>(paths.size()));
        candidate_times.push_back(candidate_total_us / static_cast<double>(paths.size()));
        candidate_gate_times.push_back(
            candidate_gate_total_us / static_cast<double>(paths.size()));
        fused_candidate_gate_times.push_back(
            fused_candidate_gate_total_us / static_cast<double>(paths.size()));
        batched_candidate_gate_times.push_back(
            batched_candidate_gate_total_us / static_cast<double>(paths.size()));
        correction_times.push_back(correction_total_us / static_cast<double>(paths.size()));
        diagonal_correction_times.push_back(
            diagonal_correction_total_us / static_cast<double>(paths.size()));
        shared_hybrid_times.push_back(
            shared_hybrid_total_us / static_cast<double>(paths.size()));
        if (hints_schedule_applicable) {
            hints_schedule_times.push_back(
                hints_schedule_total_us / static_cast<double>(paths.size()));
        }
        router_online_times.push_back(
            router_online_total_us / static_cast<double>(paths.size()));
        router_forced_times.push_back(
            router_forced_total_us / static_cast<double>(paths.size()));
        for (std::size_t item = 0; item < paths.size(); ++item) {
            const auto& baseline_outcome = baseline_outcomes[item];
            const auto& operator_outcome = operator_outcomes[item];
            const auto& online_outcome = router_online_outcomes[item];
            const auto& forced_outcome = router_forced_outcomes[item];
            if (!baseline_outcome.success || !operator_outcome.success) ++report.failures;
            const bool used_operator = operator_outcome.success &&
                !operator_outcome.blocks.empty() &&
                operator_outcome.blocks.front().path == SolvePath::corrected_accept &&
                std::find(
                    operator_outcome.blocks.front().attempted_experts.begin(),
                    operator_outcome.blocks.front().attempted_experts.end(),
                    artifact.expert_version) !=
                    operator_outcome.blocks.front().attempted_experts.end();
            if (used_operator) ++report.accepted;
            else ++report.fallbacks;
            if (!online_outcome.success) ++router_online_failures;
            if (!forced_outcome.success) ++router_forced_failures;
            const auto used_runtime_operator = [&](const SolveOutcome& outcome) {
                return outcome.success && !outcome.blocks.empty() &&
                    outcome.blocks.front().path == SolvePath::corrected_accept &&
                    std::find(
                        outcome.blocks.front().attempted_experts.begin(),
                        outcome.blocks.front().attempted_experts.end(),
                        artifact.expert_version) !=
                        outcome.blocks.front().attempted_experts.end();
            };
            if (used_runtime_operator(online_outcome)) ++router_online_operator_uses;
            if (used_runtime_operator(forced_outcome)) ++router_forced_operator_uses;
            for (const auto& output : artifact.outputs) {
                report.maximum_full_state_error = std::max(
                    report.maximum_full_state_error,
                    normalized_error(
                        baseline_outcome.values.at(output),
                        operator_outcome.values.at(output),
                        tolerance.qoi_relative));
                if (raw_candidates[item].status == "candidate") {
                    report.maximum_candidate_full_state_error = std::max(
                        report.maximum_candidate_full_state_error,
                        normalized_error(
                            baseline_outcome.values.at(output),
                            raw_candidates[item].candidate.at(output),
                            tolerance.qoi_relative));
                }
                if (baseline_outcome.success && online_outcome.success &&
                    forced_outcome.success) {
                    const double online_error = normalized_error(
                        baseline_outcome.values.at(output),
                        online_outcome.values.at(output),
                        tolerance.qoi_relative);
                    const double forced_error = normalized_error(
                        baseline_outcome.values.at(output),
                        forced_outcome.values.at(output),
                        tolerance.qoi_relative);
                    router_maximum_mixed_qoi_error = std::max(
                        {router_maximum_mixed_qoi_error, online_error, forced_error});
                    if (normalized_error(
                            online_outcome.values.at(output),
                            forced_outcome.values.at(output),
                            tolerance.qoi_relative) > 1.0) {
                        ++router_result_mismatches;
                    }
                }
            }
            for (const auto& qoi : artifact.qoi_outputs) {
                report.maximum_qoi_error = std::max(
                    report.maximum_qoi_error,
                    normalized_error(
                        baseline_outcome.values.at(qoi),
                        operator_outcome.values.at(qoi),
                        tolerance.qoi_relative));
                if (raw_candidates[item].status == "candidate") {
                    report.maximum_candidate_qoi_error = std::max(
                        report.maximum_candidate_qoi_error,
                        normalized_error(
                            baseline_outcome.values.at(qoi),
                            raw_candidates[item].candidate.at(qoi),
                            tolerance.qoi_relative));
                }
            }
            if (baseline_outcome.success && shared_hybrid_success[item]) {
                for (const auto& output : artifact.outputs) {
                    shared_hybrid_maximum_mixed_qoi_error = std::max(
                        shared_hybrid_maximum_mixed_qoi_error,
                        normalized_error(
                            baseline_outcome.values.at(output),
                            shared_hybrid_values[item].at(output),
                            tolerance.qoi_relative));
                }
            }
            if (hints_schedule_applicable && baseline_outcome.success &&
                hints_schedule_success[item]) {
                for (const auto& output : artifact.outputs) {
                    hints_schedule_maximum_mixed_qoi_error = std::max(
                        hints_schedule_maximum_mixed_qoi_error,
                        normalized_error(
                            baseline_outcome.values.at(output),
                            hints_schedule_values[item].at(output),
                            tolerance.qoi_relative));
                }
            }
            for (std::size_t external_index = 0;
                 external_index < external_baselines.size(); ++external_index) {
                auto& external = external_baselines[external_index];
                const auto& external_outcome = external_outcomes[external_index][item];
                ++external.attempted;
                if (!external_outcome.success) ++external.failures;
                bool native_use = false;
                if (!external_outcome.blocks.empty()) {
                    const auto& external_block = external_outcome.blocks.front();
                    native_use = external_block.path != SolvePath::full_fallback &&
                        std::find(
                            external_block.attempted_experts.begin(),
                            external_block.attempted_experts.end(), external.backend) !=
                            external_block.attempted_experts.end();
                }
                if (native_use) ++external.native_uses;
                else ++external.fallbacks;
                if (baseline_outcome.success && external_outcome.success) {
                    for (const auto& output : artifact.outputs) {
                        external.maximum_mixed_qoi_error = std::max(
                            external.maximum_mixed_qoi_error,
                            normalized_error(
                                baseline_outcome.values.at(output),
                                external_outcome.values.at(output),
                                tolerance.qoi_relative));
                    }
                }
            }
        }
    }
    const auto correction_sweep_repetitions =
        std::min(repetitions, correction_budget_sweep_repetitions);
    for (std::size_t repetition = 0;
         repetition < correction_sweep_repetitions; ++repetition) {
        const auto candidate_started = std::chrono::steady_clock::now();
        const auto candidates = operator_expert.solve_batch_on_device(
            "cpu", model.blocks.front(), contexts);
        const double candidate_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - candidate_started).count();
        for (std::size_t offset = 0; offset < correction_budget_statistics.size();
             ++offset) {
            auto& statistics = correction_budget_statistics[
                (repetition + offset) % correction_budget_statistics.size()];
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t item = 0; item < scenarios.size(); ++item) {
                SolveOutcome outcome;
                bool accepted = false;
                if (candidates[item].status != "candidate") {
                    outcome = fallback_only.solve(
                        scenarios[item], trace_directory / "correction-budget" /
                            std::to_string(statistics.budget) /
                            std::to_string(repetition) / std::to_string(item));
                } else if (statistics.budget == 0) {
                    outcome = corrector.commit_corrected_candidate(
                        scenarios[item], model.blocks.front().id,
                        candidates[item].candidate, artifact.expert_version,
                        trace_directory / "correction-budget" / "0" /
                            std::to_string(repetition) / std::to_string(item));
                    accepted = outcome.success && !outcome.blocks.empty() &&
                        outcome.blocks.front().path == SolvePath::corrected_accept;
                } else {
                    outcome = corrector.correct_candidate(
                        scenarios[item], model.blocks.front().id,
                        candidates[item].candidate, artifact.expert_version,
                        trace_directory / "correction-budget" /
                            std::to_string(statistics.budget) /
                            std::to_string(repetition) / std::to_string(item),
                        static_cast<int>(statistics.budget));
                    accepted = outcome.success && !outcome.blocks.empty() &&
                        outcome.blocks.front().path == SolvePath::corrected_accept;
                }
                if (accepted) ++statistics.accepts;
                else ++statistics.fallbacks;
                if (!outcome.success || outcome.blocks.empty()) {
                    ++statistics.failures;
                    continue;
                }
                if (accepted) {
                    statistics.correction_iterations += static_cast<std::size_t>(
                        std::max(0, outcome.blocks.front().expert_iterations));
                    statistics.maximum_accepted_residual = std::max(
                        statistics.maximum_accepted_residual,
                        outcome.blocks.front().gate.residual_inf);
                }
            }
            const double correction_and_fallback_us =
                std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - started).count();
            statistics.complete_times.push_back(
                (candidate_us + correction_and_fallback_us) /
                static_cast<double>(paths.size()));
        }
    }
    constexpr std::uint64_t bootstrap_seed = 20260720;
    constexpr std::size_t bootstrap_resamples = 10000;
    const auto paired_interval = paired_median_bootstrap(
        baseline_times, operator_times, bootstrap_seed, bootstrap_resamples);
    std::vector<double> paired_savings;
    paired_savings.reserve(baseline_times.size());
    for (std::size_t index = 0; index < baseline_times.size(); ++index) {
        paired_savings.push_back(baseline_times[index] - operator_times[index]);
    }
    report.baseline_median_us = quantile(baseline_times, 0.5);
    report.operator_median_us = quantile(operator_times, 0.5);
    report.online_speedup = paired_interval.median;
    report.paired_speedup_ci95_lower = paired_interval.lower;
    report.paired_speedup_ci95_upper = paired_interval.upper;
    report.paired_median_saving_us = quantile(paired_savings, 0.5);
    report.break_even_queries = report.paired_median_saving_us > 0.0
        ? static_cast<std::size_t>(
              std::ceil(report.training_wall_us / report.paired_median_saving_us))
        : std::numeric_limits<std::size_t>::max();
    report.amortized_speedup = paired_amortized_speedup(
        report.online_speedup, report.paired_median_saving_us,
        report.training_wall_us, projected_queries);
    report.same_accuracy = report.failures == 0 && report.maximum_full_state_error <= 1.0 &&
        report.maximum_qoi_error <= 1.0;
    report.candidate_qoi_within_tolerance =
        report.maximum_candidate_qoi_error <= 1.0;
    report.acceptance_rate = static_cast<double>(report.accepted) /
        static_cast<double>(report.requests * report.repetitions);
    report.break_even_met = report.paired_speedup_ci95_lower > 1.0 &&
        report.break_even_queries <= projected_queries && report.amortized_speedup > 1.0 &&
        report.acceptance_rate >= 0.95 && report.same_accuracy;
    report.seal();
    report.validate();
    std::filesystem::create_directories(trace_directory);
    const auto router_interval = paired_median_bootstrap(
        router_online_times, router_forced_times, bootstrap_seed, bootstrap_resamples);
    const auto shared_hybrid_interval = paired_median_bootstrap(
        baseline_times, shared_hybrid_times, bootstrap_seed, bootstrap_resamples);
    const auto operator_vs_shared_hybrid_interval = paired_median_bootstrap(
        shared_hybrid_times, operator_times, bootstrap_seed + 1, bootstrap_resamples);
    BootstrapInterval hints_schedule_interval;
    BootstrapInterval operator_vs_hints_schedule_interval;
    if (hints_schedule_applicable) {
        hints_schedule_interval = paired_median_bootstrap(
            baseline_times, hints_schedule_times, bootstrap_seed, bootstrap_resamples);
        operator_vs_hints_schedule_interval = paired_median_bootstrap(
            hints_schedule_times, operator_times, bootstrap_seed + 2, bootstrap_resamples);
    }
    std::unordered_map<std::string, double> rejected_candidate;
    for (const auto& output : artifact.outputs) {
        rejected_candidate.emplace(output, 1.0e12);
    }
    const auto fallback_probe = fallback_only.correct_candidate(
        scenarios.front(), model.blocks.front().id, rejected_candidate,
        artifact.expert_version, trace_directory / "same-workload-fallback-probe", 1);
    const bool fallback_original_solver_used = fallback_probe.success &&
        !fallback_probe.blocks.empty() &&
        fallback_probe.blocks.front().path == SolvePath::full_fallback;
    const std::vector<double> hot_baseline_times(
        baseline_times.begin() + 1, baseline_times.end());
    const std::vector<double> hot_operator_times(
        operator_times.begin() + 1, operator_times.end());
    const auto peak_rss_bytes = process_peak_rss_bytes();
    std::ofstream statistics(trace_directory / "operator-statistics.txt");
    if (!statistics) throw std::runtime_error("cannot write operator statistics report");
    statistics << std::setprecision(17)
               << "SMAVE_OPERATOR_STATISTICS 1\n"
               << "repetitions=" << repetitions << '\n'
               << "cold_semantics=first-measured-batch\n"
               << "cold_baseline_us=" << baseline_times.front() << '\n'
               << "cold_operator_us=" << operator_times.front() << '\n'
               << "hot_semantics=remaining-measured-batches\n"
               << "hot_repetitions=" << hot_baseline_times.size() << '\n'
               << "hot_baseline_median_us="
               << (hot_baseline_times.empty() ? 0.0 : quantile(hot_baseline_times, 0.5))
               << '\n'
               << "hot_operator_median_us="
               << (hot_operator_times.empty() ? 0.0 : quantile(hot_operator_times, 0.5))
               << '\n'
               << "runtime_setup_us=" << runtime_setup_us << '\n'
               << "runtime_setup_semantics=baseline-and-corrector-construction-plus-bundle-validation\n"
               << "operator_setup_us=" << operator_setup_us << '\n'
               << "operator_setup_semantics=latent-expert-construction-from-in-memory-artifact\n"
               << "rss_before_bytes=" << rss_before_bytes << '\n'
               << "rss_after_setup_bytes=" << rss_after_setup_bytes << '\n'
               << "peak_rss_bytes=" << peak_rss_bytes << '\n'
               << "peak_rss_semantics=process-lifetime-high-water-mark\n"
               << "energy_available=0\n"
               << "energy_source=unavailable-portable-process-counter\n"
               << "baseline_median_us=" << quantile(baseline_times, 0.5) << '\n'
               << "baseline_p90_us=" << quantile(baseline_times, 0.9) << '\n'
               << "baseline_p99_us=" << quantile(baseline_times, 0.99) << '\n'
               << "baseline_worst_us=" << quantile(baseline_times, 1.0) << '\n'
               << "operator_median_us=" << quantile(operator_times, 0.5) << '\n'
               << "operator_p90_us=" << quantile(operator_times, 0.9) << '\n'
               << "operator_p99_us=" << quantile(operator_times, 0.99) << '\n'
               << "operator_worst_us=" << quantile(operator_times, 1.0) << '\n'
               << "paired_median_speedup=" << paired_interval.median << '\n'
               << "bootstrap_seed=" << bootstrap_seed << '\n'
               << "bootstrap_resamples=" << bootstrap_resamples << '\n'
               << "bootstrap_95_lower=" << paired_interval.lower << '\n'
               << "bootstrap_95_upper=" << paired_interval.upper << '\n'
               << "stable_speedup=" << (paired_interval.lower > 1.0) << '\n'
               << "END\n";
    std::ofstream ablation(trace_directory / "operator-ablation.txt");
    if (!ablation) throw std::runtime_error("cannot write operator ablation report");
    const auto candidate_samples = report.requests * report.repetitions;
    const auto correction_budget_samples = report.requests *
        correction_sweep_repetitions;
    const auto best_correction_budget = std::min_element(
        correction_budget_statistics.begin(), correction_budget_statistics.end(),
        [](const auto& left, const auto& right) {
            return quantile(left.complete_times, 0.5) <
                quantile(right.complete_times, 0.5);
        });
    const auto minimum_full_acceptance_budget = std::find_if(
        correction_budget_statistics.begin(), correction_budget_statistics.end(),
        [&](const auto& statistics) {
            return statistics.accepts == correction_budget_samples;
        });
    if (minimum_full_acceptance_budget == correction_budget_statistics.end()) {
        throw std::runtime_error(
            "production corrector budget sweep has no full-acceptance budget");
    }
    ablation << std::setprecision(17)
             << "SMAVE_OPERATOR_ABLATION 1\n"
             << "requests=" << report.requests << '\n'
             << "repetitions=" << report.repetitions << '\n'
             << "classic_median_us=" << quantile(baseline_times, 0.5) << '\n'
             << "raw_candidate_median_us=" << quantile(candidate_times, 0.5) << '\n'
             << "independent_gate_median_us="
             << quantile(candidate_gate_times, 0.5) << '\n'
             << "fused_original_gate_median_us="
             << quantile(fused_candidate_gate_times, 0.5) << '\n'
             << "fused_original_gate_speedup="
             << quantile(candidate_gate_times, 0.5) /
                    quantile(fused_candidate_gate_times, 0.5) << '\n'
             << "fused_original_gate_mismatches=" << fused_gate_mismatches << '\n'
             << "fused_original_gate_false_accepts=" << fused_gate_false_accepts << '\n'
             << "fused_original_gate_false_rejects=" << fused_gate_false_rejects << '\n'
             << "fused_original_gate_residual_mismatches="
             << fused_gate_residual_mismatches << '\n'
             << "batched_original_gate_median_us="
             << quantile(batched_candidate_gate_times, 0.5) << '\n'
             << "batched_original_gate_speedup="
             << quantile(candidate_gate_times, 0.5) /
                    quantile(batched_candidate_gate_times, 0.5) << '\n'
             << "batched_original_gate_mismatches=" << batched_gate_mismatches << '\n'
             << "batched_original_gate_false_accepts="
             << batched_gate_false_accepts << '\n'
             << "batched_original_gate_false_rejects="
             << batched_gate_false_rejects << '\n'
             << "batched_original_gate_residual_mismatches="
             << batched_gate_residual_mismatches << '\n'
             << "correction_and_runtime_gate_median_us="
             << quantile(correction_times, 0.5) << '\n'
             << "external_corrector_contract=weighted-diagonal-residual-jacobi-plus-strict-gate\n"
             << "external_corrector_reimplementation=1\n"
             << "external_corrector_public_code_used=0\n"
             << "external_corrector_maximum_iterations=32\n"
             << "external_corrector_median_us="
             << quantile(diagonal_correction_times, 0.5) << '\n'
             << "external_corrector_accepts=" << diagonal_correction_accepts << '\n'
             << "external_corrector_failures=" << diagonal_correction_failures << '\n'
             << "external_corrector_acceptance_rate="
             << static_cast<double>(diagonal_correction_accepts) /
                    static_cast<double>(candidate_samples) << '\n'
             << "external_corrector_total_iterations="
             << diagonal_correction_iterations << '\n'
             << "external_corrector_gate_mismatches="
             << diagonal_correction_gate_mismatches << '\n'
             << "external_corrector_maximum_residual="
             << diagonal_correction_maximum_residual << '\n'
             << "production_corrector_budget_sweep=0,1,2,4,8,16,32\n"
             << "production_corrector_budget_sweep_repetitions="
             << correction_sweep_repetitions << '\n'
             << "production_corrector_budget_sweep_samples_per_budget="
             << correction_budget_samples << '\n'
             << "production_corrector_budget_sweep_cost_semantics="
                "candidate-plus-observed-correction-gate-and-fallback;batch-normalized-per-request\n"
             << "production_corrector_minimum_full_acceptance_budget="
             << minimum_full_acceptance_budget->budget << '\n'
             << "production_corrector_best_budget=" << best_correction_budget->budget << '\n'
             << "production_corrector_best_complete_median_us="
             << quantile(best_correction_budget->complete_times, 0.5) << '\n'
             << "production_corrector_best_complete_over_classic="
             << quantile(best_correction_budget->complete_times, 0.5) /
                    quantile(baseline_times, 0.5) << '\n';
    for (const auto& statistics : correction_budget_statistics) {
        ablation << "production_budget." << statistics.budget
                 << ".complete_median_us="
                 << quantile(statistics.complete_times, 0.5) << '\n'
                 << "production_budget." << statistics.budget
                 << ".complete_over_classic="
                 << quantile(statistics.complete_times, 0.5) /
                        quantile(baseline_times, 0.5) << '\n'
                 << "production_budget." << statistics.budget
                 << ".acceptance_rate="
                 << static_cast<double>(statistics.accepts) /
                        static_cast<double>(correction_budget_samples) << '\n'
                 << "production_budget." << statistics.budget
                 << ".fallback_rate="
                 << static_cast<double>(statistics.fallbacks) /
                        static_cast<double>(correction_budget_samples) << '\n'
                 << "production_budget." << statistics.budget
                 << ".failures=" << statistics.failures << '\n'
                 << "production_budget." << statistics.budget
                 << ".correction_iterations=" << statistics.correction_iterations << '\n'
                 << "production_budget." << statistics.budget
                 << ".maximum_accepted_residual="
                 << statistics.maximum_accepted_residual << '\n';
    }
    ablation
             << "full_verified_median_us=" << quantile(operator_times, 0.5) << '\n'
             << "single_workload_component_matrix=1\n"
             << "router_online_median_us=" << quantile(router_online_times, 0.5) << '\n'
             << "router_forced_operator_median_us="
             << quantile(router_forced_times, 0.5) << '\n'
             << "router_online_over_forced_median=" << router_interval.median << '\n'
             << "router_online_over_forced_bootstrap_95_lower="
             << router_interval.lower << '\n'
             << "router_online_over_forced_bootstrap_95_upper="
             << router_interval.upper << '\n'
             << "router_online_failures=" << router_online_failures << '\n'
             << "router_forced_failures=" << router_forced_failures << '\n'
             << "router_online_operator_uses=" << router_online_operator_uses << '\n'
             << "router_forced_operator_uses=" << router_forced_operator_uses << '\n'
             << "router_result_mismatches=" << router_result_mismatches << '\n'
             << "router_maximum_mixed_qoi_error="
             << router_maximum_mixed_qoi_error << '\n'
             << "same_workload_fallback_probe=1\n"
             << "fallback_candidate_correction_invoked=1\n"
             << "fallback_candidate_complete_shape="
             << (rejected_candidate.size() == model.blocks.front().unknowns.size()) << '\n'
             << "fallback_candidate_finite=1\n"
             << "fallback_correction_maximum_iterations=1\n"
             << "fallback_original_solver_used=" << fallback_original_solver_used << '\n'
             << "fallback_probe_success=" << fallback_probe.success << '\n'
             << "raw_candidate_gate_nonreject=" << candidate_gate_nonreject << '\n'
             << "raw_candidate_gate_nonreject_rate="
             << static_cast<double>(candidate_gate_nonreject) /
                    static_cast<double>(candidate_samples) << '\n'
             << "raw_candidate_gate_maximum_residual="
             << candidate_gate_maximum_residual << '\n'
             << "raw_candidate_maximum_full_state_error="
             << report.maximum_candidate_full_state_error << '\n'
             << "raw_candidate_maximum_qoi_error="
             << report.maximum_candidate_qoi_error << '\n'
             << "full_verified_maximum_full_state_error="
             << report.maximum_full_state_error << '\n'
             << "full_verified_maximum_qoi_error="
             << report.maximum_qoi_error << '\n'
             << "full_verified_acceptance_rate=" << report.acceptance_rate << '\n'
             << "fallbacks=" << report.fallbacks << '\n'
             << "failures=" << report.failures << '\n'
             << "END\n";
    std::ofstream external_report(
        trace_directory / "operator-external-baselines.txt");
    if (!external_report) {
        throw std::runtime_error("cannot write operator external baseline report");
    }
    external_report << std::setprecision(17)
                    << "SMAVE_OPERATOR_EXTERNAL_BASELINES 1\n"
                    << "contract=paired-complete-runtime-external-vs-verified-operator\n"
                    << "entries=" << external_baselines.size() << '\n';
    for (const auto& external : external_baselines) {
        const auto interval = paired_median_bootstrap(
            external.times, operator_times, bootstrap_seed, bootstrap_resamples);
        external_report
            << "BASELINE " << std::quoted(external.backend)
            << " attempted=" << external.attempted
            << " native_uses=" << external.native_uses
            << " fallbacks=" << external.fallbacks
            << " failures=" << external.failures
            << " external_median_us=" << quantile(external.times, 0.5)
            << " verified_operator_median_us=" << quantile(operator_times, 0.5)
            << " verified_operator_paired_median_speedup=" << interval.median
            << " verified_operator_bootstrap_95_lower=" << interval.lower
            << " verified_operator_bootstrap_95_upper=" << interval.upper
            << " maximum_mixed_qoi_error=" << external.maximum_mixed_qoi_error
            << " same_accuracy="
            << (external.failures == 0 &&
                external.maximum_mixed_qoi_error <= 1.0) << '\n';
    }
    external_report << "END\n";
    std::ofstream shared_hybrid_report(
        trace_directory / "operator-shared-hybrid-baseline.txt");
    if (!shared_hybrid_report) {
        throw std::runtime_error("cannot write shared hybrid baseline report");
    }
    const auto shared_hybrid_attempted = report.requests * report.repetitions;
    shared_hybrid_report
        << std::setprecision(17)
        << "SMAVE_OPERATOR_SHARED_HYBRID_BASELINE 1\n"
        << "contract=paired-complete-runtime-learned-candidate-weighted-jacobi-strict-gate-fallback\n"
        << "candidate=latent-operator-fp64\n"
        << "training_scenarios=" << artifact.training_samples << '\n'
        << "evaluation_scenarios=" << report.requests << '\n'
        << "repetitions=" << report.repetitions << '\n'
        << "attempted=" << shared_hybrid_attempted << '\n'
        << "candidate_uses=" << shared_hybrid_candidate_uses << '\n'
        << "correction=weighted-diagonal-residual-jacobi\n"
        << "correction_maximum_iterations=32\n"
        << "strict_original_gate=1\n"
        << "independent_runtime_commit_gate_for_accepts=1\n"
        << "mandatory_original_solver_fallback=1\n"
        << "accepted_trace_persistence=1\n"
        << "fallback_trace_persistence=1\n"
        << "accepted=" << shared_hybrid_accepts << '\n'
        << "fallbacks=" << shared_hybrid_fallbacks << '\n'
        << "failures=" << shared_hybrid_failures << '\n'
        << "acceptance_rate="
        << static_cast<double>(shared_hybrid_accepts) /
               static_cast<double>(shared_hybrid_attempted) << '\n'
        << "fallback_rate="
        << static_cast<double>(shared_hybrid_fallbacks) /
               static_cast<double>(shared_hybrid_attempted) << '\n'
        << "correction_total_iterations=" << shared_hybrid_total_iterations << '\n'
        << "gate_mismatches=" << shared_hybrid_gate_mismatches << '\n'
        << "maximum_residual=" << shared_hybrid_maximum_residual << '\n'
        << "baseline_median_us=" << quantile(baseline_times, 0.5) << '\n'
        << "shared_hybrid_median_us=" << quantile(shared_hybrid_times, 0.5) << '\n'
        << "paired_median_speedup=" << shared_hybrid_interval.median << '\n'
        << "bootstrap_seed=" << bootstrap_seed << '\n'
        << "bootstrap_resamples=" << bootstrap_resamples << '\n'
        << "bootstrap_95_lower=" << shared_hybrid_interval.lower << '\n'
        << "bootstrap_95_upper=" << shared_hybrid_interval.upper << '\n'
        << "verified_operator_vs_shared_hybrid_paired_median_speedup="
        << operator_vs_shared_hybrid_interval.median << '\n'
        << "verified_operator_vs_shared_hybrid_bootstrap_95_lower="
        << operator_vs_shared_hybrid_interval.lower << '\n'
        << "verified_operator_vs_shared_hybrid_bootstrap_95_upper="
        << operator_vs_shared_hybrid_interval.upper << '\n'
        << "maximum_mixed_qoi_error=" << shared_hybrid_maximum_mixed_qoi_error << '\n'
        << "same_accuracy="
        << (shared_hybrid_failures == 0 &&
            shared_hybrid_maximum_mixed_qoi_error <= 1.0) << '\n'
        << "all_failures_retained=1\n"
        << "END\n";
    if (hints_schedule_applicable) {
        std::ofstream hints_report(
            trace_directory / "operator-hints-schedule-baseline.txt");
        if (!hints_report) {
            throw std::runtime_error("cannot write HINTS schedule baseline report");
        }
        const auto hints_attempted = report.requests * report.repetitions;
        hints_report
            << std::setprecision(17)
            << "SMAVE_OPERATOR_HINTS_SCHEDULE_BASELINE 1\n"
            << "contract=paired-complete-runtime-hints-alternation-common-latent-operator-original-gate-fallback\n"
            << "published_method=HINTS\n"
            << "published_paper_doi=10.1038/s42256-024-00910-x\n"
            << "official_code_revision=0c8b712f81ed08bdf27c3a215f8edb99910f5e2f\n"
            << "official_schedule_source=HINTS_numpy/iterative_solver.py:243-258\n"
            << "official_poisson_config_source=HINTS_numpy/example_configs/configs_1D_Poisson_HINTS_Jacobi.py:48-69\n"
            << "algorithmic_schedule_reimplementation=1\n"
            << "official_public_code_executed=0\n"
            << "deep_onet_architecture_reproduced=0\n"
            << "official_pretrained_weights_used=0\n"
            << "shared_latent_operator_weights=1\n"
            << "scope=official-alternation-pattern-with-common-candidate-and-stopping-contract\n"
            << "training_scenarios=" << artifact.training_samples << '\n'
            << "evaluation_scenarios=" << report.requests << '\n'
            << "repetitions=" << report.repetitions << '\n'
            << "attempted=" << hints_attempted << '\n'
            << "initial_state=declared-variable-starts\n"
            << "linear_matrix_assembly_in_timing=0\n"
            << "right_hand_side_update_in_timing=1\n"
            << "residual_kernel=preassembled-linear-system-multiply\n"
            << "numerical_method=weighted-diagonal-jacobi\n"
            << "jacobi_weight=" << hints_jacobi_weight << '\n'
            << "numerical_to_learned_ratio="
            << hints_numerical_to_learned_ratio << '\n'
            << "maximum_iterations=" << hints_maximum_iterations << '\n'
            << "early_stop_on_original_gate=1\n"
            << "learned_input=current-original-equation-residual\n"
            << "learned_output=zero-anchored-additive-correction\n"
            << "router_used=0\n"
            << "certificate_ood_rejection_used=0\n"
            << "strict_original_gate=1\n"
            << "gate_evaluated_after_every_iteration=1\n"
            << "independent_runtime_commit_gate_for_accepts=1\n"
            << "mandatory_original_solver_fallback=1\n"
            << "accepted_trace_persistence=1\n"
            << "fallback_trace_persistence=1\n"
            << "accepted=" << hints_schedule_accepts << '\n'
            << "fallbacks=" << hints_schedule_fallbacks << '\n'
            << "failures=" << hints_schedule_failures << '\n'
            << "acceptance_rate="
            << static_cast<double>(hints_schedule_accepts) /
                   static_cast<double>(hints_attempted) << '\n'
            << "fallback_rate="
            << static_cast<double>(hints_schedule_fallbacks) /
                   static_cast<double>(hints_attempted) << '\n'
            << "total_iterations=" << hints_schedule_iterations << '\n'
            << "jacobi_iterations=" << hints_schedule_jacobi_iterations << '\n'
            << "learned_corrections=" << hints_schedule_learned_corrections << '\n'
            << "gate_decision_mismatches="
            << hints_schedule_gate_decision_mismatches << '\n'
            << "gate_residual_mismatches="
            << hints_schedule_gate_residual_mismatches << '\n'
            << "maximum_gate_residual_difference="
            << hints_schedule_maximum_gate_residual_difference << '\n'
            << "maximum_residual=" << hints_schedule_maximum_residual << '\n'
            << "baseline_median_us=" << quantile(baseline_times, 0.5) << '\n'
            << "hints_schedule_median_us="
            << quantile(hints_schedule_times, 0.5) << '\n'
            << "paired_median_speedup=" << hints_schedule_interval.median << '\n'
            << "bootstrap_seed=" << bootstrap_seed << '\n'
            << "bootstrap_resamples=" << bootstrap_resamples << '\n'
            << "bootstrap_95_lower=" << hints_schedule_interval.lower << '\n'
            << "bootstrap_95_upper=" << hints_schedule_interval.upper << '\n'
            << "verified_operator_vs_hints_schedule_paired_median_speedup="
            << operator_vs_hints_schedule_interval.median << '\n'
            << "verified_operator_vs_hints_schedule_bootstrap_95_lower="
            << operator_vs_hints_schedule_interval.lower << '\n'
            << "verified_operator_vs_hints_schedule_bootstrap_95_upper="
            << operator_vs_hints_schedule_interval.upper << '\n'
            << "maximum_mixed_qoi_error="
            << hints_schedule_maximum_mixed_qoi_error << '\n'
            << "same_accuracy="
            << (hints_schedule_failures == 0 &&
                hints_schedule_maximum_mixed_qoi_error <= 1.0) << '\n'
            << "published_full_implementation_claim=0\n"
            << "all_failures_retained=1\n"
            << "END\n";
    }
    std::ofstream batch_trace(trace_directory / "operator-batch.trace");
    if (!batch_trace) throw std::runtime_error("cannot write operator batch trace");
    batch_trace << std::setprecision(17)
                << "SMAVE_OPERATOR_BATCH 1\n"
                << "expert_version=" << artifact.expert_version << '\n'
                << "block_fingerprint=" << artifact.block_fingerprint << '\n'
                << "shape=" << artifact.outputs.size() << '\n'
                << "dtype=fp64\n"
                << "requested_device=" << device << '\n'
                << "selected_device=" << selected_device << '\n'
                << "device_backend=" << device_backend << '\n'
                << "device_name=" << device_name << '\n'
                << "device_batches=" << device_batches << '\n'
                << "device_rejections=" << device_rejections << '\n'
                << "device_upload_us=" << device_upload_us << '\n'
                << "device_kernel_us=" << device_kernel_us << '\n'
                << "device_download_us=" << device_download_us << '\n'
                << "device_maximum_absolute_error="
                << device_maximum_absolute_error << '\n'
                << "device_maximum_relative_error="
                << device_maximum_relative_error << '\n'
                << "device_reason=" << std::quoted(device_reason) << '\n'
                << "tolerance_relative=" << tolerance.relative << '\n'
                << "qoi_relative=" << tolerance.qoi_relative << '\n'
                << "batches=" << report.batches << '\n'
                << "average_batch=" << report.average_batch << '\n'
                << "accepted=" << report.accepted << '\n'
                << "fallbacks=" << report.fallbacks << '\n'
                << "report_hash=" << report.report_hash << '\n'
                << "END\n";
    return report;
}

void write_operator_benchmark_report(
    const OperatorBenchmarkReport& report,
    const std::filesystem::path& path) {
    report.validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write operator benchmark report");
    output << std::setprecision(17)
           << "SMAVE_OPERATOR_BENCHMARK " << report.schema_version << '\n';
    if (report.schema_version >= 2) {
        output << "dataset_id=" << report.dataset_id << '\n'
               << "dataset_version=" << report.dataset_version << '\n'
               << "dataset_manifest_hash=" << report.dataset_manifest_hash << '\n';
    }
    output << "requests=" << report.requests << '\n'
           << "repetitions=" << report.repetitions << '\n'
           << "batches=" << report.batches << '\n'
           << "average_batch=" << report.average_batch << '\n'
           << "accepted=" << report.accepted << '\n'
           << "fallbacks=" << report.fallbacks << '\n'
           << "failures=" << report.failures << '\n'
           << "acceptance_rate=" << report.acceptance_rate << '\n'
           << "baseline_median_us=" << report.baseline_median_us << '\n'
           << "operator_median_us=" << report.operator_median_us << '\n'
           << "online_speedup=" << report.online_speedup << '\n';
    if (report.schema_version >= 3) {
        output << "marginal_median_semantics=diagnostic-not-speedup-denominator\n"
               << "online_speedup_semantics=paired-median-of-per-repetition-ratios\n"
               << "paired_speedup_ci95_lower="
               << report.paired_speedup_ci95_lower << '\n'
               << "paired_speedup_ci95_upper="
               << report.paired_speedup_ci95_upper << '\n'
               << "paired_median_saving_us=" << report.paired_median_saving_us << '\n'
               << "break_even_semantics=training-wall-over-paired-median-saving\n"
               << "amortized_speedup_semantics=paired-representative-projected-ratio\n";
    }
    output << "training_wall_us=" << report.training_wall_us << '\n'
           << "break_even_queries=" << report.break_even_queries << '\n'
           << "projected_queries=" << report.projected_queries << '\n'
           << "amortized_speedup=" << report.amortized_speedup << '\n'
           << "maximum_full_state_error=" << report.maximum_full_state_error << '\n'
           << "maximum_qoi_error=" << report.maximum_qoi_error << '\n'
           << "maximum_candidate_full_state_error="
           << report.maximum_candidate_full_state_error << '\n'
           << "maximum_candidate_qoi_error=" << report.maximum_candidate_qoi_error << '\n'
           << "candidate_qoi_within_tolerance="
           << report.candidate_qoi_within_tolerance << '\n'
           << "same_accuracy=" << report.same_accuracy << '\n'
           << "break_even_met=" << report.break_even_met << '\n'
           << "artifact_hash=" << report.artifact_hash << '\n'
           << "certificate_hash=" << report.certificate_hash << '\n'
           << "report_hash=" << report.report_hash << '\n'
           << "END\n";
}

OperatorBenchmarkReport read_operator_benchmark_report(
    const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read operator benchmark report");
    std::string magic;
    int version{};
    input >> magic >> version;
    if (magic != "SMAVE_OPERATOR_BENCHMARK" ||
        (version != 1 && version != 2 && version != 3)) {
        throw std::invalid_argument("unsupported operator benchmark report schema");
    }
    std::unordered_map<std::string, std::string> fields;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line) && line != "END") {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::invalid_argument("invalid operator benchmark report field");
        }
        fields.emplace(line.substr(0, separator), line.substr(separator + 1));
    }
    const auto value = [&](const std::string& name) -> const std::string& {
        const auto iterator = fields.find(name);
        if (iterator == fields.end()) {
            throw std::invalid_argument("operator benchmark report lacks " + name);
        }
        return iterator->second;
    };
    OperatorBenchmarkReport report;
    report.schema_version = version;
    if (version >= 2) {
        report.dataset_id = value("dataset_id");
        report.dataset_version = value("dataset_version");
        report.dataset_manifest_hash = value("dataset_manifest_hash");
    }
    report.requests = std::stoull(value("requests"));
    report.repetitions = std::stoull(value("repetitions"));
    report.batches = std::stoull(value("batches"));
    report.average_batch = std::stod(value("average_batch"));
    report.accepted = std::stoull(value("accepted"));
    report.fallbacks = std::stoull(value("fallbacks"));
    report.failures = std::stoull(value("failures"));
    report.acceptance_rate = std::stod(value("acceptance_rate"));
    report.baseline_median_us = std::stod(value("baseline_median_us"));
    report.operator_median_us = std::stod(value("operator_median_us"));
    report.online_speedup = std::stod(value("online_speedup"));
    if (version >= 3) {
        report.paired_speedup_ci95_lower =
            std::stod(value("paired_speedup_ci95_lower"));
        report.paired_speedup_ci95_upper =
            std::stod(value("paired_speedup_ci95_upper"));
        report.paired_median_saving_us =
            std::stod(value("paired_median_saving_us"));
    }
    report.training_wall_us = std::stod(value("training_wall_us"));
    report.break_even_queries = std::stoull(value("break_even_queries"));
    report.projected_queries = std::stoull(value("projected_queries"));
    report.amortized_speedup = std::stod(value("amortized_speedup"));
    report.maximum_full_state_error = std::stod(value("maximum_full_state_error"));
    report.maximum_qoi_error = std::stod(value("maximum_qoi_error"));
    report.maximum_candidate_full_state_error =
        std::stod(value("maximum_candidate_full_state_error"));
    report.maximum_candidate_qoi_error =
        std::stod(value("maximum_candidate_qoi_error"));
    report.candidate_qoi_within_tolerance =
        std::stoi(value("candidate_qoi_within_tolerance")) != 0;
    report.same_accuracy = std::stoi(value("same_accuracy")) != 0;
    report.break_even_met = std::stoi(value("break_even_met")) != 0;
    report.artifact_hash = value("artifact_hash");
    report.certificate_hash = value("certificate_hash");
    report.report_hash = value("report_hash");
    report.validate();
    return report;
}

}  // namespace smave
