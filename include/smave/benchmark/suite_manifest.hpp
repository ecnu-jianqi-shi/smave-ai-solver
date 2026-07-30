#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace smave::benchmark {

struct BenchmarkFamilyStatus {
    std::string family;
    std::size_t discovered{};
    std::size_t executable{};
    std::string required_tool;
    std::string tool_path;
    std::string status;
    std::string reason;
};

[[nodiscard]] std::vector<BenchmarkFamilyStatus> inspect_benchmark_suite(
    const std::filesystem::path& benchmark_root);
void write_benchmark_manifest(
    const std::vector<BenchmarkFamilyStatus>& statuses,
    const std::filesystem::path& output_path);

}  // namespace smave::benchmark
