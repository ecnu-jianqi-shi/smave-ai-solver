#include "smave/pdebench_training.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void require_rejected(Function function, const char* message) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, message);
}

std::uint64_t append_hash(
    const std::vector<float>& values, std::uint64_t hash) {
    for (const auto value : values) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void write_tensor(const std::filesystem::path& path,
                  const std::vector<float>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(float)));
}

void write_manifest(
    const std::filesystem::path& path,
    bool solver_label,
    const std::string& operator_id,
    bool certified,
    const std::string& checksum,
    bool trailing = false) {
    std::ofstream output(path);
    output << "SMAVE_PDEBENCH_TRAINING_SET 1\n"
           << "FAMILY \"advection\"\n"
           << "SOURCE \"authoritative.h5\"\n"
           << "SAMPLES 2\n"
           << "VALUES_PER_SAMPLE 3\n"
           << "TARGET_KIND \""
           << (solver_label ? "same-discrete-operator-solver-label" :
                              "authoritative-next-state-pretraining") << "\"\n"
           << "SOLVER_LABEL " << solver_label << "\n"
           << "DISCRETE_OPERATOR_ID \"" << operator_id << "\"\n"
           << "ORIGINAL_RESIDUAL_CERTIFIED " << certified << "\n"
           << "DTYPE \"fp32\"\n"
           << "LAYOUT \"sample-major-contiguous\"\n"
           << "CHECKSUM \"" << checksum << "\"\nEND\n";
    if (trailing) output << "UNTRUSTED 1\n";
}

}  // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() /
            "smave-pdebench-training-unit";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const auto prefix = directory / "advection";
        const std::vector<float> inputs{1, 2, 3, 4, 5, 6};
        const std::vector<float> targets{6, 5, 4, 3, 2, 1};
        write_tensor(prefix.string() + ".inputs.f32", inputs);
        write_tensor(prefix.string() + ".targets.f32", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ostringstream checksum;
        checksum << std::hex << std::setw(16) << std::setfill('0') << hash;

        write_manifest(prefix.string() + ".manifest.txt", false, "none", false,
                       checksum.str());
        const auto pretraining = smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::Pretraining);
        require(pretraining.samples == 2 && pretraining.values_per_sample == 3,
                "valid pretraining manifest was not loaded");
        require_rejected([&] {
            smave::PdebenchTrainingManifest::read_and_verify(
                prefix, smave::PdebenchTrainingUse::SolverLabelTraining,
                "periodic-implicit-upwind-v1");
        }, "next-state data entered solver-label training");

        write_manifest(prefix.string() + ".manifest.txt", true,
                       "periodic-implicit-upwind-v1", false, checksum.str());
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::SolverLabelTraining,
            "periodic-implicit-upwind-v1");
        require_rejected([&] {
            smave::PdebenchTrainingManifest::read_and_verify(
                prefix, smave::PdebenchTrainingUse::SolverLabelTraining,
                "frozen-burgers-v1");
        }, "mismatched operator id entered solver-label training");
        require_rejected([&] {
            smave::PdebenchTrainingManifest::read_and_verify(
                prefix, smave::PdebenchTrainingUse::DirectDeployment,
                "periodic-implicit-upwind-v1");
        }, "uncertified labels granted Direct authority");

        write_manifest(prefix.string() + ".manifest.txt", true,
                       "periodic-implicit-upwind-v1", true, checksum.str());
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "periodic-implicit-upwind-v1");

        write_manifest(prefix.string() + ".manifest.txt", true,
                       "periodic-implicit-upwind-v1", true, "0000000000000000");
        require_rejected([&] {
            smave::PdebenchTrainingManifest::read_and_verify(
                prefix, smave::PdebenchTrainingUse::DirectDeployment,
                "periodic-implicit-upwind-v1");
        }, "tensor checksum tampering was accepted");

        write_manifest(prefix.string() + ".manifest.txt", true,
                       "periodic-implicit-upwind-v1", true, checksum.str(), true);
        require_rejected([&] {
            smave::PdebenchTrainingManifest::read(prefix.string() + ".manifest.txt");
        }, "manifest trailing content was accepted");
        std::filesystem::remove_all(directory);
        std::cout << "PDEBench training contract unit passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench training contract unit failed: "
                  << error.what() << '\n';
        return 1;
    }
}
