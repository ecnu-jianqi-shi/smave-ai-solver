#include "smave/benchmark/sparse_suite.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"
#include "smave/solve_service.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t top_k = 3;
constexpr int maximum_work_iterations = 250;
constexpr int restart_dimension = 40;
constexpr std::size_t repetitions = 5;
constexpr std::size_t matrix_row_limit = 10000;
constexpr std::size_t built_in_direct_row_limit = 512;
constexpr double minimum_family_anchor_gain_fraction = 0.05;
constexpr std::array<int, 4> iterative_budgets{20, 50, 100, 250};
constexpr std::array<double, 2> requested_tolerances{1.0e-6, 1.0e-10};

const std::vector<std::string> full_feature_names{
    "sparse:log_rows",
    "sparse:log_nonzeros",
    "sparse:log_average_row_nonzeros",
    "sparse:log_diagonal_condition",
    "sparse:log_coefficient_dynamic_range",
    "sparse:row_nonzero_coefficient_of_variation",
    "sparse:log_row_l1_condition",
    "sparse:diagonal_dominance_fraction",
    "sparse:mean_diagonal_row_l1_fraction",
    "sparse:normalized_mean_bandwidth",
    "sparse:structurally_symmetric",
    "sparse:numerically_positive_definite",
    "sparse:right_hand_side_roughness",
    "sparse:right_hand_side_sign_change_fraction",
    "sparse:requested_digits",
};

std::vector<std::size_t> feature_indices(
    std::initializer_list<std::string_view> names) {
    std::vector<std::size_t> result;
    result.reserve(names.size());
    for (const auto name : names) {
        const auto found = std::find(full_feature_names.begin(), full_feature_names.end(), name);
        if (found == full_feature_names.end()) {
            throw std::invalid_argument("unknown SuiteSparse routing feature selection");
        }
        result.push_back(static_cast<std::size_t>(found - full_feature_names.begin()));
    }
    return result;
}

struct MatrixSpec {
    std::string split;
    std::string collection_group;
    std::string name;
    std::filesystem::path relative_path;
    std::string numeric_class;
    bool collection_positive_definite{};
};

std::vector<MatrixSpec> matrix_specs{
    {"training", "Boeing", "msc00726", "small/msc00726/msc00726.mtx",
     "spd", true},
    {"training", "Boeing", "crystm01", "small/crystm01/crystm01.mtx",
     "spd", true},
    {"training", "Bai", "bfwb398", "large/bfwb398/bfwb398.mtx",
     "symmetric-non-spd", false},
    {"training", "HB", "saylr4", "small/saylr4/saylr4.mtx",
     "symmetric-non-spd", false},
    {"training", "HB", "impcol_e", "small/impcol_e/impcol_e.mtx",
     "nonsymmetric", false},
    {"training", "HB", "west0479", "small/west0479/west0479.mtx",
     "nonsymmetric", false},
    {"calibration", "Nasa", "nasa2910", "small/nasa2910/nasa2910.mtx",
     "spd", true},
    {"calibration", "Gset", "G10", "large/G10/G10.mtx",
     "symmetric-non-spd", false},
    {"calibration", "Bai", "ck400", "large/ck400/ck400.mtx",
     "nonsymmetric", false},
    {"calibration", "Bai", "dw256A", "large/dw256A/dw256A.mtx",
     "nonsymmetric", false},
    {"heldout", "ND", "nd3k",
     "final-heldout-v6/nd3k/nd3k.mtx", "spd", true},
    {"heldout", "Grund", "meg4",
     "final-heldout-v6/meg4/meg4.mtx", "symmetric-non-spd", false},
    {"heldout", "Hollinger", "g7jac020",
     "final-heldout-v6/g7jac020/g7jac020.mtx", "nonsymmetric", false},
};

bool development_mode{};
std::filesystem::path development_split_manifest;

std::vector<MatrixSpec> read_matrix_specs(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read matrix split manifest");
    std::vector<MatrixSpec> result;
    std::set<std::string> names;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#' ||
            line == "SMAVE_SUITESPARSE_DEVELOPMENT_SPLIT 1" || line == "END") {
            continue;
        }
        std::istringstream row(line);
        MatrixSpec spec;
        int collection_positive_definite{};
        if (!(row >> spec.split >> spec.collection_group >> spec.name >>
              spec.relative_path >> spec.numeric_class >>
              collection_positive_definite)) {
            throw std::runtime_error("invalid matrix split manifest row: " + line);
        }
        std::string trailing;
        if (row >> trailing ||
            (spec.split != "training" && spec.split != "calibration" &&
             spec.split != "heldout") ||
            (spec.numeric_class != "spd" &&
             spec.numeric_class != "symmetric-non-spd" &&
             spec.numeric_class != "nonsymmetric") ||
            (collection_positive_definite != 0 && collection_positive_definite != 1) ||
            !names.insert(spec.name).second) {
            throw std::runtime_error("invalid matrix split manifest contract: " + line);
        }
        spec.collection_positive_definite = collection_positive_definite != 0;
        result.push_back(std::move(spec));
    }
    if (result.empty()) throw std::runtime_error("matrix split manifest is empty");
    return result;
}

const std::set<std::string> pre_v6_locked_groups{
    "Bai", "Bates", "Boeing", "DNVS", "FIDAP", "Freescale",
    "GHS_indef", "Gset", "HB", "Hamm", "JGD_Trefethen", "Janna",
    "MathWorks", "Morandini", "Muite", "Nasa", "PARSEC", "POLYFLOW",
    "Rajat", "Rommes", "Schenk_IBMNA", "TKK", "VDOL", "Wang",
    "JGD_BIBD", "Cannizzo", "JGD_SPG", "TSOPF", "JGD_Homology",
    "Norris", "Cylshell", "Andrianov", "CPM", "Okunbor",
    "Oberwolfach", "Zitney"};

struct ActionSpec {
    std::string expert;
    int work_iterations{};

    [[nodiscard]] std::string key() const {
        return expert + "@" + std::to_string(work_iterations);
    }

    auto operator<=>(const ActionSpec&) const = default;
};

smave::RouteActionReference action_reference(const ActionSpec& action);

struct RawActionSample {
    std::string split;
    std::string matrix;
    std::string request_id;
    std::string request_kind;
    double relative_tolerance{};
    ActionSpec action;
    std::size_t repetition{};
    std::size_t schedule_position{};
    std::vector<double> features;
    double attempt_wall_us{};
    double terminal_reference_wall_us{};
    bool passed{};
    std::string status;
    int executed_iterations{};
    double residual_inf{};
    bool service_success{};
    bool used_terminal_fallback{};
};

struct ActionObservation {
    ActionSpec action;
    double attempt_wall_us{};
    bool passed{};
    std::size_t pass_count{};
    int executed_iterations{};
    double residual_inf{};
    bool stable_status{};
};

struct TerminalObservation {
    double wall_us{};
    bool success{};
    std::size_t attempt_count{};
};

struct RequestObservation {
    std::string split;
    std::string matrix;
    std::string request_id;
    std::string request_kind;
    double relative_tolerance{};
    smave::LinearSystem system;
    smave::SparseLinearProfile profile;
    std::vector<double> features;
    std::vector<ActionObservation> actions;
    TerminalObservation terminal;
};

struct StaticActionProfile {
    double median_cost{};
    double pass_probability{};
    std::size_t attempts{};
    std::size_t passes{};
};

struct PredictionMetrics {
    double cost_median_relative_error{};
    double cost_p95_relative_error{};
    double cost_maximum_relative_error{};
    double selected_cost_median_relative_error{};
    double selected_cost_p95_relative_error{};
    double selected_cost_maximum_relative_error{};
    double anchor_cost_p95_relative_error{};
    double anchor_cost_maximum_relative_error{};
    std::size_t selected_cost_predictions{};
    std::size_t anchor_cost_predictions{};
    double pass_brier_score{};
    double pass_ece{};
    double pass_maximum_action_calibration_error{};
};

struct PolicyMetrics {
    double regret{};
    double median_request_regret{};
    std::size_t distinct_plans{};
    double modal_plan_change_fraction{};
    double rhs_changed_plan_group_fraction{};
};

struct EvaluationResult {
    PredictionMetrics prediction;
    PolicyMetrics raw_conditioned;
    PolicyMetrics control_aware_anchor;
    PolicyMetrics conditioned_without_interactions;
    PolicyMetrics conditioned;
    PolicyMetrics size_only;
    PolicyMetrics rhs_only;
    PolicyMetrics tolerance_only;
    double static_regret{};
    double fixed_regret{};
    double training_family_fixed_regret{};
    double family_fixed_regret{};
    double oracle_total_us{};
    std::string fixed_action;
    std::string training_family_fixed_actions;
    std::string family_fixed_actions;
    std::size_t dp_exhaustive_mismatches{};
    std::size_t production_successes{};
    std::size_t production_failures{};
    std::size_t production_fallbacks{};
    std::size_t production_gate_mismatches{};
    std::size_t production_plan_order_mismatches{};
    std::size_t terminal_only_successes{};
    std::size_t interaction_plan_changed_requests{};
    std::size_t independent_plans_with_calibrated_transition{};
    std::size_t interaction_plans_with_calibrated_transition{};
};

struct GateMetrics {
    double residual_inf{};
    double backward_error{};
    bool accepted{};
};

struct InteractionMetrics {
    double maximum_interaction_delta{};
    double maximum_order_delta{};
    std::size_t eligible_families{};
};

struct ConditionalCostEvidence {
    std::vector<smave::RouteConditionalCostCalibration> calibrations;
    std::size_t candidate_transitions{};
    std::size_t selected_transitions{};
    std::size_t maximum_training_transition_groups{};
    std::size_t training_groups{};
    std::size_t calibration_groups{};
};

struct FamilyAdaptationEvidence {
    std::set<std::string> anchor_only_families;
    std::size_t adaptive_families{};
    std::size_t independent_calibration_groups{};
};

struct FamilyAnchorEvidence {
    std::map<std::string, smave::RouteActionReference> guarded_anchors;
    std::set<std::string> specialized_families;
    std::set<std::string> global_fixed_families;
    std::size_t independent_calibration_groups{};
};

std::string numeric_family(const RequestObservation& request);

double median(std::vector<double> values) {
    if (values.empty()) throw std::invalid_argument("median requires samples");
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0
        ? 0.5 * (values[middle - 1] + values[middle])
        : values[middle];
}

double percentile(std::vector<double> values, double probability) {
    if (values.empty() || probability < 0.0 || probability > 1.0) {
        throw std::invalid_argument("invalid percentile contract");
    }
    std::sort(values.begin(), values.end());
    const double position = probability * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) return values[lower];
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const double value : values) result = std::max(result, std::abs(value));
    return result;
}

double right_hand_side_roughness(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    double total{};
    for (std::size_t index = 1; index < values.size(); ++index) {
        total += std::abs(values[index] - values[index - 1]);
    }
    return total / static_cast<double>(values.size() - 1) /
        std::max(1.0, infinity_norm(values));
}

double right_hand_side_sign_change_fraction(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    std::size_t comparisons{};
    std::size_t changes{};
    double previous{};
    bool have_previous{};
    for (const double value : values) {
        if (value == 0.0) continue;
        if (have_previous) {
            ++comparisons;
            changes += std::signbit(value) != std::signbit(previous) ? 1U : 0U;
        }
        previous = value;
        have_previous = true;
    }
    return comparisons == 0 ? 0.0
        : static_cast<double>(changes) / static_cast<double>(comparisons);
}

std::uint64_t splitmix64(std::uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

std::vector<double> reference_solution(
    std::size_t size, const std::string& kind, std::uint64_t seed) {
    std::vector<double> result(size, 0.0);
    constexpr double pi = 3.141592653589793238462643383279502884;
    if (kind == "smooth") {
        for (std::size_t index = 0; index < size; ++index) {
            const double phase = 2.0 * pi * static_cast<double>(index + 1) /
                static_cast<double>(size + 1);
            result[index] = 1.0 + 0.25 * std::sin(phase);
        }
    } else if (kind == "oscillatory") {
        for (std::size_t index = 0; index < size; ++index) {
            const double phase = 18.0 * pi * static_cast<double>(index + 1) /
                static_cast<double>(size + 1);
            const double sign = index % 2 == 0 ? 1.0 : -1.0;
            result[index] = sign * (0.75 + 0.25 * std::sin(phase));
        }
    } else if (kind == "sparse") {
        const std::size_t stride = std::max<std::size_t>(3, size / 23);
        for (std::size_t index = 0; index < size; index += stride) {
            result[index] = 0.5 + static_cast<double>((index / stride) % 7) / 7.0;
        }
    } else if (kind == "random-like") {
        for (std::size_t index = 0; index < size; ++index) {
            const auto bits = splitmix64(seed ^ static_cast<std::uint64_t>(index));
            const double unit = static_cast<double>(bits >> 11U) /
                static_cast<double>(UINT64_C(1) << 53U);
            result[index] = 2.0 * unit - 1.0;
        }
    } else {
        throw std::invalid_argument("unknown request kind");
    }
    return result;
}

smave::LinearSystem make_system(
    const smave::benchmark::SparseMatrix& matrix,
    const std::vector<double>& right_hand_side,
    const MatrixSpec& spec) {
    smave::LinearSystem system;
    system.sparsity.row_count = matrix.rows;
    system.sparsity.column_count = matrix.columns;
    system.sparsity.row_offsets = matrix.row_offsets;
    system.sparsity.column_indices = matrix.column_indices;
    system.sparse_values = matrix.values;
    system.right_hand_side = right_hand_side;
    smave::classify_linear_system(system);
    if (spec.collection_positive_definite && !system.symmetric) {
        throw std::runtime_error(
            "SuiteSparse positive-definite metadata conflicts with matrix symmetry");
    }
    if (spec.numeric_class == "symmetric-non-spd" && !system.symmetric) {
        throw std::runtime_error(
            "frozen symmetric non-SPD class conflicts with numeric symmetry: " +
            spec.name);
    }
    if (spec.numeric_class == "nonsymmetric" && system.symmetric) {
        throw std::runtime_error(
            "frozen nonsymmetric class conflicts with numeric symmetry: " + spec.name);
    }
    system.positive_definite = spec.collection_positive_definite;
    return system;
}

smave::SparseLinearProfile sparse_profile(
    const RequestObservation& request,
    const std::vector<std::string>& feature_names = full_feature_names) {
    auto profile = request.profile;
    (void)smave::extract_sparse_routing_features(feature_names, profile);
    return profile;
}

smave::SparseLinearProfile make_profile(
    const MatrixSpec& spec,
    const smave::LinearSystem& system,
    const std::string& request_id,
    double absolute_tolerance,
    double relative_tolerance) {
    return smave::SparseLinearProfile{
        .fingerprint = spec.name + ":" + request_id,
        .rows = system.size(),
        .columns = system.size(),
        .nonzeros = system.nonzeros(),
        .structurally_symmetric = system.symmetric,
        .numerically_symmetric = system.symmetric,
        .numerically_positive_definite = system.positive_definite,
        .diagonal_condition_estimate = system.diagonal_condition_estimate,
        .coefficient_dynamic_range = system.coefficient_dynamic_range,
        .row_nonzero_coefficient_of_variation =
            system.row_nonzero_coefficient_of_variation,
        .row_l1_condition_estimate = system.row_l1_condition_estimate,
        .diagonal_dominance_fraction = system.diagonal_dominance_fraction,
        .mean_diagonal_row_l1_fraction = system.mean_diagonal_row_l1_fraction,
        .normalized_mean_bandwidth = system.normalized_mean_bandwidth,
        .right_hand_side_inf = infinity_norm(system.right_hand_side),
        .right_hand_side_roughness =
            right_hand_side_roughness(system.right_hand_side),
        .right_hand_side_sign_change_fraction =
            right_hand_side_sign_change_fraction(system.right_hand_side),
        .absolute_tolerance = absolute_tolerance,
        .relative_tolerance = relative_tolerance,
        .maximum_work_iterations = maximum_work_iterations,
        .restart_dimension = restart_dimension,
    };
}

std::vector<ActionSpec> compatible_actions(const RequestObservation& request) {
    std::vector<ActionSpec> actions;
    const auto append_budgets = [&](const std::string& expert) {
        for (const int budget : iterative_budgets) {
            actions.push_back(ActionSpec{expert, budget});
        }
    };
    if (request.system.positive_definite) {
        append_budgets("pcg-ic0-cpu-v1");
        append_budgets("pcg-jacobi-cpu-v1");
    } else {
        if (request.system.size() <= 1024) append_budgets("gmres-ilut-cpu-v1");
        append_budgets("gmres-ilu0-cpu-v1");
    }
    if (smave::industrial_sparse_direct_available()) {
        actions.push_back({smave::industrial_sparse_direct_backend(), 0});
    }
    if (smave::superlu_sparse_direct_available()) {
        actions.push_back({smave::superlu_sparse_direct_backend(), 0});
    }
    if (request.system.size() <= built_in_direct_row_limit) {
        actions.push_back({"sparse-ordered-threshold-pivot-cpu-v2", 0});
    }
    return actions;
}

smave::RequestConditionedRoutingModel fixed_prediction_model(
    const std::vector<ActionSpec>& actions,
    const std::map<ActionSpec, double>& costs = {}) {
    smave::RequestConditionedRoutingModel model;
    model.feature_names = full_feature_names;
    model.feature_means.assign(full_feature_names.size(), 0.0);
    model.feature_scales.assign(full_feature_names.size(), 1.0);
    for (const auto& action : actions) {
        std::vector<double> log_cost(full_feature_names.size() + 1, 0.0);
        const auto found = costs.find(action);
        log_cost.front() = std::log(found == costs.end() ? 1.0 : found->second);
        std::vector<double> pass_logit(full_feature_names.size() + 1, 0.0);
        pass_logit.front() = 0.0;
        model.actions[action.expert].push_back(smave::RouteActionPredictor{
            .work_iterations = action.work_iterations,
            .training_samples = 1,
            .independent_training_groups = 1,
            .independent_calibration_groups = 1,
            .log_cost_coefficients = std::move(log_cost),
            .pass_logit_coefficients = std::move(pass_logit),
        });
    }
    return model;
}

smave::RoutingConfig single_action_routing(const ActionSpec& action) {
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.risk_weight = 0.0;
    routing.expert_allowlist = {action.expert};
    routing.request_conditioned_model = fixed_prediction_model({action});
    routing.calibrated_terminal_fallback_cost_us = 1.0e12;
    return routing;
}

smave::RoutingConfig ordered_pair_routing(
    const ActionSpec& first, const ActionSpec& second) {
    smave::RoutingConfig routing;
    routing.top_k = 2;
    routing.risk_weight = 0.0;
    routing.expert_allowlist = {first.expert, second.expert};
    routing.request_conditioned_model = fixed_prediction_model(
        {first, second}, {{first, 1.0}, {second, 2.0}});
    routing.calibrated_terminal_fallback_cost_us = 1.0e12;
    return routing;
}

smave::RoutingConfig terminal_only_routing(bool positive_definite) {
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.risk_weight = 0.0;
    routing.expert_allowlist = {
        positive_definite ? "gmres-ilu0-cpu-v1" : "pcg-jacobi-cpu-v1"};
    return routing;
}

smave::VerifiedLinearSolveOptions solve_options(
    double relative_tolerance, const smave::RoutingConfig& routing) {
    return smave::VerifiedLinearSolveOptions{
        .absolute_tolerance = 1.0e-12,
        .relative_tolerance = relative_tolerance,
        .maximum_work_iterations = maximum_work_iterations,
        .restart_dimension = restart_dimension,
        .built_in_sparse_direct_row_limit = built_in_direct_row_limit,
        .routing = routing,
    };
}

const smave::VerifiedLinearSolveAttempt& find_attempt(
    const smave::VerifiedLinearSolveResult& result,
    const ActionSpec& action) {
    const auto found = std::find_if(
        result.attempts.begin(), result.attempts.end(), [&](const auto& attempt) {
            return attempt.backend == action.expert &&
                attempt.work_iterations == action.work_iterations;
        });
    if (found == result.attempts.end()) {
        throw std::runtime_error("production service did not execute action " + action.key());
    }
    return *found;
}

GateMetrics recompute_gate(
    const smave::LinearSystem& system,
    const std::vector<double>& solution,
    double absolute_tolerance,
    double relative_tolerance) {
    GateMetrics metrics;
    if (solution.size() != system.size()) return metrics;
    double matrix_norm_inf{};
    double solution_norm_inf = infinity_norm(solution);
    const double right_norm_inf = infinity_norm(system.right_hand_side);
    for (std::size_t row = 0; row < system.size(); ++row) {
        double product{};
        double row_sum{};
        for (std::size_t offset = system.sparsity.row_offsets[row];
             offset < system.sparsity.row_offsets[row + 1]; ++offset) {
            const double coefficient = system.sparse_values[offset];
            product += coefficient * solution[system.sparsity.column_indices[offset]];
            row_sum += std::abs(coefficient);
        }
        metrics.residual_inf = std::max(
            metrics.residual_inf,
            std::abs(product - system.right_hand_side[row]));
        matrix_norm_inf = std::max(matrix_norm_inf, row_sum);
    }
    const double scale = matrix_norm_inf * solution_norm_inf + right_norm_inf;
    metrics.backward_error = metrics.residual_inf /
        std::max(scale, std::numeric_limits<double>::min());
    metrics.accepted = std::isfinite(metrics.backward_error) &&
        metrics.residual_inf <= absolute_tolerance + relative_tolerance * scale;
    return metrics;
}

std::vector<RequestObservation> build_requests(
    const std::filesystem::path& suite_root) {
    std::vector<RequestObservation> requests;
    const std::array<std::string, 4> kinds{
        "smooth", "oscillatory", "sparse", "random-like"};
    std::uint64_t seed = UINT64_C(0x5a17e5d3c92b4f81);
    for (const auto& spec : matrix_specs) {
        const auto matrix = smave::benchmark::read_matrix_market(
            suite_root / spec.relative_path);
        if (matrix.rows != matrix.columns || matrix.rows > matrix_row_limit) {
            throw std::runtime_error("matrix is outside the frozen experiment contract: " +
                                     spec.name);
        }
        for (const auto& kind : kinds) {
            for (const double relative_tolerance : requested_tolerances) {
                const auto expected = reference_solution(matrix.columns, kind, seed++);
                const auto right_hand_side = matrix.multiply(expected);
                auto system = make_system(matrix, right_hand_side, spec);
                const std::string request_id = kind + "-tol" +
                    std::to_string(static_cast<int>(std::round(-std::log10(
                        relative_tolerance))));
                const auto profile = make_profile(
                    spec, system, request_id, 1.0e-12, relative_tolerance);
                requests.push_back(RequestObservation{
                    .split = spec.split,
                    .matrix = spec.name,
                    .request_id = request_id,
                    .request_kind = kind,
                    .relative_tolerance = relative_tolerance,
                    .system = std::move(system),
                    .profile = profile,
                    .features = smave::extract_sparse_routing_features(
                        full_feature_names, profile),
                });
            }
        }
    }
    return requests;
}

void observe_requests(
    std::vector<RequestObservation>& requests,
    std::vector<RawActionSample>& raw_samples,
    const std::filesystem::path& terminal_trace_path) {
    std::ofstream terminal_trace(terminal_trace_path);
    if (!terminal_trace) throw std::runtime_error("cannot write terminal traces");
    terminal_trace << "split\tmatrix\trequest\trepetition\tschedule_position"
                   << "\tattempt_index\tbackend\tbudget"
                   << "\texecuted_iterations\tstatus\twall_us\tresidual_inf\n";
    for (auto& request : requests) {
        const auto actions = compatible_actions(request);
        std::map<ActionSpec, std::vector<RawActionSample>> per_action;
        std::vector<double> terminal_walls;
        std::vector<std::size_t> terminal_attempts;
        std::size_t terminal_successes{};
        const std::size_t schedule_size = actions.size() + 1;
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            const std::size_t rotation = repetition * schedule_size / repetitions;
            std::vector<RawActionSample> repetition_action_samples;
            repetition_action_samples.reserve(actions.size());
            double repetition_terminal_wall{};
            bool terminal_observed{};
            for (std::size_t position = 0; position < schedule_size; ++position) {
                const std::size_t scheduled = repetition % 2 == 0
                    ? (position + rotation) % schedule_size
                    : (rotation + schedule_size - position) % schedule_size;
                if (scheduled == actions.size()) {
                    const auto result = smave::verified_linear_solve(
                        request.system,
                        solve_options(
                            request.relative_tolerance,
                            terminal_only_routing(request.system.positive_definite)));
                    double wall{};
                    for (std::size_t index = 0; index < result.attempts.size(); ++index) {
                        const auto& attempt = result.attempts[index];
                        wall += attempt.wall_us;
                        terminal_trace << request.split << '\t' << request.matrix << '\t'
                                       << request.request_id << '\t' << repetition << '\t'
                                       << position << '\t' << index << '\t'
                                       << attempt.backend << '\t'
                                       << attempt.work_iterations << '\t'
                                       << attempt.executed_iterations << '\t'
                                       << attempt.status << '\t' << std::setprecision(17)
                                       << attempt.wall_us << '\t'
                                       << attempt.residual_inf << '\n';
                    }
                    repetition_terminal_wall = std::max(wall, 1.0e-6);
                    terminal_observed = true;
                    terminal_walls.push_back(repetition_terminal_wall);
                    terminal_attempts.push_back(result.attempts.size());
                    terminal_successes += result.success && result.used_fallback ? 1U : 0U;
                    continue;
                }
                const auto& action = actions[scheduled];
                const auto result = smave::verified_linear_solve(
                    request.system,
                    solve_options(request.relative_tolerance,
                                  single_action_routing(action)));
                const auto& attempt = find_attempt(result, action);
                RawActionSample sample{
                    .split = request.split,
                    .matrix = request.matrix,
                    .request_id = request.request_id,
                    .request_kind = request.request_kind,
                    .relative_tolerance = request.relative_tolerance,
                    .action = action,
                    .repetition = repetition,
                    .schedule_position = position,
                    .features = request.features,
                    .attempt_wall_us = std::max(attempt.wall_us, 1.0e-6),
                    .passed = attempt.status == "accepted",
                    .status = attempt.status,
                    .executed_iterations = attempt.executed_iterations,
                    .residual_inf = attempt.residual_inf,
                    .service_success = result.success,
                    .used_terminal_fallback = result.used_fallback,
                };
                repetition_action_samples.push_back(std::move(sample));
            }
            if (!terminal_observed) {
                throw std::runtime_error(
                    "counterbalanced cost schedule omitted terminal reference");
            }
            for (auto& sample : repetition_action_samples) {
                sample.terminal_reference_wall_us = repetition_terminal_wall;
                raw_samples.push_back(sample);
                per_action[sample.action].push_back(std::move(sample));
            }
        }
        for (const auto& action : actions) {
            const auto& samples = per_action.at(action);
            std::vector<double> walls;
            std::vector<double> residuals;
            std::vector<double> iterations;
            std::size_t passes{};
            for (const auto& sample : samples) {
                walls.push_back(sample.attempt_wall_us);
                iterations.push_back(static_cast<double>(sample.executed_iterations));
                if (std::isfinite(sample.residual_inf)) residuals.push_back(sample.residual_inf);
                passes += sample.passed ? 1U : 0U;
            }
            request.actions.push_back(ActionObservation{
                .action = action,
                .attempt_wall_us = median(std::move(walls)),
                .passed = passes * 2 >= samples.size(),
                .pass_count = passes,
                .executed_iterations = static_cast<int>(std::llround(
                    median(std::move(iterations)))),
                .residual_inf = residuals.empty()
                    ? std::numeric_limits<double>::infinity()
                    : median(std::move(residuals)),
                .stable_status = passes == 0 || passes == samples.size(),
            });
        }

        std::vector<double> attempt_counts;
        for (const auto count : terminal_attempts) {
            attempt_counts.push_back(static_cast<double>(count));
        }
        request.terminal = TerminalObservation{
            .wall_us = median(std::move(terminal_walls)),
            .success = terminal_successes == repetitions,
            .attempt_count = static_cast<std::size_t>(std::llround(
                median(std::move(attempt_counts)))),
        };
    }
}

std::vector<smave::RouteActionTrainingSample> training_samples(
    const std::vector<RawActionSample>& samples,
    const std::vector<RequestObservation>& requests,
    const std::string& split,
    const std::vector<std::size_t>& feature_indices) {
    std::vector<smave::RouteActionTrainingSample> result;
    std::map<std::pair<std::string, std::string>, std::string> routing_families;
    for (const auto& request : requests) {
        routing_families[{request.matrix, request.request_id}] = numeric_family(request);
    }
    for (const auto& sample : samples) {
        if (sample.split != split) continue;
        std::vector<double> features;
        features.reserve(feature_indices.size());
        for (const auto index : feature_indices) features.push_back(sample.features[index]);
        result.push_back(smave::RouteActionTrainingSample{
            .expert_version = sample.action.expert,
            .work_iterations = sample.action.work_iterations,
            .independent_group = sample.matrix,
            .routing_family = routing_families.at(
                {sample.matrix, sample.request_id}),
            .features = std::move(features),
            .attempt_wall_us = sample.attempt_wall_us,
            .terminal_reference_wall_us = sample.terminal_reference_wall_us,
            .cost_relative_to_terminal = true,
            .passed = sample.passed,
        });
    }
    for (const auto& request : requests) {
        if (request.split != split) continue;
        std::vector<double> features;
        features.reserve(feature_indices.size());
        for (const auto index : feature_indices) {
            features.push_back(request.features[index]);
        }
        result.push_back(smave::RouteActionTrainingSample{
            .expert_version = smave::assess_equation(request.profile).mandatory_fallback,
            .work_iterations = 0,
            .independent_group = request.matrix,
            .routing_family = numeric_family(request),
            .features = std::move(features),
            .attempt_wall_us = request.terminal.wall_us,
            .passed = request.terminal.success,
        });
    }
    return result;
}

smave::RequestConditionedRoutingModel train_model(
    const std::vector<RawActionSample>& samples,
    const std::vector<RequestObservation>& requests,
    const std::vector<std::size_t>& feature_indices) {
    std::vector<std::string> feature_names;
    feature_names.reserve(feature_indices.size());
    for (const auto index : feature_indices) feature_names.push_back(full_feature_names[index]);
    return smave::train_request_conditioned_routing_model(
        feature_names,
        training_samples(samples, requests, "training", feature_indices),
        training_samples(samples, requests, "calibration", feature_indices),
        100.0,
        5.0e-2,
        128,
        std::log(4.0),
        1.0);
}

std::vector<const RequestObservation*> split_requests(
    const std::vector<RequestObservation>& requests,
    const std::string& split) {
    std::vector<const RequestObservation*> result;
    for (const auto& request : requests) {
        if (request.split == split) result.push_back(&request);
    }
    return result;
}

const ActionObservation* find_action(
    const RequestObservation& request, const ActionSpec& action) {
    const auto found = std::find_if(
        request.actions.begin(), request.actions.end(), [&](const auto& observation) {
            return observation.action == action;
        });
    return found == request.actions.end() ? nullptr : &*found;
}

std::map<std::string, std::map<ActionSpec, StaticActionProfile>> static_profiles(
    const std::vector<RequestObservation>& requests) {
    std::map<std::string, std::map<ActionSpec, std::vector<const ActionObservation*>>>
        grouped;
    for (const auto& request : requests) {
        if (request.split != "training") continue;
        const std::string family = request.system.positive_definite ? "pcg" : "gmres";
        for (const auto& action : request.actions) grouped[family][action.action].push_back(&action);
    }
    std::map<std::string, std::map<ActionSpec, StaticActionProfile>> result;
    for (const auto& [family, actions] : grouped) {
        for (const auto& [action, observations] : actions) {
            std::vector<double> costs;
            std::size_t passes{};
            for (const auto* observation : observations) {
                costs.push_back(observation->attempt_wall_us);
                passes += observation->passed ? 1U : 0U;
            }
            result[family][action] = StaticActionProfile{
                .median_cost = median(std::move(costs)),
                .pass_probability = static_cast<double>(passes) /
                    static_cast<double>(observations.size()),
                .attempts = observations.size(),
                .passes = passes,
            };
        }
    }
    return result;
}

double median_terminal_cost(const std::vector<RequestObservation>& requests) {
    std::vector<double> values;
    for (const auto& request : requests) {
        if (request.split == "training") values.push_back(request.terminal.wall_us);
    }
    return median(std::move(values));
}

smave::RoutingConfig conditioned_config(
    const smave::RequestConditionedRoutingModel& model,
    double terminal_cost,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    const std::vector<smave::RouteConditionalCostCalibration>&
        conditional_cost_calibrations = {},
    const std::set<std::string>& anchor_only_families = {},
    const ActionSpec* global_fixed = nullptr) {
    smave::RoutingConfig routing;
    routing.top_k = top_k;
    routing.risk_weight = 0.0;
    routing.request_conditioned_model = model;
    routing.request_conditioned_family_anchors = family_anchors;
    if (global_fixed != nullptr) {
        routing.request_conditioned_global_fixed_anchor =
            action_reference(*global_fixed);
    }
    routing.request_conditioned_anchor_only_families = anchor_only_families;
    routing.conditional_cost_calibrations = conditional_cost_calibrations;
    routing.minimum_family_anchor_gain_fraction =
        minimum_family_anchor_gain_fraction;
    routing.calibrated_terminal_fallback_cost_us = terminal_cost;
    for (const auto& [expert, predictors] : model.actions) {
        (void)predictors;
        if (expert == "terminal-numerical-linear-cascade-v1") continue;
        routing.expert_allowlist.insert(expert);
    }
    return routing;
}

double predicted_terminal_cost(
    const smave::RequestConditionedRoutingModel& model,
    const RequestObservation& request,
    double default_cost) {
    const auto terminal = smave::assess_equation(request.profile).mandatory_fallback;
    if (!model.actions.contains(terminal)) return default_cost;
    const auto features = smave::extract_sparse_routing_features(
        model.feature_names, sparse_profile(request, model.feature_names));
    return smave::predict_request_conditioned_action(
        model, terminal, 0, features, 0.0,
        numeric_family(request)).attempt_wall_us;
}

std::vector<smave::SolveStep> predicted_alternatives(
    const smave::RequestConditionedRoutingModel& model,
    const RequestObservation& request) {
    const auto features = smave::extract_sparse_routing_features(
        model.feature_names, sparse_profile(request, model.feature_names));
    const auto terminal_expert = smave::assess_equation(request.profile).mandatory_fallback;
    const double terminal_reference = smave::predict_request_conditioned_action(
        model, terminal_expert, 0, features, 0.0,
        numeric_family(request)).attempt_wall_us;
    std::vector<smave::SolveStep> result;
    for (const auto& action : request.actions) {
        const auto prediction = smave::predict_request_conditioned_action(
            model, action.action.expert, action.action.work_iterations, features,
            terminal_reference, numeric_family(request));
        if (prediction.pass_probability <= 0.0) continue;
        result.push_back(smave::SolveStep{
            .expert_version = action.action.expert,
            .permission = smave::Permission::direct,
            .budget = smave::SolveBudget{
                .work_iterations = action.action.work_iterations},
            .estimated_cost_us = prediction.attempt_wall_us,
            .pass_probability = prediction.pass_probability,
            .risk_score = prediction.risk_score,
            .cost_relative_uncertainty = prediction.cost_relative_uncertainty,
            .pass_probability_uncertainty = prediction.pass_probability_uncertainty,
            .support_extrapolation = prediction.support_extrapolation,
            .backend_role = smave::BackendRole::linear_solver,
            .builtin = true,
        });
    }
    return result;
}

std::vector<smave::SolveStep> static_alternatives(
    const RequestObservation& request,
    const std::map<std::string, std::map<ActionSpec, StaticActionProfile>>& profiles) {
    const std::string family = request.system.positive_definite ? "pcg" : "gmres";
    std::vector<smave::SolveStep> result;
    for (const auto& observation : request.actions) {
        const auto found = profiles.at(family).find(observation.action);
        if (found == profiles.at(family).end() ||
            found->second.pass_probability <= 0.0) continue;
        result.push_back(smave::SolveStep{
            .expert_version = observation.action.expert,
            .permission = smave::Permission::direct,
            .budget = smave::SolveBudget{
                .work_iterations = observation.action.work_iterations},
            .estimated_cost_us = found->second.median_cost,
            .pass_probability = found->second.pass_probability,
            .backend_role = smave::BackendRole::linear_solver,
            .builtin = true,
        });
    }
    return result;
}

std::string plan_signature(const std::vector<smave::SolveStep>& steps) {
    std::ostringstream output;
    for (std::size_t index = 0; index < steps.size(); ++index) {
        if (index != 0) output << ',';
        output << steps[index].expert_version << '@'
               << steps[index].budget.work_iterations;
    }
    return output.str();
}

std::size_t calibrated_transition_count(
    const std::vector<smave::SolveStep>& steps,
    const std::vector<smave::RouteConditionalCostCalibration>& calibrations) {
    std::size_t count{};
    for (std::size_t index = 1; index < steps.size(); ++index) {
        const auto& previous = steps[index - 1];
        const auto& next = steps[index];
        count += std::any_of(
            calibrations.begin(), calibrations.end(), [&](const auto& calibration) {
                return calibration.previous.expert_version ==
                        previous.expert_version &&
                    calibration.previous.work_iterations ==
                        previous.budget.work_iterations &&
                    calibration.next.expert_version == next.expert_version &&
                    calibration.next.work_iterations == next.budget.work_iterations;
            }) ? 1U : 0U;
    }
    return count;
}

double realized_cost(
    const std::vector<smave::SolveStep>& steps,
    const RequestObservation& request) {
    double cost{};
    for (const auto& step : steps) {
        const auto* observation = find_action(
            request, {step.expert_version, step.budget.work_iterations});
        if (observation == nullptr) continue;
        cost += observation->attempt_wall_us;
        if (observation->passed) return cost;
    }
    return cost + request.terminal.wall_us;
}

double realized_oracle(const RequestObservation& request) {
    double best = request.terminal.wall_us;
    const std::size_t subsets = std::size_t{1} << request.actions.size();
    for (std::size_t mask = 1; mask < subsets; ++mask) {
        std::vector<const ActionObservation*> selected;
        std::set<std::string> experts;
        bool valid = true;
        for (std::size_t index = 0; index < request.actions.size(); ++index) {
            if ((mask & (std::size_t{1} << index)) == 0) continue;
            if (!experts.insert(request.actions[index].action.expert).second) {
                valid = false;
                break;
            }
            selected.push_back(&request.actions[index]);
        }
        if (!valid || selected.size() > top_k) continue;
        std::sort(selected.begin(), selected.end(), [](const auto* left, const auto* right) {
            return left->action < right->action;
        });
        do {
            double cost{};
            bool accepted{};
            for (const auto* observation : selected) {
                cost += observation->attempt_wall_us;
                if (observation->passed) {
                    accepted = true;
                    break;
                }
            }
            best = std::min(
                best, cost + (accepted ? 0.0 : request.terminal.wall_us));
        } while (std::next_permutation(
            selected.begin(), selected.end(), [](const auto* left, const auto* right) {
                return left->action < right->action;
            }));
    }
    return best;
}

double exhaustive_expected_cost(
    std::vector<smave::SolveStep> alternatives,
    double terminal_cost,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions = {}) {
    std::map<std::string, std::vector<smave::SolveStep>> grouped;
    for (auto& alternative : alternatives) {
        grouped[alternative.expert_version].push_back(std::move(alternative));
    }
    std::vector<std::vector<smave::SolveStep>> groups;
    for (auto& [expert, actions] : grouped) {
        (void)expert;
        groups.push_back(std::move(actions));
    }
    double best = terminal_cost;
    std::vector<smave::SolveStep> selected;
    const auto visit = [&](const auto& self, std::size_t group_index) -> void {
        if (selected.size() > top_k) return;
        if (group_index == groups.size()) {
            if (selected.empty()) return;
            std::sort(selected.begin(), selected.end(), [](const auto& left, const auto& right) {
                return std::tie(left.expert_version, left.budget.work_iterations) <
                    std::tie(right.expert_version, right.budget.work_iterations);
            });
            do {
                best = std::min(
                    best, smave::expected_interaction_aware_cascade_cost(
                        selected, terminal_cost, interactions));
            } while (std::next_permutation(
                selected.begin(), selected.end(), [](const auto& left, const auto& right) {
                    return std::tie(left.expert_version, left.budget.work_iterations) <
                        std::tie(right.expert_version, right.budget.work_iterations);
                }));
            return;
        }
        self(self, group_index + 1);
        for (const auto& action : groups[group_index]) {
            selected.push_back(action);
            self(self, group_index + 1);
            selected.pop_back();
        }
    };
    visit(visit, 0);
    return best;
}

PredictionMetrics prediction_metrics(
    const smave::RequestConditionedRoutingModel& model,
    const std::vector<const RequestObservation*>& heldout,
    double terminal_cost,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions,
    const std::set<std::string>& anchor_only_families,
    const ActionSpec* global_fixed,
    const std::filesystem::path& path) {
    std::vector<double> cost_errors;
    std::vector<double> selected_cost_errors;
    std::vector<double> anchor_cost_errors;
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write prediction diagnostics");
    output << "matrix\trequest\tfamily\taction\tobserved_cost_us"
              "\tpredicted_cost_us\trelative_error\tselected_in_final_plan"
              "\tfamily_anchor\tglobal_fixed\tterminal_action"
              "\tcost_relative_uncertainty\tpass_probability_uncertainty"
              "\tsupport_extrapolation\n";
    output << std::setprecision(17);
    const auto routing = conditioned_config(
        model, terminal_cost, family_anchors, interactions,
        anchor_only_families, global_fixed);
    double brier{};
    std::array<double, 10> bin_predictions{};
    std::array<double, 10> bin_outcomes{};
    std::array<std::size_t, 10> bin_counts{};
    std::map<ActionSpec, std::tuple<double, double, std::size_t>> action_calibration;
    std::size_t observations{};
    for (const auto* request : heldout) {
        const auto plan = smave::route_sparse_linear_system(request->profile, routing);
        std::set<ActionSpec> selected_actions;
        for (const auto& step : plan.steps) {
            selected_actions.insert(
                ActionSpec{step.expert_version, step.budget.work_iterations});
        }
        const auto& anchor_reference = family_anchors.at(numeric_family(*request));
        const ActionSpec anchor_action{
            anchor_reference.expert_version, anchor_reference.work_iterations};
        const ActionSpec global_fixed_action = global_fixed == nullptr
            ? ActionSpec{}
            : *global_fixed;
        const auto features = smave::extract_sparse_routing_features(
            model.feature_names, sparse_profile(*request, model.feature_names));
        const ActionSpec terminal_action{
            smave::assess_equation(request->profile).mandatory_fallback, 0};
        const auto terminal_prediction = smave::predict_request_conditioned_action(
            model, terminal_action.expert, 0, features, 0.0,
            numeric_family(*request));
        for (const auto& observation : request->actions) {
            const auto prediction = smave::predict_request_conditioned_action(
                model, observation.action.expert,
                observation.action.work_iterations, features,
                terminal_prediction.attempt_wall_us,
                numeric_family(*request));
            const double cost_error = std::abs(
                prediction.attempt_wall_us - observation.attempt_wall_us) /
                observation.attempt_wall_us;
            cost_errors.push_back(cost_error);
            const bool selected = selected_actions.contains(observation.action);
            const bool family_anchor = !anchor_action.expert.empty() &&
                observation.action == anchor_action;
            const bool fixed_action = !global_fixed_action.expert.empty() &&
                observation.action == global_fixed_action;
            if (selected) selected_cost_errors.push_back(cost_error);
            if (family_anchor) anchor_cost_errors.push_back(cost_error);
            output << request->matrix << '\t' << request->request_id << '\t'
                   << numeric_family(*request) << '\t'
                   << observation.action.key() << '\t'
                   << observation.attempt_wall_us << '\t'
                   << prediction.attempt_wall_us << '\t' << cost_error << '\t'
                   << (selected ? 1 : 0) << '\t' << (family_anchor ? 1 : 0)
                   << '\t' << (fixed_action ? 1 : 0) << "\t0\t"
                   << prediction.cost_relative_uncertainty << '\t'
                   << prediction.pass_probability_uncertainty << '\t'
                   << prediction.support_extrapolation << '\n';
            const double outcome = observation.passed ? 1.0 : 0.0;
            const double error = prediction.pass_probability - outcome;
            brier += error * error;
            const std::size_t bin = std::min<std::size_t>(
                9, static_cast<std::size_t>(prediction.pass_probability * 10.0));
            bin_predictions[bin] += prediction.pass_probability;
            bin_outcomes[bin] += outcome;
            ++bin_counts[bin];
            auto& [predicted, observed, count] = action_calibration[observation.action];
            predicted += prediction.pass_probability;
            observed += outcome;
            ++count;
            ++observations;
        }
        const double terminal_cost_error = std::abs(
            terminal_prediction.attempt_wall_us - request->terminal.wall_us) /
            request->terminal.wall_us;
        cost_errors.push_back(terminal_cost_error);
        const bool terminal_selected = plan.steps.empty();
        const bool terminal_anchor = anchor_reference.expert_version.empty() ||
            find_action(*request, anchor_action) == nullptr;
        if (terminal_selected) selected_cost_errors.push_back(terminal_cost_error);
        if (terminal_anchor) anchor_cost_errors.push_back(terminal_cost_error);
        output << request->matrix << '\t' << request->request_id << '\t'
               << numeric_family(*request) << '\t' << terminal_action.key() << '\t'
               << request->terminal.wall_us << '\t'
               << terminal_prediction.attempt_wall_us << '\t'
               << terminal_cost_error << '\t' << (terminal_selected ? 1 : 0)
               << '\t' << (terminal_anchor ? 1 : 0) << "\t0\t1\t"
               << terminal_prediction.cost_relative_uncertainty << '\t'
               << terminal_prediction.pass_probability_uncertainty << '\t'
               << terminal_prediction.support_extrapolation << '\n';
        const double terminal_outcome = request->terminal.success ? 1.0 : 0.0;
        const double terminal_error =
            terminal_prediction.pass_probability - terminal_outcome;
        brier += terminal_error * terminal_error;
        const std::size_t terminal_bin = std::min<std::size_t>(
            9, static_cast<std::size_t>(terminal_prediction.pass_probability * 10.0));
        bin_predictions[terminal_bin] += terminal_prediction.pass_probability;
        bin_outcomes[terminal_bin] += terminal_outcome;
        ++bin_counts[terminal_bin];
        auto& [predicted, observed, count] = action_calibration[terminal_action];
        predicted += terminal_prediction.pass_probability;
        observed += terminal_outcome;
        ++count;
        ++observations;
    }
    double ece{};
    for (std::size_t bin = 0; bin < bin_counts.size(); ++bin) {
        if (bin_counts[bin] == 0) continue;
        const double count = static_cast<double>(bin_counts[bin]);
        ece += count / static_cast<double>(observations) *
            std::abs(bin_predictions[bin] / count - bin_outcomes[bin] / count);
    }
    double maximum_action_calibration_error{};
    for (const auto& [action, values] : action_calibration) {
        (void)action;
        const auto& [predicted, observed, count] = values;
        if (count == 0) continue;
        maximum_action_calibration_error = std::max(
            maximum_action_calibration_error,
            std::abs(predicted / static_cast<double>(count) -
                     observed / static_cast<double>(count)));
    }
    return PredictionMetrics{
        .cost_median_relative_error = median(cost_errors),
        .cost_p95_relative_error = percentile(cost_errors, 0.95),
        .cost_maximum_relative_error = *std::max_element(
            cost_errors.begin(), cost_errors.end()),
        .selected_cost_median_relative_error = median(selected_cost_errors),
        .selected_cost_p95_relative_error = percentile(selected_cost_errors, 0.95),
        .selected_cost_maximum_relative_error = *std::max_element(
            selected_cost_errors.begin(), selected_cost_errors.end()),
        .anchor_cost_p95_relative_error = percentile(anchor_cost_errors, 0.95),
        .anchor_cost_maximum_relative_error = *std::max_element(
            anchor_cost_errors.begin(), anchor_cost_errors.end()),
        .selected_cost_predictions = selected_cost_errors.size(),
        .anchor_cost_predictions = anchor_cost_errors.size(),
        .pass_brier_score = brier / static_cast<double>(observations),
        .pass_ece = ece,
        .pass_maximum_action_calibration_error = maximum_action_calibration_error,
    };
}

PolicyMetrics evaluate_conditioned_policy(
    const smave::RequestConditionedRoutingModel& model,
    const std::vector<const RequestObservation*>& heldout,
    double terminal_cost,
    const std::vector<double>& oracles,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions = {},
    const std::set<std::string>& anchor_only_families = {},
    const ActionSpec* global_fixed = nullptr) {
    const auto routing = conditioned_config(
        model, terminal_cost, family_anchors, interactions,
        anchor_only_families, global_fixed);
    double total{};
    double oracle_total{};
    std::vector<double> regrets;
    std::map<std::string, std::size_t> plan_counts;
    std::map<std::pair<std::string, double>, std::set<std::string>> rhs_groups;
    for (std::size_t index = 0; index < heldout.size(); ++index) {
        const auto* request = heldout[index];
        const auto plan = smave::route_sparse_linear_system(request->profile, routing);
        const auto signature = plan_signature(plan.steps);
        ++plan_counts[signature];
        rhs_groups[{request->matrix, request->relative_tolerance}].insert(signature);
        const double cost = realized_cost(plan.steps, *request);
        total += cost;
        oracle_total += oracles[index];
        regrets.push_back(cost / oracles[index]);
    }
    const auto modal = std::max_element(
        plan_counts.begin(), plan_counts.end(), [](const auto& left, const auto& right) {
            return left.second < right.second;
        });
    std::size_t changed_groups{};
    for (const auto& [group, signatures] : rhs_groups) {
        (void)group;
        changed_groups += signatures.size() > 1 ? 1U : 0U;
    }
    return PolicyMetrics{
        .regret = total / oracle_total,
        .median_request_regret = median(std::move(regrets)),
        .distinct_plans = plan_counts.size(),
        .modal_plan_change_fraction = 1.0 -
            static_cast<double>(modal->second) / static_cast<double>(heldout.size()),
        .rhs_changed_plan_group_fraction = static_cast<double>(changed_groups) /
            static_cast<double>(rhs_groups.size()),
    };
}

ActionSpec best_fixed_action(
    const std::vector<const RequestObservation*>& training,
    double& training_cost) {
    std::set<ActionSpec> actions;
    for (const auto* request : training) {
        for (const auto& action : request->actions) actions.insert(action.action);
    }
    ActionSpec best;
    training_cost = std::accumulate(
        training.begin(), training.end(), 0.0,
        [](double total, const auto* request) { return total + request->terminal.wall_us; });
    for (const auto& action : actions) {
        double total{};
        for (const auto* request : training) {
            const auto* observation = find_action(*request, action);
            total += observation == nullptr
                ? request->terminal.wall_us
                : observation->attempt_wall_us +
                    (observation->passed ? 0.0 : request->terminal.wall_us);
        }
        if (total < training_cost) {
            training_cost = total;
            best = action;
        }
    }
    return best;
}

std::string numeric_family(const RequestObservation& request) {
    if (request.system.positive_definite) return "spd";
    if (request.system.symmetric) return "symmetric-indefinite";
    return "nonsymmetric";
}

std::map<std::string, smave::RouteActionReference> family_anchor_actions(
    const std::vector<const RequestObservation*>& training) {
    std::map<std::string, smave::RouteActionReference> result;
    for (const std::string family : {"spd", "symmetric-indefinite", "nonsymmetric"}) {
        std::vector<const RequestObservation*> family_training;
        for (const auto* request : training) {
            if (numeric_family(*request) == family) family_training.push_back(request);
        }
        double training_cost{};
        const auto action = best_fixed_action(family_training, training_cost);
        result[family] = smave::RouteActionReference{
            .expert_version = action.expert,
            .work_iterations = action.work_iterations,
        };
    }
    return result;
}

double realized_action_cost(
    const RequestObservation& request,
    const ActionSpec& action) {
    if (action.expert.empty()) return request.terminal.wall_us;
    const auto* observation = find_action(request, action);
    if (observation == nullptr) return request.terminal.wall_us;
    return observation->attempt_wall_us +
        (observation->passed ? 0.0 : request.terminal.wall_us);
}

std::string action_name(const ActionSpec& action) {
    return action.expert.empty() ? "terminal" : action.key();
}

ActionSpec action_spec(const smave::RouteActionReference& reference) {
    return ActionSpec{reference.expert_version, reference.work_iterations};
}

smave::RouteActionReference action_reference(const ActionSpec& action) {
    return smave::RouteActionReference{
        .expert_version = action.expert,
        .work_iterations = action.work_iterations,
    };
}

FamilyAnchorEvidence calibrate_family_anchors(
    const std::vector<RequestObservation>& requests,
    const std::map<std::string, smave::RouteActionReference>& training_anchors,
    const ActionSpec& global_fixed,
    const std::filesystem::path& path) {
    using GroupTotals = std::pair<double, double>;
    std::map<std::string, std::map<std::string, GroupTotals>> totals;
    for (const auto& request : requests) {
        if (request.split != "calibration") continue;
        const auto family = numeric_family(request);
        const auto provisional = action_spec(training_anchors.at(family));
        auto& [family_total, global_total] = totals[family][request.matrix];
        family_total += realized_action_cost(request, provisional);
        global_total += realized_action_cost(request, global_fixed);
    }

    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write family anchor evidence");
    output << "family\ttraining_anchor\tglobal_fixed"
              "\tindependent_calibration_groups\tmean_group_cost_ratio"
              "\tmedian_group_cost_ratio\tmaximum_group_cost_ratio"
              "\tspecialized_enabled\tguarded_anchor\tgroup_cost_ratios\n";
    output << std::setprecision(17);

    FamilyAnchorEvidence result;
    for (const auto& [family, reference] : training_anchors) {
        const auto provisional = action_spec(reference);
        std::vector<double> ratios;
        if (const auto found = totals.find(family); found != totals.end()) {
            for (const auto& [group, group_totals] : found->second) {
                (void)group;
                if (group_totals.second <= 0.0) continue;
                ratios.push_back(group_totals.first / group_totals.second);
            }
        }
        result.independent_calibration_groups += ratios.size();
        const double median_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : median(ratios);
        const double mean_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : std::accumulate(ratios.begin(), ratios.end(), 0.0) /
                static_cast<double>(ratios.size());
        const double maximum_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : *std::max_element(ratios.begin(), ratios.end());
        const bool specialized_enabled = provisional != global_fixed &&
            ratios.size() >= 2 && maximum_ratio <= 1.0 + 1.0e-12 &&
            mean_ratio < 1.0 - minimum_family_anchor_gain_fraction;
        const auto guarded = specialized_enabled ? provisional : global_fixed;
        result.guarded_anchors[family] = action_reference(guarded);
        if (specialized_enabled) result.specialized_families.insert(family);
        else result.global_fixed_families.insert(family);

        output << family << '\t' << action_name(provisional) << '\t'
               << action_name(global_fixed) << '\t' << ratios.size() << '\t'
               << mean_ratio << '\t' << median_ratio << '\t' << maximum_ratio
               << '\t' << (specialized_enabled ? 1 : 0) << '\t'
               << action_name(guarded) << '\t';
        for (std::size_t index = 0; index < ratios.size(); ++index) {
            if (index != 0) output << ',';
            output << ratios[index];
        }
        output << '\n';
    }
    return result;
}

ConditionalCostEvidence collect_conditional_cost_evidence(
    const std::vector<RequestObservation>& requests,
    const smave::RequestConditionedRoutingModel& model,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    double terminal_cost,
    const ActionSpec* global_fixed,
    const std::filesystem::path& path) {
    using Transition = std::pair<ActionSpec, ActionSpec>;
    std::map<Transition, std::set<std::string>> training_transition_groups;
    for (const auto& request : requests) {
        if (request.split != "training") continue;
        const auto plan = smave::route_sparse_linear_system(
            request.profile,
            conditioned_config(
                model, terminal_cost, family_anchors, {}, {}, global_fixed));
        for (std::size_t index = 1; index < plan.steps.size(); ++index) {
            const Transition transition{
                ActionSpec{plan.steps[index - 1].expert_version,
                           plan.steps[index - 1].budget.work_iterations},
                ActionSpec{plan.steps[index].expert_version,
                           plan.steps[index].budget.work_iterations}};
            const auto* first = find_action(request, transition.first);
            if (first == nullptr || first->passed) continue;
            training_transition_groups[transition].insert(request.matrix);
        }
    }
    ConditionalCostEvidence result;
    result.candidate_transitions = training_transition_groups.size();
    std::vector<Transition> selected;
    for (const auto& [transition, groups] : training_transition_groups) {
        result.maximum_training_transition_groups = std::max(
            result.maximum_training_transition_groups, groups.size());
        if (groups.size() >= 2) selected.push_back(transition);
    }
    constexpr std::size_t maximum_selected_transitions = 8;
    std::stable_sort(selected.begin(), selected.end(), [&](const auto& left, const auto& right) {
        const auto left_count = training_transition_groups.at(left).size();
        const auto right_count = training_transition_groups.at(right).size();
        if (left_count != right_count) return left_count > right_count;
        return left < right;
    });
    if (selected.size() > maximum_selected_transitions) {
        selected.resize(maximum_selected_transitions);
    }

    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write conditional cost evidence");
    output << "split\tmatrix\trequest\tprevious\tnext\tisolated_next_us"
              "\tconditional_next_us\tmultiplier\tprevious_status\tnext_status\n";
    std::map<Transition, std::map<std::string, std::vector<double>>> multipliers;
    std::set<std::pair<Transition, std::string>> collected_matrix_transitions;
    for (const auto& request : requests) {
        if (request.split != "training" && request.split != "calibration") continue;
        for (const auto& transition : selected) {
            const auto matrix_transition = std::make_pair(transition, request.matrix);
            if (collected_matrix_transitions.contains(matrix_transition)) continue;
            const auto* isolated_previous = find_action(request, transition.first);
            const auto* isolated_next = find_action(request, transition.second);
            if (isolated_previous == nullptr || isolated_next == nullptr ||
                !isolated_previous->stable_status || isolated_previous->passed) {
                continue;
            }
            std::vector<double> conditional_samples;
            std::string previous_status;
            std::string next_status;
            for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
                const auto solve = smave::verified_linear_solve(
                    request.system,
                    solve_options(
                        request.relative_tolerance,
                        ordered_pair_routing(
                            transition.first, transition.second)));
                if (solve.attempts.size() < 2) continue;
                previous_status = solve.attempts[0].status;
                next_status = solve.attempts[1].status;
                conditional_samples.push_back(
                    std::max(solve.attempts[1].wall_us, 1.0e-6));
            }
            if (conditional_samples.empty()) continue;
            collected_matrix_transitions.insert(matrix_transition);
            const double conditional_next = median(std::move(conditional_samples));
            const double multiplier = conditional_next / isolated_next->attempt_wall_us;
            multipliers[transition][request.matrix].push_back(multiplier);
            output << request.split << '\t' << request.matrix << '\t'
                   << request.request_id << '\t' << transition.first.key() << '\t'
                   << transition.second.key() << '\t'
                   << isolated_next->attempt_wall_us << '\t' << conditional_next << '\t'
                   << multiplier << '\t' << previous_status << '\t' << next_status << '\n';
        }
    }

    result.selected_transitions = selected.size();
    for (const auto& transition : selected) {
        std::vector<double> training_group_medians;
        std::vector<double> calibration_group_medians;
        for (auto& [group, values] : multipliers[transition]) {
            const auto spec = std::find_if(
                matrix_specs.begin(), matrix_specs.end(), [&](const auto& candidate) {
                    return candidate.name == group;
                });
            if (spec == matrix_specs.end()) continue;
            const double value = median(std::move(values));
            if (spec->split == "training") training_group_medians.push_back(value);
            else if (spec->split == "calibration") calibration_group_medians.push_back(value);
        }
        if (training_group_medians.size() < 2 || calibration_group_medians.empty()) continue;
        const double multiplier = median(training_group_medians);
        std::vector<double> calibration_errors;
        for (const double value : calibration_group_medians) {
            calibration_errors.push_back(std::abs(value - multiplier));
        }
        result.training_groups += training_group_medians.size();
        result.calibration_groups += calibration_group_medians.size();
        result.calibrations.push_back(smave::RouteConditionalCostCalibration{
            .previous = smave::RouteActionReference{
                .expert_version = transition.first.expert,
                .work_iterations = transition.first.work_iterations},
            .next = smave::RouteActionReference{
                .expert_version = transition.second.expert,
                .work_iterations = transition.second.work_iterations},
            .independent_training_groups = training_group_medians.size(),
            .independent_calibration_groups = calibration_group_medians.size(),
            .conditional_cost_multiplier = multiplier,
            .conditional_cost_multiplier_upper = std::max(
                multiplier, multiplier +
                    percentile(calibration_errors, 0.95)),
        });
    }
    return result;
}

double realized_family_anchor_cost(
    const RequestObservation& request,
    const std::map<std::string, smave::RouteActionReference>& family_anchors) {
    return realized_action_cost(
        request, action_spec(family_anchors.at(numeric_family(request))));
}

std::pair<double, std::string> evaluate_family_fixed_baseline(
    const std::vector<const RequestObservation*>& heldout,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    double oracle_total_us) {
    double total{};
    for (const auto* request : heldout) {
        total += realized_family_anchor_cost(*request, family_anchors);
    }
    std::ostringstream actions;
    bool first = true;
    for (const auto& [family, anchor] : family_anchors) {
        if (!first) actions << ',';
        actions << family << '=' << action_name(action_spec(anchor));
        first = false;
    }
    return {total / oracle_total_us, actions.str()};
}

FamilyAdaptationEvidence calibrate_family_adaptation(
    const std::vector<RequestObservation>& requests,
    const smave::RequestConditionedRoutingModel& model,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    double terminal_cost,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions,
    const ActionSpec* global_fixed,
    const std::filesystem::path& path) {
    using GroupTotals = std::pair<double, double>;
    std::map<std::string, std::map<std::string, GroupTotals>> totals;
    const auto routing = conditioned_config(
        model, terminal_cost, family_anchors, interactions, {}, global_fixed);
    std::set<std::string> anchor_only_families;
    for (const auto& [family, anchor] : family_anchors) {
        (void)anchor;
        anchor_only_families.insert(family);
    }
    const auto anchor_routing = conditioned_config(
        model, terminal_cost, family_anchors, interactions,
        anchor_only_families, global_fixed);
    for (const auto& request : requests) {
        if (request.split != "calibration") continue;
        const auto plan = smave::route_sparse_linear_system(request.profile, routing);
        const auto anchor_plan = smave::route_sparse_linear_system(
            request.profile, anchor_routing);
        auto& [conditioned_total, anchor_total] =
            totals[numeric_family(request)][request.matrix];
        conditioned_total += realized_cost(plan.steps, request);
        anchor_total += realized_cost(anchor_plan.steps, request);
    }

    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write family adaptation evidence");
    output << "family\tindependent_calibration_groups\tmean_group_cost_ratio"
              "\tmedian_group_cost_ratio\tmaximum_group_cost_ratio"
              "\tadaptive_enabled\tgroup_cost_ratios\n";
    output << std::setprecision(17);
    FamilyAdaptationEvidence result;
    for (const auto& [family, anchor] : family_anchors) {
        (void)anchor;
        std::vector<double> ratios;
        if (const auto found = totals.find(family); found != totals.end()) {
            for (const auto& [group, group_totals] : found->second) {
                (void)group;
                if (group_totals.second <= 0.0) continue;
                ratios.push_back(group_totals.first / group_totals.second);
            }
        }
        result.independent_calibration_groups += ratios.size();
        const double median_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : median(ratios);
        const double mean_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : std::accumulate(ratios.begin(), ratios.end(), 0.0) /
                static_cast<double>(ratios.size());
        const double maximum_ratio = ratios.empty()
            ? std::numeric_limits<double>::infinity()
            : *std::max_element(ratios.begin(), ratios.end());
        const bool adaptive_enabled = ratios.size() >= 2 &&
            maximum_ratio <= 1.0 + 1.0e-12 &&
            mean_ratio < 1.0 - minimum_family_anchor_gain_fraction;
        if (adaptive_enabled) ++result.adaptive_families;
        else result.anchor_only_families.insert(family);
        output << family << '\t' << ratios.size() << '\t' << mean_ratio << '\t'
               << median_ratio << '\t' << maximum_ratio << '\t'
               << (adaptive_enabled ? 1 : 0) << '\t';
        for (std::size_t index = 0; index < ratios.size(); ++index) {
            if (index != 0) output << ',';
            output << ratios[index];
        }
        output << '\n';
    }
    return result;
}

EvaluationResult evaluate(
    const std::vector<RequestObservation>& requests,
    const smave::RequestConditionedRoutingModel& full_model,
    const smave::RequestConditionedRoutingModel& size_model,
    const smave::RequestConditionedRoutingModel& rhs_model,
    const smave::RequestConditionedRoutingModel& tolerance_model,
    const std::map<std::string, smave::RouteActionReference>&
        training_family_anchors,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    const ActionSpec& fixed,
    const std::vector<smave::RouteConditionalCostCalibration>& interactions,
    const std::set<std::string>& anchor_only_families,
    const std::filesystem::path& production_trace_path,
    const std::filesystem::path& prediction_diagnostics_path) {
    EvaluationResult result;
    const auto heldout = split_requests(requests, "heldout");
    const double terminal_cost = median_terminal_cost(requests);
    std::vector<double> oracles;
    oracles.reserve(heldout.size());
    for (const auto* request : heldout) {
        oracles.push_back(realized_oracle(*request));
        result.oracle_total_us += oracles.back();
        const auto alternatives = predicted_alternatives(full_model, *request);
        const double request_terminal_cost = predicted_terminal_cost(
            full_model, *request, terminal_cost);
        const auto dp = smave::optimize_interaction_aware_calibrated_cascade(
            alternatives, interactions, top_k, request_terminal_cost);
        const double dp_cost = smave::expected_interaction_aware_cascade_cost(
            dp, request_terminal_cost, interactions);
        const double exhaustive = exhaustive_expected_cost(
            alternatives, request_terminal_cost, interactions);
        if (std::abs(dp_cost - exhaustive) >
            1.0e-8 * std::max(1.0, exhaustive)) {
            ++result.dp_exhaustive_mismatches;
        }
    }
    result.prediction = prediction_metrics(
        full_model, heldout, terminal_cost, family_anchors, interactions,
        anchor_only_families, &fixed, prediction_diagnostics_path);
    result.raw_conditioned = evaluate_conditioned_policy(
        full_model, heldout, terminal_cost, oracles, {});
    std::set<std::string> all_anchor_only_families;
    for (const auto& [family, anchor] : family_anchors) {
        (void)anchor;
        all_anchor_only_families.insert(family);
    }
    result.control_aware_anchor = evaluate_conditioned_policy(
        full_model, heldout, terminal_cost, oracles, family_anchors, {},
        all_anchor_only_families, &fixed);
    result.conditioned_without_interactions = evaluate_conditioned_policy(
        full_model, heldout, terminal_cost, oracles, family_anchors, {},
        anchor_only_families, &fixed);
    result.conditioned = evaluate_conditioned_policy(
        full_model, heldout, terminal_cost, oracles, family_anchors, interactions,
        anchor_only_families, &fixed);
    result.size_only = evaluate_conditioned_policy(
        size_model, heldout, terminal_cost, oracles, family_anchors, {}, {}, &fixed);
    result.rhs_only = evaluate_conditioned_policy(
        rhs_model, heldout, terminal_cost, oracles, family_anchors, {}, {}, &fixed);
    result.tolerance_only = evaluate_conditioned_policy(
        tolerance_model, heldout, terminal_cost, oracles, family_anchors, {}, {}, &fixed);

    const auto profiles = static_profiles(requests);
    double static_total{};
    for (std::size_t index = 0; index < heldout.size(); ++index) {
        const auto alternatives = static_alternatives(*heldout[index], profiles);
        const auto plan = smave::optimize_joint_calibrated_cascade(
            alternatives, top_k, terminal_cost);
        static_total += realized_cost(plan, *heldout[index]);
    }
    result.static_regret = static_total / result.oracle_total_us;

    result.fixed_action = action_name(fixed);
    double fixed_total{};
    for (const auto* request : heldout) {
        fixed_total += realized_action_cost(*request, fixed);
    }
    result.fixed_regret = fixed_total / result.oracle_total_us;

    std::tie(
        result.training_family_fixed_regret,
        result.training_family_fixed_actions) = evaluate_family_fixed_baseline(
            heldout, training_family_anchors, result.oracle_total_us);
    std::tie(result.family_fixed_regret, result.family_fixed_actions) =
        evaluate_family_fixed_baseline(
            heldout, family_anchors, result.oracle_total_us);

    std::ofstream trace(production_trace_path);
    if (!trace) throw std::runtime_error("cannot write production traces");
    trace << "matrix\trequest\tplan\tattempt_index\tphase\tbackend\tbudget"
          << "\texecuted_iterations\tstatus\twall_us\tresidual_inf\n";
    const auto routing = conditioned_config(
        full_model, terminal_cost, family_anchors, interactions,
        anchor_only_families, &fixed);
    const auto independent_routing = conditioned_config(
        full_model, terminal_cost, family_anchors, {},
        anchor_only_families, &fixed);
    for (const auto* request : heldout) {
        const auto plan = smave::route_sparse_linear_system(request->profile, routing);
        const auto independent_plan = smave::route_sparse_linear_system(
            request->profile, independent_routing);
        result.interaction_plan_changed_requests +=
            plan_signature(plan.steps) != plan_signature(independent_plan.steps) ? 1U : 0U;
        result.independent_plans_with_calibrated_transition +=
            calibrated_transition_count(independent_plan.steps, interactions) > 0 ? 1U : 0U;
        result.interaction_plans_with_calibrated_transition +=
            calibrated_transition_count(plan.steps, interactions) > 0 ? 1U : 0U;
        const auto solve = smave::verified_linear_solve(
            request->system,
            solve_options(request->relative_tolerance, routing));
        if (!solve.success) {
            ++result.production_failures;
        } else {
            ++result.production_successes;
            result.production_fallbacks += solve.used_fallback ? 1U : 0U;
            const auto gate = recompute_gate(
                request->system, solve.solution, 1.0e-12,
                request->relative_tolerance);
            const double tolerance = 1.0e-12 * std::max(1.0, gate.residual_inf);
            if (!gate.accepted ||
                std::abs(gate.residual_inf - solve.residual_inf) > tolerance ||
                std::abs(gate.backward_error - solve.backward_error) > 1.0e-12) {
                ++result.production_gate_mismatches;
            }
        }
        bool plan_finished{};
        for (std::size_t index = 0; index < solve.attempts.size(); ++index) {
            const bool planned = index < plan.steps.size() && !plan_finished;
            if (planned) {
                const auto& step = plan.steps[index];
                if (solve.attempts[index].backend != step.expert_version ||
                    solve.attempts[index].work_iterations !=
                        step.budget.work_iterations) {
                    ++result.production_plan_order_mismatches;
                }
                if (solve.attempts[index].status == "accepted") plan_finished = true;
            }
            trace << request->matrix << '\t' << request->request_id << '\t'
                  << plan_signature(plan.steps) << '\t' << index << '\t'
                  << (planned ? "planned" : "terminal") << '\t'
                  << solve.attempts[index].backend << '\t'
                  << solve.attempts[index].work_iterations << '\t'
                  << solve.attempts[index].executed_iterations << '\t'
                  << solve.attempts[index].status << '\t' << std::setprecision(17)
                  << solve.attempts[index].wall_us << '\t'
                  << solve.attempts[index].residual_inf << '\n';
        }
        result.terminal_only_successes += request->terminal.success ? 1U : 0U;
    }
    return result;
}

InteractionMetrics interaction_evidence(
    const std::vector<RequestObservation>& requests,
    const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write interaction evidence");
    output << "family\tmatrix\trequest\torder\tisolated_sum_us\tcombined_sum_us"
           << "\tinteraction_delta\tfirst_status\tsecond_status\n";
    InteractionMetrics metrics;
    for (const bool positive_definite : {true, false}) {
        const RequestObservation* selected{};
        const std::array<ActionSpec, 2> actions = positive_definite
            ? std::array<ActionSpec, 2>{ActionSpec{"pcg-ic0-cpu-v1", 1},
                                        ActionSpec{"pcg-jacobi-cpu-v1", 1}}
            : std::array<ActionSpec, 2>{ActionSpec{"gmres-ilut-cpu-v1", 1},
                                        ActionSpec{"gmres-ilu0-cpu-v1", 1}};
        std::array<double, 2> isolated{};
        for (const auto& request : requests) {
            if (request.split != "heldout" ||
                request.system.positive_definite != positive_definite ||
                request.relative_tolerance != 1.0e-10 ||
                request.request_kind != "random-like" ||
                (!positive_definite && request.system.size() > 1024)) continue;
            bool both_failed = true;
            for (std::size_t index = 0; index < actions.size(); ++index) {
                const auto solve = smave::verified_linear_solve(
                    request.system,
                    solve_options(request.relative_tolerance,
                                  single_action_routing(actions[index])));
                const auto& attempt = find_attempt(solve, actions[index]);
                isolated[index] = attempt.wall_us;
                both_failed = both_failed && attempt.status != "accepted";
            }
            if (both_failed) {
                selected = &request;
                break;
            }
        }
        if (selected == nullptr) {
            output << (positive_definite ? "pcg" : "gmres")
                   << "\t-\t-\tunavailable\t0\t0\t0"
                   << "\tno-two-failed-actions\tno-two-failed-actions\n";
            continue;
        }
        ++metrics.eligible_families;
        std::array<std::vector<double>, 2> isolated_samples;
        std::array<std::string, 2> isolated_status;
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            for (std::size_t position = 0; position < actions.size(); ++position) {
                const std::size_t index = (position + repetition) % actions.size();
                const auto solve = smave::verified_linear_solve(
                    selected->system,
                    solve_options(selected->relative_tolerance,
                                  single_action_routing(actions[index])));
                const auto& attempt = find_attempt(solve, actions[index]);
                isolated_samples[index].push_back(attempt.wall_us);
                if (isolated_status[index].empty()) {
                    isolated_status[index] = attempt.status;
                } else if (isolated_status[index] != attempt.status) {
                    throw std::runtime_error(
                        "interaction isolated action status is unstable");
                }
            }
        }
        for (std::size_t index = 0; index < actions.size(); ++index) {
            isolated[index] = median(std::move(isolated_samples[index]));
        }
        std::array<double, 2> combined{};
        for (std::size_t order = 0; order < 2; ++order) {
            const ActionSpec first = actions[order];
            const ActionSpec second = actions[1 - order];
            const std::map<ActionSpec, double> costs{{first, 1.0}, {second, 2.0}};
            smave::RoutingConfig routing;
            routing.top_k = 2;
            routing.risk_weight = 0.0;
            routing.expert_allowlist = {first.expert, second.expert};
            routing.request_conditioned_model = fixed_prediction_model(
                {first, second}, costs);
            routing.calibrated_terminal_fallback_cost_us = 1.0e12;
            const auto plan = smave::route_sparse_linear_system(
                selected->profile, routing);
            if (plan.steps.size() != 2 || plan.steps[0].expert_version != first.expert ||
                plan.steps[1].expert_version != second.expert) {
                throw std::runtime_error("interaction route order is not deterministic");
            }
            std::vector<double> combined_samples;
            std::array<std::string, 2> combined_status;
            for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
                const auto solve = smave::verified_linear_solve(
                    selected->system,
                    solve_options(selected->relative_tolerance, routing));
                if (solve.attempts.size() < 2) {
                    throw std::runtime_error(
                        "interaction route did not execute both actions");
                }
                combined_samples.push_back(
                    solve.attempts[0].wall_us + solve.attempts[1].wall_us);
                for (std::size_t index = 0; index < combined_status.size(); ++index) {
                    if (combined_status[index].empty()) {
                        combined_status[index] = solve.attempts[index].status;
                    } else if (combined_status[index] != solve.attempts[index].status) {
                        throw std::runtime_error(
                            "interaction combined action status is unstable");
                    }
                }
            }
            combined[order] = median(std::move(combined_samples));
            const double isolated_sum = isolated[0] + isolated[1];
            const double delta = combined[order] / isolated_sum - 1.0;
            metrics.maximum_interaction_delta = std::max(
                metrics.maximum_interaction_delta, std::abs(delta));
            output << (positive_definite ? "pcg" : "gmres") << '\t'
                   << selected->matrix << '\t' << selected->request_id << '\t'
                   << first.key() << ',' << second.key() << '\t'
                   << isolated_sum << '\t' << combined[order] << '\t' << delta << '\t'
                   << combined_status[0] << '\t' << combined_status[1] << '\n';
        }
        metrics.maximum_order_delta = std::max(
            metrics.maximum_order_delta,
            std::abs(combined[0] - combined[1]) /
                std::max(1.0, std::min(combined[0], combined[1])));
    }
    return metrics;
}

void write_observations(
    const std::filesystem::path& path,
    const std::vector<RawActionSample>& samples) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write action observations");
    output << "split\tmatrix\trequest\trequest_kind\trelative_tolerance\texpert"
           << "\twork_iterations\trepetition\tschedule_position"
           << "\tattempt_wall_us\tterminal_reference_wall_us\tpassed\tstatus"
           << "\texecuted_iterations\tresidual_inf\tservice_success"
           << "\tused_terminal_fallback";
    for (const auto& name : full_feature_names) output << '\t' << name;
    output << '\n';
    output << std::setprecision(17);
    for (const auto& sample : samples) {
        output << sample.split << '\t' << sample.matrix << '\t' << sample.request_id
               << '\t' << sample.request_kind << '\t' << sample.relative_tolerance
               << '\t' << sample.action.expert << '\t'
               << sample.action.work_iterations << '\t' << sample.repetition << '\t'
               << sample.schedule_position << '\t' << sample.attempt_wall_us << '\t'
               << sample.terminal_reference_wall_us << '\t'
               << (sample.passed ? 1 : 0) << '\t'
               << sample.status << '\t' << sample.executed_iterations << '\t'
               << sample.residual_inf << '\t' << (sample.service_success ? 1 : 0)
               << '\t' << (sample.used_terminal_fallback ? 1 : 0);
        for (const double feature : sample.features) output << '\t' << feature;
        output << '\n';
    }
}

void write_request_summary(
    const std::filesystem::path& path,
    const std::vector<RequestObservation>& requests) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write request summary");
    output << "split\tmatrix\trequest\trows\tnonzeros\tpositive_definite"
           << "\taction_count\tterminal_wall_us\tterminal_success"
           << "\tterminal_attempt_count\n";
    output << std::setprecision(17);
    for (const auto& request : requests) {
        output << request.split << '\t' << request.matrix << '\t'
               << request.request_id << '\t' << request.system.size() << '\t'
               << request.system.nonzeros() << '\t'
               << (request.system.positive_definite ? 1 : 0) << '\t'
               << request.actions.size() << '\t' << request.terminal.wall_us << '\t'
               << (request.terminal.success ? 1 : 0) << '\t'
               << request.terminal.attempt_count << '\n';
    }
}

void write_model(
    const std::filesystem::path& path,
    const smave::RequestConditionedRoutingModel& model) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write routing model");
    output << std::setprecision(17)
           << "SMAVE_REQUEST_CONDITIONED_ROUTING_MODEL 2\n"
           << "FEATURES " << model.feature_names.size();
    for (const auto& name : model.feature_names) output << ' ' << std::quoted(name);
    output << "\nMEANS";
    for (const double value : model.feature_means) output << ' ' << value;
    output << "\nSCALES";
    for (const double value : model.feature_scales) output << ' ' << value;
    output << '\n';
    std::size_t action_count{};
    for (const auto& [expert, predictors] : model.actions) {
        for (const auto& predictor : predictors) {
            ++action_count;
            output << "ACTION " << std::quoted(expert) << ' '
                   << predictor.work_iterations << ' ' << predictor.training_samples
                   << ' ' << predictor.independent_training_groups
                   << ' ' << predictor.independent_calibration_groups
                   << ' ' << (predictor.cost_relative_to_terminal ? 1 : 0)
                   << ' ' << predictor.cost_calibration_error << ' '
                   << predictor.pass_calibration_error << ' '
                   << predictor.cost_calibration_upper_error << ' '
                   << predictor.pass_calibration_upper_error << ' '
                   << predictor.log_cost_calibration_offset << ' '
                   << predictor.pass_logit_calibration_offset << "\nCOST";
            for (const double value : predictor.log_cost_coefficients) {
                output << ' ' << value;
            }
            output << "\nPASS";
            for (const double value : predictor.pass_logit_coefficients) {
                output << ' ' << value;
            }
            output << "\nSUPPORT_MIN";
            for (const double value : predictor.support_feature_minimums) {
                output << ' ' << value;
            }
            output << "\nSUPPORT_MAX";
            for (const double value : predictor.support_feature_maximums) {
                output << ' ' << value;
            }
            output << "\nJOINT_SUPPORT "
                   << predictor.joint_support_feature_indices.size() << ' '
                   << predictor.joint_support_group_centers.size() << ' '
                   << predictor.joint_support_nearest_distance_upper;
            for (const auto index : predictor.joint_support_feature_indices) {
                output << ' ' << index;
            }
            for (const auto& center : predictor.joint_support_group_centers) {
                for (const double value : center) output << ' ' << value;
            }
            output << "\nFAMILY_PRIORS " << predictor.family_priors.size();
            for (const auto& prior : predictor.family_priors) {
                output << ' ' << std::quoted(prior.routing_family) << ' '
                       << prior.independent_training_groups << ' '
                       << prior.independent_calibration_groups << ' '
                       << prior.pooled_log_cost << ' '
                       << prior.pooled_pass_probability << ' '
                       << prior.cost_regression_weight << ' '
                       << prior.pass_regression_weight << ' '
                       << prior.cost_calibration_upper_error << ' '
                       << prior.pass_calibration_upper_error;
            }
            output << '\n';
        }
    }
    output << "ACTION_COUNT " << action_count << "\nEND\n";
}

void write_matrix_split(const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write matrix split");
    output << "SMAVE_SUITESPARSE_MATRIX_SPLIT 1\n";
    for (const auto& spec : matrix_specs) {
        output << spec.split << '\t' << spec.collection_group << '\t'
               << spec.name << '\t'
               << spec.relative_path.generic_string() << '\t'
               << spec.numeric_class
               << '\n';
    }
    output << "END\n";
}

std::size_t model_action_count(const smave::RequestConditionedRoutingModel& model) {
    std::size_t result{};
    for (const auto& [expert, predictors] : model.actions) {
        (void)expert;
        result += predictors.size();
    }
    return result;
}

std::size_t candidate_model_action_count(
    const smave::RequestConditionedRoutingModel& model) {
    std::size_t result{};
    for (const auto& [expert, predictors] : model.actions) {
        if (expert == "terminal-numerical-linear-cascade-v1") continue;
        result += predictors.size();
    }
    return result;
}

std::string matrix_ids(const std::string& split) {
    std::ostringstream output;
    bool first = true;
    for (const auto& spec : matrix_specs) {
        if (spec.split != split) continue;
        if (!first) output << ',';
        output << spec.name;
        first = false;
    }
    return output.str();
}

bool matrix_disjoint() {
    std::map<std::string, std::string> seen;
    for (const auto& spec : matrix_specs) {
        if (!seen.emplace(spec.name, spec.split).second) return false;
    }
    return true;
}

bool final_heldout_group_disjoint() {
    std::set<std::string> heldout_groups;
    for (const auto& spec : matrix_specs) {
        if (spec.split == "heldout") {
            if (pre_v6_locked_groups.contains(spec.collection_group) ||
                !heldout_groups.insert(spec.collection_group).second) {
                return false;
            }
        }
    }
    return heldout_groups.size() == 3;
}

bool all_split_groups_disjoint() {
    std::map<std::string, std::string> groups;
    for (const auto& spec : matrix_specs) {
        const auto [found, inserted] = groups.emplace(
            spec.collection_group, spec.split);
        if (!inserted && found->second != spec.split) return false;
    }
    return true;
}

std::size_t matrix_count(const std::string& split) {
    return static_cast<std::size_t>(std::count_if(
        matrix_specs.begin(), matrix_specs.end(), [&](const auto& spec) {
            return spec.split == split;
        }));
}

std::string matrix_groups(const std::string& split) {
    std::ostringstream output;
    bool first = true;
    for (const auto& spec : matrix_specs) {
        if (spec.split != split) continue;
        if (!first) output << ',';
        output << spec.collection_group;
        first = false;
    }
    return output.str();
}

std::string numeric_class_counts(const std::string& split) {
    std::map<std::string, std::size_t> counts;
    for (const auto& spec : matrix_specs) {
        if (spec.split == split) ++counts[spec.numeric_class];
    }
    std::ostringstream output;
    output << "spd:" << counts["spd"]
           << ",symmetric-non-spd:" << counts["symmetric-non-spd"]
           << ",nonsymmetric:" << counts["nonsymmetric"];
    return output.str();
}

std::size_t request_count(
    const std::vector<RequestObservation>& requests, const std::string& split) {
    return static_cast<std::size_t>(std::count_if(
        requests.begin(), requests.end(), [&](const auto& request) {
            return request.split == split;
        }));
}

std::size_t unstable_action_observations(
    const std::vector<RequestObservation>& requests) {
    std::size_t result{};
    for (const auto& request : requests) {
        for (const auto& action : request.actions) {
            result += action.stable_status ? 0U : 1U;
        }
    }
    return result;
}

struct FrozenRequestKey {
    std::string split;
    std::string matrix;
    std::string request;

    auto operator<=>(const FrozenRequestKey&) const = default;
};

struct FrozenDataset {
    std::vector<RequestObservation> requests;
    std::vector<RawActionSample> raw_samples;
    std::map<FrozenRequestKey, std::set<ActionSpec>> stable_failures;
};

struct TransitionAttrition {
    std::set<std::string> isolated_training_matrices;
    std::set<std::string> isolated_calibration_matrices;
    std::size_t modeled_alternative_requests{};
    std::size_t unguarded_both_selected_requests{};
    std::size_t unguarded_ordered_requests{};
    std::size_t unguarded_adjacent_requests{};
    std::size_t unguarded_candidate_requests{};
    std::set<std::string> unguarded_candidate_matrices;
    std::size_t final_both_selected_requests{};
    std::size_t final_ordered_requests{};
    std::size_t final_adjacent_requests{};
    std::size_t final_candidate_requests{};
    std::set<std::string> final_candidate_matrices;
};

struct CandidateTransitionEvidence {
    std::size_t requests{};
    std::set<std::string> matrices;
    std::set<std::string> isolated_training_matrices;
    std::set<std::string> isolated_calibration_matrices;
};

struct FrozenAttritionAnalysis {
    std::string version;
    bool model_byte_identical{};
    std::set<std::pair<ActionSpec, ActionSpec>> development_pairs;
    std::map<std::pair<ActionSpec, ActionSpec>, TransitionAttrition> attrition;
    std::size_t training_requests{};
    std::size_t unguarded_multistep_requests{};
    std::size_t final_terminal_abstention_requests{};
    std::size_t final_single_action_requests{};
    std::size_t final_multistep_requests{};
    std::size_t control_aware_changed_requests{};
    std::map<std::pair<ActionSpec, ActionSpec>, CandidateTransitionEvidence>
        unguarded_candidate_transitions;
    std::map<std::pair<ActionSpec, ActionSpec>, CandidateTransitionEvidence>
        final_candidate_transitions;
};

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin{};
    while (true) {
        const auto tab = line.find('\t', begin);
        fields.push_back(line.substr(begin, tab - begin));
        if (tab == std::string::npos) break;
        begin = tab + 1;
    }
    return fields;
}

std::map<std::string, std::size_t> header_indices(const std::string& line) {
    std::map<std::string, std::size_t> result;
    const auto fields = split_tabs(line);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (!result.emplace(fields[index], index).second) {
            throw std::runtime_error("duplicate TSV header: " + fields[index]);
        }
    }
    return result;
}

const std::string& field(
    const std::vector<std::string>& fields,
    const std::map<std::string, std::size_t>& indices,
    const std::string& name) {
    const auto found = indices.find(name);
    if (found == indices.end() || found->second >= fields.size()) {
        throw std::runtime_error("missing TSV field: " + name);
    }
    return fields[found->second];
}

std::string file_contents(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read frozen file: " + path.string());
    return std::string(
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

double inverse_log1p(double value) {
    return std::max(0.0, std::expm1(value));
}

FrozenDataset read_frozen_dataset(const std::filesystem::path& directory) {
    FrozenDataset result;
    std::map<FrozenRequestKey, std::size_t> request_indices;
    {
        std::ifstream input(directory / "request-summary.tsv");
        if (!input) throw std::runtime_error("cannot read frozen request summary");
        std::string line;
        if (!std::getline(input, line)) throw std::runtime_error("empty request summary");
        const auto indices = header_indices(line);
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            const auto fields = split_tabs(line);
            const FrozenRequestKey key{
                field(fields, indices, "split"),
                field(fields, indices, "matrix"),
                field(fields, indices, "request")};
            RequestObservation request;
            request.split = key.split;
            request.matrix = key.matrix;
            request.request_id = key.request;
            request.profile.fingerprint = key.matrix + ":" + key.request;
            request.profile.rows = std::stoull(field(fields, indices, "rows"));
            request.profile.columns = request.profile.rows;
            request.profile.nonzeros = std::stoull(field(fields, indices, "nonzeros"));
            request.profile.maximum_work_iterations = maximum_work_iterations;
            request.profile.restart_dimension = restart_dimension;
            request.terminal = TerminalObservation{
                .wall_us = std::stod(field(fields, indices, "terminal_wall_us")),
                .success = std::stoi(field(fields, indices, "terminal_success")) != 0,
                .attempt_count = std::stoull(
                    field(fields, indices, "terminal_attempt_count")),
            };
            if (!request_indices.emplace(key, result.requests.size()).second) {
                throw std::runtime_error("duplicate frozen request summary row");
            }
            result.requests.push_back(std::move(request));
        }
    }

    using RequestAction = std::pair<std::size_t, ActionSpec>;
    std::map<RequestAction, std::vector<RawActionSample>> grouped_samples;
    std::map<RequestAction, std::set<std::string>> grouped_statuses;
    {
        std::ifstream input(directory / "action-observations.tsv");
        if (!input) throw std::runtime_error("cannot read frozen action observations");
        std::string line;
        if (!std::getline(input, line)) {
            throw std::runtime_error("empty frozen action observations");
        }
        const auto indices = header_indices(line);
        for (const auto& name : full_feature_names) {
            if (!indices.contains(name)) {
                throw std::runtime_error("missing frozen routing feature: " + name);
            }
        }
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            const auto fields = split_tabs(line);
            const FrozenRequestKey key{
                field(fields, indices, "split"),
                field(fields, indices, "matrix"),
                field(fields, indices, "request")};
            const auto request_found = request_indices.find(key);
            if (request_found == request_indices.end()) {
                throw std::runtime_error("action observation has no request summary");
            }
            auto& request = result.requests[request_found->second];
            std::vector<double> features;
            features.reserve(full_feature_names.size());
            for (const auto& name : full_feature_names) {
                features.push_back(std::stod(field(fields, indices, name)));
            }
            if (request.features.empty()) {
                request.features = features;
                request.request_kind = field(fields, indices, "request_kind");
                request.relative_tolerance = std::stod(
                    field(fields, indices, "relative_tolerance"));
                request.profile.structurally_symmetric = features[10] > 0.5;
                request.profile.numerically_symmetric =
                    request.profile.structurally_symmetric;
                request.profile.numerically_positive_definite = features[11] > 0.5;
                request.profile.diagonal_condition_estimate = inverse_log1p(features[3]);
                request.profile.coefficient_dynamic_range =
                    std::max(1.0, inverse_log1p(features[4]));
                request.profile.row_nonzero_coefficient_of_variation = features[5];
                request.profile.row_l1_condition_estimate =
                    std::max(1.0, inverse_log1p(features[6]));
                request.profile.diagonal_dominance_fraction = features[7];
                request.profile.mean_diagonal_row_l1_fraction = features[8];
                request.profile.normalized_mean_bandwidth = features[9];
                request.profile.right_hand_side_inf = 0.0;
                request.profile.right_hand_side_roughness = features[12];
                request.profile.right_hand_side_sign_change_fraction = features[13];
                request.profile.absolute_tolerance = 1.0e-12;
                request.profile.relative_tolerance = request.relative_tolerance;
                request.system.symmetric = request.profile.numerically_symmetric;
                request.system.positive_definite =
                    request.profile.numerically_positive_definite;
            } else if (request.features != features) {
                throw std::runtime_error("frozen request features changed across actions");
            }
            RawActionSample sample{
                .split = key.split,
                .matrix = key.matrix,
                .request_id = key.request,
                .request_kind = field(fields, indices, "request_kind"),
                .relative_tolerance = std::stod(
                    field(fields, indices, "relative_tolerance")),
                .action = ActionSpec{
                    field(fields, indices, "expert"),
                    std::stoi(field(fields, indices, "work_iterations"))},
                .repetition = std::stoull(field(fields, indices, "repetition")),
                .schedule_position = std::stoull(
                    field(fields, indices, "schedule_position")),
                .features = std::move(features),
                .attempt_wall_us = std::stod(
                    field(fields, indices, "attempt_wall_us")),
                .terminal_reference_wall_us = std::stod(
                    field(fields, indices, "terminal_reference_wall_us")),
                .passed = std::stoi(field(fields, indices, "passed")) != 0,
                .status = field(fields, indices, "status"),
                .executed_iterations = std::stoi(
                    field(fields, indices, "executed_iterations")),
                .residual_inf = std::stod(field(fields, indices, "residual_inf")),
                .service_success = std::stoi(
                    field(fields, indices, "service_success")) != 0,
                .used_terminal_fallback = std::stoi(
                    field(fields, indices, "used_terminal_fallback")) != 0,
            };
            const RequestAction request_action{request_found->second, sample.action};
            grouped_statuses[request_action].insert(sample.status);
            grouped_samples[request_action].push_back(sample);
            result.raw_samples.push_back(std::move(sample));
        }
    }

    for (const auto& [request_action, samples] : grouped_samples) {
        if (samples.size() != repetitions) {
            throw std::runtime_error("frozen action does not have five repetitions");
        }
        std::vector<double> walls;
        std::vector<double> residuals;
        std::vector<double> iterations;
        std::size_t passes{};
        for (const auto& sample : samples) {
            walls.push_back(sample.attempt_wall_us);
            iterations.push_back(static_cast<double>(sample.executed_iterations));
            if (std::isfinite(sample.residual_inf)) residuals.push_back(sample.residual_inf);
            passes += sample.passed ? 1U : 0U;
        }
        auto& request = result.requests[request_action.first];
        request.actions.push_back(ActionObservation{
            .action = request_action.second,
            .attempt_wall_us = median(std::move(walls)),
            .passed = passes * 2 >= samples.size(),
            .pass_count = passes,
            .executed_iterations = static_cast<int>(std::llround(
                median(std::move(iterations)))),
            .residual_inf = residuals.empty()
                ? std::numeric_limits<double>::infinity()
                : median(std::move(residuals)),
            .stable_status = passes == 0 || passes == samples.size(),
        });
        if (passes == 0 && grouped_statuses.at(request_action).size() == 1) {
            const FrozenRequestKey key{
                request.split, request.matrix, request.request_id};
            result.stable_failures[key].insert(request_action.second);
        }
    }
    for (auto& request : result.requests) {
        std::sort(request.actions.begin(), request.actions.end(), [](const auto& left, const auto& right) {
            return left.action < right.action;
        });
        const auto reconstructed = smave::extract_sparse_routing_features(
            full_feature_names, request.profile);
        for (std::size_t index = 0; index < reconstructed.size(); ++index) {
            if (std::abs(reconstructed[index] - request.features[index]) > 1.0e-10) {
                throw std::runtime_error("frozen sparse profile reconstruction mismatch");
            }
        }
    }
    return result;
}

double conservative_plan_cost(
    const std::vector<smave::SolveStep>& steps, double terminal_cost) {
    double reach_probability = 1.0;
    double expected_cost{};
    for (const auto& step : steps) {
        const double support_multiplier = std::exp(std::min(
            step.support_extrapolation, std::log(1.0e6)));
        const double inflated_cost = step.estimated_cost_us *
            (1.0 + step.cost_relative_uncertainty) * support_multiplier;
        const double lower_pass_probability = std::clamp(
            step.pass_probability - step.pass_probability_uncertainty -
                std::min(1.0, step.support_extrapolation),
            0.0, 1.0);
        expected_cost += reach_probability * inflated_cost;
        reach_probability *= 1.0 - lower_pass_probability;
    }
    return expected_cost + reach_probability * terminal_cost;
}

std::vector<smave::SolveStep> control_aware_plan(
    const std::vector<smave::SolveStep>& alternatives,
    std::vector<smave::SolveStep> optimized,
    const std::string& family,
    const std::map<std::string, smave::RouteActionReference>& family_anchors,
    const ActionSpec& global_fixed,
    double terminal_cost) {
    const auto anchor = family_anchors.find(family);
    if (anchor == family_anchors.end()) return optimized;
    const auto locate = [&](const smave::RouteActionReference& reference) {
        if (reference.expert_version.empty()) return alternatives.end();
        return std::find_if(alternatives.begin(), alternatives.end(), [&](const auto& step) {
            return step.expert_version == reference.expert_version &&
                step.budget.work_iterations == reference.work_iterations;
        });
    };
    const auto cost = [&](const auto step) {
        return step == alternatives.end()
            ? terminal_cost
            : conservative_plan_cost(std::vector<smave::SolveStep>{*step}, terminal_cost);
    };
    auto anchor_step = locate(anchor->second);
    bool terminal_anchor = anchor_step == alternatives.end();
    double anchor_upper = cost(anchor_step);
    const auto global_step = locate(action_reference(global_fixed));
    const bool terminal_global = global_step == alternatives.end();
    const double global_upper = cost(global_step);
    const bool anchor_out_of_support = !terminal_anchor &&
        anchor_step->support_extrapolation >= std::log(4.0);
    const double required_global_upper = global_upper *
        (1.0 - minimum_family_anchor_gain_fraction);
    if (anchor_out_of_support || !(anchor_upper < required_global_upper)) {
        anchor_step = global_step;
        terminal_anchor = terminal_global;
        anchor_upper = global_upper;
    }
    const double conditioned_upper = conservative_plan_cost(optimized, terminal_cost);
    if (!terminal_anchor && anchor_step->support_extrapolation >= std::log(4.0) &&
        !(anchor_upper < terminal_cost)) {
        terminal_anchor = true;
        anchor_upper = terminal_cost;
    }
    const double required_upper = anchor_upper *
        (1.0 - minimum_family_anchor_gain_fraction);
    if (!(conditioned_upper < required_upper)) {
        if (terminal_anchor) optimized.clear();
        else optimized = {*anchor_step};
    }
    return optimized;
}

std::set<std::pair<ActionSpec, ActionSpec>> development_supported_pairs(
    const FrozenDataset& dataset) {
    using Transition = std::pair<ActionSpec, ActionSpec>;
    std::map<std::string, std::map<Transition, std::set<std::string>>> supports;
    for (const auto& request : dataset.requests) {
        const FrozenRequestKey key{
            request.split, request.matrix, request.request_id};
        const auto found = dataset.stable_failures.find(key);
        if (found == dataset.stable_failures.end()) continue;
        for (const auto& previous : found->second) {
            for (const auto& next : found->second) {
                if (previous.expert == next.expert) continue;
                supports[request.split][{previous, next}].insert(request.matrix);
            }
        }
    }
    std::set<Transition> result;
    for (const auto& [transition, training] : supports["training"]) {
        const auto calibration = supports["calibration"].find(transition);
        if (training.size() >= 2 && calibration != supports["calibration"].end() &&
            calibration->second.size() >= 2) {
            result.insert(transition);
        }
    }
    return result;
}

std::optional<std::size_t> step_index(
    const std::vector<smave::SolveStep>& steps, const ActionSpec& action) {
    for (std::size_t index = 0; index < steps.size(); ++index) {
        if (steps[index].expert_version == action.expert &&
            steps[index].budget.work_iterations == action.work_iterations) {
            return index;
        }
    }
    return std::nullopt;
}

std::string step_signature(const std::vector<smave::SolveStep>& steps) {
    std::ostringstream output;
    for (std::size_t index = 0; index < steps.size(); ++index) {
        if (index != 0) output << ',';
        output << steps[index].expert_version << '@'
               << steps[index].budget.work_iterations;
    }
    return output.str();
}

void record_candidate_transition(
    std::map<std::pair<ActionSpec, ActionSpec>, CandidateTransitionEvidence>& evidence,
    const std::pair<ActionSpec, ActionSpec>& transition,
    const std::string& matrix) {
    auto& counts = evidence[transition];
    ++counts.requests;
    counts.matrices.insert(matrix);
}

std::set<std::string> isolated_failure_matrices(
    const FrozenDataset& dataset,
    const std::pair<ActionSpec, ActionSpec>& transition,
    const std::string& split) {
    std::set<std::string> matrices;
    for (const auto& request : dataset.requests) {
        if (request.split != split) continue;
        const FrozenRequestKey key{
            request.split, request.matrix, request.request_id};
        const auto failures = dataset.stable_failures.find(key);
        if (failures != dataset.stable_failures.end() &&
            failures->second.contains(transition.first) &&
            failures->second.contains(transition.second)) {
            matrices.insert(request.matrix);
        }
    }
    return matrices;
}

FrozenAttritionAnalysis analyze_frozen_attrition(
    const std::string& version,
    const std::filesystem::path& directory,
    const std::filesystem::path& output) {
    auto dataset = read_frozen_dataset(directory);
    std::vector<std::size_t> feature_indices(full_feature_names.size());
    std::iota(feature_indices.begin(), feature_indices.end(), 0U);
    const auto model = train_model(dataset.raw_samples, dataset.requests, feature_indices);
    const auto retrained_model_path = output / (version + "-retrained-model.txt");
    write_model(retrained_model_path, model);
    const bool model_byte_identical =
        file_contents(retrained_model_path) ==
        file_contents(directory / "request-conditioned-model.txt");
    if (!model_byte_identical) {
        throw std::runtime_error(version + " frozen model reconstruction changed");
    }
    const auto training = split_requests(dataset.requests, "training");
    const auto family_anchors = family_anchor_actions(training);
    double fixed_training_cost{};
    const auto global_fixed = best_fixed_action(training, fixed_training_cost);
    (void)fixed_training_cost;
    const double default_terminal_cost = median_terminal_cost(dataset.requests);

    FrozenAttritionAnalysis result;
    result.version = version;
    result.model_byte_identical = model_byte_identical;
    result.development_pairs = development_supported_pairs(dataset);
    for (const auto& transition : result.development_pairs) {
        result.attrition.emplace(transition, TransitionAttrition{});
    }

    std::ofstream plans(output / (version + "-request-plans.tsv"));
    if (!plans) throw std::runtime_error("cannot write frozen request plans");
    plans << "version\tsplit\tmatrix\trequest\tfamily\talternatives"
             "\tunguarded_plan\tcontrol_aware_plan\tplan_changed\n";
    for (const auto& request : dataset.requests) {
        if (request.split != "training") continue;
        ++result.training_requests;
        const auto alternatives = predicted_alternatives(model, request);
        const double terminal_cost = predicted_terminal_cost(
            model, request, default_terminal_cost);
        const auto unguarded = smave::optimize_interaction_aware_calibrated_cascade(
            alternatives, {}, top_k, terminal_cost);
        const auto reconstructed_guarded = control_aware_plan(
            alternatives, unguarded, numeric_family(request), family_anchors,
            global_fixed, terminal_cost);
        const auto exact_guarded = smave::route_sparse_linear_system(
            request.profile,
            conditioned_config(
                model, default_terminal_cost, family_anchors, {}, {}, &global_fixed));
        const auto& guarded = exact_guarded.steps;
        if (step_signature(reconstructed_guarded) != step_signature(guarded)) {
            throw std::runtime_error(
                version + " control-aware plan reconstruction changed");
        }
        result.unguarded_multistep_requests += unguarded.size() > 1 ? 1U : 0U;
        result.final_terminal_abstention_requests += guarded.empty() ? 1U : 0U;
        result.final_single_action_requests += guarded.size() == 1 ? 1U : 0U;
        result.final_multistep_requests += guarded.size() > 1 ? 1U : 0U;
        const bool changed = step_signature(unguarded) != step_signature(guarded);
        result.control_aware_changed_requests += changed ? 1U : 0U;
        plans << version << '\t' << request.split << '\t' << request.matrix << '\t'
              << request.request_id << '\t' << numeric_family(request) << '\t'
              << step_signature(alternatives) << '\t' << step_signature(unguarded)
              << '\t' << step_signature(guarded) << '\t' << (changed ? 1 : 0) << '\n';

        for (std::size_t index = 1; index < unguarded.size(); ++index) {
            const std::pair<ActionSpec, ActionSpec> transition{
                ActionSpec{unguarded[index - 1].expert_version,
                           unguarded[index - 1].budget.work_iterations},
                ActionSpec{unguarded[index].expert_version,
                           unguarded[index].budget.work_iterations}};
            const auto* previous = find_action(request, transition.first);
            if (previous != nullptr && !previous->passed) {
                record_candidate_transition(
                    result.unguarded_candidate_transitions, transition, request.matrix);
            }
        }
        for (std::size_t index = 1; index < guarded.size(); ++index) {
            const std::pair<ActionSpec, ActionSpec> transition{
                ActionSpec{guarded[index - 1].expert_version,
                           guarded[index - 1].budget.work_iterations},
                ActionSpec{guarded[index].expert_version,
                           guarded[index].budget.work_iterations}};
            const auto* previous = find_action(request, transition.first);
            if (previous != nullptr && !previous->passed) {
                record_candidate_transition(
                    result.final_candidate_transitions, transition, request.matrix);
            }
        }

        for (const auto& transition : result.development_pairs) {
            auto& counts = result.attrition.at(transition);
            const FrozenRequestKey key{
                request.split, request.matrix, request.request_id};
            const auto failures = dataset.stable_failures.find(key);
            if (failures != dataset.stable_failures.end() &&
                failures->second.contains(transition.first) &&
                failures->second.contains(transition.second)) {
                counts.isolated_training_matrices.insert(request.matrix);
            }
            const auto alternative_previous = step_index(alternatives, transition.first);
            const auto alternative_next = step_index(alternatives, transition.second);
            counts.modeled_alternative_requests +=
                alternative_previous.has_value() && alternative_next.has_value() ? 1U : 0U;

            const auto unguarded_previous = step_index(unguarded, transition.first);
            const auto unguarded_next = step_index(unguarded, transition.second);
            const bool unguarded_both =
                unguarded_previous.has_value() && unguarded_next.has_value();
            const bool unguarded_ordered = unguarded_both &&
                *unguarded_previous < *unguarded_next;
            const bool unguarded_adjacent = unguarded_ordered &&
                *unguarded_next == *unguarded_previous + 1;
            const auto* unguarded_first = find_action(request, transition.first);
            const bool unguarded_candidate = unguarded_adjacent &&
                unguarded_first != nullptr && !unguarded_first->passed;
            counts.unguarded_both_selected_requests += unguarded_both ? 1U : 0U;
            counts.unguarded_ordered_requests += unguarded_ordered ? 1U : 0U;
            counts.unguarded_adjacent_requests += unguarded_adjacent ? 1U : 0U;
            counts.unguarded_candidate_requests += unguarded_candidate ? 1U : 0U;
            if (unguarded_candidate) {
                counts.unguarded_candidate_matrices.insert(request.matrix);
            }

            const auto guarded_previous = step_index(guarded, transition.first);
            const auto guarded_next = step_index(guarded, transition.second);
            const bool guarded_both = guarded_previous.has_value() && guarded_next.has_value();
            const bool guarded_ordered = guarded_both && *guarded_previous < *guarded_next;
            const bool guarded_adjacent = guarded_ordered &&
                *guarded_next == *guarded_previous + 1;
            const auto* guarded_first = find_action(request, transition.first);
            const bool guarded_candidate = guarded_adjacent &&
                guarded_first != nullptr && !guarded_first->passed;
            counts.final_both_selected_requests += guarded_both ? 1U : 0U;
            counts.final_ordered_requests += guarded_ordered ? 1U : 0U;
            counts.final_adjacent_requests += guarded_adjacent ? 1U : 0U;
            counts.final_candidate_requests += guarded_candidate ? 1U : 0U;
            if (guarded_candidate) counts.final_candidate_matrices.insert(request.matrix);
        }
    }
    for (const auto& transition : result.development_pairs) {
        result.attrition.at(transition).isolated_calibration_matrices =
            isolated_failure_matrices(dataset, transition, "calibration");
    }
    const std::array candidate_stages{
        &result.unguarded_candidate_transitions,
        &result.final_candidate_transitions};
    for (auto* stage_candidates : candidate_stages) {
        for (auto& [transition, counts] : *stage_candidates) {
            counts.isolated_training_matrices =
                isolated_failure_matrices(dataset, transition, "training");
            counts.isolated_calibration_matrices =
                isolated_failure_matrices(dataset, transition, "calibration");
        }
    }
    return result;
}

std::string attrition_class(const TransitionAttrition& counts) {
    if (counts.modeled_alternative_requests == 0) return "not-modeled-alternatives";
    if (counts.unguarded_both_selected_requests == 0) {
        return "eliminated-at-unguarded-top3-selection";
    }
    if (counts.unguarded_ordered_requests == 0) return "not-unguarded-ordered";
    if (counts.unguarded_adjacent_requests == 0) return "not-unguarded-adjacent";
    if (counts.unguarded_candidate_requests == 0) return "unguarded-first-did-not-fail";
    if (counts.final_both_selected_requests == 0) {
        return "removed-by-control-aware-plan";
    }
    if (counts.final_ordered_requests == 0) return "not-final-ordered";
    if (counts.final_adjacent_requests == 0) return "not-final-adjacent";
    if (counts.final_candidate_requests == 0) return "final-first-did-not-fail";
    return "final-candidate";
}

int write_frozen_transition_attrition(
    const std::filesystem::path& v5,
    const std::filesystem::path& v6,
    const std::filesystem::path& contract,
    const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    const auto v5_analysis = analyze_frozen_attrition("v5", v5, output);
    const auto v6_analysis = analyze_frozen_attrition("v6", v6, output);
    if (v5_analysis.development_pairs != v6_analysis.development_pairs) {
        throw std::runtime_error("v5/v6 reconstructed development support changed");
    }
    std::filesystem::copy_file(
        contract, output / contract.filename(),
        std::filesystem::copy_options::overwrite_existing);

    std::ofstream transitions(output / "transition-attrition.tsv");
    if (!transitions) throw std::runtime_error("cannot write transition attrition");
    transitions << "version\ttransition\tisolated_training_matrices"
                   "\tisolated_calibration_matrices"
                   "\tmodeled_alternative_requests\tunguarded_both_selected_requests"
                   "\tunguarded_ordered_requests\tunguarded_adjacent_requests"
                   "\tunguarded_candidate_requests\tunguarded_candidate_matrices"
                   "\tfinal_both_selected_requests\tfinal_ordered_requests"
                   "\tfinal_adjacent_requests\tfinal_candidate_requests"
                   "\tfinal_candidate_matrices\tattrition_class\n";
    const std::array<const FrozenAttritionAnalysis*, 2> analyses{
        &v5_analysis, &v6_analysis};
    for (const auto* analysis : analyses) {
        for (const auto& transition : analysis->development_pairs) {
            const auto& counts = analysis->attrition.at(transition);
            transitions << analysis->version << '\t' << transition.first.key() << "->"
                        << transition.second.key() << '\t'
                        << counts.isolated_training_matrices.size() << '\t'
                        << counts.isolated_calibration_matrices.size() << '\t'
                        << counts.modeled_alternative_requests << '\t'
                        << counts.unguarded_both_selected_requests << '\t'
                        << counts.unguarded_ordered_requests << '\t'
                        << counts.unguarded_adjacent_requests << '\t'
                        << counts.unguarded_candidate_requests << '\t'
                        << counts.unguarded_candidate_matrices.size() << '\t'
                        << counts.final_both_selected_requests << '\t'
                        << counts.final_ordered_requests << '\t'
                        << counts.final_adjacent_requests << '\t'
                        << counts.final_candidate_requests << '\t'
                        << counts.final_candidate_matrices.size() << '\t'
                        << attrition_class(counts) << '\n';
        }
    }

    std::ofstream candidates(output / "candidate-transitions.tsv");
    if (!candidates) throw std::runtime_error("cannot write candidate transitions");
    candidates << "version\tstage\ttransition\tcandidate_requests"
                  "\tcandidate_matrices\tisolated_training_matrices"
                  "\tisolated_calibration_matrices\tdevelopment_supported\n";
    for (const auto* analysis : analyses) {
        const std::array candidate_stages{
            std::pair{
                "unguarded",
                &analysis->unguarded_candidate_transitions},
            std::pair{
                "control-aware-final",
                &analysis->final_candidate_transitions}};
        for (const auto& [stage, stage_candidates] : candidate_stages) {
            for (const auto& [transition, counts] : *stage_candidates) {
                candidates << analysis->version << '\t' << stage << '\t'
                           << transition.first.key() << "->" << transition.second.key()
                           << '\t' << counts.requests << '\t' << counts.matrices.size()
                           << '\t' << counts.isolated_training_matrices.size()
                           << '\t' << counts.isolated_calibration_matrices.size()
                           << '\t'
                           << (analysis->development_pairs.contains(transition) ? 1 : 0)
                           << '\n';
            }
        }
    }

    std::ofstream evidence(output / "evidence.txt");
    if (!evidence) throw std::runtime_error("cannot write attrition evidence");
    evidence << "SMAVE_FROZEN_TRANSITION_ATTRITION_ROUND53 1\n"
             << "analysis_mode=posthoc-frozen-observation-zero-execution-diagnostic\n"
             << "candidate_rule=adjacent-actions-in-control-aware-training-plan-with-first-action-failed\n"
             << "heldout_excluded_from_all_attrition_counts=1\n"
             << "exact_control_aware_route_crosscheck=1\n";
    for (const auto* analysis : analyses) {
        std::size_t unguarded_supported{};
        std::size_t final_supported{};
        std::size_t removed_by_anchor{};
        std::size_t never_unguarded{};
        std::size_t all_candidates_removed_by_anchor{};
        std::size_t development_supported_unguarded_candidates{};
        std::map<std::string, std::size_t> attrition_classes;
        for (const auto& [transition, counts] : analysis->attrition) {
            (void)transition;
            unguarded_supported += counts.unguarded_candidate_requests > 0 ? 1U : 0U;
            final_supported += counts.final_candidate_requests > 0 ? 1U : 0U;
            removed_by_anchor += counts.unguarded_candidate_requests > 0 &&
                counts.final_candidate_requests == 0 ? 1U : 0U;
            never_unguarded += counts.unguarded_candidate_requests == 0 ? 1U : 0U;
            ++attrition_classes[attrition_class(counts)];
        }
        for (const auto& [transition, counts] :
             analysis->unguarded_candidate_transitions) {
            (void)counts;
            all_candidates_removed_by_anchor +=
                analysis->final_candidate_transitions.contains(transition) ? 0U : 1U;
            development_supported_unguarded_candidates +=
                analysis->development_pairs.contains(transition) ? 1U : 0U;
        }
        evidence << analysis->version << ".model_byte_identical="
                 << (analysis->model_byte_identical ? 1 : 0) << '\n'
                 << analysis->version << ".training_requests="
                 << analysis->training_requests << '\n'
                 << analysis->version << ".development_supported_pairs="
                 << analysis->development_pairs.size() << '\n'
                 << analysis->version << ".unguarded_multistep_requests="
                 << analysis->unguarded_multistep_requests << '\n'
                 << analysis->version << ".final_terminal_abstention_requests="
                 << analysis->final_terminal_abstention_requests << '\n'
                 << analysis->version << ".final_single_action_requests="
                 << analysis->final_single_action_requests << '\n'
                 << analysis->version << ".final_multistep_requests="
                 << analysis->final_multistep_requests << '\n'
                 << analysis->version << ".control_aware_changed_requests="
                 << analysis->control_aware_changed_requests << '\n'
                 << analysis->version << ".unguarded_candidate_transition_count="
                 << analysis->unguarded_candidate_transitions.size() << '\n'
                 << analysis->version << ".unguarded_candidate_transition_requests="
                 << std::accumulate(
                        analysis->unguarded_candidate_transitions.begin(),
                        analysis->unguarded_candidate_transitions.end(), std::size_t{},
                        [](std::size_t total, const auto& candidate) {
                            return total + candidate.second.requests;
                        })
                 << '\n'
                 << analysis->version
                 << ".unguarded_candidate_transitions_development_supported="
                 << development_supported_unguarded_candidates << '\n'
                 << analysis->version << ".final_candidate_transition_count="
                 << analysis->final_candidate_transitions.size() << '\n'
                 << analysis->version << ".development_pairs_with_unguarded_candidate="
                 << unguarded_supported << '\n'
                 << analysis->version << ".development_pairs_with_final_candidate="
                 << final_supported << '\n'
                 << analysis->version << ".development_pairs_removed_by_anchor="
                 << removed_by_anchor << '\n'
                 << analysis->version << ".development_pairs_never_unguarded_candidate="
                 << never_unguarded << '\n'
                 << analysis->version
                 << ".development_pairs_eliminated_at_unguarded_top3_selection="
                 << attrition_classes["eliminated-at-unguarded-top3-selection"]
                 << '\n'
                 << analysis->version
                 << ".all_unguarded_candidates_removed_by_control_aware_plan="
                 << all_candidates_removed_by_anchor
                 << '\n';
    }
    evidence << "conditional_timing_inference=0\n"
             << "counterfactual_gain_inference=0\n"
             << "policy_tuning=0\n"
             << "cohort_search=0\n"
             << "solver_execution=0\n"
             << "transition_attrition=transition-attrition.tsv\n"
             << "candidate_transitions=candidate-transitions.tsv\n"
             << "v5_request_plans=v5-request-plans.tsv\n"
             << "v6_request_plans=v6-request-plans.tsv\n"
             << "END\n";
    std::cout << "SMAVE_FROZEN_TRANSITION_ATTRITION_ROUND53 1\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 6 && std::string(argv[1]) == "--frozen-transition-attrition") {
            return write_frozen_transition_attrition(
                argv[2], argv[3], argv[4], argv[5]);
        }
        if (argc != 3 && argc != 4) {
            throw std::invalid_argument(
                "usage: suitesparse-request-conditioned-route-evidence "
                "SUITESPARSE_ROOT OUTPUT_DIRECTORY [DEVELOPMENT_SPLIT_MANIFEST]");
        }
        const std::filesystem::path suite_root = argv[1];
        const std::filesystem::path output = argv[2];
        if (argc == 4) {
            development_mode = true;
            development_split_manifest = argv[3];
            matrix_specs = read_matrix_specs(development_split_manifest);
        }
        std::filesystem::create_directories(output);

        auto requests = build_requests(suite_root);
        std::vector<RawActionSample> raw_samples;
        observe_requests(requests, raw_samples, output / "terminal-attempt-traces.tsv");
        write_observations(output / "action-observations.tsv", raw_samples);
        write_request_summary(output / "request-summary.tsv", requests);
        write_matrix_split(output / "matrix-split.txt");

        std::vector<std::size_t> all_feature_indices(full_feature_names.size());
        std::iota(all_feature_indices.begin(), all_feature_indices.end(), 0U);
        const auto full_model = train_model(raw_samples, requests, all_feature_indices);
        const auto size_model = train_model(
            raw_samples, requests,
            feature_indices({"sparse:log_rows", "sparse:log_nonzeros",
                             "sparse:log_average_row_nonzeros"}));
        const auto rhs_model = train_model(
            raw_samples, requests,
            feature_indices({"sparse:right_hand_side_roughness",
                             "sparse:right_hand_side_sign_change_fraction"}));
        const auto tolerance_model = train_model(
            raw_samples, requests,
            feature_indices({"sparse:requested_digits"}));
        write_model(output / "request-conditioned-model.txt", full_model);
        write_model(output / "size-only-model.txt", size_model);
        write_model(output / "rhs-only-model.txt", rhs_model);
        write_model(output / "tolerance-only-model.txt", tolerance_model);

        const auto training = split_requests(requests, "training");
        const auto training_family_anchors = family_anchor_actions(training);
        double fixed_training_cost{};
        const auto fixed = best_fixed_action(training, fixed_training_cost);
        (void)fixed_training_cost;
        const auto family_anchor_evidence = calibrate_family_anchors(
            requests, training_family_anchors, fixed,
            output / "family-anchor-calibration.tsv");
        const auto& family_anchors = training_family_anchors;
        const double terminal_cost = median_terminal_cost(requests);
        const auto conditional_cost_evidence = collect_conditional_cost_evidence(
            requests, full_model, family_anchors, terminal_cost, &fixed,
            output / "conditional-cost-observations.tsv");
        const auto family_adaptation_evidence = calibrate_family_adaptation(
            requests, full_model, family_anchors, terminal_cost,
            conditional_cost_evidence.calibrations, &fixed,
            output / "family-adaptation-calibration.tsv");
        const auto result = evaluate(
            requests, full_model, size_model, rhs_model, tolerance_model,
            training_family_anchors, family_anchors, fixed,
            conditional_cost_evidence.calibrations,
            family_adaptation_evidence.anchor_only_families,
            output / "production-attempt-traces.tsv",
            output / "heldout-prediction-diagnostics.tsv");
        const auto interactions = interaction_evidence(
            requests, output / "action-interactions.tsv");

        const std::size_t training_requests = request_count(requests, "training");
        const std::size_t calibration_requests = request_count(requests, "calibration");
        const std::size_t heldout_requests = request_count(requests, "heldout");
        const std::size_t unstable = unstable_action_observations(requests);
        std::vector<std::string> contract_failures;
        const auto require_contract = [&](bool condition, std::string name) {
            if (!condition) contract_failures.push_back(std::move(name));
        };
        require_contract(matrix_disjoint(), "matrix-id-disjoint");
        if (development_mode) {
            require_contract(all_split_groups_disjoint(), "all-split-group-disjoint");
            require_contract(training_requests > 0, "training-request-count");
            require_contract(calibration_requests > 0, "calibration-request-count");
            require_contract(heldout_requests > 0, "heldout-request-count");
        } else {
            require_contract(
                final_heldout_group_disjoint(), "final-heldout-group-disjoint");
            require_contract(training_requests == 48, "training-request-count");
            require_contract(calibration_requests == 32, "calibration-request-count");
            require_contract(heldout_requests == 24, "heldout-request-count");
        }
        require_contract(model_action_count(full_model) == 20, "model-action-count");
        require_contract(
            candidate_model_action_count(full_model) == 19,
            "candidate-model-action-count");
        require_contract(
            std::all_of(raw_samples.begin(), raw_samples.end(), [](const auto& sample) {
                return std::isfinite(sample.terminal_reference_wall_us) &&
                    sample.terminal_reference_wall_us > 0.0;
            }),
            "paired-terminal-reference");
        require_contract(
            family_anchor_evidence.specialized_families.size() +
                    family_anchor_evidence.global_fixed_families.size() ==
                family_anchor_evidence.guarded_anchors.size(),
            "family-anchor-accounting");
        require_contract(
            family_anchor_evidence.independent_calibration_groups > 0,
            "family-anchor-calibration-groups");
        bool family_anchor_fallbacks_valid = true;
        for (const auto& family : family_anchor_evidence.global_fixed_families) {
            family_anchor_fallbacks_valid = family_anchor_fallbacks_valid &&
                action_spec(family_anchor_evidence.guarded_anchors.at(family)) == fixed;
        }
        require_contract(
            family_anchor_fallbacks_valid, "family-anchor-global-fixed-fallback");
        require_contract(
            family_adaptation_evidence.adaptive_families +
                    family_adaptation_evidence.anchor_only_families.size() ==
                family_anchors.size(),
            "family-adaptation-accounting");
        require_contract(
            family_adaptation_evidence.independent_calibration_groups > 0,
            "family-adaptation-calibration-groups");
        require_contract(
            conditional_cost_evidence.calibrations.size() <=
                conditional_cost_evidence.selected_transitions,
            "conditional-cost-calibration-count");
        bool conditional_cost_calibrations_valid = true;
        for (const auto& calibration : conditional_cost_evidence.calibrations) {
            conditional_cost_calibrations_valid =
                conditional_cost_calibrations_valid &&
                calibration.independent_training_groups >= 2 &&
                calibration.independent_calibration_groups >= 1 &&
                std::isfinite(calibration.conditional_cost_multiplier) &&
                std::isfinite(calibration.conditional_cost_multiplier_upper) &&
                calibration.conditional_cost_multiplier > 0.0 &&
                calibration.conditional_cost_multiplier_upper >=
                    calibration.conditional_cost_multiplier;
        }
        require_contract(
            conditional_cost_calibrations_valid,
            "conditional-cost-calibration-values");
        require_contract(
            !conditional_cost_evidence.calibrations.empty() ||
                (conditional_cost_evidence.training_groups == 0 &&
                 conditional_cost_evidence.calibration_groups == 0),
            "conditional-cost-empty-calibration-accounting");
        require_contract(
            !conditional_cost_evidence.calibrations.empty() ||
                (result.interaction_plan_changed_requests == 0 &&
                 result.conditioned.regret ==
                    result.conditioned_without_interactions.regret),
            "empty-interaction-degenerate-control");
        require_contract(result.dp_exhaustive_mismatches == 0, "dp-exhaustive");
        require_contract(
            result.production_successes == heldout_requests, "production-successes");
        require_contract(result.production_failures == 0, "production-failures");
        require_contract(
            result.production_gate_mismatches == 0, "production-gate-mismatches");
        require_contract(
            result.production_plan_order_mismatches == 0,
            "production-plan-order-mismatches");
        require_contract(
            result.terminal_only_successes == heldout_requests,
            "terminal-only-successes");
        require_contract(std::isfinite(result.conditioned.regret), "conditioned-regret");
        require_contract(
            std::isfinite(result.raw_conditioned.regret), "raw-conditioned-regret");
        require_contract(
            std::isfinite(result.control_aware_anchor.regret),
            "control-aware-anchor-regret");
        require_contract(std::isfinite(result.static_regret), "static-regret");
        require_contract(std::isfinite(result.fixed_regret), "fixed-regret");
        require_contract(
            std::isfinite(result.training_family_fixed_regret),
            "training-family-fixed-regret");
        require_contract(
            std::isfinite(result.family_fixed_regret), "family-fixed-regret");
        require_contract(
            std::isfinite(result.prediction.cost_maximum_relative_error),
            "cost-prediction-error");
        require_contract(
            std::isfinite(
                result.prediction.selected_cost_maximum_relative_error) &&
                result.prediction.selected_cost_predictions > 0,
            "selected-cost-prediction-error");
        require_contract(
            std::isfinite(result.prediction.anchor_cost_maximum_relative_error) &&
                result.prediction.anchor_cost_predictions > 0,
            "anchor-cost-prediction-error");
        require_contract(
            std::isfinite(interactions.maximum_interaction_delta),
            "action-interaction");
        require_contract(
            std::isfinite(interactions.maximum_order_delta), "action-order");
        if (!contract_failures.empty()) {
            std::ostringstream message;
            message << "SuiteSparse request-conditioned route evidence contract failed:";
            for (const auto& failure : contract_failures) message << ' ' << failure;
            throw std::runtime_error(message.str());
        }

        std::ofstream evidence(output / "evidence.txt");
        if (!evidence) throw std::runtime_error("cannot write evidence");
        std::ostringstream anchor_only_families;
        bool first_anchor_only_family = true;
        for (const auto& family : family_adaptation_evidence.anchor_only_families) {
            if (!first_anchor_only_family) anchor_only_families << ',';
            anchor_only_families << family;
            first_anchor_only_family = false;
        }
        std::ostringstream specialized_anchor_families;
        bool first_specialized_anchor_family = true;
        for (const auto& family : family_anchor_evidence.specialized_families) {
            if (!first_specialized_anchor_family) specialized_anchor_families << ',';
            specialized_anchor_families << family;
            first_specialized_anchor_family = false;
        }
        std::ostringstream global_fixed_anchor_families;
        bool first_global_fixed_anchor_family = true;
        for (const auto& family : family_anchor_evidence.global_fixed_families) {
            if (!first_global_fixed_anchor_family) global_fixed_anchor_families << ',';
            global_fixed_anchor_families << family;
            first_global_fixed_anchor_family = false;
        }
        evidence << std::setprecision(17)
                 << "SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE 1\n"
                 << "contract=" << (development_mode
                    ? "group-disjoint-seen-cohort-robust-routing-development"
                    : "group-disjoint-final-heldout-v6-production-sparse-expert-budget-routing")
                 << "\nsnapshot_date=2026-07-27\n"
                 << "training_matrix_ids=" << matrix_ids("training") << '\n'
                 << "calibration_matrix_ids=" << matrix_ids("calibration") << '\n'
                 << "heldout_matrix_ids=" << matrix_ids("heldout") << '\n'
                 << "heldout_matrix_groups=" << matrix_groups("heldout") << '\n';
        if (development_mode) {
            evidence << "development_only=1\n"
                     << "development_split_manifest="
                     << development_split_manifest.generic_string() << '\n'
                     << "all_split_collection_group_disjoint=1\n";
        } else {
            evidence << "prior_router_development_matrix_ids=nasa4704,G26,rdb450,fs_541_1\n"
                     << "prior_terminal_solver_development_matrix_ids=plbuckle,Si2,rotor2,ex10,laser,Chebyshev2\n"
                     << "prior_final_heldout_v2_development_matrix_ids=Trefethen_700,spaceShuttleEntry_1,M10PI_n1,Chem97ZtZ,c-18,TS\n"
                     << "all_prior_development_heldout_excluded=1\n"
                     << "prior_final_heldout_v4_contract_failed_matrix_ids=s3rmt3m3,fxm3_6,cz5108\n"
                     << "prior_final_heldout_v5_negative_matrix_ids=aft01,rail_5177,hydr1\n"
                     << "all_pre_v6_locked_groups_excluded=1\n"
                     << "matrix_class_source=locked-assets-direct-numeric-audit-and-cholesky-v1\n"
                     << "matrix_split_rule=fixed-training-calibration-plus-prefrozen-group-disjoint-final-heldout-v6\n"
                     << "final_heldout_selection_manifest=benchmark/data-lock/suitesparse-final-heldout-v6-selection.tsv\n"
                     << "final_heldout_selection_sha256=b845e43fd9c35bc4b455906a202d0d93bc341825e03794c97082d892f4e0584f\n"
                     << "final_heldout_payload_manifest=benchmark/data-lock/suitesparse-final-heldout-v6-payload.tsv\n"
                     << "final_heldout_payload_sha256=cf666e8645238635051efc5f226229c87d5bbc530f4801cd2979ef71861f002c\n"
                     << "final_heldout_freeze_manifest=benchmark/data-lock/suitesparse-final-heldout-v6.tsv\n"
                     << "final_heldout_freeze_sha256=87c4772409407ada2bc7b6748360238ef295e5c31168d253cd8e9fa8b23d20ba\n"
                     << "final_heldout_source_index_sha256=9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad\n"
                     << "final_heldout_frozen_before_action_timing=1\n";
        }
        evidence << "training_numeric_classes=" << numeric_class_counts("training") << '\n'
                 << "calibration_numeric_classes=" << numeric_class_counts("calibration") << '\n'
                 << "heldout_numeric_classes=" << numeric_class_counts("heldout") << '\n'
                 << "matrix_id_disjoint=1\n"
                 << "collection_group_disjoint=1\n"
                 << "training_matrix_count=" << matrix_count("training") << '\n'
                 << "calibration_matrix_count=" << matrix_count("calibration") << '\n'
                 << "heldout_matrix_count=" << matrix_count("heldout") << '\n'
                 << "requests_per_matrix=8\n"
                 << "request_kinds=smooth,oscillatory,sparse,random-like\n"
                 << "requested_relative_tolerances=1e-6,1e-10\n"
                 << "training_requests=" << training_requests << '\n'
                 << "calibration_requests=" << calibration_requests << '\n'
                 << "heldout_requests=" << heldout_requests << '\n'
                 << "action_repetitions=" << repetitions << '\n'
                 << "cost_measurement_schedule="
                    "same-request-action-terminal-counterbalanced\n"
                 << "paired_terminal_reference_per_repetition=1\n"
                 << "counterbalanced_forward_reverse_rotation=1\n"
                 << "raw_action_observations=" << raw_samples.size() << '\n'
                 << "model_action_count=" << model_action_count(full_model) << '\n'
                 << "candidate_model_action_count="
                 << candidate_model_action_count(full_model) << '\n'
                 << "request_conditioned_terminal_cost_predictor=1\n"
                 << "calibration_offsets_applied=1\n"
                 << "cost_ridge_regularization=100\n"
                 << "maximum_cost_calibration_multiplier=4\n"
                 << "maximum_pass_logit_calibration_offset=1\n"
                 << "family_anchor_global_specialization_diagnostic=1\n"
                 << "family_anchor_global_diagnostic_split=calibration\n"
                 << "family_anchor_heldout_excluded=1\n"
                 << "family_anchor_global_fixed_action=" << action_name(fixed) << '\n'
                 << "family_anchor_minimum_independent_groups=2\n"
                 << "family_anchor_minimum_gain_fraction="
                 << minimum_family_anchor_gain_fraction << '\n'
                 << "family_anchor_no_calibration_group_regression=1\n"
                 << "family_anchor_equal_group_weighting=1\n"
                 << "family_anchor_independent_calibration_groups="
                 << family_anchor_evidence.independent_calibration_groups << '\n'
                 << "family_anchor_specialized_family_count="
                 << family_anchor_evidence.specialized_families.size() << '\n'
                 << "family_anchor_specialized_families="
                 << specialized_anchor_families.str() << '\n'
                 << "family_anchor_global_fixed_families="
                 << global_fixed_anchor_families.str() << '\n'
                 << "control_aware_family_anchor_gate=1\n"
                 << "control_aware_global_fixed_selection=training\n"
                 << "control_aware_request_comparison=conservative-complete-cost-upper\n"
                 << "control_aware_minimum_gain_fraction="
                 << minimum_family_anchor_gain_fraction << '\n'
                 << "control_aware_severe_support_extrapolation=log(4)\n"
                 << "control_aware_heldout_excluded_from_selection_and_calibration=1\n"
                 << "family_anchor_calibrated_abstention=1\n"
                 << "family_adaptation_calibration_gate=1\n"
                 << "family_adaptation_selection_split=calibration\n"
                 << "family_adaptation_heldout_excluded=1\n"
                 << "family_adaptation_minimum_independent_groups=2\n"
                 << "family_adaptation_minimum_gain_fraction="
                 << minimum_family_anchor_gain_fraction << '\n'
                 << "family_adaptation_no_calibration_group_regression=1\n"
                 << "family_adaptation_equal_group_weighting=1\n"
                 << "family_adaptation_independent_calibration_groups="
                 << family_adaptation_evidence.independent_calibration_groups << '\n'
                 << "family_adaptation_enabled_family_count="
                 << family_adaptation_evidence.adaptive_families << '\n'
                 << "family_adaptation_anchor_only_families="
                 << anchor_only_families.str() << '\n'
                 << "interaction_aware_optimizer=1\n"
                 << "conditional_cost_transition_selection="
                    "training-plan-failed-adjacent-actions\n"
                 << "conditional_cost_heldout_excluded_from_selection_and_calibration=1\n"
                 << "conditional_cost_selected_transition_count="
                 << conditional_cost_evidence.selected_transitions << '\n'
                 << "conditional_cost_candidate_transition_count="
                 << conditional_cost_evidence.candidate_transitions << '\n'
                 << "conditional_cost_maximum_training_transition_groups="
                 << conditional_cost_evidence.maximum_training_transition_groups << '\n'
                 << "conditional_cost_calibration_count="
                 << conditional_cost_evidence.calibrations.size() << '\n'
                 << "conditional_cost_independent_training_groups="
                 << conditional_cost_evidence.training_groups << '\n'
                 << "conditional_cost_independent_calibration_groups="
                 << conditional_cost_evidence.calibration_groups << '\n'
                 << "cost_calibration_upper_quantile=0.95\n"
                 << "pass_calibration_upper_error_contract="
                    "absolute_bias_plus_sqrt(log(40)/(2*n))\n"
                 << "support_domain_contract="
                    "action_specific_normalized_training_feature_min_max\n"
                 << "support_extrapolation_contract="
                    "summed_normalized_distance_outside_action_support\n"
                 << "minimum_family_anchor_gain_fraction=0.05\n"
                 << "features=";
        for (std::size_t index = 0; index < full_feature_names.size(); ++index) {
            if (index != 0) evidence << ',';
            evidence << full_feature_names[index];
        }
        evidence << '\n'
                 << "iterative_budgets=20,50,100,250\n"
                 << "direct_action_budget=0\n"
                 << "top_k=" << top_k << '\n'
                 << "matrix_row_limit=" << matrix_row_limit << '\n'
                 << "built_in_direct_row_limit=" << built_in_direct_row_limit << '\n'
                 << "all_frozen_compatible_actions_executed=1\n"
                 << "unstable_action_observations=" << unstable << '\n'
                 << "cost_prediction_median_relative_error="
                 << result.prediction.cost_median_relative_error << '\n'
                 << "cost_prediction_p95_relative_error="
                 << result.prediction.cost_p95_relative_error << '\n'
                 << "cost_prediction_maximum_relative_error="
                 << result.prediction.cost_maximum_relative_error << '\n'
                 << "selected_cost_prediction_count="
                 << result.prediction.selected_cost_predictions << '\n'
                 << "selected_cost_prediction_median_relative_error="
                 << result.prediction.selected_cost_median_relative_error << '\n'
                 << "selected_cost_prediction_p95_relative_error="
                 << result.prediction.selected_cost_p95_relative_error << '\n'
                 << "selected_cost_prediction_maximum_relative_error="
                 << result.prediction.selected_cost_maximum_relative_error << '\n'
                 << "anchor_cost_prediction_count="
                 << result.prediction.anchor_cost_predictions << '\n'
                 << "anchor_cost_prediction_p95_relative_error="
                 << result.prediction.anchor_cost_p95_relative_error << '\n'
                 << "anchor_cost_prediction_maximum_relative_error="
                 << result.prediction.anchor_cost_maximum_relative_error << '\n'
                 << "pass_prediction_brier_score="
                 << result.prediction.pass_brier_score << '\n'
                 << "pass_prediction_ece=" << result.prediction.pass_ece << '\n'
                 << "pass_prediction_maximum_action_calibration_error="
                 << result.prediction.pass_maximum_action_calibration_error << '\n'
                 << "raw_conditioned_heldout_regret="
                 << result.raw_conditioned.regret << '\n'
                 << "control_aware_anchor_heldout_regret="
                 << result.control_aware_anchor.regret << '\n'
                 << "conditioned_without_interactions_heldout_regret="
                 << result.conditioned_without_interactions.regret << '\n'
                 << "conditioned_heldout_regret=" << result.conditioned.regret << '\n'
                 << "interaction_plan_changed_requests="
                 << result.interaction_plan_changed_requests << '\n'
                 << "independent_plans_with_calibrated_transition="
                 << result.independent_plans_with_calibrated_transition << '\n'
                 << "interaction_plans_with_calibrated_transition="
                 << result.interaction_plans_with_calibrated_transition << '\n'
                 << "static_profile_heldout_regret=" << result.static_regret << '\n'
                 << "fixed_action_heldout_regret=" << result.fixed_regret << '\n'
                 << "fixed_action=" << result.fixed_action << '\n'
                 << "training_family_fixed_action_heldout_regret="
                 << result.training_family_fixed_regret << '\n'
                 << "training_family_fixed_actions="
                 << result.training_family_fixed_actions << '\n'
                 << "family_fixed_action_heldout_regret="
                 << result.family_fixed_regret << '\n'
                 << "family_fixed_actions=" << result.family_fixed_actions << '\n'
                 << "size_only_heldout_regret=" << result.size_only.regret << '\n'
                 << "rhs_only_heldout_regret=" << result.rhs_only.regret << '\n'
                 << "tolerance_only_heldout_regret="
                 << result.tolerance_only.regret << '\n'
                 << "conditioned_median_request_regret="
                 << result.conditioned.median_request_regret << '\n'
                 << "distinct_conditioned_plans="
                 << result.conditioned.distinct_plans << '\n'
                 << "feature_changed_plan_fraction="
                 << result.conditioned.modal_plan_change_fraction << '\n'
                 << "rhs_changed_plan_group_fraction="
                 << result.conditioned.rhs_changed_plan_group_fraction << '\n'
                 << "dp_exhaustive_mismatches="
                 << result.dp_exhaustive_mismatches << '\n'
                 << "production_successes=" << result.production_successes << '\n'
                 << "production_failures=" << result.production_failures << '\n'
                 << "production_fallbacks=" << result.production_fallbacks << '\n'
                 << "production_gate_mismatches="
                 << result.production_gate_mismatches << '\n'
                 << "production_plan_order_mismatches="
                 << result.production_plan_order_mismatches << '\n'
                 << "terminal_only_successes=" << result.terminal_only_successes << '\n'
                 << "action_interaction_eligible_families="
                 << interactions.eligible_families << '\n'
                 << "maximum_action_interaction_delta="
                 << interactions.maximum_interaction_delta << '\n'
                 << "maximum_action_order_delta="
                 << interactions.maximum_order_delta << '\n'
                 << "conditioned_beats_static="
                 << (result.conditioned.regret < result.static_regret ? 1 : 0) << '\n'
                 << "conditioned_beats_fixed="
                 << (result.conditioned.regret < result.fixed_regret ? 1 : 0) << '\n'
                 << "conditioned_beats_family_fixed="
                 << (result.conditioned.regret < result.family_fixed_regret ? 1 : 0)
                 << '\n'
                 << "conditioned_beats_control_aware_anchor="
                 << (result.conditioned.regret < result.control_aware_anchor.regret
                    ? 1 : 0) << '\n'
                 << "conditioned_beats_training_family_fixed="
                 << (result.conditioned.regret < result.training_family_fixed_regret
                    ? 1 : 0) << '\n'
                 << "negative_results_retained=1\n"
                 << "original_equation_gate_recomputed=1\n"
                 << "terminal_numerical_fallback_preserved=1\n"
                 << "action_observation_table=action-observations.tsv\n"
                 << "request_summary=request-summary.tsv\n"
                 << "matrix_split=matrix-split.txt\n"
                 << "request_conditioned_model=request-conditioned-model.txt\n"
                 << "production_attempt_traces=production-attempt-traces.tsv\n"
                 << "heldout_prediction_diagnostics="
                    "heldout-prediction-diagnostics.tsv\n"
                 << "terminal_attempt_traces=terminal-attempt-traces.tsv\n"
                 << "conditional_cost_observations="
                    "conditional-cost-observations.tsv\n"
                 << "family_anchor_calibration=family-anchor-calibration.tsv\n"
                 << "family_adaptation_calibration="
                    "family-adaptation-calibration.tsv\n"
                 << "action_interactions=action-interactions.tsv\n"
                 << "END\n";
        std::cout << "SMAVE_SUITESPARSE_REQUEST_CONDITIONED_ROUTE 1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
