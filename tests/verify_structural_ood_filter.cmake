if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "structural OOD filter evidence is missing")
endif()

file(READ "${EVIDENCE}" content)
foreach(required
        "SMAVE_STRUCTURAL_OOD_FILTER 1"
        "contract=hard-structural-filter-before-learned-route-permission"
        "filter_executed_before_runtime_router=1"
        "embedding_similarity_not_substitute=1"
        "router_score_not_substitute=1"
        "eligible_coverage=1"
        "structural_ood_rejects=4"
        "false_accepts=0"
        "false_rejects=0"
        "dangerous_misroutes=0"
        "negative_results_retained=1"
        "END")
    string(FIND "${content}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing structural OOD field: ${required}")
    endif()
endforeach()

# Every rejection must record an explicit STRUCTURAL_OOD_REJECT with a concrete reason.
string(REGEX MATCHALL "STRUCTURAL_OOD_REJECT label=" reject_matches "${content}")
list(LENGTH reject_matches reject_count)
if(NOT reject_count EQUAL 4)
    message(FATAL_ERROR "expected 4 STRUCTURAL_OOD_REJECT entries, found ${reject_count}")
endif()

foreach(reason block_family hardware_profile tolerance_profile)
    string(FIND "${content}" "reason=${reason}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "no STRUCTURAL_OOD_REJECT for reason=${reason}")
    endif()
endforeach()

message(STATUS "structural OOD filter evidence passed")
