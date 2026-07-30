#include "smave/device.hpp"
#include "smave/pdebench_training.hpp"

#include <hdf5.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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

std::vector<double> read_velocity(
    hid_t file, hsize_t sample, hsize_t time, hsize_t component,
    std::size_t source_width, std::size_t width) {
    Handle dataset(H5Dopen2(file, "/velocity", H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {sample, time, 0, 0, component};
    const hsize_t count[] = {1, 1, source_width, source_width, 1};
    if (H5Sselect_hyperslab(file_space.get(), H5S_SELECT_SET, start, nullptr,
                            count, nullptr) < 0) {
        throw std::runtime_error("NS label hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(5, count, nullptr), H5Sclose);
    std::vector<float> raw(source_width * source_width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("NS label velocity read failed");
    }
    std::vector<double> output(width * width);
    for (std::size_t row = 0; row < width; ++row) {
        const auto source_row = row * source_width / width;
        for (std::size_t column = 0; column < width; ++column) {
            const auto source_column = column * source_width / width;
            output[row * width + column] = raw[source_row * source_width + source_column];
        }
    }
    return output;
}

double relative_residual(const std::vector<double>& right,
                         const std::vector<double>& solution,
                         std::size_t width, double diffusion) {
    double residual_squared{};
    double right_squared{};
    for (std::size_t row = 0; row < width; ++row) {
        const auto south = row == 0 ? width - 1 : row - 1;
        const auto north = row + 1 == width ? 0 : row + 1;
        for (std::size_t column = 0; column < width; ++column) {
            const auto west = column == 0 ? width - 1 : column - 1;
            const auto east = column + 1 == width ? 0 : column + 1;
            const auto index = row * width + column;
            const auto product = (1.0 + 4.0 * diffusion) * solution[index] -
                diffusion * (solution[row * width + west] +
                    solution[row * width + east] + solution[south * width + column] +
                    solution[north * width + column]);
            const auto residual = right[index] - product;
            residual_squared += residual * residual;
            right_squared += right[index] * right[index];
        }
    }
    return std::sqrt(residual_squared) /
        std::max(1.0, std::sqrt(right_squared));
}

std::uint64_t append_hash(const std::vector<double>& values, std::uint64_t hash) {
    for (const auto value : values) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            hash ^= bytes[index]; hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void write_tensor(const std::filesystem::path& path,
                  const std::vector<double>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(double)));
    if (!output) throw std::runtime_error("cannot write NS labels");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::invalid_argument(
            "usage: ns_labels FILE PREFIX SAMPLE TIME_SLICES");
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path prefix(argv[2]);
        const auto sample = std::stoull(argv[3]);
        const auto time_slices = std::stoull(argv[4]);
        constexpr std::size_t source_width = 512;
        constexpr std::size_t width = 64;
        constexpr std::size_t components = 2;
        constexpr double viscosity = 1.0e-3;
        constexpr double delta_time = 0.01;
        const auto delta_x = 1.0 / width;
        const auto diffusion = viscosity * delta_time / (delta_x * delta_x);
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        smave::AcceleratePeriodicHelmholtz2DPlan plan(width, diffusion);
        if (!plan.available()) throw std::runtime_error(plan.reason());
        std::vector<double> inputs;
        std::vector<double> targets;
        double maximum_residual{};
        for (std::size_t time = 0; time < time_slices; ++time) {
            for (std::size_t component = 0; component < components; ++component) {
                const auto right = read_velocity(
                    file.get(), sample, time, component, source_width, width);
                std::vector<double> solution;
                if (!plan.solve(right, solution)) throw std::runtime_error("NS FFT label solve failed");
                maximum_residual = std::max(maximum_residual,
                    relative_residual(right, solution, width, diffusion));
                inputs.insert(inputs.end(), right.begin(), right.end());
                targets.insert(targets.end(), solution.begin(), solution.end());
            }
        }
        if (maximum_residual > 1.0e-12) {
            throw std::runtime_error("NS labels failed original residual gate");
        }
        std::filesystem::create_directories(prefix.parent_path());
        write_tensor(prefix.string() + ".inputs.f64", inputs);
        write_tensor(prefix.string() + ".targets.f64", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << smave::kPdebenchTrainingSetSchemaVersion << '\n'
                 << "FAMILY \"ns-incompressible\"\n"
                 << "SOURCE \"" << input_path.string() << "#samples="
                 << sample << ':' << sample + 1 << ";times=0:" << time_slices
                 << ";components=0:2;grid=64\"\n"
                 << "SAMPLES " << time_slices * components << "\n"
                 << "VALUES_PER_SAMPLE " << width * width << "\n"
                 << "TARGET_KIND \"same-discrete-operator-solver-label\"\n"
                 << "SOLVER_LABEL 1\n"
                 << "DISCRETE_OPERATOR_ID \"periodic-viscous-helmholtz-v1\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 1\n"
                 << "DTYPE \"fp64\"\nLAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16)
                 << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write NS manifest");
        manifest.close();
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "periodic-viscous-helmholtz-v1");
        std::cout << "NS labels=" << time_slices * components
                  << " residual=" << maximum_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "NS label export failed: " << error.what() << '\n';
        return 2;
    }
}
