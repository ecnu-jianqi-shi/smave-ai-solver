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

std::vector<double> read_field(
    hid_t file, std::size_t sample, hsize_t time, std::size_t source_width) {
    std::ostringstream path;
    path << '/' << std::setw(4) << std::setfill('0') << sample << "/data";
    Handle dataset(H5Dopen2(file, path.str().c_str(), H5P_DEFAULT), H5Dclose);
    Handle file_space(H5Dget_space(dataset.get()), H5Sclose);
    const hsize_t start[] = {time, 0, 0, 0};
    const hsize_t count[] = {1, source_width, source_width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("shallow-water hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(4, count, nullptr), H5Sclose);
    std::vector<float> raw(source_width * source_width);
    if (H5Dread(dataset.get(), H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("shallow-water field read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> downsample(
    const std::vector<double>& field,
    std::size_t source_width,
    std::size_t width) {
    std::vector<double> output(width * width);
    for (std::size_t row = 0; row < width; ++row) {
        const auto source_row = row * source_width / width;
        for (std::size_t column = 0; column < width; ++column) {
            const auto source_column = column * source_width / width;
            output[row * width + column] =
                field[source_row * source_width + source_column];
        }
    }
    return output;
}

double relative_residual(
    const std::vector<double>& right,
    const std::vector<double>& solution,
    std::size_t width,
    double wave_number) {
    double residual_squared{};
    double right_squared{};
    for (std::size_t row = 0; row < width; ++row) {
        const auto south = row == 0 ? width - 1 : row - 1;
        const auto north = row + 1 == width ? 0 : row + 1;
        for (std::size_t column = 0; column < width; ++column) {
            const auto west = column == 0 ? width - 1 : column - 1;
            const auto east = column + 1 == width ? 0 : column + 1;
            const auto index = row * width + column;
            const auto product = (1.0 + 4.0 * wave_number) * solution[index] -
                wave_number * (solution[row * width + west] +
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
    if (!output) throw std::runtime_error("cannot write shallow-water labels");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 6) throw std::invalid_argument(
            "usage: shallow_labels FILE PREFIX SAMPLE_BEGIN SAMPLE_COUNT STEPS");
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path prefix(argv[2]);
        const auto sample_begin = std::stoull(argv[3]);
        const auto sample_count = std::stoull(argv[4]);
        const auto steps = std::stoull(argv[5]);
        constexpr std::size_t source_width = 128;
        constexpr std::size_t width = 32;
        constexpr double delta_time = 0.01;
        constexpr double domain_width = 5.0;
        const auto delta_x = domain_width / width;
        const auto wave_number = delta_time * delta_time / (delta_x * delta_x);
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        smave::AcceleratePeriodicHelmholtz2DPlan plan(width, wave_number);
        if (!plan.available()) throw std::runtime_error(plan.reason());
        std::vector<double> inputs;
        std::vector<double> targets;
        double maximum_residual{};
        for (std::size_t offset = 0; offset < sample_count; ++offset) {
            auto previous = downsample(read_field(
                file.get(), sample_begin + offset, 0, source_width), source_width, width);
            auto current = downsample(read_field(
                file.get(), sample_begin + offset, 1, source_width), source_width, width);
            for (std::size_t step = 0; step < steps; ++step) {
                std::vector<double> right(width * width);
                for (std::size_t index = 0; index < right.size(); ++index) {
                    right[index] = 2.0 * current[index] - previous[index];
                }
                std::vector<double> solution;
                if (!plan.solve(right, solution)) throw std::runtime_error("FFT label solve failed");
                maximum_residual = std::max(maximum_residual,
                    relative_residual(right, solution, width, wave_number));
                inputs.insert(inputs.end(), right.begin(), right.end());
                targets.insert(targets.end(), solution.begin(), solution.end());
                previous = current;
                current = std::move(solution);
            }
        }
        if (maximum_residual > 1.0e-12) {
            throw std::runtime_error("shallow-water labels failed FP64 residual gate");
        }
        std::filesystem::create_directories(prefix.parent_path());
        write_tensor(prefix.string() + ".inputs.f64", inputs);
        write_tensor(prefix.string() + ".targets.f64", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << smave::kPdebenchTrainingSetSchemaVersion << '\n'
                 << "FAMILY \"shallow-water\"\n"
                 << "SOURCE \"" << input_path.string() << "#samples="
                 << sample_begin << ':' << sample_begin + sample_count
                 << ";steps=0:" << steps << ";grid=32\"\n"
                 << "SAMPLES " << sample_count * steps << "\n"
                 << "VALUES_PER_SAMPLE 1024\n"
                 << "TARGET_KIND \"same-discrete-operator-solver-label\"\n"
                 << "SOLVER_LABEL 1\n"
                 << "DISCRETE_OPERATOR_ID \"periodic-wave-helmholtz-v1\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 1\n"
                 << "DTYPE \"fp64\"\nLAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16)
                 << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write shallow manifest");
        manifest.close();
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "periodic-wave-helmholtz-v1");
        std::cout << "Shallow-water labels=" << sample_count * steps
                  << " residual=" << maximum_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Shallow-water label export failed: " << error.what() << '\n';
        return 2;
    }
}
