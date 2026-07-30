#include "smave/device.hpp"
#include "smave/pdebench_training.hpp"

#include <hdf5.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
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

std::vector<float> read_initial(hid_t dataset, hsize_t sample, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, 0, 0};
    const hsize_t count[] = {1, 1, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> output(width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, output.data()) < 0) {
        throw std::runtime_error("HDF5 tensor read failed");
    }
    return output;
}

double maximum_relative_residual(
    const std::vector<float>& inputs,
    const smave::DeviceExecutionResult& result,
    std::size_t batch,
    std::size_t width,
    double inverse_diagonal,
    double feedback) {
    if (!result.executed || result.output.size() != inputs.size()) return INFINITY;
    const auto diagonal = 1.0 / inverse_diagonal;
    const auto lower = -feedback / inverse_diagonal;
    double maximum{};
    for (std::size_t sample = 0; sample < batch; ++sample) {
        double residual_squared{};
        double input_squared{};
        const auto offset = sample * width;
        for (std::size_t index = 0; index < width; ++index) {
            const auto previous = index == 0 ? width - 1 : index - 1;
            const double product = diagonal * result.output[offset + index] +
                lower * result.output[offset + previous];
            const double residual = inputs[offset + index] - product;
            residual_squared += residual * residual;
            input_squared += static_cast<double>(inputs[offset + index]) *
                inputs[offset + index];
        }
        maximum = std::max(maximum,
            std::sqrt(residual_squared) /
                std::max(1.0, std::sqrt(input_squared)));
    }
    return maximum;
}

void report_device(
    std::ostream& output,
    const char* prefix,
    const smave::DeviceExecutionResult& result,
    double wall_us,
    double residual) {
    output << prefix << "_AVAILABLE " << result.available << '\n'
           << prefix << "_EXECUTED " << result.executed << '\n'
           << prefix << "_DEVICE_VERIFIED " << result.verified << '\n'
           << prefix << "_BACKEND \"" << result.backend << "\"\n"
           << prefix << "_DEVICE \"" << result.device_name << "\"\n"
           << prefix << "_WALL_US " << wall_us << '\n'
           << prefix << "_UPLOAD_US " << result.upload_us << '\n'
           << prefix << "_KERNEL_US " << result.kernel_us << '\n'
           << prefix << "_DOWNLOAD_US " << result.download_us << '\n'
           << prefix << "_MAXIMUM_FP64_REFERENCE_ERROR "
           << result.maximum_relative_error << '\n'
           << prefix << "_ORIGINAL_RELATIVE_RESIDUAL " << residual << '\n'
           << prefix << "_ORIGINAL_GATE_PASS "
           << (std::isfinite(residual) && residual <= 1.0e-10) << '\n'
           << prefix << "_REASON \"" << result.reason << "\"\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            throw std::invalid_argument(
                "usage: smave_pdebench_advection_device_probe FILE ARTIFACT WORK OUTPUT");
        }
        const std::filesystem::path input_path(argv[1]);
        const auto artifact =
            smave::LearnedPeriodicRecurrenceArtifact::read(argv[2]);
        const std::filesystem::path working_directory(argv[3]);
        const std::filesystem::path output_path(argv[4]);
        if (artifact.family != "advection" || artifact.width < 2) {
            throw std::invalid_argument("invalid advection device artifact");
        }
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle tensor(H5Dopen2(file.get(), "tensor", H5P_DEFAULT), H5Dclose);
        constexpr std::size_t batch = 3;
        std::vector<float> inputs;
        for (std::size_t sample = 0; sample < batch; ++sample) {
            auto initial = read_initial(tensor.get(), sample, artifact.width);
            inputs.insert(inputs.end(), initial.begin(), initial.end());
        }
        std::vector<float> weights(artifact.width * artifact.width);
        const auto normalization = artifact.inverse_diagonal /
            (1.0 - std::pow(artifact.feedback, artifact.width));
        std::vector<double> kernel(artifact.width);
        kernel.front() = normalization;
        for (std::size_t index = 1; index < artifact.width; ++index) {
            kernel[index] = kernel[index - 1] * artifact.feedback;
        }
        for (std::size_t row = 0; row < artifact.width; ++row) {
            for (std::size_t column = 0; column < artifact.width; ++column) {
                const auto lag = (row + artifact.width - column) % artifact.width;
                weights[row * artifact.width + column] =
                    static_cast<float>(kernel[lag]);
            }
        }
        const std::vector<float> bias(artifact.width);
        const auto metal_started = std::chrono::steady_clock::now();
        const auto metal = smave::metal_gpu_affine_batch(
            inputs, batch, artifact.width, weights, artifact.width, bias,
            1.0e-5, 1.0e-5);
        const auto metal_wall_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - metal_started).count();
        const auto metal_residual = maximum_relative_residual(
            inputs, metal, batch, artifact.width,
            artifact.inverse_diagonal, artifact.feedback);

        std::filesystem::create_directories(working_directory);
        const auto ane_started = std::chrono::steady_clock::now();
        const auto ane = smave::coreml_neural_engine_affine_tensor_batch(
            inputs, batch, artifact.width, weights, artifact.width, bias,
            working_directory, 2.0e-2, 1.0e-3);
        const auto ane_wall_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - ane_started).count();
        const auto ane_residual = maximum_relative_residual(
            inputs, ane, batch, artifact.width,
            artifact.inverse_diagonal, artifact.feedback);

        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_ADVECTION_DEVICE_PROBE 1\n"
               << "BATCH " << batch << '\n'
               << "WIDTH " << artifact.width << '\n'
               << "OPERATOR \"" << artifact.discrete_operator_id << "\"\n";
        report_device(output, "METAL", metal, metal_wall_us, metal_residual);
        report_device(output, "ANE", ane, ane_wall_us, ane_residual);
        output << "AUTO_SELECTED \"cpu-fp64-learned-recurrence\"\n"
               << "AUTO_REASON \"device candidates must pass the original FP64 residual gate and beat complete CPU wall time\"\n"
               << "END\n";
        if (!output) throw std::runtime_error("cannot write device probe report");
        std::cout << "Advection device probe Metal residual=" << metal_residual
                  << " ANE residual=" << ane_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench advection device probe failed: "
                  << error.what() << '\n';
        return 2;
    }
}
