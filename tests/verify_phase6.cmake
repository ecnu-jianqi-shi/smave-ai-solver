foreach(required GROUP_REPORT NATIVE_GROUP_REPORT NATIVE_GROUP_IR NATIVE_BRANCH_REPORT
        NATIVE_BRANCH_IR NATIVE_SWITCH_REPORT NATIVE_SWITCH_IR
        NATIVE_SUBSYSTEM_REPORT NATIVE_SUBSYSTEM_IR EVENT_REPORT GROUP_IR MODEL_IR
        NATIVE_GOTO_FROM_REPORT NATIVE_GOTO_FROM_IR PLANT_TRACES)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing Phase 6 artifact: ${required}")
    endif()
endforeach()
file(READ "${NATIVE_GOTO_FROM_REPORT}" native_goto_from_report)
foreach(pattern
    "SUCCESS 1"
    "COMMIT_ORDER 3 \"routing/source\" \"routing/double\" \"triple\""
    "NODE \"routing/double\" 1 0 1 \"out\" 8"
    "NODE \"triple\" 1 0 1 \"out\" 24")
    string(FIND "${native_goto_from_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Goto/From report missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_GOTO_FROM_IR}" native_goto_from_ir)
foreach(pattern
    "MODEL \"NativeGotoFrom\""
    "NODES 3"
    "NODE \"routing/double\" \"gain\""
    "CONNECTION \"routing/source\" \"out\" \"routing/double\" \"in\""
    "CONNECTION \"routing/double\" \"out\" \"triple\" \"in\"")
    string(FIND "${native_goto_from_ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Goto/From IR missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_SUBSYSTEM_REPORT}" native_subsystem_report)
foreach(pattern
    "SUCCESS 1"
    "COMMIT_ORDER 3 \"source\" \"amplifier/inner/double\" \"triple\""
    "NODE \"amplifier/inner/double\" 1 0 1 \"out\" 6"
    "NODE \"triple\" 1 0 1 \"out\" 18")
    string(FIND "${native_subsystem_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX SubSystem report missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_SUBSYSTEM_IR}" native_subsystem_ir)
foreach(pattern
    "MODEL \"NativeSubsystem\""
    "NODE \"amplifier/inner/double\" \"gain\""
    "CONNECTION \"source\" \"out\" \"amplifier/inner/double\" \"in\""
    "CONNECTION \"amplifier/inner/double\" \"out\" \"triple\" \"in\"")
    string(FIND "${native_subsystem_ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX SubSystem IR missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_SWITCH_REPORT}" native_switch_report)
foreach(pattern
    "SUCCESS 1"
    "NODE \"strict\" 1 0 1 \"out\" -10"
    "NODE \"inclusive\" 1 0 1 \"out\" 10"
    "NODE \"nonzero\" 1 0 1 \"out\" 10")
    string(FIND "${native_switch_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Switch report missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_SWITCH_IR}" native_switch_ir)
foreach(pattern
    "NODE \"strict\" \"switch\" 0.10000000000000001 0 2 \"criterion\" \"gt\" \"threshold\" \"2.000000\""
    "NODE \"inclusive\" \"switch\" 0.10000000000000001 0 2 \"criterion\" \"ge\" \"threshold\" \"2.000000\""
    "NODE \"nonzero\" \"switch\" 0.10000000000000001 0 2 \"criterion\" \"ne_zero\" \"threshold\" \"99.000000\"")
    string(FIND "${native_switch_ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Switch IR missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_BRANCH_REPORT}" native_branch_report)
foreach(pattern
    "SUCCESS 1"
    "MAX_CONNECTION_ERROR 0"
    "COMMIT_ORDER 4 \"source\" \"double\" \"triple\" \"total\""
    "NODE \"total\" 1 0 1 \"out\" -3")
    string(FIND "${native_branch_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Branch report missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_BRANCH_IR}" native_branch_ir)
foreach(pattern
    "MODEL \"NativeBranch\""
    "CONNECTIONS 4"
    "NODE \"total\" \"sum\" 0.10000000000000001 0 1 \"signs\" \"+-\""
    "CONNECTION \"source\" \"out\" \"double\" \"in\""
    "CONNECTION \"source\" \"out\" \"triple\" \"in\"")
    string(FIND "${native_branch_ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX Branch IR missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_GROUP_REPORT}" native_group_report)
foreach(pattern
    "SUCCESS 1"
    "BASE_STEP 0.10000000000000001"
    "TICKS 2"
    "TICK 0 0 3 \"source\" \"scale\" \"delay\""
    "TICK 1 0.10000000000000001 3 \"source\" \"scale\" \"delay\""
    "FINAL_OUTPUTS 3 \"delay.out\" 6 \"scale.out\" 6 \"source.out\" 3")
    string(FIND "${native_group_report}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX model-group evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${NATIVE_GROUP_IR}" native_group_ir)
foreach(pattern
    "SMAVE_BLOCK_GRAPH 2"
    "MODEL \"NativeBasic\""
    "NODE \"source\" \"constant\""
    "NODE \"scale\" \"gain\""
    "NODE \"delay\" \"unit_delay\""
    "COMMIT_ORDER 3 \"source\" \"scale\" \"delay\"")
    string(FIND "${native_group_ir}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "native SLX imported IR missing: ${pattern}")
    endif()
endforeach()
file(READ "${GROUP_REPORT}" group_report)
foreach(pattern
    "SUCCESS 1"
    "LOCAL_FALLBACKS 1"
    "MAX_CONNECTION_ERROR 0"
    "COMMIT_ORDER 4 \"source\" \"scale\" \"plant\" \"delay\"")
    if(NOT group_report MATCHES "${pattern}")
        message(FATAL_ERROR "model-group acceptance evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${EVENT_REPORT}" event_report)
foreach(pattern
    "SMAVE_HYBRID_REPORT 2"
    "SUCCESS 1"
    "FINAL_MODE \"done\""
    "CANDIDATES 3"
    "ACCEPTED_CANDIDATES 1"
    "REJECTED_CANDIDATES 2"
    "MISSED_EVENTS 0"
    "EVENT_RECALL 1"
    "MAX_EVENT_TIME_ERROR 0"
    "MAX_RESET_ERROR 0"
    "EVENTS 1"
    "EVENT 2 .* \"high-priority\" \"charging\" \"done\" 1 1")
    if(NOT event_report MATCHES "${pattern}")
        message(FATAL_ERROR "hybrid-event acceptance evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${GROUP_IR}" group_ir)
if(NOT group_ir MATCHES "SMAVE_BLOCK_GRAPH 2" OR
   NOT group_ir MATCHES "${MODEL_IR}")
    message(FATAL_ERROR "imported group IR is not self-contained or versioned")
endif()
file(GLOB plant_traces "${PLANT_TRACES}/*.trace")
list(LENGTH plant_traces trace_count)
if(NOT trace_count EQUAL 2)
    message(FATAL_ERROR "Phase 6 expected one plant fallback trace per deterministic run, got ${trace_count}")
endif()
foreach(trace IN LISTS plant_traces)
    file(READ "${trace}" content)
    foreach(pattern
        "BLOCK block-1 FULL_FALLBACK"
        "ATTEMPT \"original-damped-newton\" \"fallback\""
        "STATUS success"
        "SUMMARY direct=0 corrected=0 warm_start=0 fallback=1")
        string(FIND "${content}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Phase 6 local fallback trace missing ${pattern}: ${trace}")
        endif()
    endforeach()
endforeach()
message(STATUS "Phase 6 native SLX import/flattening, deterministic coupling, local fallback, event recall/order/time/reset gates passed")
