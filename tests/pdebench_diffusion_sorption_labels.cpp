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

std::string group_name(std::size_t sample) {
    std::ostringstream output;
    output << '/' << std::setw(4) << std::setfill('0') << sample;
    return output.str();
}

std::vector<double> read_vector(hid_t dataset) {
    Handle space(H5Dget_space(dataset), H5Sclose);
    hsize_t dimensions[1]{};
    if (H5Sget_simple_extent_dims(space.get(), dimensions, nullptr) != 1) {
        throw std::runtime_error("unexpected vector shape");
    }
    std::vector<float> raw(dimensions[0]);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 vector read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> read_state(hid_t dataset, hsize_t time, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {time, 0, 0};
    const hsize_t count[] = {1, width, 1};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 state read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> solve_tridiagonal(
    const std::vector<double>& lower,
    const std::vector<double>& diagonal,
    const std::vector<double>& upper,
    const std::vector<double>& right) {
    const auto size = diagonal.size();
    std::vector<double> modified_upper(size);
    std::vector<double> modified_right(size);
    auto pivot = diagonal.front();
    modified_upper.front() = upper.front() / pivot;
    modified_right.front() = right.front() / pivot;
    for (std::size_t index = 1; index < size; ++index) {
        pivot = diagonal[index] - lower[index] * modified_upper[index - 1];
        if (std::abs(pivot) < 1.0e-14) throw std::runtime_error("zero pivot");
        modified_upper[index] = index + 1 < size ? upper[index] / pivot : 0.0;
        modified_right[index] =
            (right[index] - lower[index] * modified_right[index - 1]) / pivot;
    }
    std::vector<double> solution(size);
    solution.back() = modified_right.back();
    for (std::size_t index = size - 1; index-- > 0;) {
        solution[index] =
            modified_right[index] - modified_upper[index] * solution[index + 1];
    }
    return solution;
}

struct System {
    std::vector<double> lower;
    std::vector<double> diagonal;
    std::vector<double> upper;
    std::vector<double> right;
};

System assemble(const std::vector<double>& state, double number) {
    constexpr double porosity = 0.29;
    constexpr double solid_density = 2880.0;
    constexpr double freundlich_coefficient = 0.00035;
    constexpr double freundlich_exponent = 0.874;
    const auto unknowns = state.size() - 1;
    System system{
        std::vector<double>(unknowns), std::vector<double>(unknowns),
        std::vector<double>(unknowns), std::vector<double>(unknowns)};
    for (std::size_t local = 0; local < unknowns; ++local) {
        const auto index = local + 1;
        const auto concentration = std::max(state[index], 1.0e-8);
        const auto retardation = porosity + solid_density *
            freundlich_coefficient * freundlich_exponent *
            std::pow(concentration, freundlich_exponent - 1.0);
        system.lower[local] = local > 0 ? -number : 0.0;
        system.diagonal[local] = retardation + 2.0 * number;
        system.upper[local] = local + 1 < unknowns ? -number : 0.0;
        system.right[local] = retardation * state[index];
    }
    system.right.front() += number;
    system.diagonal.back() -= number;
    return system;
}

double relative_residual(const System& system, const std::vector<double>& solution) {
    double residual_squared{};
    double right_squared{};
    for (std::size_t index = 0; index < solution.size(); ++index) {
        auto product = system.diagonal[index] * solution[index];
        if (index > 0) product += system.lower[index] * solution[index - 1];
        if (index + 1 < solution.size()) {
            product += system.upper[index] * solution[index + 1];
        }
        const auto residual = system.right[index] - product;
        residual_squared += residual * residual;
        right_squared += system.right[index] * system.right[index];
    }
    return std::sqrt(residual_squared) /
        std::max(1.0, std::sqrt(right_squared));
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
    if (!output) throw std::runtime_error("cannot write diffusion solver labels");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 6) {
            throw std::invalid_argument(
                "usage: smave_pdebench_diffusion_sorption_labels FILE PREFIX "
                "SAMPLE_BEGIN SAMPLE_COUNT STEPS");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path prefix(argv[2]);
        const auto sample_begin = std::stoull(argv[3]);
        const auto sample_count = std::stoull(argv[4]);
        const auto steps = std::stoull(argv[5]);
        if (sample_count == 0 || steps == 0) {
            throw std::invalid_argument("solver-label split must be nonempty");
        }
        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        constexpr double diffusion = 0.0005;
        std::vector<double> inputs;
        std::vector<double> targets;
        std::size_t width{};
        double maximum_residual{};
        for (std::size_t sample_offset = 0; sample_offset < sample_count;
             ++sample_offset) {
            const auto sample = sample_begin + sample_offset;
            const auto group = group_name(sample);
            Handle data(H5Dopen2(
                file.get(), (group + "/data").c_str(), H5P_DEFAULT), H5Dclose);
            Handle times_dataset(H5Dopen2(
                file.get(), (group + "/grid/t").c_str(), H5P_DEFAULT), H5Dclose);
            Handle grid_dataset(H5Dopen2(
                file.get(), (group + "/grid/x").c_str(), H5P_DEFAULT), H5Dclose);
            const auto times = read_vector(times_dataset.get());
            const auto grid = read_vector(grid_dataset.get());
            if (steps >= times.size() || grid.size() < 3) {
                throw std::invalid_argument("diffusion split exceeds group shape");
            }
            if (width == 0) width = grid.size();
            if (grid.size() != width) {
                throw std::runtime_error("diffusion training grids differ");
            }
            auto state = read_state(data.get(), 0, width);
            for (std::size_t step = 0; step < steps; ++step) {
                const auto delta_x = grid[1] - grid[0];
                const auto delta_time = times[step + 1] - times[step];
                const auto number = diffusion * delta_time / (delta_x * delta_x);
                const auto system = assemble(state, number);
                const auto unknown_solution = solve_tridiagonal(
                    system.lower, system.diagonal, system.upper, system.right);
                maximum_residual = std::max(
                    maximum_residual, relative_residual(system, unknown_solution));
                std::vector<double> solution(width, 1.0);
                std::copy(unknown_solution.begin(), unknown_solution.end(),
                          solution.begin() + 1);
                inputs.insert(inputs.end(), state.begin(), state.end());
                targets.insert(targets.end(), solution.begin(), solution.end());
                state.swap(solution);
            }
        }
        if (!std::isfinite(maximum_residual) || maximum_residual > 1.0e-12) {
            throw std::runtime_error("diffusion labels failed original FP64 gate");
        }
        std::filesystem::create_directories(prefix.parent_path());
        write_tensor(prefix.string() + ".inputs.f64", inputs);
        write_tensor(prefix.string() + ".targets.f64", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << smave::kPdebenchTrainingSetSchemaVersion << '\n'
                 << "FAMILY \"diffusion-sorption\"\n"
                 << "SOURCE \"" << input_path.string()
                 << "#samples=" << sample_begin << ':'
                 << sample_begin + sample_count << ";steps=0:" << steps << "\"\n"
                 << "SAMPLES " << sample_count * steps << "\n"
                 << "VALUES_PER_SAMPLE " << width << "\n"
                 << "TARGET_KIND \"same-discrete-operator-solver-label\"\n"
                 << "SOLVER_LABEL 1\n"
                 << "DISCRETE_OPERATOR_ID \"frozen-retardation-tridiagonal-v1\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 1\n"
                 << "DTYPE \"fp64\"\n"
                 << "LAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16)
                 << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write diffusion manifest");
        manifest.close();
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "frozen-retardation-tridiagonal-v1");
        std::cout << "PDEBench diffusion solver labels="
                  << sample_count * steps << " width=" << width
                  << " maximum_residual=" << maximum_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench diffusion label export failed: "
                  << error.what() << '\n';
        return 2;
    }
}
