#include "smave/pdebench_training.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 4) {
            throw std::invalid_argument(
                "usage: smave_pdebench_advection_fit TRAIN_PREFIX HELDOUT_PREFIX ARTIFACT");
        }
        const auto artifact = smave::fit_pdebench_periodic_recurrence(
            argv[1], argv[2], "periodic-implicit-upwind-v1");
        artifact.write(argv[3]);
        const auto verified =
            smave::LearnedPeriodicRecurrenceArtifact::read(argv[3]);
        std::cout << "Learned periodic recurrence inverse_diagonal="
                  << verified.inverse_diagonal
                  << " feedback=" << verified.feedback
                  << " training_residual="
                  << verified.training_maximum_relative_residual
                  << " heldout_residual="
                  << verified.heldout_maximum_relative_residual << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDEBench advection fit failed: " << error.what() << '\n';
        return 2;
    }
}
