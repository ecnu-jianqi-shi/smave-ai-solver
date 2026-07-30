#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* kIrSchemaVersion = "smave.ir.v2";
inline constexpr const char* kLegacyIrSchemaVersion = "smave.ir.v1";

struct SparsityPattern {
    std::size_t row_count{};
    std::size_t column_count{};
    std::vector<std::size_t> row_offsets;
    std::vector<std::size_t> column_indices;

    [[nodiscard]] static SparsityPattern from_rows(
        std::size_t column_count,
        const std::vector<std::vector<std::size_t>>& rows);
    [[nodiscard]] static SparsityPattern from_dense(
        const std::vector<std::vector<int>>& dense);
    void validate() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t nonzeros() const;
    [[nodiscard]] bool contains(std::size_t row, std::size_t column) const;
    [[nodiscard]] std::span<const std::size_t> row(std::size_t index) const;
    [[nodiscard]] std::vector<std::size_t> greedy_column_coloring() const;
};

struct VariableIR {
    std::string name;
    std::string kind{"algebraic"};
    double nominal{1.0};
    double start{0.0};
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::string unit{"1"};
};

struct EquationIR {
    std::string id;
    std::string residual;
    std::vector<std::string> variables;
    std::size_t source_line{};
};

struct BlockIR {
    std::string id;
    std::vector<std::string> unknowns;
    std::vector<std::string> contexts;
    std::vector<std::string> equation_ids;
    SparsityPattern jacobian_sparsity;
    bool linear{false};
    bool smooth{true};
    bool event_related{false};
    int dae_index{0};
    std::string mode{"continuous"};
    std::string original_solver{"damped-newton"};
    std::string fingerprint;
};

struct ModelIR {
    std::string schema_version{kIrSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::string frontend_version{"modelica-subset-v1"};
    std::vector<VariableIR> variables;
    std::vector<EquationIR> equations;
    std::vector<BlockIR> blocks;
    std::vector<std::string> capabilities{
        "runtime_residual", "finite_difference_jacobian",
        "original_block_fallback", "rule_compile_router"};

    void validate() const;
    void write(const std::filesystem::path& path) const;
    static ModelIR read(const std::filesystem::path& path);
};

[[nodiscard]] std::string block_fingerprint(
    const BlockIR& block, const ModelIR& model);

}  // namespace smave
