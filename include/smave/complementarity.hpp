#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* kComplementaritySchemaVersion =
    "SMAVE_COMPLEMENTARITY_1";

struct ComplementarityVariableIR {
    std::string name;
    double start{0.0};
};

struct ComplementarityIR {
    std::string schema_version{kComplementaritySchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::vector<ComplementarityVariableIR> variables;
    std::vector<std::string> gap_expressions;
    std::vector<std::vector<double>> matrix;
    std::vector<double> offset;
    std::string structural_class{"strongly-monotone-linear-complementarity"};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static ComplementarityIR read(const std::filesystem::path& path);
};

struct ComplementarityTolerance {
    double absolute{1.0e-10};
    double relative{1.0e-8};
    int maximum_pgs_iterations{4000};
    int maximum_newton_iterations{40};
    std::function<bool()> cancellation_requested;
};

struct ComplementarityAttempt {
    std::string backend;
    std::string outcome;
    std::string reason;
    int iterations{};
    double primal_violation{};
    double dual_violation{};
    double complementarity_violation{};
    double equation_residual_inf{};
};

struct ComplementarityResult {
    bool success{false};
    std::string plan_id;
    std::string accepted_backend;
    std::string reason;
    std::vector<double> solution;
    std::vector<double> gap;
    std::vector<ComplementarityAttempt> attempts;
    bool terminal_fallback_used{false};
};

[[nodiscard]] ComplementarityIR compile_complementarity(
    const std::filesystem::path& source,
    const std::string& top = {});

[[nodiscard]] ComplementarityResult solve_complementarity(
    const ComplementarityIR& model,
    const ComplementarityTolerance& tolerance = {});

void write_complementarity_report(
    const ComplementarityIR& model,
    const ComplementarityResult& result,
    const std::filesystem::path& path);

}  // namespace smave
