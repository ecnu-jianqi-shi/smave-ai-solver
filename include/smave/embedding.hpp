#pragma once

#include "smave/expression.hpp"
#include "smave/ir.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

struct EquationEmbedding {
    std::string schema_version{"smave.embedding.v1"};
    std::string block_fingerprint;
    std::vector<double> structural;
    std::vector<std::string> operator_tokens;
    double sparsity_density{};
    double normalized_bandwidth{};
    double transfer_risk{1.0};
    std::vector<std::string> family_candidates;
};

[[nodiscard]] EquationEmbedding encode_block(
    const ModelIR& model,
    const BlockIR& block);

[[nodiscard]] double embedding_similarity(
    const EquationEmbedding& left,
    const EquationEmbedding& right);

struct FamilyEntry {
    std::string family_id;
    std::string expert_version;
    std::string artifact_hash;
    EquationEmbedding embedding;
};

struct FamilyMatch {
    std::string family_id;
    std::string expert_version;
    std::string artifact_hash;
    double similarity{};
    double transfer_risk{1.0};
};

class FamilyIndex {
public:
    void add(FamilyEntry entry);
    [[nodiscard]] std::vector<FamilyMatch> query(
        const EquationEmbedding& embedding,
        std::size_t top_k = 4,
        double minimum_similarity = 0.80) const;
    void write(const std::filesystem::path& path) const;
    static FamilyIndex read(const std::filesystem::path& path);

private:
    std::vector<FamilyEntry> entries_;
};

}  // namespace smave
