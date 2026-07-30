#include "smave/benchmark/suite_manifest.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace smave::benchmark {
namespace {

std::string executable(const std::string& name) {
    const auto* path = std::getenv("PATH");
    if (path == nullptr) return {};
    std::string paths(path);
    std::size_t begin{};
    while (begin <= paths.size()) {
        const auto end = paths.find(':', begin);
        const auto directory = paths.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        const auto candidate = std::filesystem::path(directory) / name;
        if (std::filesystem::is_regular_file(candidate) &&
            (std::filesystem::status(candidate).permissions() &
             std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
            return candidate.string();
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return {};
}

std::filesystem::path home_directory() {
    const auto* home = std::getenv("HOME");
    return home == nullptr ? std::filesystem::path{} : std::filesystem::path(home);
}

std::string executable_or_file(
    const std::string& name, const std::filesystem::path& fallback) {
    const auto from_path = executable(name);
    if (!from_path.empty()) return from_path;
    return std::filesystem::is_regular_file(fallback) ? fallback.string() : "";
}

std::size_t count_extension(
    const std::filesystem::path& root,
    const std::vector<std::string>& extensions) {
    if (!std::filesystem::exists(root)) return 0;
    std::size_t count{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        for (const auto& extension : extensions) {
            if (entry.path().extension() == extension) {
                ++count;
                break;
            }
        }
    }
    return count;
}

std::size_t pdebench_complete_file_count(const std::filesystem::path& root) {
    std::ifstream manifest(root / "files.tsv");
    if (!manifest) return 0;
    std::string line;
    std::size_t complete{};
    while (std::getline(manifest, line)) {
        if (line.empty() || line.front() == '#') continue;
        std::istringstream fields(line);
        std::string relative_path;
        std::string datafile_id;
        std::uintmax_t expected_size{};
        std::string md5;
        fields >> relative_path >> datafile_id >> expected_size >> md5;
        const auto path = root / relative_path;
        if (fields && std::filesystem::is_regular_file(path) &&
            std::filesystem::file_size(path) == expected_size) {
            ++complete;
        }
    }
    return complete;
}

BenchmarkFamilyStatus status(
    std::string family,
    std::size_t discovered,
    std::string required_tool,
    std::string tool_path,
    bool directly_executable,
    std::string unavailable_reason) {
    BenchmarkFamilyStatus result;
    result.family = std::move(family);
    result.discovered = discovered;
    result.required_tool = std::move(required_tool);
    result.tool_path = std::move(tool_path);
    result.executable = directly_executable || !result.tool_path.empty() ? discovered : 0;
    result.status = result.executable == discovered && discovered != 0
        ? "ready"
        : "blocked-toolchain";
    result.reason = result.status == "ready" ? "execution toolchain available"
                                               : std::move(unavailable_reason);
    return result;
}

}  // namespace

std::vector<BenchmarkFamilyStatus> inspect_benchmark_suite(
    const std::filesystem::path& benchmark_root) {
    std::vector<BenchmarkFamilyStatus> results;
    results.push_back(status(
        "suitesparse", count_extension(benchmark_root / "suitesparse", {".mtx"}),
        "smave_sparse_suite_benchmark", "built-in", true, {}));
    results.push_back(status(
        "cops", count_extension(benchmark_root / "cops", {".mod"}),
        "ampl-community-edition",
        executable_or_file("ampl", home_directory() /
            "Library/Python/3.9/lib/python/site-packages/ampl_module_base/bin/ampl"),
        false,
        "COPS .mod/.dat assets require an AMPL-compatible model translator"));
    if (results.back().status == "ready") {
        results.back().executable = 0;
        results.back().status = "blocked-license";
        results.back().reason =
            "AMPL is installed, but the demo license rejects COPS instances above 300 variables; activate a Community Edition license";
    }
    results.push_back(status(
        "pdebench", count_extension(benchmark_root / "pdebench", {".h5", ".hdf5"}),
        "h5dump", executable_or_file("h5dump", home_directory() /
            ".local/smave-bench/hdf5-2.1.1/bin/h5dump"), false,
        "PDEBench HDF5 assets require an HDF5 reader and equation-family adapters"));
    const auto pdebench_complete = pdebench_complete_file_count(
        benchmark_root / "pdebench");
    results.back().executable = pdebench_complete;
    if (pdebench_complete != results.back().discovered) {
        results.back().status = "blocked-data";
        results.back().reason = std::to_string(pdebench_complete) + "/" +
            std::to_string(results.back().discovered) +
            " authoritative files have complete byte sizes; remaining files are being restored with files.tsv checksums";
    }
    results.push_back(status(
        "petsc-ts", count_extension(benchmark_root / "petsc-ts", {".c"}),
        "petsc", executable_or_file("petsc-config", home_directory() /
            ".local/smave-bench/petsc-3.25.3/lib/libpetsc.a"), false,
        "PETSc TS sources require a PETSc development installation"));
    results.push_back(status(
        "multiphysics-msl",
        count_extension(benchmark_root / "multiphysics-msl", {".mo"}),
        "omc-or-docker", executable("omc").empty() ? executable("docker") : executable("omc"), false,
        "MSL models require OpenModelica and the matching Modelica Standard Library"));
    return results;
}

void write_benchmark_manifest(
    const std::vector<BenchmarkFamilyStatus>& statuses,
    const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("cannot write benchmark readiness manifest");
    output << "SMAVE_BENCHMARK_READINESS 1\nFAMILIES " << statuses.size() << '\n';
    for (const auto& item : statuses) {
        output << "FAMILY " << std::quoted(item.family)
               << " DISCOVERED " << item.discovered
               << " EXECUTABLE " << item.executable
               << " TOOL " << std::quoted(item.required_tool)
               << " TOOL_PATH " << std::quoted(item.tool_path)
               << " STATUS " << std::quoted(item.status)
               << " REASON " << std::quoted(item.reason) << '\n';
    }
    output << "END\n";
}

}  // namespace smave::benchmark
