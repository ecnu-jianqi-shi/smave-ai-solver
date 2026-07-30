#include "smave/embedding.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>

namespace smave {
namespace {

const EquationIR& equation(const ModelIR& model, const std::string& id) {
    const auto iterator = std::find_if(
        model.equations.begin(), model.equations.end(),
        [&](const EquationIR& item) { return item.id == id; });
    if (iterator == model.equations.end()) throw std::logic_error("embedding equation is absent");
    return *iterator;
}

std::set<std::string> operator_tokens(const std::string& source) {
    std::set<std::string> tokens;
    for (const char character : source) {
        if (character == '+') tokens.insert("add");
        else if (character == '-') tokens.insert("sub");
        else if (character == '*') tokens.insert("mul");
        else if (character == '/') tokens.insert("div");
        else if (character == '^') tokens.insert("pow");
    }
    for (const std::string function : {"sin", "cos", "tan", "exp", "log", "sqrt", "abs"}) {
        if (source.find(function + "(") != std::string::npos) tokens.insert(function);
    }
    return tokens;
}

void validate_embedding(const EquationEmbedding& embedding) {
    if (embedding.schema_version != "smave.embedding.v1" ||
        embedding.block_fingerprint.empty() || embedding.structural.empty()) {
        throw std::invalid_argument("equation embedding is invalid");
    }
}

}  // namespace

EquationEmbedding encode_block(const ModelIR& model, const BlockIR& block) {
    EquationEmbedding embedding;
    embedding.block_fingerprint = block.fingerprint;
    const std::size_t rows = block.jacobian_sparsity.row_count;
    const std::size_t columns = block.jacobian_sparsity.column_count;
    const std::size_t nonzero = block.jacobian_sparsity.nonzeros();
    std::size_t bandwidth = 0;
    for (std::size_t row = 0; row < rows; ++row) {
        for (const auto column : block.jacobian_sparsity.row(row)) {
            bandwidth = std::max(
                bandwidth,
                row > column ? row - column : column - row);
        }
    }
    embedding.sparsity_density = rows && columns
        ? static_cast<double>(nonzero) / static_cast<double>(rows * columns)
        : 0.0;
    embedding.normalized_bandwidth = columns > 1
        ? static_cast<double>(bandwidth) / static_cast<double>(columns - 1)
        : 0.0;
    std::set<std::string> tokens;
    for (const auto& id : block.equation_ids) {
        const auto found = operator_tokens(equation(model, id).residual);
        tokens.insert(found.begin(), found.end());
    }
    embedding.operator_tokens.assign(tokens.begin(), tokens.end());
    embedding.structural = {
        std::log1p(static_cast<double>(columns)) / 10.0,
        embedding.sparsity_density,
        embedding.normalized_bandwidth,
        block.linear ? 1.0 : 0.0,
        block.smooth ? 1.0 : 0.0,
        block.event_related ? 1.0 : 0.0,
        std::min(1.0, static_cast<double>(block.dae_index) / 5.0),
        static_cast<double>(block.contexts.size()) /
            static_cast<double>(std::max<std::size_t>(1, columns + block.contexts.size())),
        tokens.contains("mul") ? 1.0 : 0.0,
        tokens.contains("pow") ? 1.0 : 0.0,
        (tokens.contains("sin") || tokens.contains("cos") || tokens.contains("exp") ||
         tokens.contains("log")) ? 1.0 : 0.0,
    };
    embedding.transfer_risk = block.event_related || block.dae_index > 1
        ? 1.0
        : block.linear ? 0.10 : 0.35;
    validate_embedding(embedding);
    return embedding;
}

double embedding_similarity(
    const EquationEmbedding& left,
    const EquationEmbedding& right) {
    validate_embedding(left);
    validate_embedding(right);
    if (left.structural.size() != right.structural.size()) return 0.0;
    double dot = 0.0;
    double left_norm = 0.0;
    double right_norm = 0.0;
    for (std::size_t index = 0; index < left.structural.size(); ++index) {
        dot += left.structural[index] * right.structural[index];
        left_norm += left.structural[index] * left.structural[index];
        right_norm += right.structural[index] * right.structural[index];
    }
    double cosine = left_norm > 0.0 && right_norm > 0.0
        ? dot / std::sqrt(left_norm * right_norm)
        : 0.0;
    std::set<std::string> left_tokens(left.operator_tokens.begin(), left.operator_tokens.end());
    std::set<std::string> right_tokens(right.operator_tokens.begin(), right.operator_tokens.end());
    std::vector<std::string> intersection;
    std::set_intersection(
        left_tokens.begin(), left_tokens.end(), right_tokens.begin(), right_tokens.end(),
        std::back_inserter(intersection));
    std::set<std::string> union_tokens = left_tokens;
    union_tokens.insert(right_tokens.begin(), right_tokens.end());
    const double jaccard = union_tokens.empty()
        ? 1.0
        : static_cast<double>(intersection.size()) / static_cast<double>(union_tokens.size());
    return std::clamp(0.8 * cosine + 0.2 * jaccard, 0.0, 1.0);
}

void FamilyIndex::add(FamilyEntry entry) {
    if (entry.family_id.empty() || entry.expert_version.empty() || entry.artifact_hash.empty()) {
        throw std::invalid_argument("family index entry is incomplete");
    }
    validate_embedding(entry.embedding);
    entries_.push_back(std::move(entry));
}

std::vector<FamilyMatch> FamilyIndex::query(
    const EquationEmbedding& embedding,
    std::size_t top_k,
    double minimum_similarity) const {
    std::vector<FamilyMatch> result;
    for (const auto& entry : entries_) {
        const double similarity = embedding_similarity(embedding, entry.embedding);
        if (similarity < minimum_similarity) continue;
        result.push_back(FamilyMatch{
            .family_id = entry.family_id,
            .expert_version = entry.expert_version,
            .artifact_hash = entry.artifact_hash,
            .similarity = similarity,
            .transfer_risk = std::max(
                embedding.transfer_risk,
                entry.embedding.transfer_risk) + (1.0 - similarity),
        });
    }
    std::sort(result.begin(), result.end(), [](const FamilyMatch& left, const FamilyMatch& right) {
        if (left.transfer_risk != right.transfer_risk) return left.transfer_risk < right.transfer_risk;
        return left.similarity > right.similarity;
    });
    if (result.size() > top_k) result.resize(top_k);
    return result;
}

void FamilyIndex::write(const std::filesystem::path& path) const {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write family index");
    output << std::setprecision(17) << "SMAVE_FAMILY_INDEX 1\n" << entries_.size() << '\n';
    for (const auto& entry : entries_) {
        output << std::quoted(entry.family_id) << ' ' << std::quoted(entry.expert_version) << ' '
               << std::quoted(entry.artifact_hash) << ' '
               << std::quoted(entry.embedding.block_fingerprint) << ' '
               << entry.embedding.transfer_risk << ' ' << entry.embedding.sparsity_density << ' '
               << entry.embedding.normalized_bandwidth << ' ' << entry.embedding.structural.size();
        for (const double value : entry.embedding.structural) output << ' ' << value;
        output << ' ' << entry.embedding.operator_tokens.size();
        for (const auto& token : entry.embedding.operator_tokens) output << ' ' << std::quoted(token);
        output << '\n';
    }
    output << "END\n";
}

FamilyIndex FamilyIndex::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read family index");
    std::string magic; int version{}; input >> magic >> version;
    if (magic != "SMAVE_FAMILY_INDEX" || version != 1) {
        throw std::runtime_error("unsupported family index schema");
    }
    std::size_t count{}; input >> count;
    FamilyIndex index;
    for (std::size_t item = 0; item < count; ++item) {
        FamilyEntry entry;
        input >> std::quoted(entry.family_id) >> std::quoted(entry.expert_version)
              >> std::quoted(entry.artifact_hash)
              >> std::quoted(entry.embedding.block_fingerprint)
              >> entry.embedding.transfer_risk >> entry.embedding.sparsity_density
              >> entry.embedding.normalized_bandwidth;
        entry.embedding.schema_version = "smave.embedding.v1";
        std::size_t structural{}; input >> structural;
        entry.embedding.structural.resize(structural);
        for (auto& value : entry.embedding.structural) input >> value;
        std::size_t tokens{}; input >> tokens;
        entry.embedding.operator_tokens.resize(tokens);
        for (auto& token : entry.embedding.operator_tokens) input >> std::quoted(token);
        index.add(std::move(entry));
    }
    input >> magic;
    if (magic != "END") throw std::runtime_error("truncated family index");
    return index;
}

}  // namespace smave
