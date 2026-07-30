if(NOT DEFINED EXECUTABLE OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "EXECUTABLE and OUTPUT are required")
endif()

execute_process(
    COMMAND "${EXECUTABLE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)

file(WRITE "${OUTPUT}" "${stdout}${stderr}")

if(NOT result EQUAL 0)
    message(FATAL_ERROR "C ABI evidence failed with ${result}: ${stderr}")
endif()

foreach(marker
        "SUCCESS 1"
        "DENSE 1"
        "CSR 1"
        "NONLINEAR 1"
        "ODE 1"
        "DAE 1"
        "EVENTS 1"
        "COMPLEMENTARITY_CAPABILITY 1"
        "NONLINEAR_FALLBACK 1"
        "NONLINEAR_SHARED_SERVICE 1"
        "DAE_SHARED_SERVICE 1"
        "EVENT_SHARED_SERVICE 1"
        "CONCURRENT 1"
        "SINGULAR_REJECTED 1"
        "STABLE_DIAGNOSTIC_CODES 1"
        "ABI_MISMATCH_REJECTED 1"
        "ALLOCATOR_BALANCED 1")
    string(FIND "${stdout}" "${marker}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "C ABI evidence missing marker: ${marker}")
    endif()
endforeach()
