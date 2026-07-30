#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct DaeMultigridArtifact;

inline constexpr const char* kIndexOneDaeSchemaVersion = "SMAVE_INDEX1_DAE_4";
inline constexpr const char* kFullyImplicitDaeSchemaVersion =
    "SMAVE_FULLY_IMPLICIT_DAE_2";

struct DaeVariableIR {
    std::string name;
    double start{0.0};
    double nominal{1.0};
};

struct DaeEquationIR {
    std::string id;
    std::string residual;
    std::vector<std::string> variables;
};

struct DaeResetIR {
    std::string variable;
    std::string expression;
};

struct DaeEventIR {
    std::string id;
    std::string guard;
    int direction{0};
    int priority{0};
    std::size_t source_order{0};
    std::vector<DaeResetIR> resets;
};

struct IndexOneDaeIR {
    std::string schema_version{kIndexOneDaeSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::map<std::string, double> parameters;
    std::vector<DaeVariableIR> states;
    std::vector<DaeVariableIR> algebraics;
    std::vector<std::string> derivatives;
    std::vector<DaeEquationIR> constraints;
    std::vector<DaeEquationIR> initial_constraints;
    std::vector<DaeEventIR> events;
    std::string structural_class{"semi-explicit-index1-candidate"};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static IndexOneDaeIR read(const std::filesystem::path& path);
};

struct FullyImplicitDaeIR {
    std::string schema_version{kFullyImplicitDaeSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::map<std::string, double> parameters;
    std::vector<DaeVariableIR> states;
    std::vector<DaeVariableIR> algebraics;
    std::vector<DaeEquationIR> equations;
    std::vector<DaeEventIR> events;
    std::string structural_class{"fully-implicit-first-order-candidate"};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static FullyImplicitDaeIR read(const std::filesystem::path& path);
};

[[nodiscard]] FullyImplicitDaeIR compile_fully_implicit_dae(
    const std::filesystem::path& source,
    const std::string& top = {});

[[nodiscard]] IndexOneDaeIR compile_index_one_dae(
    const std::filesystem::path& source,
    const std::string& top = {});

struct DaeTolerance {
    double absolute{1.0e-9};
    double relative{1.0e-7};
    double root_time{1.0e-9};
    double guard{1.0e-8};
    double algebraic_rank{1.0e-8};
    int maximum_newton_iterations{16};
};

struct DaeStepRecord {
    double time{0.0};
    double step{0.0};
    int newton_iterations{0};
    double residual_inf{0.0};
};

struct DaeEventRecord {
    std::string id;
    double time{0.0};
    bool grazing{false};
    std::unordered_map<std::string, double> pre_state;
    std::unordered_map<std::string, double> post_state;
    std::unordered_map<std::string, double> pre_algebraics;
    std::unordered_map<std::string, double> post_algebraics;
};

struct DaeRunResult {
    bool success{false};
    double final_time{0.0};
    std::unordered_map<std::string, double> initial_state;
    std::unordered_map<std::string, double> initial_algebraics;
    std::unordered_map<std::string, double> final_state;
    std::unordered_map<std::string, double> final_algebraics;
    std::vector<DaeStepRecord> steps;
    std::vector<DaeEventRecord> initial_events;
    std::vector<DaeEventRecord> events;
    std::size_t grazing_events{0};
    int initialization_iterations{0};
    int initial_event_projection_iterations{0};
    double initialization_residual_inf{0.0};
    double initial_event_projection_residual_inf{0.0};
    double maximum_event_projection_residual_inf{0.0};
    double maximum_guard_residual{0.0};
    double maximum_residual_inf{0.0};
    double minimum_algebraic_rank_margin{1.0};
    std::size_t algebraic_rank_checks{0};
    std::size_t rejected_steps{0};
    std::size_t learned_preconditioned_steps{0};
    std::size_t learned_preconditioned_newton_iterations{0};
    std::size_t learned_krylov_iterations{0};
    std::size_t learned_rejections{0};
    std::size_t dense_step_fallbacks{0};
    std::size_t sparse_newton_steps{0};
    std::size_t sparse_newton_iterations{0};
    std::size_t sparse_krylov_iterations{0};
    std::size_t sparse_jacobian_nonzeros{0};
    std::size_t sparse_jacobian_storage_bytes{0};
    std::size_t sparse_jacobian_colors{0};
    std::size_t sparse_jacobian_evaluation_batches{0};
    std::size_t sparse_jacobian_ad_batches{0};
    std::size_t sparse_jacobian_fd_fallback_batches{0};
    std::string sparse_inner_backend;
    bool sparse_initialization{false};
    std::size_t sparse_initialization_iterations{0};
    std::size_t sparse_initialization_krylov_iterations{0};
    std::size_t sparse_initialization_jacobian_nonzeros{0};
    std::size_t sparse_initialization_jacobian_storage_bytes{0};
    std::size_t sparse_initialization_jacobian_colors{0};
    std::size_t sparse_initialization_jacobian_evaluation_batches{0};
    std::size_t sparse_initialization_jacobian_ad_batches{0};
    std::size_t sparse_initialization_jacobian_fd_fallback_batches{0};
    std::string sparse_initialization_inner_backend;
    std::size_t sparse_event_projections{0};
    std::size_t sparse_event_projection_iterations{0};
    std::size_t sparse_event_projection_krylov_iterations{0};
    std::size_t sparse_event_projection_jacobian_nonzeros{0};
    std::size_t sparse_event_projection_jacobian_storage_bytes{0};
    std::size_t sparse_event_projection_jacobian_colors{0};
    std::size_t sparse_event_projection_jacobian_evaluation_batches{0};
    std::size_t sparse_event_projection_jacobian_ad_batches{0};
    std::size_t sparse_event_projection_jacobian_fd_fallback_batches{0};
    std::string sparse_event_projection_inner_backend;
    std::size_t event_root_solves{0};
    std::size_t common_event_root_solves{0};
    std::size_t sparse_event_root_solves{0};
    std::size_t sparse_event_root_newton_iterations{0};
    std::size_t sparse_event_root_krylov_iterations{0};
    std::size_t sparse_event_root_jacobian_nonzeros{0};
    std::size_t sparse_event_root_jacobian_storage_bytes{0};
    std::size_t sparse_event_root_jacobian_colors{0};
    std::size_t sparse_event_root_jacobian_evaluation_batches{0};
    std::size_t sparse_event_root_jacobian_ad_batches{0};
    std::size_t sparse_event_root_jacobian_fd_fallback_batches{0};
    std::string sparse_event_root_inner_backend;
    std::string dae_preconditioner_version;
    std::string message;
};

struct FullyImplicitDaeRunResult {
    bool success{false};
    std::string plan_id;
    std::string solver_backend;
    std::vector<std::string> backend_chain;
    std::string terminal_fallback;
    double final_time{0.0};
    std::unordered_map<std::string, double> initial_state;
    std::unordered_map<std::string, double> initial_algebraics;
    std::unordered_map<std::string, double> initial_derivatives;
    std::unordered_map<std::string, double> final_state;
    std::unordered_map<std::string, double> final_algebraics;
    std::vector<DaeStepRecord> steps;
    std::vector<DaeEventRecord> initial_events;
    std::vector<DaeEventRecord> events;
    int initialization_iterations{0};
    int initialization_krylov_iterations{0};
    double initialization_residual_inf{0.0};
    bool sparse_initialization{false};
    std::size_t initialization_jacobian_nonzeros{0};
    std::size_t initialization_jacobian_storage_bytes{0};
    std::size_t initialization_jacobian_colors{0};
    std::size_t initialization_jacobian_evaluation_batches{0};
    std::size_t initialization_jacobian_ad_batches{0};
    std::size_t initialization_jacobian_fd_fallback_batches{0};
    std::string initialization_inner_backend;
    std::size_t dense_initialization_fallbacks{0};
    double maximum_residual_inf{0.0};
    std::size_t rejected_steps{0};
    std::size_t sparse_newton_steps{0};
    std::size_t sparse_newton_iterations{0};
    std::size_t sparse_krylov_iterations{0};
    std::size_t sparse_jacobian_nonzeros{0};
    std::size_t sparse_jacobian_storage_bytes{0};
    std::size_t sparse_jacobian_colors{0};
    std::size_t sparse_jacobian_evaluation_batches{0};
    std::size_t sparse_jacobian_ad_batches{0};
    std::size_t sparse_jacobian_fd_fallback_batches{0};
    std::string sparse_inner_backend;
    std::size_t dense_step_fallbacks{0};
    std::size_t learned_preconditioned_steps{0};
    std::size_t learned_preconditioned_newton_iterations{0};
    std::size_t learned_krylov_iterations{0};
    std::size_t learned_rejections{0};
    std::string dae_preconditioner_version;
    std::size_t event_root_solves{0};
    std::size_t event_projection_solves{0};
    std::size_t event_projection_krylov_iterations{0};
    std::size_t dense_event_projection_fallbacks{0};
    double maximum_guard_residual{0.0};
    double maximum_event_projection_residual_inf{0.0};
    std::string message;
};

[[nodiscard]] FullyImplicitDaeRunResult simulate_fully_implicit_dae(
    const FullyImplicitDaeIR& model,
    double end_time,
    double maximum_step,
    DaeTolerance tolerance = {},
    const DaeMultigridArtifact* artifact = nullptr);

[[nodiscard]] std::vector<double> evaluate_fully_implicit_dae_step_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate_state,
    const std::vector<double>& candidate_algebraic,
    double target_time,
    double step);

[[nodiscard]] std::vector<double> evaluate_fully_implicit_dae_initial_residual(
    const FullyImplicitDaeIR& model,
    const std::vector<double>& state,
    const std::vector<double>& derivative,
    const std::vector<double>& algebraic,
    double time = 0.0);

void write_fully_implicit_dae_report(
    const FullyImplicitDaeIR& model,
    const FullyImplicitDaeRunResult& result,
    const std::filesystem::path& path);

[[nodiscard]] DaeRunResult simulate_index_one_dae(
    const IndexOneDaeIR& model,
    double end_time,
    double maximum_step,
    DaeTolerance tolerance = {},
    const DaeMultigridArtifact* artifact = nullptr);

[[nodiscard]] std::vector<std::vector<double>> assemble_dae_step_jacobian(
    const IndexOneDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate_state,
    const std::vector<double>& candidate_algebraic,
    double target_time,
    double step);

[[nodiscard]] std::vector<double> evaluate_dae_step_residual(
    const IndexOneDaeIR& model,
    const std::vector<double>& previous_state,
    const std::vector<double>& candidate_state,
    const std::vector<double>& candidate_algebraic,
    double target_time,
    double step);

void write_dae_report(
    const IndexOneDaeIR& model,
    const DaeRunResult& result,
    const std::filesystem::path& path);

}  // namespace smave
