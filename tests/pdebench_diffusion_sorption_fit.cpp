#include "smave/pdebench_training.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("usage: fit TRAIN HELDOUT ARTIFACT");
        const auto artifact = smave::fit_pdebench_frozen_retardation(argv[1], argv[2]);
        artifact.write(argv[3]);
        const auto verified = smave::LearnedFrozenRetardationArtifact::read(argv[3]);
        std::cout << "Learned retardation constant_ratio=" << verified.constant_ratio
                  << " power_ratio=" << verified.power_ratio
                  << " exponent=" << verified.concentration_exponent
                  << " training_residual=" << verified.training_maximum_relative_residual
                  << " heldout_residual=" << verified.heldout_maximum_relative_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench retardation fit failed: " << error.what() << '\n';
        return 2;
    }
}
