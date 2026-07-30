#pragma once

#include "smave/ir.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace smave {

inline constexpr const char* kBlockGraphSchemaVersion = "SMAVE_BLOCK_GRAPH_2";

struct BlockGraphNode {
    std::string id;
    std::string type;
    double sample_time{0.0};
    double sample_offset{0.0};
    std::map<std::string, std::string> attributes;
};

struct BlockGraphConnection {
    std::string source_node;
    std::string source_port;
    std::string target_node;
    std::string target_port;
};

struct BlockGraphIR {
    std::string schema_version{kBlockGraphSchemaVersion};
    std::string model_id;
    std::string source_hash;
    std::vector<BlockGraphNode> nodes;
    std::vector<BlockGraphConnection> connections;
    std::vector<std::string> commit_order;

    void validate(const std::filesystem::path& base_directory = {}) const;
    void write(const std::filesystem::path& path) const;
    static BlockGraphIR read(const std::filesystem::path& path);
};

[[nodiscard]] BlockGraphIR import_block_graph(const std::filesystem::path& path);

}  // namespace smave
