if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "native HINTS baseline evidence is missing")
endif()
file(READ "${EVIDENCE}" evidence)
foreach(marker
        "SMAVE_HINTS_NATIVE_BASELINE_EVIDENCE 1"
        "official_public_code_executed=1"
        "official_pretrained_weights_used=1"
        "official_deeponet_architecture_executed=1"
        "common_test_cases=750"
        "common_original_equation_gate=1"
        "smave_default_production_router=1"
        "official_failures=0"
        "smave_failures=0"
        "END")
    string(FIND "${evidence}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native HINTS baseline missing ${marker}")
    endif()
endforeach()
foreach(field
        paired_median_speedup
        bootstrap_95_lower
        bootstrap_95_upper
        amortized_speedup
        maximum_common_gate_relative_inf)
    string(REGEX MATCH "${field}=([0-9.eE+-]+)" match "${evidence}")
    if(NOT match)
        message(FATAL_ERROR "native HINTS baseline lacks ${field}")
    endif()
    set(${field} "${CMAKE_MATCH_1}")
endforeach()
if(NOT bootstrap_95_lower GREATER 1.0)
    message(FATAL_ERROR "native HINTS paired lower bound no longer exceeds one")
endif()
if(NOT amortized_speedup GREATER 1.0)
    message(FATAL_ERROR "native HINTS amortized comparison no longer exceeds one")
endif()
if(NOT maximum_common_gate_relative_inf LESS_EQUAL 1.0e-10)
    message(FATAL_ERROR "native HINTS common residual gate regressed")
endif()
message(STATUS "official native HINTS baseline, paired timing, and gate passed")
