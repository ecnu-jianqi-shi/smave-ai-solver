if(NOT DEFINED EVIDENCE)
    message(FATAL_ERROR "EVIDENCE is required")
endif()
file(READ "${EVIDENCE}" report)
foreach(marker
        "SMAVE_CASCADE_ORDERING_EVIDENCE 1"
        "stages=4"
        "permutations=24"
        "terminal_cost_us=10"
        "selected_order=c,b,d,a"
        "exhaustive_optimum_match=1"
        "END")
    string(FIND "${report}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "cascade ordering evidence missing marker: ${marker}")
    endif()
endforeach()
