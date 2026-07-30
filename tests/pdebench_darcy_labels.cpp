#include "smave/linear.hpp"
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

std::vector<double> read_coefficient(
    hid_t dataset, hsize_t sample, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, 0, 0};
    const hsize_t count[] = {1, width, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 coefficient selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width * width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 coefficient read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> downsample_square(
    const std::vector<double>& field,
    std::size_t source_width,
    std::size_t width) {
    std::vector<double> output(width * width);
    for (std::size_t row = 0; row < width; ++row) {
        const auto source_row = row * (source_width - 1) / (width - 1);
        for (std::size_t column = 0; column < width; ++column) {
            const auto source_column = column * (source_width - 1) / (width - 1);
            output[row * width + column] =
                field[source_row * source_width + source_column];
        }
    }
    return output;
}

double harmonic(double left, double right) {
    return 2.0 * left * right / (left + right);
}

struct Stencil {
    std::vector<double> west;
    std::vector<double> east;
    std::vector<double> south;
    std::vector<double> north;
    std::vector<double> diagonal;
};

Stencil assemble(
    const std::vector<double>& coefficient,
    std::size_t width,
    double spacing_scale) {
    const auto interior = width - 2;
    const auto unknowns = interior * interior;
    Stencil stencil{
        std::vector<double>(unknowns), std::vector<double>(unknowns),
        std::vector<double>(unknowns), std::vector<double>(unknowns),
        std::vector<double>(unknowns)};
    for (std::size_t row = 1; row + 1 < width; ++row) {
        for (std::size_t column = 1; column + 1 < width; ++column) {
            const auto full = row * width + column;
            const auto local = (row - 1) * interior + column - 1;
            const auto center = coefficient[full];
            stencil.west[local] =
                harmonic(center, coefficient[full - 1]) * spacing_scale;
            stencil.east[local] =
                harmonic(center, coefficient[full + 1]) * spacing_scale;
            stencil.south[local] =
                harmonic(center, coefficient[full - width]) * spacing_scale;
            stencil.north[local] =
                harmonic(center, coefficient[full + width]) * spacing_scale;
            stencil.diagonal[local] = stencil.west[local] + stencil.east[local] +
                stencil.south[local] + stencil.north[local];
        }
    }
    return stencil;
}

double relative_residual(
    const Stencil& stencil,
    const std::vector<double>& solution,
    std::size_t interior) {
    double residual_squared{};
    const auto unknowns = interior * interior;
    for (std::size_t row = 0; row < interior; ++row) {
        for (std::size_t column = 0; column < interior; ++column) {
            const auto index = row * interior + column;
            auto product = stencil.diagonal[index] * solution[index];
            if (column > 0) product -= stencil.west[index] * solution[index - 1];
            if (column + 1 < interior) {
                product -= stencil.east[index] * solution[index + 1];
            }
            if (row > 0) product -= stencil.south[index] * solution[index - interior];
            if (row + 1 < interior) {
                product -= stencil.north[index] * solution[index + interior];
            }
            const auto residual = 1.0 - product;
            residual_squared += residual * residual;
        }
    }
    return std::sqrt(residual_squared) / std::sqrt(static_cast<double>(unknowns));
}

std::uint64_t append_hash(const std::vector<double>& values, std::uint64_t hash) {
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
                  const std::vector<double>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(double)));
    if (!output) throw std::runtime_error("cannot write Darcy solver labels");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            throw std::invalid_argument(
                "usage: smave_pdebench_darcy_labels FILE PREFIX SAMPLE_BEGIN SAMPLE_COUNT");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path prefix(argv[2]);
        const auto sample_begin = std::stoull(argv[3]);
        const auto sample_count = std::stoull(argv[4]);
        if (sample_count == 0) throw std::invalid_argument("empty Darcy split");
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle coefficient_dataset(H5Dopen2(file.get(), "nu", H5P_DEFAULT), H5Dclose);
        Handle space(H5Dget_space(coefficient_dataset.get()), H5Sclose);
        hsize_t dimensions[3]{};
        if (H5Sget_simple_extent_dims(space.get(), dimensions, nullptr) != 3 ||
            dimensions[1] != dimensions[2] ||
            sample_begin + sample_count > dimensions[0]) {
            throw std::invalid_argument("Darcy split exceeds coefficient tensor");
        }
        constexpr std::size_t width = 32;
        constexpr std::size_t interior = width - 2;
        const auto spacing = 1.0 / static_cast<double>(width - 1);
        const auto spacing_scale = 1.0 / (spacing * spacing);
        const std::vector<double> right(interior * interior, 1.0);
        std::vector<double> inputs;
        std::vector<double> targets;
        inputs.reserve(sample_count * width * width);
        targets.reserve(sample_count * width * width);
        double maximum_residual{};
        for (std::size_t offset = 0; offset < sample_count; ++offset) {
            const auto coefficient = downsample_square(
                read_coefficient(
                    coefficient_dataset.get(), sample_begin + offset, dimensions[1]),
                dimensions[1], width);
            const auto stencil = assemble(coefficient, width, spacing_scale);
            const auto solved = smave::accelerate_five_point_spd_direct_solve(
                interior, stencil.west, stencil.east, stencil.south, stencil.north,
                stencil.diagonal, right);
            if (!solved.solved) {
                throw std::runtime_error("Darcy FP64 label solve failed: " + solved.reason);
            }
            maximum_residual = std::max(maximum_residual,
                relative_residual(stencil, solved.solution, interior));
            std::vector<double> full_solution(width * width);
            for (std::size_t row = 0; row < interior; ++row) {
                std::copy_n(solved.solution.begin() + row * interior, interior,
                            full_solution.begin() + (row + 1) * width + 1);
            }
            inputs.insert(inputs.end(), coefficient.begin(), coefficient.end());
            targets.insert(targets.end(), full_solution.begin(), full_solution.end());
        }
        if (!std::isfinite(maximum_residual) || maximum_residual > 1.0e-10) {
            throw std::runtime_error("Darcy labels failed original FP64 residual gate");
        }
        std::filesystem::create_directories(prefix.parent_path());
        write_tensor(prefix.string() + ".inputs.f64", inputs);
        write_tensor(prefix.string() + ".targets.f64", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << smave::kPdebenchTrainingSetSchemaVersion << '\n'
                 << "FAMILY \"darcy\"\n"
                 << "SOURCE \"" << input_path.string()
                 << "#samples=" << sample_begin << ':'
                 << sample_begin + sample_count << ";grid=32\"\n"
                 << "SAMPLES " << sample_count << "\n"
                 << "VALUES_PER_SAMPLE " << width * width << "\n"
                 << "TARGET_KIND \"same-discrete-operator-solver-label\"\n"
                 << "SOLVER_LABEL 1\n"
                 << "DISCRETE_OPERATOR_ID \"variable-darcy-five-point-v1\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 1\n"
                 << "DTYPE \"fp64\"\n"
                 << "LAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16)
                 << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write Darcy manifest");
        manifest.close();
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "variable-darcy-five-point-v1");
        std::cout << "PDEBench Darcy solver labels=" << sample_count
                  << " values=" << width * width
                  << " maximum_residual=" << maximum_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench Darcy label export failed: " << error.what() << '\n';
        return 2;
    }
}
