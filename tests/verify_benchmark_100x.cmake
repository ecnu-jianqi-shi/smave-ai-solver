if(NOT DEFINED SUMMARY)
    message(FATAL_ERROR "SUMMARY is required")
endif()
file(READ "${SUMMARY}" report)
set(failures "")
foreach(name IN ITEMS
    ADVECTION DARCY BURGERS DIFFUSION_SORPTION SHALLOW_WATER
    NS_INCOMPRESSIBLE CFD_1D)
    string(REGEX MATCH
        "PDEBENCH_${name}_CROSS_SOLVER_AGREEMENT ([0-9]+)"
        agreement_match "${report}")
    set(agreement "${CMAKE_MATCH_1}")
    string(REGEX MATCH
        "PDEBENCH_${name}_SMAVE_VS_CLASSICAL_SPEEDUP ([0-9.eE+-]+)"
        speedup_match "${report}")
    set(speedup "${CMAKE_MATCH_1}")
    if(NOT agreement_match OR NOT speedup_match OR
       NOT agreement EQUAL 1 OR speedup LESS 100.0)
        list(APPEND failures "${name}: agreement=${agreement}, speedup=${speedup}")
    endif()
endforeach()
if(failures)
    list(JOIN failures "\n  " failure_text)
    message(FATAL_ERROR
        "authoritative PDEBench 100x gate failed:\n  ${failure_text}")
endif()
message(STATUS "All authoritative PDEBench comparisons meet the 100x gate")
