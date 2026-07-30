if(NOT DEFINED CLI OR NOT DEFINED C_API_PROBE OR
   NOT DEFINED C_API_NONLINEAR_PROBE OR NOT DEFINED C_API_ODE_PROBE OR
   NOT DEFINED C_API_DAE_PROBE OR
   NOT DEFINED C_API_INDEX_TWO_DAE_PROBE OR
   NOT DEFINED C_API_CANCELLATION_PROBE OR
   NOT DEFINED C_API_EXTERNAL_LINEAR_FALLBACK_PROBE OR
   NOT DEFINED C_API_EXTERNAL_STEPPER_FALLBACK_PROBE OR
   NOT DEFINED C_API_ERROR_STACK_PROBE OR
   NOT DEFINED C_API_COMPLEMENTARITY_PROBE OR
   NOT DEFINED C_API_BLOCK_GRAPH_PROBE OR
   NOT DEFINED C_API_EVENT_PROBE OR
   NOT DEFINED C_API_HYBRID_DAE_PROBE OR
   NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "CLI, C API probes and OUTPUT_DIR are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(cli_report "${OUTPUT_DIR}/cli.txt")
set(c_api_report "${OUTPUT_DIR}/c-api.txt")
execute_process(
    COMMAND "${CLI}" solve-linear
        --storage dense
        --matrix "4,-1,0,-1,4,-1,0,-1,3"
        --rhs "3,2,2"
        --output "${cli_report}"
    RESULT_VARIABLE cli_result
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE cli_stderr)
if(NOT cli_result EQUAL 0)
    message(FATAL_ERROR "CLI shared-service solve failed: ${cli_stdout}${cli_stderr}")
endif()
execute_process(
    COMMAND "${C_API_PROBE}"
    RESULT_VARIABLE c_api_result
    OUTPUT_FILE "${c_api_report}"
    ERROR_VARIABLE c_api_stderr)
if(NOT c_api_result EQUAL 0)
    message(FATAL_ERROR "C API shared-service solve failed: ${c_api_stderr}")
endif()

file(READ "${cli_report}" cli)
file(READ "${c_api_report}" c_api)
foreach(key service_id success used_fallback backend plan_id equation_family residual_inf backward_error solution diagnostic)
    string(REGEX MATCH "${key}=[^\n]*" cli_value "${cli}")
    string(REGEX MATCH "${key}=[^\n]*" c_api_value "${c_api}")
    if(cli_value STREQUAL "" OR c_api_value STREQUAL "")
        message(FATAL_ERROR "shared-service evidence missing ${key}")
    endif()
    if(NOT cli_value STREQUAL c_api_value)
        message(FATAL_ERROR "CLI/C API mismatch for ${key}: ${cli_value} vs ${c_api_value}")
    endif()
endforeach()
set(c_api_complementarity_report "${OUTPUT_DIR}/c-api-complementarity.txt")
execute_process(
    COMMAND "${C_API_COMPLEMENTARITY_PROBE}"
    RESULT_VARIABLE c_api_complementarity_result
    OUTPUT_FILE "${c_api_complementarity_report}"
    ERROR_VARIABLE c_api_complementarity_stderr)
if(NOT c_api_complementarity_result EQUAL 0)
    message(FATAL_ERROR
        "C API complementarity shared-service solve failed: ${c_api_complementarity_stderr}")
endif()
file(READ "${c_api_complementarity_report}" c_api_complementarity)
foreach(marker
        "SMAVE_C_API_COMPLEMENTARITY_SERVICE 1"
        "COMPLEMENTARITY_CAPABILITY 1"
        "COMPLEMENTARITY_DENSE_CSR_EQUIVALENT 1"
        "COMPLEMENTARITY_ORIGINAL_GAP_GATE 1"
        "COMPLEMENTARITY_INEQUALITY_GATE 1"
        "COMPLEMENTARITY_PRODUCT_GATE 1"
        "COMPLEMENTARITY_ACTIVE_SET_FALLBACK 1"
        "COMPLEMENTARITY_NONMONOTONE_REJECTED 1")
    string(FIND "${c_api_complementarity}" "${marker}" complementarity_marker)
    if(complementarity_marker EQUAL -1)
        message(FATAL_ERROR "complementarity C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_block_graph_report "${OUTPUT_DIR}/c-api-block-graph.txt")
execute_process(
    COMMAND "${C_API_BLOCK_GRAPH_PROBE}"
    RESULT_VARIABLE c_api_block_graph_result
    OUTPUT_FILE "${c_api_block_graph_report}"
    ERROR_VARIABLE c_api_block_graph_stderr)
if(NOT c_api_block_graph_result EQUAL 0)
    message(FATAL_ERROR
        "C API block-graph shared-service solve failed: ${c_api_block_graph_stderr}")
endif()
file(READ "${c_api_block_graph_report}" c_api_block_graph)
foreach(marker
        "SMAVE_C_API_BLOCK_GRAPH_SERVICE 1"
        "BLOCK_GRAPH_CAPABILITY 1"
        "BLOCK_GRAPH_MULTIRATE_ZERO_ORDER_HOLD 1"
        "BLOCK_GRAPH_DETERMINISTIC_COMMIT_ORDER 1"
        "BLOCK_GRAPH_CALLBACK_ORIGINAL_GATE 1"
        "BLOCK_GRAPH_LOCAL_FALLBACK 1"
        "BLOCK_GRAPH_ALGEBRAIC_FIXED_POINT 1"
        "BLOCK_GRAPH_DIVERGENT_FEEDBACK_REJECTED 1"
        "BLOCK_GRAPH_MIXED_RATE_FEEDBACK_REJECTED 1")
    string(FIND "${c_api_block_graph}" "${marker}" block_graph_marker)
    if(block_graph_marker EQUAL -1)
        message(FATAL_ERROR "block-graph C API evidence missing ${marker}")
    endif()
endforeach()
set(cli_nonlinear_report "${OUTPUT_DIR}/cli-nonlinear.txt")
set(c_api_nonlinear_report "${OUTPUT_DIR}/c-api-nonlinear.txt")
execute_process(
    COMMAND "${CLI}" solve-nonlinear
        --unknowns "x,y" --initial "1.5,1.5"
        --residual "x*x+y-5" --residual "x+y*y-3"
        --output "${cli_nonlinear_report}"
    RESULT_VARIABLE cli_nonlinear_result
    OUTPUT_VARIABLE cli_nonlinear_stdout
    ERROR_VARIABLE cli_nonlinear_stderr)
if(NOT cli_nonlinear_result EQUAL 0)
    message(FATAL_ERROR
        "CLI nonlinear shared-service solve failed: ${cli_nonlinear_stdout}${cli_nonlinear_stderr}")
endif()
execute_process(
    COMMAND "${C_API_NONLINEAR_PROBE}"
    RESULT_VARIABLE c_api_nonlinear_result
    OUTPUT_FILE "${c_api_nonlinear_report}"
    ERROR_VARIABLE c_api_nonlinear_stderr)
if(NOT c_api_nonlinear_result EQUAL 0)
    message(FATAL_ERROR "C API nonlinear shared-service solve failed: ${c_api_nonlinear_stderr}")
endif()
file(READ "${cli_nonlinear_report}" cli_nonlinear)
file(READ "${c_api_nonlinear_report}" c_api_nonlinear)
foreach(marker
        "EXTERNAL_NONLINEAR_FALLBACK_CAPABILITY 1"
        "EXTERNAL_NONLINEAR_FALLBACK_AFTER_BUILTINS 1"
        "EXTERNAL_NONLINEAR_FALLBACK_ORIGINAL_GATE 1"
        "EXTERNAL_NONLINEAR_FALLBACK_CALLBACK_FAILURE 1"
        "EXTERNAL_NONLINEAR_FALLBACK_CANCEL_PRECHECK 1"
        "EXTERNAL_NONLINEAR_FALLBACK_NEGATIVE_CONTRACTS 1")
    string(FIND "${c_api_nonlinear}" "${marker}" nonlinear_fallback_marker)
    if(nonlinear_fallback_marker EQUAL -1)
        message(FATAL_ERROR "nonlinear fallback C API evidence missing ${marker}")
    endif()
endforeach()
foreach(key service_id success used_fallback backend plan_id equation_family residual_inf backward_error solution diagnostic)
    string(REGEX MATCH "${key}=[^\n]*" cli_value "${cli_nonlinear}")
    string(REGEX MATCH "${key}=[^\n]*" c_api_value "${c_api_nonlinear}")
    if(cli_value STREQUAL "" OR c_api_value STREQUAL "" OR
       NOT cli_value STREQUAL c_api_value)
        message(FATAL_ERROR "nonlinear CLI/C API mismatch for ${key}: ${cli_value} vs ${c_api_value}")
    endif()
endforeach()
set(cli_ode_report "${OUTPUT_DIR}/cli-ode.txt")
set(c_api_ode_report "${OUTPUT_DIR}/c-api-ode.txt")
execute_process(
    COMMAND "${CLI}" solve-ode
        --states "x" --initial "1"
        --rhs "-x" --start "0" --end "1" --max-step "0.1"
        --absolute-tolerance "1e-10" --relative-tolerance "1e-8"
        --output "${cli_ode_report}"
    RESULT_VARIABLE cli_ode_result
    OUTPUT_VARIABLE cli_ode_stdout
    ERROR_VARIABLE cli_ode_stderr)
if(NOT cli_ode_result EQUAL 0)
    message(FATAL_ERROR
        "CLI ODE shared-service solve failed: ${cli_ode_stdout}${cli_ode_stderr}")
endif()
execute_process(
    COMMAND "${C_API_ODE_PROBE}"
    RESULT_VARIABLE c_api_ode_result
    OUTPUT_FILE "${c_api_ode_report}"
    ERROR_VARIABLE c_api_ode_stderr)
if(NOT c_api_ode_result EQUAL 0)
    message(FATAL_ERROR "C API ODE shared-service solve failed: ${c_api_ode_stderr}")
endif()
file(READ "${cli_ode_report}" cli_ode)
file(READ "${c_api_ode_report}" c_api_ode)
string(FIND "${c_api_ode}" "legacy_ode_result_prefix_compatible=1" legacy_ode_prefix)
if(legacy_ode_prefix EQUAL -1)
    message(FATAL_ERROR "ODE C API legacy result prefix compatibility evidence missing")
endif()
foreach(key service_id success used_fallback backend plan_id equation_family residual_inf backward_error final_time maximum_scaled_local_error accepted_steps rejected_steps solution diagnostic)
    string(REGEX MATCH "${key}=[^\n]*" cli_value "${cli_ode}")
    string(REGEX MATCH "${key}=[^\n]*" c_api_value "${c_api_ode}")
    if(cli_value STREQUAL "" OR c_api_value STREQUAL "" OR
       NOT cli_value STREQUAL c_api_value)
        message(FATAL_ERROR "ODE CLI/C API mismatch for ${key}: ${cli_value} vs ${c_api_value}")
    endif()
endforeach()
set(c_api_dae_report "${OUTPUT_DIR}/c-api-dae.txt")
execute_process(
    COMMAND "${C_API_DAE_PROBE}"
    RESULT_VARIABLE c_api_dae_result
    OUTPUT_FILE "${c_api_dae_report}"
    ERROR_VARIABLE c_api_dae_stderr)
if(NOT c_api_dae_result EQUAL 0)
    message(FATAL_ERROR "C API DAE shared-service solve failed: ${c_api_dae_stderr}")
endif()
file(READ "${c_api_dae_report}" c_api_dae)
foreach(marker
        "SMAVE_C_API_DAE_SERVICE 1"
        "service_id=\"smave.verified-fully-implicit-dae-solve.v1\""
        "equation_family=\"dae-fully-implicit-first-order-smooth\""
        "accepted_steps=10"
        "DAE_FALLBACK 1"
        "DAE_INCONSISTENT_INITIAL_REJECTED 1")
    string(FIND "${c_api_dae}" "${marker}" dae_marker)
    if(dae_marker EQUAL -1)
        message(FATAL_ERROR "DAE C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_index_two_dae_report "${OUTPUT_DIR}/c-api-index-two-dae.txt")
execute_process(
    COMMAND "${C_API_INDEX_TWO_DAE_PROBE}"
    RESULT_VARIABLE c_api_index_two_dae_result
    OUTPUT_FILE "${c_api_index_two_dae_report}"
    ERROR_VARIABLE c_api_index_two_dae_stderr)
if(NOT c_api_index_two_dae_result EQUAL 0)
    message(FATAL_ERROR
        "C API index-2 DAE shared-service solve failed: ${c_api_index_two_dae_stderr}")
endif()
file(READ "${c_api_index_two_dae_report}" c_api_index_two_dae)
foreach(marker
        "SMAVE_C_API_INDEX_TWO_DAE_SERVICE 1"
        "equation_family=\"dae-fully-implicit-hessenberg-index2\""
        "differentiation_index=2"
        "INDEX_TWO_DAE_CAPABILITY 1"
        "INDEX_TWO_DAE_HIDDEN_CONSISTENCY_GATE 1"
        "INDEX_TWO_DAE_HIDDEN_RANK_GATE 1"
        "INDEX_TWO_DAE_RESULT_ABI_MISMATCH 1"
        "INDEX_TWO_DAE_EVENT_RESULT_PREFIX_PRESERVED 1"
        "INDEX_TWO_DAE_EXTENDED_RESULT_TAIL_PRESERVED 1")
    string(FIND "${c_api_index_two_dae}" "${marker}" index_two_dae_marker)
    if(index_two_dae_marker EQUAL -1)
        message(FATAL_ERROR "index-2 DAE C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_cancellation_report "${OUTPUT_DIR}/c-api-cancellation.txt")
execute_process(
    COMMAND "${C_API_CANCELLATION_PROBE}"
    RESULT_VARIABLE c_api_cancellation_result
    OUTPUT_FILE "${c_api_cancellation_report}"
    ERROR_VARIABLE c_api_cancellation_stderr)
if(NOT c_api_cancellation_result EQUAL 0)
    message(FATAL_ERROR
        "C API cancellation shared-service solve failed: ${c_api_cancellation_stderr}")
endif()
file(READ "${c_api_cancellation_report}" c_api_cancellation)
foreach(marker
        "SMAVE_C_API_CANCELLATION_SERVICE 1"
        "CANCELLATION_CAPABILITY 1"
        "CANCELLATION_CROSS_THREAD_REQUEST 1"
        "CANCELLATION_STABLE_STATUS_DIAGNOSTIC 1"
        "CANCELLATION_ATOMIC_COMMIT_BOUNDARY 1"
        "CANCELLATION_TOKEN_STICKY_RESET_REUSE 1"
        "CANCELLATION_ACTIVE_LIFECYCLE_REJECTED 1"
        "CANCELLATION_FOREIGN_LIBRARY_REJECTED 1"
        "DEADLINE_CAPABILITY 1"
        "DEADLINE_STABLE_STATUS_DIAGNOSTIC 1"
        "DEADLINE_ATOMIC_COMMIT_BOUNDARY 1"
        "DEADLINE_TOKEN_UNCHANGED_UNLIMITED_REUSE 1"
        "CANCELLATION_ALLOCATOR_BALANCED 1")
    string(FIND "${c_api_cancellation}" "${marker}" cancellation_marker)
    if(cancellation_marker EQUAL -1)
        message(FATAL_ERROR "cancellation C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_external_linear_fallback_report
    "${OUTPUT_DIR}/c-api-external-linear-fallback.txt")
execute_process(
    COMMAND "${C_API_EXTERNAL_LINEAR_FALLBACK_PROBE}"
    RESULT_VARIABLE c_api_external_linear_fallback_result
    OUTPUT_FILE "${c_api_external_linear_fallback_report}"
    ERROR_VARIABLE c_api_external_linear_fallback_stderr)
if(NOT c_api_external_linear_fallback_result EQUAL 0)
    message(FATAL_ERROR
        "C API external linear fallback shared-service solve failed: ${c_api_external_linear_fallback_stderr}")
endif()
file(READ "${c_api_external_linear_fallback_report}" c_api_external_linear_fallback)
foreach(marker
        "SMAVE_C_API_EXTERNAL_LINEAR_FALLBACK 1"
        "EXTERNAL_LINEAR_FALLBACK_CAPABILITY 1"
        "EXTERNAL_LINEAR_FALLBACK_AFTER_BUILTINS 1"
        "EXTERNAL_LINEAR_FALLBACK_ORIGINAL_GATE 1"
        "EXTERNAL_LINEAR_FALLBACK_CALLBACK_FAILURE 1"
        "EXTERNAL_LINEAR_FALLBACK_CANCEL_PRECHECK 1"
        "EXTERNAL_LINEAR_FALLBACK_NEGATIVE_CONTRACTS 1"
        "EXTERNAL_LINEAR_FALLBACK_SHARED_SERVICE 1")
    string(FIND "${c_api_external_linear_fallback}" "${marker}" fallback_marker)
    if(fallback_marker EQUAL -1)
        message(FATAL_ERROR "external linear fallback C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_external_stepper_fallback_report
    "${OUTPUT_DIR}/c-api-external-stepper-fallback.txt")
execute_process(
    COMMAND "${C_API_EXTERNAL_STEPPER_FALLBACK_PROBE}"
    RESULT_VARIABLE c_api_external_stepper_fallback_result
    OUTPUT_FILE "${c_api_external_stepper_fallback_report}"
    ERROR_VARIABLE c_api_external_stepper_fallback_stderr)
if(NOT c_api_external_stepper_fallback_result EQUAL 0)
    message(FATAL_ERROR
        "C API external stepper fallback shared-service solve failed: ${c_api_external_stepper_fallback_stderr}")
endif()
file(READ "${c_api_external_stepper_fallback_report}" c_api_external_stepper_fallback)
foreach(marker
        "SMAVE_C_API_EXTERNAL_STEPPER_FALLBACK 1"
        "EXTERNAL_ODE_STEPPER_CAPABILITY 1"
        "EXTERNAL_ODE_STEPPER_AFTER_BUILTINS 1"
        "EXTERNAL_ODE_STEPPER_DENSE_OUTPUT_GATE 1"
        "EXTERNAL_DAE_STEPPER_CAPABILITY 1"
        "EXTERNAL_DAE_STEPPER_AFTER_BUILTINS 1"
        "EXTERNAL_DAE_STEPPER_KINEMATIC_RESIDUAL_GATE 1"
        "EXTERNAL_STEPPER_FRESH_BUFFERS 1"
        "EXTERNAL_STEPPER_CALLBACK_FAILURE 1"
        "EXTERNAL_STEPPER_CONTROL_BOUNDARIES 1"
        "EXTERNAL_STEPPER_NEGATIVE_CONTRACTS 1")
    string(FIND "${c_api_external_stepper_fallback}" "${marker}" stepper_marker)
    if(stepper_marker EQUAL -1)
        message(FATAL_ERROR "external stepper C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_error_stack_report "${OUTPUT_DIR}/c-api-error-stack.txt")
execute_process(
    COMMAND "${C_API_ERROR_STACK_PROBE}"
    RESULT_VARIABLE c_api_error_stack_result
    OUTPUT_FILE "${c_api_error_stack_report}"
    ERROR_VARIABLE c_api_error_stack_stderr)
if(NOT c_api_error_stack_result EQUAL 0)
    message(FATAL_ERROR
        "C API error-stack probe failed: ${c_api_error_stack_stderr}")
endif()
file(READ "${c_api_error_stack_report}" c_api_error_stack)
foreach(marker
        "SMAVE_C_API_ERROR_STACK 1"
        "ERROR_STACK_CAPABILITY 1"
        "ERROR_STACK_NEWEST_FIRST 1"
        "ERROR_STACK_THREAD_LOCAL 1"
        "ERROR_STACK_LIFECYCLE 1"
        "ERROR_STACK_ABI_VALIDATION 1"
        "ERROR_STACK_BOUNDED 1"
        "ERROR_STACK_CLEAR 1")
    string(FIND "${c_api_error_stack}" "${marker}" error_stack_marker)
    if(error_stack_marker EQUAL -1)
        message(FATAL_ERROR "error-stack C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_dae_event_report "${OUTPUT_DIR}/c-api-dae-event.txt")
execute_process(
    COMMAND "${C_API_DAE_EVENT_PROBE}"
    RESULT_VARIABLE c_api_dae_event_result
    OUTPUT_FILE "${c_api_dae_event_report}"
    ERROR_VARIABLE c_api_dae_event_stderr)
if(NOT c_api_dae_event_result EQUAL 0)
    message(FATAL_ERROR "C API DAE event shared-service solve failed: ${c_api_dae_event_stderr}")
endif()
file(READ "${c_api_dae_event_report}" c_api_dae_event)
foreach(marker
        "SMAVE_C_API_DAE_EVENT_SERVICE 1"
        "DAE_EVENT_IMPLICIT_ROOT 1"
        "DAE_EVENT_PRIORITY_ORDER 1"
        "DAE_EVENT_ATOMIC_REINIT 1"
        "DAE_EVENT_WRONG_DIRECTION_IGNORED 1"
        "DAE_EVENT_INCONSISTENT_REINIT_REJECTED 1"
        "DAE_EVENT_ALGEBRAIC_DERIVATIVE_REJECTED 1"
        "DAE_EVENT_STUCK_REINIT_REJECTED 1"
        "DAE_EVENT_CALLBACK_FAILURE_REJECTED 1"
        "DAE_EVENT_STABLE_DIAGNOSTIC_CODES 1"
        "DAE_LEGACY_RESULT_PREFIX_COMPATIBLE 1"
        "DAE_EVENT_CONCURRENT_SHARED_SOLVER 1"
        "DAE_EVENT_CONCURRENT_BITWISE_DETERMINISTIC 1"
        "DAE_EVENT_ALLOCATOR_BALANCED 1"
        "event_count=2")
    string(FIND "${c_api_dae_event}" "${marker}" dae_event_marker)
    if(dae_event_marker EQUAL -1)
        message(FATAL_ERROR "DAE event C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_event_report "${OUTPUT_DIR}/c-api-event.txt")
execute_process(
    COMMAND "${C_API_EVENT_PROBE}"
    RESULT_VARIABLE c_api_event_result
    OUTPUT_FILE "${c_api_event_report}"
    ERROR_VARIABLE c_api_event_stderr)
if(NOT c_api_event_result EQUAL 0)
    message(FATAL_ERROR "C API event shared-service solve failed: ${c_api_event_stderr}")
endif()
file(READ "${c_api_event_report}" c_api_event)
foreach(marker
        "SMAVE_C_API_EVENT_SERVICE 1"
        "equation_family=\"explicit-ode-with-events\""
        "event_count=2"
        "EVENT_DIRECTION 1"
        "EVENT_RISING_DIRECTION 1"
        "EVENT_FALLING_DIRECTION 1"
        "EVENT_PRIORITY_ORDER 1"
        "EVENT_ATOMIC_RESET 1"
        "EVENT_STUCK_RESET_REJECTED 1")
    string(FIND "${c_api_event}" "${marker}" event_marker)
    if(event_marker EQUAL -1)
        message(FATAL_ERROR "event C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_hybrid_report "${OUTPUT_DIR}/c-api-hybrid.txt")
execute_process(
    COMMAND "${C_API_HYBRID_PROBE}"
    RESULT_VARIABLE c_api_hybrid_result
    OUTPUT_FILE "${c_api_hybrid_report}"
    ERROR_VARIABLE c_api_hybrid_stderr)
if(NOT c_api_hybrid_result EQUAL 0)
    message(FATAL_ERROR "C API hybrid shared-service solve failed: ${c_api_hybrid_stderr}")
endif()
file(READ "${c_api_hybrid_report}" c_api_hybrid)
foreach(marker
        "SMAVE_C_API_HYBRID_SERVICE 1"
        "service_id=\"smave.verified-explicit-hybrid-solve.v1\""
        "equation_family=\"explicit-hybrid-multimode\""
        "event_count=2"
        "final_mode=0"
        "HYBRID_MODE_SPECIFIC_RHS 1"
        "HYBRID_TWO_MODE_SWITCHES 1"
        "HYBRID_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1"
        "HYBRID_PRIORITY_TRANSACTION_ROLLBACK 1"
        "HYBRID_SUPERDENSE_CASCADE 1"
        "HYBRID_SUPERDENSE_CYCLE_ROLLBACK 1"
        "HYBRID_STABLE_PRE_ACROSS_MICROSTEPS 1"
        "HYBRID_DISJOINT_WRITESET_ATOMIC_MERGE 1"
        "HYBRID_OVERLAPPING_WRITESET_REJECTED 1"
        "HYBRID_TARGET_MODE_CONFLICT_REJECTED 1"
        "HYBRID_STABLE_DIAGNOSTIC_CODES 1")
    string(FIND "${c_api_hybrid}" "${marker}" hybrid_marker)
    if(hybrid_marker EQUAL -1)
        message(FATAL_ERROR "hybrid C API evidence missing ${marker}")
    endif()
endforeach()
set(c_api_hybrid_dae_report "${OUTPUT_DIR}/c-api-hybrid-dae.txt")
execute_process(
    COMMAND "${C_API_HYBRID_DAE_PROBE}"
    RESULT_VARIABLE c_api_hybrid_dae_result
    OUTPUT_FILE "${c_api_hybrid_dae_report}"
    ERROR_VARIABLE c_api_hybrid_dae_stderr)
if(NOT c_api_hybrid_dae_result EQUAL 0)
    message(FATAL_ERROR
        "C API hybrid DAE shared-service solve failed: ${c_api_hybrid_dae_stderr}")
endif()
file(READ "${c_api_hybrid_dae_report}" c_api_hybrid_dae)
foreach(marker
        "SMAVE_C_API_HYBRID_DAE_SERVICE 1"
        "service_id=\"smave.verified-fully-implicit-hybrid-dae-solve.v1\""
        "equation_family=\"dae-fully-implicit-hybrid-multimode\""
        "HYBRID_DAE_MODE_RESIDUALS 1"
        "HYBRID_DAE_TARGET_CONSISTENCY_GATE 1"
        "HYBRID_DAE_AUTOMATIC_CONSISTENCY_PROJECTION 1"
        "HYBRID_DAE_UNPROJECTABLE_REJECTED 1"
        "HYBRID_DAE_SUPERDENSE_CASCADE 1"
        "HYBRID_DAE_TRANSACTION_ROLLBACK 1"
        "HYBRID_DAE_PROJECTION_METADATA_ATOMIC 1"
        "HYBRID_DAE_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1"
        "HYBRID_DAE_STABLE_PRE_ACROSS_MICROSTEPS 1"
        "HYBRID_DAE_STATE_DERIVATIVE_WRITESET_MERGE 1"
        "HYBRID_DAE_WRITESET_CONFLICTS_REJECTED 1"
        "HYBRID_DAE_TARGET_MODE_CONFLICT_REJECTED 1"
        "HYBRID_DAE_STABLE_SHARED_SOLVER_DETERMINISTIC 1"
        "HYBRID_DAE_CONCURRENT_BITWISE_DETERMINISTIC 1")
    string(FIND "${c_api_hybrid_dae}" "${marker}" hybrid_dae_marker)
    if(hybrid_dae_marker EQUAL -1)
        message(FATAL_ERROR "hybrid DAE C API evidence missing ${marker}")
    endif()
endforeach()
file(WRITE "${OUTPUT_DIR}/evidence.txt"
    "SMAVE_SOLVE_SERVICE_REUSE 1\n"
    "CLI_USES_SERVICE 1\n"
    "C_API_USES_SERVICE 1\n"
    "IDENTICAL_EQUATION_ASSESSMENT 1\n"
    "IDENTICAL_SOLVE_PLAN 1\n"
    "IDENTICAL_BACKEND 1\n"
    "IDENTICAL_GATE_METRICS 1\n"
    "IDENTICAL_SOLUTION 1\n"
    "COMPLEMENTARITY_C_API_USES_SERVICE 1\n"
    "COMPLEMENTARITY_DENSE_CSR_EQUIVALENT 1\n"
    "COMPLEMENTARITY_ORIGINAL_GAP_GATE 1\n"
    "COMPLEMENTARITY_ACTIVE_SET_FALLBACK 1\n"
    "BLOCK_GRAPH_C_API_USES_SERVICE 1\n"
    "BLOCK_GRAPH_MULTIRATE_ZERO_ORDER_HOLD 1\n"
    "BLOCK_GRAPH_DETERMINISTIC_COMMIT_ORDER 1\n"
    "BLOCK_GRAPH_ORIGINAL_GATE 1\n"
    "BLOCK_GRAPH_LOCAL_FALLBACK 1\n"
    "BLOCK_GRAPH_ALGEBRAIC_FIXED_POINT 1\n"
    "BLOCK_GRAPH_DIVERGENT_FEEDBACK_REJECTED 1\n"
    "NONLINEAR_CLI_USES_SERVICE 1\n"
    "NONLINEAR_C_API_USES_SERVICE 1\n"
    "NONLINEAR_IDENTICAL_EQUATION_ASSESSMENT 1\n"
    "NONLINEAR_IDENTICAL_SOLVE_PLAN 1\n"
    "NONLINEAR_IDENTICAL_BACKEND 1\n"
    "NONLINEAR_IDENTICAL_GATE_METRICS 1\n"
    "NONLINEAR_IDENTICAL_SOLUTION 1\n"
    "ODE_CLI_USES_SERVICE 1\n"
    "ODE_C_API_USES_SERVICE 1\n"
    "ODE_IDENTICAL_EQUATION_ASSESSMENT 1\n"
    "ODE_IDENTICAL_SOLVE_PLAN 1\n"
    "ODE_IDENTICAL_BACKEND 1\n"
    "ODE_IDENTICAL_GATE_METRICS 1\n"
    "ODE_IDENTICAL_SOLUTION 1\n"
    "DAE_C_API_USES_SERVICE 1\n"
    "DAE_ORIGINAL_RESIDUAL_GATE 1\n"
    "DAE_FINITE_DIFFERENCE_FALLBACK 1\n"
    "DAE_INCONSISTENT_INITIAL_REJECTED 1\n"
    "DAE_INDEX_TWO_C_API_USES_SERVICE 1\n"
    "DAE_INDEX_TWO_HIDDEN_CONSISTENCY_GATE 1\n"
    "DAE_INDEX_TWO_HIDDEN_RANK_GATE 1\n"
    "DAE_INDEX_TWO_RESULT_ABI_TAIL_COMPATIBLE 1\n"
    "C_API_COOPERATIVE_CANCELLATION 1\n"
    "C_API_CANCELLATION_ATOMIC_COMMIT 1\n"
    "C_API_CANCELLATION_TOKEN_LIFECYCLE 1\n"
    "C_API_COOPERATIVE_DEADLINE 1\n"
    "C_API_DEADLINE_ATOMIC_COMMIT 1\n"
    "C_API_DEADLINE_TOKEN_NON_MUTATING 1\n"
    "C_API_EXTERNAL_LINEAR_FALLBACK 1\n"
    "C_API_EXTERNAL_LINEAR_FALLBACK_ORIGINAL_GATE 1\n"
    "C_API_EXTERNAL_LINEAR_FALLBACK_CONTROL_BOUNDARY 1\n"
    "C_API_EXTERNAL_NONLINEAR_FALLBACK 1\n"
    "C_API_EXTERNAL_NONLINEAR_FALLBACK_ORIGINAL_GATE 1\n"
    "C_API_EXTERNAL_NONLINEAR_FALLBACK_CONTROL_BOUNDARY 1\n"
    "C_API_EXTERNAL_ODE_STEPPER_FALLBACK 1\n"
    "C_API_EXTERNAL_ODE_STEPPER_ORIGINAL_GATE 1\n"
    "C_API_EXTERNAL_DAE_STEPPER_FALLBACK 1\n"
    "C_API_EXTERNAL_DAE_STEPPER_ORIGINAL_GATE 1\n"
    "C_API_EXTERNAL_STEPPER_CONTROL_BOUNDARY 1\n"
    "DAE_EVENT_C_API_USES_SERVICE 1\n"
    "DAE_EVENT_IMPLICIT_ROOT 1\n"
    "DAE_EVENT_CONSISTENT_REINIT 1\n"
    "DAE_EVENT_NEGATIVE_GATES 1\n"
    "DAE_EVENT_STABLE_DIAGNOSTIC_CODES 1\n"
    "DAE_EVENT_CONCURRENT_SHARED_SOLVER 1\n"
    "DAE_EVENT_CONCURRENT_BITWISE_DETERMINISTIC 1\n"
    "DAE_EVENT_ALLOCATOR_BALANCED 1\n"
    "DAE_RESULT_LEGACY_PREFIX_COMPATIBLE 1\n"
    "EVENT_C_API_USES_SERVICE 1\n"
    "EVENT_DIRECTIONAL_ROOT 1\n"
    "EVENT_PRIORITY_ATOMIC_RESET 1\n"
    "EVENT_STUCK_RESET_REJECTED 1\n"
    "HYBRID_C_API_USES_SERVICE 1\n"
    "HYBRID_MODE_SWITCHING 1\n"
    "HYBRID_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1\n"
    "HYBRID_TRANSACTION_ROLLBACK 1\n"
    "HYBRID_STABLE_PRE_ACROSS_MICROSTEPS 1\n"
    "HYBRID_WRITESET_CONFLICT_ARBITRATION 1\n"
    "HYBRID_STABLE_DIAGNOSTIC_CODES 1\n"
    "HYBRID_DAE_C_API_USES_SERVICE 1\n"
    "HYBRID_DAE_TARGET_CONSISTENCY_GATE 1\n"
    "HYBRID_DAE_AUTOMATIC_CONSISTENCY_PROJECTION 1\n"
    "HYBRID_DAE_UNPROJECTABLE_REJECTED 1\n"
    "HYBRID_DAE_SUPERDENSE_CASCADE 1\n"
    "HYBRID_DAE_TRANSACTION_ROLLBACK 1\n"
    "HYBRID_DAE_PROJECTION_METADATA_ATOMIC 1\n"
    "HYBRID_DAE_LEGACY_TRANSITION_PREFIX_COMPATIBLE 1\n"
    "HYBRID_DAE_STABLE_PRE_ACROSS_MICROSTEPS 1\n"
    "HYBRID_DAE_STATE_DERIVATIVE_WRITESET_MERGE 1\n"
    "HYBRID_DAE_WRITESET_CONFLICT_ARBITRATION 1\n"
    "HYBRID_DAE_STABLE_SHARED_SOLVER_DETERMINISTIC 1\n"
    "HYBRID_DAE_CONCURRENT_BITWISE_DETERMINISTIC 1\n"
    "ODE_RESULT_LEGACY_PREFIX_COMPATIBLE 1\n"
    "${cli}\n${cli_nonlinear}\n${cli_ode}\n${c_api_dae}\n${c_api_index_two_dae}\n${c_api_cancellation}\n${c_api_dae_event}\n"
    "${c_api_event}\n${c_api_hybrid}\n${c_api_hybrid_dae}")
