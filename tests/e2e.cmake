file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(SOURCE_DIRECTORY "${SOURCE}" DIRECTORY)
execute_process(
    COMMAND "${SMAVE}" compile "${SOURCE}" --top Coupled --output "${WORK}/model.ir"
    RESULT_VARIABLE compile_result
)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "compile command failed")
endif()
execute_process(
    COMMAND "${SMAVE}" inspect "${WORK}/model.ir"
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_output
)
if(NOT inspect_result EQUAL 0 OR NOT inspect_output MATCHES "blocks: 1")
    message(FATAL_ERROR "inspect command failed: ${inspect_output}")
endif()
execute_process(
    COMMAND "${SMAVE}" bundle "${WORK}/model.ir" --output "${WORK}/runtime.bundle"
    RESULT_VARIABLE bundle_result
)
if(NOT bundle_result EQUAL 0)
    message(FATAL_ERROR "bundle command failed")
endif()
execute_process(
    COMMAND "${SMAVE}" solve "${WORK}/model.ir" --scenario "${SOURCE_DIRECTORY}/case.conf" --config "${SOURCE_DIRECTORY}/smave.yaml" --bundle "${WORK}/runtime.bundle" --trace-dir "${WORK}/traces"
    RESULT_VARIABLE solve_result
    OUTPUT_VARIABLE solve_output
)
if(NOT solve_result EQUAL 0 OR NOT solve_output MATCHES "status: success")
    message(FATAL_ERROR "solve command failed: ${solve_output}")
endif()
