#include "smave/pdebench_training.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_burgers_fit TRAIN HELDOUT ARTIFACT");
        }
        const auto artifact =
            smave::fit_pdebench_frozen_burgers(argv[1], argv[2]);
        artifact.write(argv[3]);
        const auto verified = smave::LearnedFrozenBurgersArtifact::read(argv[3]);
        std::cout << "Learned frozen Burgers diffusion_number="
                  << verified.diffusion_number
                  << " convection_scale=" << verified.convection_scale
                  << " training_residual="
                  << verified.training_maximum_relative_residual
                  << " heldout_residual="
                  << verified.heldout_maximum_relative_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench Burgers fit failed: " << error.what() << '\n';
        return 2;
    }
}
