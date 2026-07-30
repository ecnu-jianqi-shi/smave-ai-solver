#include "smave/data_registry.hpp"

#include "smave/release.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace smave {
namespace {

void validate_identifier(const std::string& identifier) {
    if (identifier.empty() || !std::all_of(
            identifier.begin(), identifier.end(), [](unsigned char character) {
                return std::isalnum(character) || character == '-' ||
                    character == '_' || character == '.';
            })) {
        throw std::invalid_argument(
            "dataset id/version must contain only letters, digits, '.', '_' or '-'");
    }
}

void validate_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) {
        throw std::invalid_argument("dataset file path must be non-empty and relative");
    }
    for (const auto& component : path) {
        if (component == "." || component == ".." || component.empty()) {
            throw std::invalid_argument("dataset file path contains an unsafe component");
        }
    }
}

std::string contract(const DatasetManifest& manifest) {
    std::ostringstream output;
    output << kDatasetManifestSchemaVersion << '\n'
           << "DATASET " << std::quoted(manifest.dataset_id) << '\n'
           << "TOTAL " << manifest.total_bytes << '\n';
    for (const auto& file : manifest.files) {
        output << "FILE " << std::quoted(file.relative_path) << ' '
               << file.bytes << ' ' << std::quoted(file.sha256) << '\n';
    }
    return output.str();
}

void verify_directory(
    const std::filesystem::path& directory,
    const DatasetManifest& manifest) {
    std::set<std::string> expected;
    for (const auto& file : manifest.files) expected.insert(file.relative_path);
    std::set<std::string> observed;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        const auto relative = std::filesystem::relative(entry.path(), directory);
        if (relative == "dataset.manifest") continue;
        if (entry.is_symlink()) {
            throw std::invalid_argument("dataset store contains a symbolic link");
        }
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) {
            throw std::invalid_argument("dataset store contains a non-regular file");
        }
        validate_relative_path(relative);
        observed.insert(relative.generic_string());
    }
    if (observed != expected) {
        throw std::invalid_argument("dataset store file set differs from its manifest");
    }
    for (const auto& file : manifest.files) {
        const auto path = directory / std::filesystem::path(file.relative_path);
        if (std::filesystem::file_size(path) != file.bytes ||
            sha256_file(path) != file.sha256) {
            throw std::invalid_argument(
                "dataset payload integrity check failed: " + file.relative_path);
        }
    }
}

}  // namespace

void DatasetManifest::seal() {
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return left.relative_path < right.relative_path;
    });
    total_bytes = 0;
    for (const auto& file : files) total_bytes += file.bytes;
    manifest_hash = sha256_text(contract(*this));
    version = manifest_hash;
}

void DatasetManifest::validate() const {
    validate_identifier(dataset_id);
    validate_identifier(version);
    if (files.empty()) throw std::invalid_argument("dataset manifest must contain files");
    std::set<std::string> paths;
    std::size_t bytes = 0;
    std::string previous;
    for (const auto& file : files) {
        const std::filesystem::path path(file.relative_path);
        validate_relative_path(path);
        if (!paths.insert(file.relative_path).second ||
            (!previous.empty() && previous >= file.relative_path)) {
            throw std::invalid_argument("dataset manifest paths must be unique and sorted");
        }
        if (file.sha256.size() != 64 || !std::all_of(
                file.sha256.begin(), file.sha256.end(), [](unsigned char character) {
                    return std::isdigit(character) ||
                        (character >= 'a' && character <= 'f');
                })) {
            throw std::invalid_argument("dataset file has an invalid SHA-256 digest");
        }
        previous = file.relative_path;
        bytes += file.bytes;
    }
    const auto expected = sha256_text(contract(*this));
    if (total_bytes != bytes || manifest_hash != expected || version != expected) {
        throw std::invalid_argument("dataset manifest integrity check failed");
    }
}

void DatasetManifest::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write dataset manifest: " + path.string());
    output << kDatasetManifestSchemaVersion << '\n'
           << "DATASET " << std::quoted(dataset_id) << '\n'
           << "VERSION " << std::quoted(version) << '\n'
           << "TOTAL " << total_bytes << '\n';
    for (const auto& file : files) {
        output << "FILE " << std::quoted(file.relative_path) << ' '
               << file.bytes << ' ' << std::quoted(file.sha256) << '\n';
    }
    output << "HASH " << std::quoted(manifest_hash) << "\nEND\n";
}

DatasetManifest DatasetManifest::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read dataset manifest: " + path.string());
    std::string tag;
    input >> tag;
    if (tag != kDatasetManifestSchemaVersion) {
        throw std::invalid_argument("unsupported dataset manifest schema");
    }
    DatasetManifest manifest;
    input >> tag;
    if (tag != "DATASET") throw std::invalid_argument("dataset manifest lacks DATASET");
    input >> std::quoted(manifest.dataset_id);
    input >> tag;
    if (tag != "VERSION") throw std::invalid_argument("dataset manifest lacks VERSION");
    input >> std::quoted(manifest.version);
    input >> tag;
    if (tag != "TOTAL") throw std::invalid_argument("dataset manifest lacks TOTAL");
    input >> manifest.total_bytes;
    while (input >> tag && tag == "FILE") {
        DatasetFile file;
        input >> std::quoted(file.relative_path) >> file.bytes >> std::quoted(file.sha256);
        manifest.files.push_back(std::move(file));
    }
    if (tag != "HASH") throw std::invalid_argument("dataset manifest lacks HASH");
    input >> std::quoted(manifest.manifest_hash);
    input >> tag;
    if (tag != "END") throw std::invalid_argument("dataset manifest lacks END");
    if (input >> tag) throw std::invalid_argument("dataset manifest has trailing content");
    manifest.validate();
    return manifest;
}

DatasetStore::DatasetStore(std::filesystem::path root) : root_(std::move(root)) {
    if (root_.empty()) throw std::invalid_argument("dataset store root must not be empty");
}

DatasetManifest DatasetStore::snapshot(
    const std::filesystem::path& source_directory,
    const std::string& dataset_id) const {
    validate_identifier(dataset_id);
    if (!std::filesystem::is_directory(source_directory)) {
        throw std::invalid_argument("dataset source must be a directory");
    }
    DatasetManifest manifest;
    manifest.dataset_id = dataset_id;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_directory)) {
        if (entry.is_symlink()) {
            throw std::invalid_argument("dataset source contains a symbolic link");
        }
        if (entry.is_directory()) continue;
        if (!entry.is_regular_file()) {
            throw std::invalid_argument("dataset source contains a non-regular file");
        }
        const auto relative = std::filesystem::relative(entry.path(), source_directory);
        validate_relative_path(relative);
        manifest.files.push_back(DatasetFile{
            .relative_path = relative.generic_string(),
            .bytes = static_cast<std::size_t>(entry.file_size()),
            .sha256 = sha256_file(entry.path()),
        });
    }
    manifest.seal();
    manifest.validate();

    const auto final_directory = version_directory(dataset_id, manifest.version);
    if (std::filesystem::exists(final_directory)) {
        const auto existing = verify(dataset_id, manifest.version);
        if (existing.manifest_hash != manifest.manifest_hash) {
            throw std::invalid_argument("dataset version collision in immutable store");
        }
        return existing;
    }
    std::filesystem::create_directories(final_directory.parent_path());
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = final_directory.parent_path() /
        (".staging-" + std::to_string(nonce));
    std::filesystem::remove_all(staging);
    try {
        std::filesystem::create_directories(staging);
        for (const auto& file : manifest.files) {
            const auto destination = staging / std::filesystem::path(file.relative_path);
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(
                source_directory / std::filesystem::path(file.relative_path),
                destination,
                std::filesystem::copy_options::none);
        }
        manifest.write(staging / "dataset.manifest");
        verify_directory(staging, manifest);
        std::filesystem::rename(staging, final_directory);
    } catch (...) {
        std::filesystem::remove_all(staging);
        throw;
    }
    return verify(dataset_id, manifest.version);
}

DatasetManifest DatasetStore::verify(
    const std::string& dataset_id,
    const std::string& version) const {
    validate_identifier(dataset_id);
    validate_identifier(version);
    const auto directory = version_directory(dataset_id, version);
    const auto manifest = DatasetManifest::read(directory / "dataset.manifest");
    if (manifest.dataset_id != dataset_id || manifest.version != version) {
        throw std::invalid_argument("dataset store path does not match manifest identity");
    }
    verify_directory(directory, manifest);
    return manifest;
}

std::filesystem::path DatasetStore::version_directory(
    const std::string& dataset_id,
    const std::string& version) const {
    validate_identifier(dataset_id);
    validate_identifier(version);
    return root_ / "datasets" / dataset_id / version;
}

}  // namespace smave
