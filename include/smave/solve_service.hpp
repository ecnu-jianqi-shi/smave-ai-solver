#pragma once

#include "smave/complementarity.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* verified_linear_solve_service_v1 =
    "smave.verified-linear-solve.v1";
inline constexpr const char* verified_nonlinear_solve_service_v1 =
    "smave.verified-nonlinear-solve.v1";
inline constexpr const char* verified_ode_solve_service_v1 =
    "smave.verified-explicit-ode-solve.v1";
inline constexpr const char* verified_dae_solve_service_v1 =
    "smave.verified-fully-implicit-dae-solve.v1";
inline constexpr const char* verified_hybrid_solve_service_v1 =
    "smave.verified-explicit-hybrid-solve.v1";
inline constexpr const char* verified_hybrid_dae_solve_service_v1 =
    "smave.verified-fully-implicit-hybrid-dae-solve.v1";
inline constexpr const char* verified_complementarity_solve_service_v1 =
    "smave.verified-complementarity-solve.v1";
inline constexpr const char* verified_block_graph_solve_service_v1 =
    "smave.verified-block-graph-solve.v1";

enum class VerifiedSolveDiagnosticCode : std::uint32_t {
    success = 0,
    invalid_contract = 1,
    callback_failure = 2,
    numerical_failure = 3,
    original_gate_rejected = 4,
    iteration_limit = 5,
    event_reinit_callback_failure = 6,
    event_reinit_consistency_rejected = 7,
    event_guard_not_released = 8,
    event_reset_conflict = 9,
    cancelled = 10,
};

using CancellationRequestedFunction = std::function<bool()>;
using LinearFallbackFunction = std::function<bool(std::vector<double>&)>;

struct VerifiedLinearSolveOptions {
    double absolute_tolerance{1.0e-12};
    double relative_tolerance{1.0e-10};
    int maximum_work_iterations{1000};
    int restart_dimension{40};
    std::size_t built_in_sparse_direct_row_limit{5000};
    std::optional<RoutingConfig> routing;
    CancellationRequestedFunction cancellation_requested;
    LinearFallbackFunction external_fallback;
};

struct VerifiedLinearSolveAttempt {
    std::string backend;
    int work_iterations{};
    int executed_iterations{};
    std::string status;
    double wall_us{};
    double residual_inf{};
};

struct VerifiedLinearSolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> solution;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double residual_inf{0.0};
    double backward_error{0.0};
    std::vector<VerifiedLinearSolveAttempt> attempts;
};

[[nodiscard]] VerifiedLinearSolveResult verified_linear_solve(
    const LinearSystem& system,
    const VerifiedLinearSolveOptions& options = {});

struct VerifiedComplementaritySolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> solution;
    std::vector<double> gap;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double primal_violation{0.0};
    double dual_violation{0.0};
    double complementarity_violation{0.0};
    double residual_inf{0.0};
    std::size_t attempts{};
};

[[nodiscard]] VerifiedComplementaritySolveResult verified_complementarity_solve(
    const ComplementarityIR& problem,
    const ComplementarityTolerance& tolerance = {});

enum class VerifiedBlockNodeKind : std::uint32_t {
    constant = 1,
    gain = 2,
    sum = 3,
    unit_delay = 4,
    switch_gt = 5,
    switch_ge = 6,
    switch_ne_zero = 7,
    callback = 8,
};

using BlockGraphEvaluateFunction = std::function<bool(
    double time,
    const std::vector<double>& inputs,
    std::vector<double>& outputs,
    double& original_gate_residual)>;

struct VerifiedBlockGraphNode {
    VerifiedBlockNodeKind kind{VerifiedBlockNodeKind::constant};
    double sample_time{0.0};
    double sample_offset{0.0};
    double parameter{0.0};
    double initial_output{0.0};
    std::vector<double> sum_weights;
    std::size_t input_count{};
    std::size_t output_count{1};
    BlockGraphEvaluateFunction evaluate;
    BlockGraphEvaluateFunction fallback;
};

struct VerifiedBlockGraphConnection {
    std::size_t source_node{};
    std::size_t source_port{};
    std::size_t target_node{};
    std::size_t target_port{};
};

struct VerifiedBlockGraphProblem {
    std::vector<VerifiedBlockGraphNode> nodes;
    std::vector<VerifiedBlockGraphConnection> connections;
    double end_time{0.0};
    double base_step{1.0};
};

struct VerifiedBlockGraphSolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> outputs;
    std::vector<std::size_t> output_offsets;
    std::vector<std::size_t> commit_order;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double final_time{0.0};
    double maximum_connection_error{0.0};
    double maximum_original_gate_residual{0.0};
    double maximum_fixed_point_residual{0.0};
    std::size_t ticks{};
    std::size_t node_executions{};
    std::size_t fallback_count{};
    std::size_t fixed_point_components{};
    std::size_t fixed_point_iterations{};
};

struct VerifiedBlockGraphSolveOptions {
    double absolute_tolerance{1.0e-12};
    double relative_tolerance{1.0e-10};
    int maximum_fixed_point_iterations{1000};
    CancellationRequestedFunction cancellation_requested;
};

[[nodiscard]] VerifiedBlockGraphSolveResult verified_block_graph_solve(
    const VerifiedBlockGraphProblem& problem,
    const VerifiedBlockGraphSolveOptions& options = {});

using NonlinearResidualFunction = std::function<bool(
    const std::vector<double>& state,
    std::vector<double>& residual)>;
using NonlinearJacobianFunction = std::function<bool(
    const std::vector<double>& state,
    std::vector<std::vector<double>>& jacobian)>;
using NonlinearFallbackFunction = std::function<bool(std::vector<double>&)>;

struct VerifiedNonlinearSolveProblem {
    std::vector<double> initial_state;
    NonlinearResidualFunction residual;
    NonlinearJacobianFunction jacobian;
};

struct VerifiedNonlinearSolveOptions {
    double absolute_tolerance{1.0e-12};
    double relative_tolerance{1.0e-10};
    int maximum_iterations{1000};
    CancellationRequestedFunction cancellation_requested;
    NonlinearFallbackFunction external_fallback;
};

struct VerifiedNonlinearSolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> solution;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double residual_inf{0.0};
    double backward_error{0.0};
};

[[nodiscard]] VerifiedNonlinearSolveResult verified_nonlinear_solve(
    const VerifiedNonlinearSolveProblem& problem,
    const VerifiedNonlinearSolveOptions& options = {});

using OdeRhsFunction = std::function<bool(
    double time,
    const std::vector<double>& state,
    std::vector<double>& derivative)>;
using OdeDenseStepFallbackFunction = std::function<bool(
    double from_time,
    const std::vector<double>& previous_state,
    double to_time,
    std::vector<double>& quarter_state,
    std::vector<double>& midpoint_state,
    std::vector<double>& three_quarter_state,
    std::vector<double>& next_state)>;

struct VerifiedOdeEvent {
    using GuardFunction = std::function<bool(
        double time,
        const std::vector<double>& state,
        double& guard)>;
    using ResetFunction = std::function<bool(
        double time,
        const std::vector<double>& pre_state,
        std::vector<double>& post_state)>;
    int direction{};
    int priority{};
    GuardFunction guard;
    ResetFunction reset;
};

using OdeEventClusterResetFunction = std::function<bool(
    double time,
    const std::vector<std::size_t>& initial_events,
    const std::vector<double>& pre_state,
    std::vector<double>& post_state,
    std::size_t& committed_events,
    VerifiedSolveDiagnosticCode& failure_code)>;

struct VerifiedOdeSolveProblem {
    std::vector<double> initial_state;
    OdeRhsFunction right_hand_side;
    std::vector<VerifiedOdeEvent> events;
    OdeEventClusterResetFunction event_cluster_reset;
};

struct VerifiedOdeSolveOptions {
    double start_time{0.0};
    double end_time{1.0};
    double maximum_step{0.01};
    double absolute_tolerance{1.0e-9};
    double relative_tolerance{1.0e-7};
    int maximum_steps{100000};
    CancellationRequestedFunction cancellation_requested;
    OdeDenseStepFallbackFunction external_step_fallback;
};

struct VerifiedOdeSolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> solution;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double final_time{0.0};
    double maximum_scaled_local_error{0.0};
    std::size_t accepted_steps{};
    std::size_t rejected_steps{};
    std::size_t event_count{};
    double last_event_time{0.0};
};

[[nodiscard]] VerifiedOdeSolveResult verified_ode_solve(
    const VerifiedOdeSolveProblem& problem,
    const VerifiedOdeSolveOptions& options = {});

struct VerifiedHybridMode {
    OdeRhsFunction right_hand_side;
};

struct VerifiedHybridTransition {
    using StableResetFunction = std::function<bool(
        double time,
        const std::vector<double>& stable_pre_state,
        const std::vector<double>& current_state,
        std::vector<double>& proposed_state)>;
    std::size_t source_mode{};
    std::size_t target_mode{};
    int direction{};
    int priority{};
    VerifiedOdeEvent::GuardFunction guard;
    VerifiedOdeEvent::ResetFunction reset;
    StableResetFunction stable_reset;
    std::vector<std::uint8_t> write_mask;
};

struct VerifiedHybridSolveProblem {
    std::vector<double> initial_state;
    std::size_t initial_mode{};
    std::vector<VerifiedHybridMode> modes;
    std::vector<VerifiedHybridTransition> transitions;
};

struct VerifiedHybridSolveResult : VerifiedOdeSolveResult {
    std::size_t final_mode{};
};

[[nodiscard]] VerifiedHybridSolveResult verified_hybrid_solve(
    const VerifiedHybridSolveProblem& problem,
    const VerifiedOdeSolveOptions& options = {});

using DaeResidualFunction = std::function<bool(
    double time,
    const std::vector<double>& state,
    const std::vector<double>& derivative,
    std::vector<double>& residual)>;
using DaeJacobianFunction = std::function<bool(
    double time,
    const std::vector<double>& state,
    const std::vector<double>& derivative,
    double derivative_scale,
    std::vector<std::vector<double>>& jacobian)>;

struct VerifiedDaeEvent {
    using GuardFunction = std::function<bool(
        double time,
        const std::vector<double>& state,
        const std::vector<double>& derivative,
        double& guard)>;
    using ResetFunction = std::function<bool(
        double time,
        const std::vector<double>& pre_state,
        const std::vector<double>& pre_derivative,
        std::vector<double>& post_state,
        std::vector<double>& post_derivative)>;
    int direction{};
    int priority{};
    GuardFunction guard;
    ResetFunction reset;
};

using DaeEventClusterResetFunction = std::function<bool(
    double time,
    const std::vector<std::size_t>& initial_events,
    const std::vector<double>& pre_state,
    const std::vector<double>& pre_derivative,
    std::vector<double>& post_state,
    std::vector<double>& post_derivative,
    std::size_t& committed_events,
    VerifiedSolveDiagnosticCode& failure_code)>;
using DaeStepFallbackFunction = std::function<bool(
    double from_time,
    const std::vector<double>& previous_state,
    const std::vector<double>& previous_derivative,
    double to_time,
    std::vector<double>& next_state,
    std::vector<double>& next_derivative)>;

struct VerifiedDaeSolveProblem {
    std::vector<std::uint8_t> differential_mask;
    std::vector<double> initial_state;
    std::vector<double> initial_derivative;
    DaeResidualFunction residual;
    DaeJacobianFunction jacobian;
    std::vector<VerifiedDaeEvent> events;
    DaeEventClusterResetFunction event_cluster_reset;
};

struct VerifiedDaeSolveOptions {
    double start_time{0.0};
    double end_time{1.0};
    double maximum_step{0.01};
    double absolute_tolerance{1.0e-9};
    double relative_tolerance{1.0e-7};
    int maximum_iterations{100000};
    int maximum_newton_iterations{100};
    CancellationRequestedFunction cancellation_requested;
    DaeStepFallbackFunction external_step_fallback;
};

struct VerifiedDaeSolveResult {
    bool success{false};
    bool used_fallback{false};
    std::vector<double> solution;
    std::string backend;
    std::string equation_family;
    std::string plan_id;
    std::string diagnostic;
    VerifiedSolveDiagnosticCode diagnostic_code{
        VerifiedSolveDiagnosticCode::invalid_contract};
    double final_time{0.0};
    double maximum_residual_inf{0.0};
    std::size_t accepted_steps{};
    std::size_t rejected_steps{};
    std::size_t event_count{};
    double last_event_time{0.0};
    std::size_t differentiation_index{1};
    std::size_t hidden_rank_checks{};
    double minimum_hidden_rank_margin{1.0};
    double maximum_hidden_residual_inf{0.0};
};

[[nodiscard]] VerifiedDaeSolveResult verified_dae_solve(
    const VerifiedDaeSolveProblem& problem,
    const VerifiedDaeSolveOptions& options = {});

struct VerifiedHybridDaeMode {
    DaeResidualFunction residual;
    DaeJacobianFunction jacobian;
};

struct VerifiedHybridDaeTransition {
    using StableResetFunction = std::function<bool(
        double time,
        const std::vector<double>& stable_pre_state,
        const std::vector<double>& stable_pre_derivative,
        const std::vector<double>& current_state,
        const std::vector<double>& current_derivative,
        std::vector<double>& proposed_state,
        std::vector<double>& proposed_derivative)>;
    std::size_t source_mode{};
    std::size_t target_mode{};
    int direction{};
    int priority{};
    VerifiedDaeEvent::GuardFunction guard;
    VerifiedDaeEvent::ResetFunction reset;
    StableResetFunction stable_reset;
    std::vector<std::uint8_t> state_write_mask;
    std::vector<std::uint8_t> derivative_write_mask;
};

struct VerifiedHybridDaeSolveProblem {
    std::vector<std::uint8_t> differential_mask;
    std::vector<double> initial_state;
    std::vector<double> initial_derivative;
    std::size_t initial_mode{};
    std::vector<VerifiedHybridDaeMode> modes;
    std::vector<VerifiedHybridDaeTransition> transitions;
};

struct VerifiedHybridDaeSolveResult : VerifiedDaeSolveResult {
    std::size_t final_mode{};
    std::size_t consistency_projection_count{};
};

[[nodiscard]] VerifiedHybridDaeSolveResult verified_hybrid_dae_solve(
    const VerifiedHybridDaeSolveProblem& problem,
    const VerifiedDaeSolveOptions& options = {});

}
