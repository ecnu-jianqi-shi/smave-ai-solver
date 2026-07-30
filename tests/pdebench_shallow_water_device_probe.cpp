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

std::vector<double> read_field(hid_t file, std::size_t sample, hsize_t time) {
    std::ostringstream path;
    path << '/' << std::setw(4) << std::setfill('0') << sample << "/data";
    Handle dataset(H5Dopen2(file, path.str().c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {time, 0, 0, 0};
    const hsize_t count[] = {1, 128, 128, 1};
    if (H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start, nullptr,
                            count, nullptr) < 0) {
        throw std::runtime_error("shallow device hyperslab failed");
    }
    Handle memory_space(H5Screate_simple(4, count, nullptr), H5Sclose);
    std::vector<float> raw(128 * 128);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("shallow device field read failed");
    }
    std::vector<double> output(32 * 32);
    for (std::size_t row = 0; row < 32; ++row) {
        for (std::size_t column = 0; column < 32; ++column) {
            output[row * 32 + column] = raw[(row * 4) * 128 + column * 4];
        }
    }
    return output;
}

double residual(
    const std::vector<float>& right,
    const smave::DeviceExecutionResult& result,
    std::size_t batch,
    std::size_t width,
    double number) {
    if (!result.executed || result.output.size() != right.size()) return INFINITY;
    double maximum{};
    for (std::size_t sample = 0; sample < batch; ++sample) {
        const auto base = sample * width * width;
        double residual_squared{};
        double right_squared{};
        for (std::size_t row = 0; row < width; ++row) {
            const auto south = row == 0 ? width - 1 : row - 1;
            const auto north = row + 1 == width ? 0 : row + 1;
            for (std::size_t column = 0; column < width; ++column) {
                const auto west = column == 0 ? width - 1 : column - 1;
                const auto east = column + 1 == width ? 0 : column + 1;
                const auto index = row * width + column;
                const auto product = (1.0 + 4.0 * number) *
                    result.output[base + index] - number * (
                        result.output[base + row * width + west] +
                        result.output[base + row * width + east] +
                        result.output[base + south * width + column] +
                        result.output[base + north * width + column]);
                const auto difference = right[base + index] - product;
                residual_squared += difference * difference;
                right_squared += static_cast<double>(right[base + index]) *
                    right[base + index];
            }
        }
        maximum = std::max(maximum, std::sqrt(residual_squared) /
            std::max(1.0, std::sqrt(right_squared)));
    }
    return maximum;
}

void report(std::ostream& output, const char* name,
            const smave::DeviceExecutionResult& result,
            double wall_us, double original_residual) {
    output << name << "_AVAILABLE " << result.available << '\n'
           << name << "_EXECUTED " << result.executed << '\n'
           << name << "_DEVICE_VERIFIED " << result.verified << '\n'
           << name << "_BACKEND \"" << result.backend << "\"\n"
           << name << "_DEVICE \"" << result.device_name << "\"\n"
           << name << "_WALL_US " << wall_us << '\n'
           << name << "_UPLOAD_US " << result.upload_us << '\n'
           << name << "_KERNEL_US " << result.kernel_us << '\n'
           << name << "_DOWNLOAD_US " << result.download_us << '\n'
           << name << "_ORIGINAL_RELATIVE_RESIDUAL " << original_residual << '\n'
           << name << "_ORIGINAL_GATE_PASS "
           << (original_residual <= 1.0e-10) << '\n'
           << name << "_REASON \"" << result.reason << "\"\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::invalid_argument(
            "usage: shallow_device FILE ARTIFACT WORK OUTPUT");
        const std::filesystem::path input_path(argv[1]);
        const auto artifact =
            smave::LearnedPeriodicHelmholtzArtifact::read(argv[2]);
        const std::filesystem::path work(argv[3]);
        const std::filesystem::path output_path(argv[4]);
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr std::size_t batch = 3;
        const auto width = artifact.width;
        std::vector<float> inputs;
        for (std::size_t sample = 0; sample < batch; ++sample) {
            const auto previous = read_field(file.get(), sample, 0);
            const auto current = read_field(file.get(), sample, 1);
            for (std::size_t index = 0; index < width * width; ++index) {
                inputs.push_back(static_cast<float>(
                    2.0 * current[index] - previous[index]));
            }
        }
        smave::AcceleratePeriodicHelmholtz2DPlan plan(
            width, artifact.stencil_number);
        std::vector<float> weights(width * width * width * width);
        std::vector<double> impulse(width * width);
        impulse[0] = 1.0;
        std::vector<double> kernel;
        if (!plan.solve(impulse, kernel)) throw std::runtime_error("kernel solve failed");
        for (std::size_t output = 0; output < width * width; ++output) {
            const auto output_row = output / width;
            const auto output_column = output % width;
            for (std::size_t input = 0; input < width * width; ++input) {
                const auto input_row = input / width;
                const auto input_column = input % width;
                const auto delta_row = (output_row + width - input_row) % width;
                const auto delta_column =
                    (output_column + width - input_column) % width;
                weights[output * width * width + input] =
                    static_cast<float>(kernel[delta_row * width + delta_column]);
            }
        }
        const std::vector<float> bias(width * width);
        const auto metal_started = std::chrono::steady_clock::now();
        const auto metal = smave::metal_gpu_affine_batch(
            inputs, batch, width * width, weights, width * width, bias,
            1.0e-5, 1.0e-5);
        const auto metal_wall = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - metal_started).count();
        const auto metal_residual = residual(
            inputs, metal, batch, width, artifact.stencil_number);
        std::filesystem::create_directories(work);
        const auto ane_started = std::chrono::steady_clock::now();
        const auto ane = smave::coreml_neural_engine_affine_tensor_batch(
            inputs, batch, width * width, weights, width * width, bias, work,
            2.0e-2, 1.0e-3);
        const auto ane_wall = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - ane_started).count();
        const auto ane_residual = residual(
            inputs, ane, batch, width, artifact.stencil_number);
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path);
        output << std::setprecision(17)
               << "SMAVE_PDEBENCH_SHALLOW_WATER_DEVICE_PROBE 1\n"
               << "BATCH " << batch << "\nWIDTH " << width << '\n';
        report(output, "METAL", metal, metal_wall, metal_residual);
        report(output, "ANE", ane, ane_wall, ane_residual);
        output << "AUTO_SELECTED \"cpu-fp64-accelerate-fft\"\nEND\n";
        std::cout << "Shallow device Metal residual=" << metal_residual
                  << " ANE residual=" << ane_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Shallow device probe failed: " << error.what() << '\n';
        return 2;
    }
}
