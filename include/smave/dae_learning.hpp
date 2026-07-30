#pragma once

#include "smave/dae.hpp"
#include "smave/learning.hpp"

#include <filesystem>
#include <string>

namespace smave {

struct DaeMultigridArtifact {
    std::string schema_version{"smave.dae-multigrid.v1"};
    std::string training_dataset_id;
    std::string training_dataset_version;
    std::string training_dataset_manifest_hash;
    std::string model_source_hash;
    std::string residual_family{"semi-explicit-index1-step"};
    std::size_t unknown_count{};
    double minimum_step{};
    double maximum_step{};
    std::size_t training_samples{};
    LearnedMultigridArtifact hierarchy;
    std::string artifact_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static DaeMultigridArtifact read(const std::filesystem::path& path);
};

[[nodiscard]] DaeMultigridArtifact train_dae_multigrid(
    const IndexOneDaeIR& model,
    const std::filesystem::path& scenario_directory);
[[nodiscard]] DaeMultigridArtifact train_dae_multigrid(
    const FullyImplicitDaeIR& model,
    const std::filesystem::path& scenario_directory);

}  // namespace smave
