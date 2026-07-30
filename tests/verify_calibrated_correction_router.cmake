if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_CALIBRATED_CORRECTION_ROUTER 1"
        "contract=production-router-propagates-profiled-correction-budget"
        "budget0.plan_budget=0"
        "budget0.full_fallback=1"
        "budget2.plan_budget=2"
        "budget2.warm_start_accept=1"
        "budget2.expert_iterations=2"
        "budget2.original_equation_gate_accept=1"
        "zero_budget_raw_residual_check_preserved=1"
        "numerical_fallback_preserved=1"
        "END")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "calibrated correction Router evidence missing marker: ${marker}")
    endif()
endforeach()
