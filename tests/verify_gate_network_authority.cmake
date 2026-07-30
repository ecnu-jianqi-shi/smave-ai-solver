if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "network gate authority evidence is missing")
endif()

set(command
    python3
    "${CMAKE_CURRENT_LIST_DIR}/verify_gate_network_authority.py"
    --evidence "${EVIDENCE}")
if(DEFINED EXPECT_DOCKER_BRIDGE AND EXPECT_DOCKER_BRIDGE)
    list(APPEND command --expect-docker-bridge)
endif()
execute_process(
    COMMAND ${command}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR
        "network gate authority verification failed:\n${output}${error}")
endif()
message(STATUS "${output}")
