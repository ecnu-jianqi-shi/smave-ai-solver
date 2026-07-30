#pragma once

#include "smave/expression.hpp"
#include "smave/routing.hpp"
#include "smave/ir.hpp"
#include "smave/linear.hpp"
#include "smave/residency.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

enum class GateDecision { reject, need_correction, direct_accept };
enum class SolvePath { direct_accept, corrected_accept, warm_start_accept, full_fallback };

struct Tolerance {
    double absolute{1.0e-10};
    double relative{1.0e-8};
    double qoi_relative{1.0e-4};
};

struct GateResult {
    GateDecision decision{GateDecision::reject};
    std::vector<double> scaled_residuals;
    double residual_inf{};
    std::string reason;
};

struct TimingBreakdown {
    double routing_us{};
    double expert_us{};
    double gate_us{};
    double correction_us{};
    double fallback_us{};
    double total_us{};
};

struct ExpertAttemptRecord {
    std::string expert_version;
    std::string outcome;
    std::string reason;
    double estimated_cost_us{-1.0};
    int iterations{0};
    double residual_inf{0.0};
};

struct ExpertResidencyRecord {
    std::string expert_version;
    std::string device{"cpu"};
    std::string outcome;
    std::string reason;
    std::size_t expert_bytes{};
    std::size_t resident_bytes{};
    std::uint64_t invocation_heat{};
    std::vector<std::string> evicted_experts;
};

struct BlockOutcome {
    std::string block_id;
    std::string plan_id;
    SolvePath path{SolvePath::full_fallback};
    std::unordered_map<std::string, double> solution;
    GateResult gate;
    int expert_iterations{};
    int fallback_iterations{};
    int krylov_iterations{};
    double krylov_initial_residual{};
    double krylov_final_residual{};
    double condition_estimate{};
    bool linear_spd{false};
    bool krylov_breakdown{false};
    bool krylov_stagnated{false};
    bool jacobian_preconditioner{false};
    std::string preconditioner_version;
    std::string inner_linear_backend;
    std::size_t inner_jacobian_nonzeros{};
    std::size_t inner_jacobian_storage_bytes{};
    std::size_t inner_jacobian_colors{};
    std::size_t inner_jacobian_evaluation_batches{};
    std::size_t inner_jacobian_ad_batches{};
    std::size_t inner_jacobian_fd_fallback_batches{};
    bool inner_matrix_free{};
    std::size_t inner_operator_applications{};
    std::size_t inner_operator_ad_applications{};
    std::size_t inner_operator_fd_fallback_applications{};
    std::size_t inner_preconditioner_storage_bytes{};
    std::size_t inner_preconditioner_setup_entries{};
    std::size_t inner_preconditioner_ad_entries{};
    std::size_t inner_preconditioner_fd_fallback_entries{};
    std::size_t inner_preconditioner_identity_entries{};
    std::vector<std::string> attempted_experts;
    std::vector<double> estimated_costs_us;
    std::vector<ExpertAttemptRecord> attempt_records;
    std::vector<ExpertResidencyRecord> residency_records;
    TimingBreakdown timing;
};

struct SolveOutcome {
    bool success{false};
    std::unordered_map<std::string, double> values;
    std::vector<BlockOutcome> blocks;
    std::string trace_id;
    std::string message;
    TimingBreakdown timing;
    std::size_t direct_count{};
    std::size_t corrected_count{};
    std::size_t warm_start_count{};
    std::size_t fallback_count{};
    std::size_t residency_load_count{};
    std::size_t residency_hit_count{};
    std::size_t residency_rejection_count{};
    std::size_t residency_eviction_count{};
    std::size_t resident_expert_bytes{};
};

class Runtime;

class SolveGatePolicy {
public:
    virtual ~SolveGatePolicy() = default;
    [[nodiscard]] virtual GateResult evaluate(
        const Runtime& runtime,
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) = 0;
};

class Runtime {
public:
    explicit Runtime(
        ModelIR model,
        Tolerance tolerance = {},
        RoutingConfig routing = {},
        ResidencyConfig residency = {},
        std::shared_ptr<SolveGatePolicy> solve_gate_policy = {});
    Runtime(
        ModelIR model,
        Registry registry,
        RuntimeBundle bundle,
        Tolerance tolerance = {},
        RoutingConfig routing = {},
        ResidencyConfig residency = {},
        std::shared_ptr<SolveGatePolicy> solve_gate_policy = {});

    [[nodiscard]] SolveOutcome solve(
        const std::unordered_map<std::string, double>& context,
        const std::filesystem::path& trace_directory = ".smave/traces") const;

    [[nodiscard]] GateResult evaluate_gate(
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) const;
    [[nodiscard]] GateResult evaluate_gate_reference(
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) const;
    [[nodiscard]] GateResult evaluate_gate_fused(
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) const;
    [[nodiscard]] GateResult evaluate_gate_with_residuals(
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        const std::vector<double>& residuals,
        bool direct_permission) const;
    [[nodiscard]] GateResult evaluate_solve_gate(
        const BlockIR& block,
        const std::unordered_map<std::string, double>& values,
        bool direct_permission) const;
    [[nodiscard]] std::shared_ptr<const void> solve_gate_identity() const noexcept;
    [[nodiscard]] std::vector<GateResult> evaluate_gate_batch(
        const BlockIR& block,
        const std::vector<std::unordered_map<std::string, double>>& values,
        bool direct_permission) const;

    [[nodiscard]] SolveOutcome correct_candidate(
        const std::unordered_map<std::string, double>& context,
        const std::string& block_id,
        const std::unordered_map<std::string, double>& candidate,
        const std::string& expert_version,
        const std::filesystem::path& trace_directory = ".smave/candidate-traces",
        int maximum_iterations = 8) const;

    [[nodiscard]] SolveOutcome commit_corrected_candidate(
        const std::unordered_map<std::string, double>& context,
        const std::string& block_id,
        const std::unordered_map<std::string, double>& candidate,
        const std::string& expert_version,
        const std::filesystem::path& trace_directory =
            ".smave/corrected-candidate-traces") const;

private:
    struct GateEquationPlan {
        const Expression* residual{};
        std::vector<std::size_t> variable_indices;
    };
    struct GateBlockPlan {
        std::vector<std::size_t> unknown_variable_indices;
        std::vector<GateEquationPlan> equations;
    };
    ModelIR model_;
    Registry registry_;
    RuntimeBundle bundle_;
    Tolerance tolerance_;
    CompileRouter compile_router_;
    RuntimeRouter runtime_router_;
    std::shared_ptr<ExpertResidencyManager> residency_;
    std::shared_ptr<SolveGatePolicy> solve_gate_policy_;
    std::shared_ptr<const void> solve_gate_identity_;
    std::unordered_map<std::string, Expression> residuals_;
    std::unordered_map<std::string, std::size_t> variable_indices_by_name_;
    std::unordered_map<std::string, std::size_t> equation_indices_by_id_;
    std::unordered_map<std::string, LinearSystem> constant_linear_systems_;
    std::unordered_map<std::string, GateBlockPlan> gate_plans_;
};

[[nodiscard]] std::unordered_map<std::string, double> read_scenario(
    const std::filesystem::path& path);
[[nodiscard]] std::string to_string(GateDecision decision);
[[nodiscard]] std::string to_string(SolvePath path);

}  // namespace smave
