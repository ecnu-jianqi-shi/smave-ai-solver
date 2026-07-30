if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "AMG backend evidence is missing")
endif()
file(READ "${EVIDENCE}" evidence)
foreach(marker
        "SMAVE_AGGREGATION_AMG_BACKEND 1"
        "backend=pcg-aggregation-amg-cpu-v1"
        "contract=square-five-point-numerically-spd-csr"
        "repetitions_per_scale=10"
        "scales=3"
        "router_admitted=1"
        "router_irregular_rejected=1"
        "non_spd_rejected=1"
        "irregular_topology_rejected=1"
        "verified_linear_service_backend=pcg-aggregation-amg-cpu-v1"
        "verified_linear_service_success=1"
        "verified_linear_service_fallback=0"
        "END")
    string(FIND "${evidence}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "AMG backend evidence missing ${marker}")
    endif()
endforeach()
foreach(width 16 32 64)
    string(REGEX MATCH
        "SCALE width=${width} unknowns=([0-9]+) nonzeros=([0-9]+) levels=([0-9]+) storage_bytes=([0-9]+) dense_bytes=([0-9]+) amg_median_us=([0-9.eE+-]+) ic0_median_us=([0-9.eE+-]+) amg_mean_iterations=([0-9.eE+-]+) ic0_mean_iterations=([0-9.eE+-]+) amg_speedup=([0-9.eE+-]+) amg_residual_inf=([0-9.eE+-]+) ic0_residual_inf=([0-9.eE+-]+)"
        match "${evidence}")
    if(NOT match)
        message(FATAL_ERROR "AMG backend scale ${width} is incomplete")
    endif()
    if(CMAKE_MATCH_3 LESS 2 OR CMAKE_MATCH_4 GREATER_EQUAL CMAKE_MATCH_5 OR
       CMAKE_MATCH_6 LESS_EQUAL 0 OR CMAKE_MATCH_7 LESS_EQUAL 0 OR
       CMAKE_MATCH_8 LESS_EQUAL 0 OR CMAKE_MATCH_9 LESS_EQUAL 0 OR
       CMAKE_MATCH_10 LESS_EQUAL 0 OR CMAKE_MATCH_11 GREATER 1.0e-7 OR
       CMAKE_MATCH_12 GREATER 1.0e-7)
        message(FATAL_ERROR "AMG backend scale ${width} failed quality gate")
    endif()
endforeach()
foreach(field largest_unknowns largest_nonzeros largest_levels
              largest_storage_bytes largest_dense_bytes largest_amg_median_us
              largest_ic0_median_us largest_amg_speedup
              largest_amg_mean_iterations largest_ic0_mean_iterations
              largest_amg_residual_inf rss_before_bytes rss_after_bytes)
    string(REGEX MATCH "(^|\n)${field}=([0-9.eE+-]+)" field_match "${evidence}")
    if(NOT field_match)
        message(FATAL_ERROR "AMG backend evidence lacks ${field}")
    endif()
    set(${field} "${CMAKE_MATCH_2}")
endforeach()
if(NOT largest_unknowns EQUAL 4096 OR largest_levels LESS 2 OR
   largest_storage_bytes GREATER_EQUAL largest_dense_bytes OR
   largest_amg_speedup LESS_EQUAL 1 OR
   largest_amg_mean_iterations GREATER_EQUAL largest_ic0_mean_iterations OR
   largest_amg_residual_inf GREATER 1.0e-7)
    message(FATAL_ERROR "AMG largest-scale benefit or accuracy gate failed")
endif()
message(STATUS "aggregation AMG backend and scale evidence passed")
