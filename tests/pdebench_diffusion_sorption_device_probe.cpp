#include "smave/device.hpp"
#include "smave/pdebench_training.hpp"

#include <hdf5.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
class Handle {
public:
    Handle(hid_t value, herr_t (*closer)(hid_t)) : value_(value), closer_(closer) {
        if (value_ < 0) throw std::runtime_error("HDF5 handle creation failed");
    }
    ~Handle() { if (value_ >= 0) closer_(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    [[nodiscard]] hid_t get() const { return value_; }
private:
    hid_t value_;
    herr_t (*closer_)(hid_t);
};

std::vector<double> read_initial(hid_t file, std::size_t sample, std::size_t width) {
    std::ostringstream path;
    path << '/' << std::setw(4) << std::setfill('0') << sample << "/data";
    Handle dataset(H5Dopen2(file, path.str().c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {0, 0, 0};
    const hsize_t count[] = {1, width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 state read failed");
    }
    return {raw.begin(), raw.end()};
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            throw std::invalid_argument(
                "usage: diffusion_device_probe FILE ARTIFACT OUTPUT");
        }
        const std::filesystem::path input_path(argv[1]);
        const auto artifact =
            smave::LearnedFrozenRetardationArtifact::read(argv[2]);
        const std::filesystem::path output_path(argv[3]);
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t batch = 3;
        std::vector<double> states;
        for (std::size_t sample = 0; sample < batch; ++sample) {
            auto initial = read_initial(file.get(), sample, artifact.width);
            states.insert(states.end(), initial.begin(), initial.end());
        }
        const auto started = std::chrono::steady_clock::now();
        const auto result = smave::metal_frozen_retardation_1d_batch(
            states, batch, artifact.width, artifact.constant_ratio,
            artifact.power_ratio, artifact.concentration_exponent,
            4096, 0.8, 1.0e-10);
        const auto wall_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - started).count();
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_DIFFUSION_SORPTION_DEVICE_PROBE 1\n"
               << "BATCH " << batch << '\n'
               << "WIDTH " << artifact.width << '\n'
               << "LEARNED_TRAINING_RESIDUAL "
               << artifact.training_maximum_relative_residual << '\n'
               << "LEARNED_HELDOUT_RESIDUAL "
               << artifact.heldout_maximum_relative_residual << '\n'
               << "METAL_AVAILABLE " << result.available << '\n'
               << "METAL_EXECUTED " << result.executed << '\n'
               << "METAL_VERIFIED " << result.verified << '\n'
               << "METAL_BACKEND \"" << result.backend << "\"\n"
               << "METAL_DEVICE \"" << result.device_name << "\"\n"
               << "METAL_ITERATIONS " << result.iterations << '\n'
               << "METAL_SETUP_US " << result.setup_us << '\n'
               << "METAL_KERNEL_US " << result.kernel_us << '\n'
               << "METAL_DOWNLOAD_US " << result.download_us << '\n'
               << "METAL_WALL_US " << wall_us << '\n'
               << "METAL_ORIGINAL_RELATIVE_RESIDUAL "
               << result.maximum_relative_residual << '\n'
               << "METAL_REASON \"" << result.reason << "\"\n"
               << "ANE_SUPPORTED 0\n"
               << "ANE_REASON \"current Core ML affine graph cannot represent state-dependent retardation and iterative boundary coupling\"\n"
               << "AUTO_SELECTED \"cpu-fp64-learned-retardation\"\nEND\n";
        if (!output) throw std::runtime_error("cannot write diffusion device report");
        std::cout << "Diffusion Metal residual="
                  << result.maximum_relative_residual
                  << " wall_us=" << wall_us << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Diffusion device probe failed: " << error.what() << '\n';
        return 2;
    }
}
