if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "HINTS schedule evidence is missing")
endif()

file(READ "${EVIDENCE}" evidence)
foreach(marker
    "SMAVE_HINTS_SCHEDULE_EVIDENCE 1"
    "published_method=HINTS"
    "published_paper_doi=10.1038/s42256-024-00910-x"
    "official_code_revision=0c8b712f81ed08bdf27c3a215f8edb99910f5e2f"
    "algorithmic_schedule_reimplementation=1"
    "official_public_code_executed=0"
    "deep_onet_architecture_reproduced=0"
    "official_pretrained_weights_used=0"
    "shared_latent_operator_weights=1"
    "evaluation_scenarios=64"
    "repetitions=100"
    "attempted=6400"
    "accepted=6400"
    "fallbacks=0"
    "failures=0"
    "gate_decision_mismatches=0"
    "same_accuracy=1"
    "linear_matrix_assembly_in_timing=0"
    "right_hand_side_update_in_timing=1"
    "residual_kernel=preassembled-linear-system-multiply"
    "numerical_method=weighted-diagonal-jacobi"
    "numerical_to_learned_ratio=25"
    "maximum_iterations=400"
    "published_full_implementation_claim=0"
    "negative_result_retained=1"
    "all_failures_retained=1"
    "END")
    string(FIND "${evidence}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "HINTS schedule evidence missing ${marker}")
    endif()
endforeach()

foreach(field
    average_iterations average_learned_corrections
    gate_residual_mismatches maximum_gate_residual_difference
    paired_median_speedup bootstrap_95_lower bootstrap_95_upper
    verified_operator_vs_hints_schedule_paired_median_speedup
    verified_operator_vs_hints_schedule_bootstrap_95_lower
    verified_operator_vs_hints_schedule_bootstrap_95_upper
    maximum_mixed_qoi_error)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" match "${evidence}")
    if(NOT match)
        message(FATAL_ERROR "HINTS schedule evidence lacks ${field}")
    endif()
    set(${field} "${CMAKE_MATCH_1}")
endforeach()

if(NOT average_iterations GREATER 0 OR NOT average_iterations LESS_EQUAL 400)
    message(FATAL_ERROR "HINTS schedule iteration count is invalid")
endif()
if(NOT average_learned_corrections GREATER 0)
    message(FATAL_ERROR "HINTS schedule never used the learned correction")
endif()
if(NOT bootstrap_95_upper LESS 1.0)
    message(FATAL_ERROR "HINTS schedule negative complete-cost result is not stable")
endif()
if(NOT verified_operator_vs_hints_schedule_bootstrap_95_lower GREATER 1.0)
    message(FATAL_ERROR "verified operator does not stably beat the HINTS schedule control")
endif()
if(maximum_mixed_qoi_error GREATER 1.0)
    message(FATAL_ERROR "HINTS schedule violates the common accuracy contract")
endif()
if(maximum_gate_residual_difference GREATER 1.0e-6)
    message(FATAL_ERROR "HINTS schedule residual kernel diverges from the reference gate")
endif()

message(STATUS "HINTS schedule contract, negative result, and accuracy passed")
