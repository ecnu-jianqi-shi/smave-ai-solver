if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_JOINT_ROUTE_BUDGET_SHIFT 1"
        "contract=training-profile-to-heldout-production-joint-policy"
        "families=quadratic,cubic"
        "JointQuadratic.training_scenarios=32"
        "JointQuadratic.heldout_scenarios=32"
        "JointQuadratic.actions=3"
        "JointQuadratic.selected_correction_budget=2"
        "JointCubic.training_scenarios=32"
        "JointCubic.heldout_scenarios=32"
        "JointCubic.actions=3"
        "JointCubic.selected_correction_budget=4"
        "budget_changes_across_families=1"
        "all_heldout_success=1"
        "all_heldout_zero_gate_mismatches=1"
        "END")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "joint shift evidence missing marker: ${marker}")
    endif()
endforeach()
string(REGEX MATCH "maximum_heldout_regret=([0-9.]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 GREATER 1.25)
    message(FATAL_ERROR "joint heldout regret exceeded 1.25")
endif()
string(REGEX MATCH "maximum_action_calibration_error=([0-9.]+)" _ "${report}")
if("${CMAKE_MATCH_1}" STREQUAL "" OR CMAKE_MATCH_1 GREATER 0.1)
    message(FATAL_ERROR "joint action calibration error exceeded 0.1")
endif()
