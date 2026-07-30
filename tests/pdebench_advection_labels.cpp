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

std::vector<double> read_slice(
    hid_t dataset, hsize_t sample, hsize_t time, hsize_t width) {
    Handle file_space(H5Dget_space(dataset), H5Sclose);
    const hsize_t start[] = {sample, time, 0};
    const hsize_t count[] = {1, 1, width};
    if (H5Sselect_hyperslab(
            file_space.get(), H5S_SELECT_SET, start, nullptr, count, nullptr) < 0) {
        throw std::runtime_error("HDF5 hyperslab selection failed");
    }
    Handle memory_space(H5Screate_simple(3, count, nullptr), H5Sclose);
    std::vector<float> raw(width);
    if (H5Dread(dataset, H5T_NATIVE_FLOAT, memory_space.get(), file_space.get(),
                H5P_DEFAULT, raw.data()) < 0) {
        throw std::runtime_error("HDF5 tensor read failed");
    }
    return {raw.begin(), raw.end()};
}

std::vector<double> solve_periodic_implicit_upwind(
    const std::vector<double>& right_hand_side, double courant) {
    const auto diagonal = 1.0 + courant;
    std::vector<double> affine(right_hand_side.size());
    std::vector<double> coefficient(right_hand_side.size());
    affine.front() = right_hand_side.front() / diagonal;
    coefficient.front() = courant / diagonal;
    for (std::size_t index = 1; index < right_hand_side.size(); ++index) {
        affine[index] =
            (right_hand_side[index] + courant * affine[index - 1]) / diagonal;
        coefficient[index] = courant * coefficient[index - 1] / diagonal;
    }
    const auto last = affine.back() / (1.0 - coefficient.back());
    std::vector<double> solution(right_hand_side.size());
    for (std::size_t index = 0; index < solution.size(); ++index) {
        solution[index] = affine[index] + coefficient[index] * last;
    }
    return solution;
}

double relative_residual(
    const std::vector<double>& right_hand_side,
    const std::vector<double>& solution,
    double courant) {
    double residual_squared{};
    double right_hand_side_squared{};
    for (std::size_t index = 0; index < solution.size(); ++index) {
        const auto previous = index == 0 ? solution.size() - 1 : index - 1;
        const auto residual = right_hand_side[index] -
            ((1.0 + courant) * solution[index] - courant * solution[previous]);
        residual_squared += residual * residual;
        right_hand_side_squared += right_hand_side[index] * right_hand_side[index];
    }
    return std::sqrt(residual_squared) /
        std::max(1.0, std::sqrt(right_hand_side_squared));
}

std::uint64_t append_hash(
    const std::vector<double>& values, std::uint64_t hash) {
    for (const auto value : values) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t index = 0; index < sizeof(value); ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void write_tensor(
    const std::filesystem::path& path,
    const std::vector<double>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(double)));
    if (!output) throw std::runtime_error("cannot write solver-label tensor");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 6) {
            throw std::invalid_argument(
                "usage: smave_pdebench_advection_labels FILE OUTPUT_PREFIX "
                "SAMPLE_BEGIN SAMPLE_COUNT STEPS");
        }
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path prefix(argv[2]);
        const auto sample_begin = std::stoull(argv[3]);
        const auto sample_count = std::stoull(argv[4]);
        const auto requested_steps = std::stoull(argv[5]);
        if (sample_count == 0 || requested_steps == 0) {
            throw std::invalid_argument("solver-label split must be nonempty");
        }

        Handle file(H5Fopen(input_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        Handle tensor(H5Dopen2(file.get(), "tensor", H5P_DEFAULT), H5Dclose);
        Handle tensor_space(H5Dget_space(tensor.get()), H5Sclose);
        hsize_t dimensions[3]{};
        if (H5Sget_simple_extent_dims(tensor_space.get(), dimensions, nullptr) != 3 ||
            sample_begin + sample_count > dimensions[0] ||
            requested_steps >= dimensions[1] || dimensions[2] < 2) {
            throw std::invalid_argument("solver-label split exceeds HDF5 tensor shape");
        }
        Handle beta_attribute(H5Aopen(file.get(), "beta", H5P_DEFAULT), H5Aclose);
        double beta{};
        if (H5Aread(beta_attribute.get(), H5T_NATIVE_DOUBLE, &beta) < 0) {
            throw std::runtime_error("cannot read advection beta");
        }
        constexpr double delta_time = 0.01;
        const auto width = static_cast<std::size_t>(dimensions[2]);
        const double courant = beta * delta_time * static_cast<double>(width);
        std::vector<double> inputs;
        std::vector<double> targets;
        inputs.reserve(sample_count * requested_steps * width);
        targets.reserve(sample_count * requested_steps * width);
        double maximum_residual{};
        for (std::size_t sample_offset = 0; sample_offset < sample_count;
             ++sample_offset) {
            auto state = read_slice(
                tensor.get(), sample_begin + sample_offset, 0, dimensions[2]);
            for (std::size_t step = 0; step < requested_steps; ++step) {
                auto solution = solve_periodic_implicit_upwind(state, courant);
                maximum_residual = std::max(
                    maximum_residual, relative_residual(state, solution, courant));
                inputs.insert(inputs.end(), state.begin(), state.end());
                targets.insert(targets.end(), solution.begin(), solution.end());
                state.swap(solution);
            }
        }
        if (!std::isfinite(maximum_residual) || maximum_residual > 1.0e-12) {
            throw std::runtime_error(
                "generated advection label failed original FP64 residual gate");
        }

        std::filesystem::create_directories(prefix.parent_path());
        write_tensor(prefix.string() + ".inputs.f64", inputs);
        write_tensor(prefix.string() + ".targets.f64", targets);
        auto hash = append_hash(inputs, 1469598103934665603ULL);
        hash = append_hash(targets, hash);
        std::ofstream manifest(prefix.string() + ".manifest.txt");
        manifest << smave::kPdebenchTrainingSetSchemaVersion << '\n'
                 << "FAMILY \"advection\"\n"
                 << "SOURCE \"" << input_path.string()
                 << "#samples=" << sample_begin << ':'
                 << sample_begin + sample_count << ";steps=0:"
                 << requested_steps << "\"\n"
                 << "SAMPLES " << sample_count * requested_steps << "\n"
                 << "VALUES_PER_SAMPLE " << width << "\n"
                 << "TARGET_KIND \"same-discrete-operator-solver-label\"\n"
                 << "SOLVER_LABEL 1\n"
                 << "DISCRETE_OPERATOR_ID \"periodic-implicit-upwind-v1\"\n"
                 << "ORIGINAL_RESIDUAL_CERTIFIED 1\n"
                 << "DTYPE \"fp64\"\n"
                 << "LAYOUT \"sample-major-contiguous\"\n"
                 << "CHECKSUM \"" << std::hex << std::setw(16)
                 << std::setfill('0') << hash << "\"\nEND\n";
        if (!manifest) throw std::runtime_error("cannot write solver-label manifest");
        manifest.close();
        smave::PdebenchTrainingManifest::read_and_verify(
            prefix, smave::PdebenchTrainingUse::DirectDeployment,
            "periodic-implicit-upwind-v1");
        std::cout << "PDEBench advection solver labels="
                  << sample_count * requested_steps
                  << " width=" << width
                  << " maximum_residual=" << maximum_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench advection solver-label export failed: "
                  << error.what() << '\n';
        return 2;
    }
}
