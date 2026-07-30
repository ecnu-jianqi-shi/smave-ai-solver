#pragma once

#include "smave/dae.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

inline constexpr const char* kIndexTwoDaeSchemaVersion = "SMAVE_INDEX2_DAE_1";

struct IndexTwoDaeIR {
    std::string schema_version{kIndexTwoDaeSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::vector<DaeVariableIR> states;
    std::vector<DaeVariableIR> multipliers;
    std::vector<std::string> dynamics;
    std::vector<DaeEquationIR> constraints;
    std::string structural_class{"hessenberg-index2-affine-constraint"};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static IndexTwoDaeIR read(const std::filesystem::path& path);
};

struct IndexTwoDaeTolerance {
    double absolute{1.0e-10};
    double relative{1.0e-8};
    double hidden_rank{1.0e-10};
    int maximum_newton_iterations{20};
};

struct IndexTwoDaeStepRecord {
    double time{};
    double step{};
    std::string solver_backend;
    int newton_iterations{};
    double dynamic_residual_inf{};
    double constraint_residual_inf{};
    double hidden_residual_inf{};
    double hidden_rank_margin{};
};

struct IndexTwoDaeResult {
    bool success{false};
    std::string plan_id;
    std::string solver_backend;
    std::string reason;
    std::unordered_map<std::string, double> initial_state;
    std::unordered_map<std::string, double> initial_multipliers;
    std::unordered_map<std::string, double> final_state;
    std::unordered_map<std::string, double> final_multipliers;
    std::vector<IndexTwoDaeStepRecord> steps;
    int initialization_iterations{};
    double initialization_constraint_residual_inf{};
    double initialization_hidden_residual_inf{};
    double minimum_hidden_rank_margin{1.0};
    std::size_t hidden_rank_checks{};
    std::size_t rejected_steps{};
    bool terminal_fallback_used{false};
};

[[nodiscard]] IndexTwoDaeIR compile_index_two_dae(
    const std::filesystem::path& source,
    const std::string& top = {});

[[nodiscard]] IndexTwoDaeResult simulate_index_two_dae(
    const IndexTwoDaeIR& model,
    double end_time,
    double maximum_step,
    const IndexTwoDaeTolerance& tolerance = {});

void write_index_two_dae_report(
    const IndexTwoDaeIR& model,
    const IndexTwoDaeResult& result,
    const std::filesystem::path& path);

}  // namespace smave
