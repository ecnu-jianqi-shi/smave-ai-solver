#pragma once

#include "smave/expert.hpp"
#include "smave/dae.hpp"
#include "smave/ir.hpp"

#include <string>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace smave {

struct ComplementarityIR;
struct IndexTwoDaeIR;

struct EquationAssessment {
    std::string equation_family;
    std::size_t unknown_count{};
    std::size_t equation_count{};
    std::size_t structural_nonzeros{};
    double structural_density{};
    std::string scale_class;
    std::size_t estimated_dense_bytes{};
    std::size_t estimated_sparse_bytes{};
    bool dense_direct_eligible{false};
    bool linear{false};
    bool smooth{false};
    bool event_related{false};
    bool structurally_square{false};
    bool structurally_symmetric{false};
    bool runtime_positive_definite_check_required{false};
    bool numeric_probe_available{false};
    bool numerically_symmetric{false};
    bool numerically_positive_definite{false};
    double diagonal_condition_estimate{};
    std::vector<BackendRole> admissible_backend_roles;
    std::vector<BackendRole> forbidden_backend_roles;
    std::string mandatory_fallback{"original-damped-newton"};
    std::vector<std::string> reasons;
};

struct RouteBudgetCalibration {
    int work_iterations{};
    std::size_t attempts{};
    std::size_t passes{};
    std::size_t fallbacks{};
    std::size_t failures{};
    std::size_t erroneous_accepts{};
    double pass_probability{};
    double calibration_error{};
    double median_attempt_wall_us{};
};

struct RouteFamilyPrior {
    std::string routing_family;
    std::size_t independent_training_groups{};
    std::size_t independent_calibration_groups{};
    double pooled_log_cost{};
    double pooled_pass_probability{};
    double cost_regression_weight{1.0};
    double pass_regression_weight{1.0};
    double cost_calibration_upper_error{};
    double pass_calibration_upper_error{};
};

struct RouteCalibration {
    std::size_t attempts{};
    std::size_t passes{};
    std::size_t fallbacks{};
    std::size_t failures{};
    double pass_probability{};
    double calibration_error{};
    double median_wall_us{};
    int work_iterations{8};
    std::vector<RouteBudgetCalibration> budget_options;
};

struct RouteActionPredictor {
    int work_iterations{};
    std::size_t training_samples{};
    std::size_t independent_training_groups{};
    std::size_t independent_calibration_groups{};
    bool cost_relative_to_terminal{};
    std::vector<double> log_cost_coefficients;
    std::vector<double> pass_logit_coefficients;
    double log_cost_calibration_offset{};
    double pass_logit_calibration_offset{};
    double cost_calibration_error{};
    double pass_calibration_error{};
    double cost_calibration_upper_error{};
    double pass_calibration_upper_error{};
    std::vector<double> support_feature_minimums;
    std::vector<double> support_feature_maximums;
    std::vector<std::size_t> joint_support_feature_indices;
    std::vector<std::vector<double>> joint_support_group_centers;
    double joint_support_nearest_distance_upper{};
    std::vector<RouteFamilyPrior> family_priors;
};

struct RequestConditionedRoutingModel {
    std::vector<std::string> feature_names;
    std::vector<double> feature_means;
    std::vector<double> feature_scales;
    std::map<std::string, std::vector<RouteActionPredictor>> actions;
};

struct RouteActionTrainingSample {
    std::string expert_version;
    int work_iterations{};
    std::string independent_group;
    std::string routing_family;
    std::vector<double> features;
    double attempt_wall_us{};
    double terminal_reference_wall_us{};
    bool cost_relative_to_terminal{};
    bool passed{false};
};

struct RouteActionPrediction {
    double attempt_wall_us{};
    double pass_probability{};
    double risk_score{};
    double cost_relative_uncertainty{};
    double pass_probability_uncertainty{};
    double support_extrapolation{};
};

struct RouteActionReference {
    std::string expert_version;
    int work_iterations{};
};

struct RouteConditionalCostCalibration {
    RouteActionReference previous;
    RouteActionReference next;
    std::size_t independent_training_groups{};
    std::size_t independent_calibration_groups{};
    double conditional_cost_multiplier{1.0};
    double conditional_cost_multiplier_upper{1.0};
};

[[nodiscard]] std::vector<double> extract_routing_features(
    const std::vector<std::string>& feature_names,
    const BlockIR& block,
    const BlockContext& context);
[[nodiscard]] RequestConditionedRoutingModel train_request_conditioned_routing_model(
    const std::vector<std::string>& feature_names,
    const std::vector<RouteActionTrainingSample>& training_samples,
    const std::vector<RouteActionTrainingSample>& calibration_samples,
    double cost_ridge_regularization = 1.0e-6,
    double pass_logistic_regularization = 1.0e-3,
    std::size_t maximum_logistic_iterations = 64,
    double maximum_absolute_log_cost_calibration_offset = 40.0,
    double maximum_absolute_pass_logit_calibration_offset = 40.0);
[[nodiscard]] RouteActionPrediction predict_request_conditioned_action(
    const RequestConditionedRoutingModel& model,
    const std::string& expert_version,
    int work_iterations,
    const std::vector<double>& features,
    double terminal_reference_wall_us = 0.0,
    const std::string& routing_family = {});

struct RoutingConfig {
    std::size_t top_k{4};
    double minimum_pass_probability{0.0};
    double risk_weight{1000.0};
    bool require_original_fallback{true};
    std::set<std::string> expert_allowlist;
    std::string calibration_block_fingerprint;
    std::string calibration_winner;
    std::map<std::string, RouteCalibration> calibrations;
    std::optional<RequestConditionedRoutingModel> request_conditioned_model;
    std::map<std::string, RouteActionReference> request_conditioned_family_anchors;
    std::optional<RouteActionReference> request_conditioned_global_fixed_anchor;
    std::set<std::string> request_conditioned_anchor_only_families;
    std::vector<RouteConditionalCostCalibration> conditional_cost_calibrations;
    double minimum_family_anchor_gain_fraction{};
    double calibrated_terminal_fallback_cost_us{};
    std::size_t maximum_joint_states{1000000};
};

struct CandidateExpert {
    std::string expert_version;
    Permission permission{Permission::shadow};
    BackendRole backend_role{BackendRole::nonlinear_solver};
    std::string selection_reason;
    bool builtin{false};
};

struct SolveStep {
    std::string expert_version;
    Permission permission{Permission::shadow};
    SolveBudget budget;
    double estimated_cost_us{};
    double pass_probability{};
    double risk_score{};
    double cost_relative_uncertainty{};
    double pass_probability_uncertainty{};
    double support_extrapolation{};
    BackendRole backend_role{BackendRole::nonlinear_solver};
    std::vector<std::string> backend_chain;
    std::string selection_reason;
    bool builtin{false};
};

struct SolvePlan {
    std::string plan_id;
    std::string block_fingerprint;
    EquationAssessment assessment;
    std::vector<SolveStep> steps;
    std::string terminal_fallback;
};

struct CascadeOptimizationDiagnostics {
    std::size_t estimated_states{};
    std::size_t visited_states{};
    std::size_t memo_hits{};
    std::size_t recursive_transitions{};
    std::size_t terminal_states{};
    bool state_limit_exceeded{false};
};

[[nodiscard]] double cascade_ordering_index(const SolveStep& step);
[[nodiscard]] double expected_cascade_cost(
    const std::vector<SolveStep>& steps, double terminal_cost_us);
[[nodiscard]] double expected_interaction_aware_cascade_cost(
    const std::vector<SolveStep>& steps,
    double terminal_cost_us,
    const std::vector<RouteConditionalCostCalibration>& conditional_cost_calibrations);
void order_cascade_steps(std::vector<SolveStep>& steps);
[[nodiscard]] std::vector<SolveStep> optimize_joint_calibrated_cascade(
    const std::vector<SolveStep>& alternatives,
    std::size_t top_k,
    double terminal_cost_us,
    std::size_t maximum_states = 1000000,
    CascadeOptimizationDiagnostics* diagnostics = nullptr);
[[nodiscard]] std::vector<SolveStep> optimize_interaction_aware_calibrated_cascade(
    const std::vector<SolveStep>& alternatives,
    const std::vector<RouteConditionalCostCalibration>& conditional_cost_calibrations,
    std::size_t top_k,
    double terminal_cost_us,
    std::size_t maximum_states = 1000000,
    CascadeOptimizationDiagnostics* diagnostics = nullptr);

struct SparseLinearProfile {
    std::string fingerprint;
    std::size_t rows{};
    std::size_t columns{};
    std::size_t nonzeros{};
    bool structurally_symmetric{false};
    bool numerically_symmetric{false};
    bool numerically_positive_definite{false};
    double diagonal_condition_estimate{};
    double coefficient_dynamic_range{1.0};
    double row_nonzero_coefficient_of_variation{};
    double row_l1_condition_estimate{1.0};
    double diagonal_dominance_fraction{};
    double mean_diagonal_row_l1_fraction{};
    double normalized_mean_bandwidth{};
    bool regular_grid{false};
    std::size_t grid_dimension{};
    std::size_t batch_size{1};
    std::size_t expected_reuses{1};
    bool apple_accelerate_available{false};
    bool metal_available{false};
    bool learned_expert_available{false};
    bool learned_expert_resident{false};
    std::string structured_direct_backend;
    bool dense_direct_available{false};
    double right_hand_side_inf{};
    double right_hand_side_roughness{};
    double right_hand_side_sign_change_fraction{};
    double absolute_tolerance{1.0e-12};
    double relative_tolerance{1.0e-10};
    int maximum_work_iterations{1000};
    int restart_dimension{40};
};

[[nodiscard]] std::vector<double> extract_sparse_routing_features(
    const std::vector<std::string>& feature_names,
    const SparseLinearProfile& profile);

struct NonlinearAlgebraicProfile {
    std::string fingerprint;
    std::size_t dimension{};
    std::size_t jacobian_nonzeros{};
    bool jacobian_available{false};
    bool smooth{true};
};

struct ExplicitOdeProfile {
    std::string fingerprint;
    std::size_t state_dimension{};
    bool smooth{true};
    bool events{false};
};

[[nodiscard]] EquationAssessment assess_equation(const BlockIR& block);
[[nodiscard]] EquationAssessment assess_equation(const SparseLinearProfile& profile);
[[nodiscard]] SolvePlan route_sparse_linear_system(
    const SparseLinearProfile& profile,
    const RoutingConfig& routing = {});
[[nodiscard]] EquationAssessment assess_equation(
    const NonlinearAlgebraicProfile& profile);
[[nodiscard]] SolvePlan route_nonlinear_algebraic_system(
    const NonlinearAlgebraicProfile& profile,
    const RoutingConfig& routing = {});
[[nodiscard]] EquationAssessment assess_equation(const ExplicitOdeProfile& profile);
[[nodiscard]] SolvePlan route_explicit_ode(
    const ExplicitOdeProfile& profile,
    const RoutingConfig& routing = {});
[[nodiscard]] EquationAssessment assess_equation(const FullyImplicitDaeIR& model);
[[nodiscard]] EquationAssessment assess_equation(const ComplementarityIR& model);
[[nodiscard]] EquationAssessment assess_equation(const IndexTwoDaeIR& model);
[[nodiscard]] SolvePlan route_fully_implicit_dae(
    const FullyImplicitDaeIR& model,
    const RoutingConfig& routing = {},
    const DaeMultigridArtifact* artifact = nullptr);
[[nodiscard]] SolvePlan route_complementarity(
    const ComplementarityIR& model,
    const RoutingConfig& routing = {});
[[nodiscard]] SolvePlan route_index_two_dae(
    const IndexTwoDaeIR& model,
    const RoutingConfig& routing = {});
[[nodiscard]] std::string to_string(BackendRole role);
void write_equation_assessment_report(
    const ModelIR& model,
    const BlockIR& block,
    const BlockContext& context,
    const Registry& registry,
    const RuntimeBundle& bundle,
    const RoutingConfig& routing,
    const std::filesystem::path& path);
void write_equation_assessment_report(
    const FullyImplicitDaeIR& model,
    const RoutingConfig& routing,
    const std::filesystem::path& path,
    const DaeMultigridArtifact* artifact = nullptr);

class CompileRouter {
public:
    [[nodiscard]] std::vector<CandidateExpert> lookup(
        const BlockIR& block,
        const Registry& registry,
        const RuntimeBundle& bundle) const;
};

class RuntimeRouter {
public:
    explicit RuntimeRouter(RoutingConfig config = {});
    [[nodiscard]] SolvePlan route(
        const BlockIR& block,
        const BlockContext& context,
        const std::vector<CandidateExpert>& candidates,
        const Registry& registry,
        const RuntimeBundle& bundle) const;

private:
    RoutingConfig config_;
};

}  // namespace smave
