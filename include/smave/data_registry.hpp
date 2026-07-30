#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace smave {

inline constexpr const char* kDatasetManifestSchemaVersion = "SMAVE_DATASET_MANIFEST_1";

struct DatasetFile {
    std::string relative_path;
    std::size_t bytes{};
    std::string sha256;
};

struct DatasetManifest {
    std::string dataset_id;
    std::string version;
    std::vector<DatasetFile> files;
    std::size_t total_bytes{};
    std::string manifest_hash;

    void seal();
    void validate() const;
    void write(const std::filesystem::path& path) const;
    static DatasetManifest read(const std::filesystem::path& path);
};

class DatasetStore {
public:
    explicit DatasetStore(std::filesystem::path root);

    [[nodiscard]] DatasetManifest snapshot(
        const std::filesystem::path& source_directory,
        const std::string& dataset_id) const;
    [[nodiscard]] DatasetManifest verify(
        const std::string& dataset_id,
        const std::string& version) const;
    [[nodiscard]] std::filesystem::path version_directory(
        const std::string& dataset_id,
        const std::string& version) const;

private:
    std::filesystem::path root_;
};

}  // namespace smave
