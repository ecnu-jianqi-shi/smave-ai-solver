foreach(required COUPLED_DIR CUBIC_DIR)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing nonlinear cascade evidence directory: ${required}")
    endif()
endforeach()

foreach(family_dir "${COUPLED_DIR}" "${CUBIC_DIR}")
    foreach(comparison classic-vs-fixed classic-vs-online fixed-vs-online)
        set(report_path "${family_dir}/${comparison}.txt")
        if(NOT EXISTS "${report_path}")
            message(FATAL_ERROR "missing nonlinear cascade report: ${report_path}")
        endif()
        file(READ "${report_path}" report)
        foreach(required "SMAVE_PERFORMANCE 1" "repetitions=100" "bootstrap_samples=10000"
                         "baseline_failures=0" "accelerated_failures=0"
                         "gate_mismatches=0" "same_accuracy=1")
            string(FIND "${report}" "${required}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "nonlinear cascade report missing ${required}: ${report_path}")
            endif()
        endforeach()
    endforeach()

    file(READ "${family_dir}/evidence.txt" evidence)
    string(REGEX MATCH "EXPERT_VERSION \"([^\"]+)\"" expert_match "${evidence}")
    if(NOT expert_match)
        message(FATAL_ERROR "nonlinear cascade expert identity missing: ${family_dir}")
    endif()
    set(expert_version "${CMAKE_MATCH_1}")
    foreach(plan classic fixed online)
        file(READ "${family_dir}/${plan}-plan.txt" ${plan}_plan)
        if(NOT ${plan}_plan MATCHES "PLAN_ID \"[^\"]+\"")
            message(FATAL_ERROR "nonlinear cascade ${plan} plan id missing: ${family_dir}")
        endif()
    endforeach()
    if(NOT fixed_plan MATCHES "PLAN_STEPS 1")
        message(FATAL_ERROR "fixed AI cascade must contain exactly one routed expert: ${family_dir}")
    endif()
    string(FIND "${fixed_plan}" "PLAN_STEP 0 EXPERT \"${expert_version}\"" fixed_expert)
    if(fixed_expert EQUAL -1)
        message(FATAL_ERROR "fixed AI cascade does not force the affine expert: ${family_dir}")
    endif()
    string(FIND "${online_plan}" "EXPERT \"${expert_version}\"" online_affine)
    string(FIND "${online_plan}" "EXPERT \"continuation-warm-start-v1\"" online_continuation)
    if(online_affine EQUAL -1 OR online_continuation EQUAL -1)
        message(FATAL_ERROR "online Equation-MoE plan lacks affine or continuation expert: ${family_dir}")
    endif()
    if(fixed_plan STREQUAL online_plan)
        message(FATAL_ERROR "fixed cascade and online Equation-MoE plans are identical: ${family_dir}")
    endif()
    string(FIND "${evidence}" "FIXED_ATTEMPT \"${expert_version}\"" fixed_attempt)
    string(FIND "${evidence}" "ONLINE_ATTEMPT \"${expert_version}\"" online_attempt)
    if(fixed_attempt EQUAL -1 OR online_attempt EQUAL -1)
        message(FATAL_ERROR "nonlinear cascade probe did not execute affine expert: ${family_dir}")
    endif()
    foreach(required "REJECTION_FIXED_PATH 3"
                     "REJECTION_FIXED_ATTEMPTS 2"
                     "REJECTION_FIXED_ATTEMPT \"cascade-reject-probe-v1\""
                     "REJECTION_FIXED_ATTEMPT \"original-damped-newton\""
                     "REJECTION_ONLINE_PATH 2"
                     "REJECTION_ONLINE_ATTEMPTS 2"
                     "REJECTION_ONLINE_ATTEMPT \"cascade-reject-probe-v1\""
                     "REJECTION_ONLINE_ATTEMPT \"continuation-warm-start-v1\"")
        string(FIND "${evidence}" "${required}" rejection_found)
        if(rejection_found EQUAL -1)
            message(FATAL_ERROR "nonlinear dynamic cascade evidence missing ${required}: ${family_dir}")
        endif()
    endforeach()
    if(NOT evidence MATCHES
       "REJECTION_FIXED_ATTEMPT \"cascade-reject-probe-v1\"\nREJECTION_FIXED_ATTEMPT \"original-damped-newton\"")
        message(FATAL_ERROR "fixed cascade rejection order is invalid: ${family_dir}")
    endif()
    if(NOT evidence MATCHES
       "REJECTION_ONLINE_ATTEMPT \"cascade-reject-probe-v1\"\nREJECTION_ONLINE_ATTEMPT \"continuation-warm-start-v1\"")
        message(FATAL_ERROR "online Equation-MoE rejection order is invalid: ${family_dir}")
    endif()
endforeach()

file(WRITE "${COUPLED_DIR}/../summary.txt"
    "SMAVE_NONLINEAR_CASCADE_SUMMARY 1\n"
    "FAMILIES 2\n"
    "COMPARISONS_PER_FAMILY 3\n"
    "REPETITIONS 100\n"
    "BOOTSTRAP_SAMPLES 10000\n"
    "FIXED_CASCADE_PLAN_VERIFIED 1\n"
    "ONLINE_EQUATION_MOE_PLAN_VERIFIED 1\n"
    "DYNAMIC_REJECTION_CASCADE_VERIFIED 1\n"
    "PAIRED_ACCURACY_VERIFIED 1\n"
    "END\n")
message(STATUS "two-family fixed cascade versus online Equation-MoE evidence passed")
