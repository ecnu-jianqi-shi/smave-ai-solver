foreach(required CERTIFICATE REFINED_CERTIFICATE BUNDLE COUNTEREXAMPLES SAFE_TRACES DIFFICULT_TRACES OOD_TRACES)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Phase 3 artifact: ${required}")
    endif()
endforeach()

file(READ "${REFINED_CERTIFICATE}" refined_certificate)
foreach(pattern
    "PROBES 45"
    "CELLS 6"
    "COUNTEREXAMPLES 1"
    "\"p\" -1 -0.5"
    "\"p\" -0.5 -0.25"
    "\"p\" -0.25 -0.125"
    "\"p\" 0.125 0.25"
    "\"p\" 0.25 0.5"
    "\"p\" 0.5 1"
    "\"p\" 0")
    string(FIND "${refined_certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 refined certificate missing ${pattern}")
    endif()
endforeach()

file(READ "${CERTIFICATE}" certificate)
foreach(pattern
    "PROBES 45"
    "CELLS 6"
    "COUNTEREXAMPLES 1"
    "\"p\" -1 -0.5"
    "\"p\" -0.5 -0.25"
    "\"p\" -0.25 -0.125"
    "\"p\" 0.125 0.25"
    "\"p\" 0.25 0.5"
    "\"p\" 0.5 1"
    "\"p\" 0")
    string(FIND "${certificate}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 certificate missing ${pattern}")
    endif()
endforeach()

file(GLOB counterexamples "${COUNTEREXAMPLES}/counterexample-*.conf")
list(LENGTH counterexamples counterexample_count)
if(NOT counterexample_count EQUAL 1)
    message(FATAL_ERROR "Phase 3 expected one deduplicated counterexample, got ${counterexample_count}")
endif()
file(READ "${counterexamples}" counterexample)
foreach(pattern "p=0" "candidate required full fallback")
    string(FIND "${counterexample}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 exported counterexample missing ${pattern}")
    endif()
endforeach()

string(REGEX MATCH "EXPERT \"(affine-warm-start-[0-9a-f]+)\"" expert_match "${certificate}")
if(NOT expert_match)
    message(FATAL_ERROR "Phase 3 certificate lacks affine expert identity")
endif()
set(learned_expert "${CMAKE_MATCH_1}")
string(REGEX MATCH "HASH \"([0-9a-f]+)\"" hash_match "${certificate}")
if(NOT hash_match)
    message(FATAL_ERROR "Phase 3 certificate lacks integrity hash")
endif()
set(certificate_hash "${CMAKE_MATCH_1}")
file(READ "${BUNDLE}" bundle)
foreach(pattern "\"${learned_expert}\"" "\"${certificate_hash}\"")
    string(FIND "${bundle}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 bundle is not bound to ${pattern}")
    endif()
endforeach()

foreach(kind SAFE DIFFICULT OOD)
    file(GLOB traces "${${kind}_TRACES}/*.trace")
    list(LENGTH traces trace_count)
    if(NOT trace_count EQUAL 1)
        message(FATAL_ERROR "Phase 3 ${kind} path expected one trace, got ${trace_count}")
    endif()
    file(READ "${traces}" ${kind}_trace)
endforeach()

foreach(pattern
    "INPUT p 0.75"
    "BLOCK block-1 WARM_START_ACCEPT"
    "ATTEMPT \"${learned_expert}\" \"accepted\""
    "SUMMARY direct=0 corrected=0 warm_start=1 fallback=0")
    string(FIND "${SAFE_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 verified path missing ${pattern}")
    endif()
endforeach()

foreach(pattern
    "INPUT p 0"
    "BLOCK block-1 FULL_FALLBACK"
    "ATTEMPT \"original-damped-newton\" \"fallback\""
    "STATUS success"
    "SUMMARY direct=0 corrected=0 warm_start=0 fallback=1")
    string(FIND "${DIFFICULT_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 difficult-point path missing ${pattern}")
    endif()
endforeach()
string(FIND "${DIFFICULT_trace}" "${learned_expert}" difficult_learned)
if(NOT difficult_learned EQUAL -1)
    message(FATAL_ERROR "Phase 3 learned expert was routed at the degenerate point")
endif()

foreach(pattern
    "INPUT p 2"
    "BLOCK block-1 WARM_START_ACCEPT"
    "ATTEMPT \"continuation-warm-start-v1\" \"accepted\""
    "STATUS success")
    string(FIND "${OOD_trace}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Phase 3 OOD path missing ${pattern}")
    endif()
endforeach()
string(FIND "${OOD_trace}" "${learned_expert}" ood_learned)
if(NOT ood_learned EQUAL -1)
    message(FATAL_ERROR "Phase 3 learned expert was routed outside its training domain")
endif()

message(STATUS "Phase 3 deterministic CEGIS refinement, certificate binding, and safe fallback gates passed")
