#include "smave/pdebench_training.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("usage: shallow_fit TRAIN HELDOUT ARTIFACT");
        auto artifact = smave::fit_pdebench_periodic_helmholtz(
            argv[1], argv[2], "shallow-water", "periodic-wave-helmholtz-v1");
        artifact.write(argv[3]);
        const auto verified = smave::LearnedPeriodicHelmholtzArtifact::read(argv[3]);
        std::cout << "Learned shallow Helmholtz number=" << verified.stencil_number
                  << " training_residual=" << verified.training_maximum_relative_residual
                  << " heldout_residual=" << verified.heldout_maximum_relative_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Shallow-water fit failed: " << error.what() << '\n';
        return 2;
    }
}
