#include "smave/pdebench_training.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("usage: darcy_fit TRAIN HELDOUT PREFIX");
        auto artifact = smave::fit_pdebench_darcy_nearest(argv[1], argv[2], 4);
        artifact.write(argv[3]);
        const auto verified = smave::LearnedDarcyNearestArtifact::read(argv[3]);
        std::cout << "Learned Darcy nearest prototypes=" << verified.prototypes
                  << " heldout_mean_error=" << verified.heldout_mean_relative_inf_error
                  << " heldout_max_error=" << verified.heldout_maximum_relative_inf_error
                  << " heldout_max_residual=" << verified.heldout_maximum_relative_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Darcy nearest fit failed: " << error.what() << '\n';
        return 2;
    }
}
