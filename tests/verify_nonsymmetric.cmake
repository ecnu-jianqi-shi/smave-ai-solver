foreach(required OUTPUT REPEAT_OUTPUT TRACE REPEAT_TRACE PIVOT_OUTPUT
        PIVOT_REPEAT_OUTPUT PIVOT_TRACE PIVOT_REPEAT_TRACE)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "missing non-symmetric artifact: ${required}")
    endif()
endforeach()
if(NOT DEFINED SPARSE_DIRECT_EVIDENCE_TEXT)
    message(FATAL_ERROR "missing non-symmetric artifact: SPARSE_DIRECT_EVIDENCE_TEXT")
endif()
foreach(pattern
    "SMAVE_SPARSE_DIRECT_EVIDENCE 1"
    "SUCCESS 1"
    "SCALE 9.9999999999999998e-17"
    "MAXIMUM_SOLUTION_ERROR 2.2204460492503131e-16"
    "INITIAL_NONZEROS 10"
    "ORDERING_FILL_EDGES 0"
    "NATURAL_FILL_EDGES 5"
    "ROW_SWAPS 2"
    "MINIMUM_SCALED_PIVOT 0.5"
    "SINGULAR_REJECTED 1"
    "INVALID_THRESHOLD_REJECTED 1"
    "COLUMN_ORDER 3 4 0 1 2")
    string(FIND "${SPARSE_DIRECT_EVIDENCE_TEXT}" "${pattern}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ordered sparse direct evidence missing: ${pattern}")
    endif()
endforeach()
file(READ "${PIVOT_OUTPUT}" pivot_output)
file(READ "${PIVOT_REPEAT_OUTPUT}" pivot_repeat_output)
foreach(text IN ITEMS "${pivot_output}" "${pivot_repeat_output}")
    foreach(pattern
        "status: success"
        "path=DIRECT_ACCEPT"
        "x1=1"
        "x2=1"
        "x3=1"
        "fallback=0")
        string(FIND "${text}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "pivoted sparse solve evidence missing: ${pattern}")
        endif()
    endforeach()
endforeach()
file(READ "${OUTPUT}" output)
file(READ "${REPEAT_OUTPUT}" repeat_output)
foreach(text IN ITEMS "${output}" "${repeat_output}")
    foreach(pattern
        "status: success"
        "path=DIRECT_ACCEPT"
        "x1=1"
        "x2=1"
        "x3=1"
        "x4=1"
        "fallback=0")
        string(FIND "${text}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "non-symmetric solve evidence missing: ${pattern}")
        endif()
    endforeach()
endforeach()
file(READ "${TRACE}" trace)
file(READ "${REPEAT_TRACE}" repeat_trace)
foreach(text IN ITEMS "${trace}" "${repeat_trace}")
    foreach(pattern
        "BLOCK block-1 DIRECT_ACCEPT"
        "SUMMARY direct=1 corrected=0 warm_start=0 fallback=0")
        string(FIND "${text}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "non-symmetric solve trace evidence missing: ${pattern}")
        endif()
    endforeach()
    # The 4x4 banded non-symmetric system may be solved by the structured
    # tridiagonal direct fast-path (correct for tridiagonal structure) or by
    # restarted GMRES+ILU(0).  Both must pass the independent residual gate.
    string(FIND "${text}" "ATTEMPT \"structured-tridiagonal-direct-cpu-v1\" \"accepted\"" structured_found)
    string(FIND "${text}" "preconditioner=\"gmres-ilut-cpu-v1\"" gmres_found)
    string(FIND "${text}" "ATTEMPT \"gmres-ilut-cpu-v1\" \"accepted\"" gmres_attempt_found)
    if(structured_found EQUAL -1 AND (gmres_found EQUAL -1 OR gmres_attempt_found EQUAL -1))
        message(FATAL_ERROR "non-symmetric trace must accept via structured-tridiagonal-direct or GMRES+ILU(0)")
    endif()
    if(text MATCHES "breakdown=1|stagnated=1")
        message(FATAL_ERROR "non-symmetric Krylov path broke down or stagnated")
    endif()
    if(text MATCHES "EXPERT pcg-|ATTEMPT \"pcg-")
        message(FATAL_ERROR "numeric probe retained an ineligible PCG backend")
    endif()
endforeach()
file(READ "${PIVOT_TRACE}" pivot_trace)
file(READ "${PIVOT_REPEAT_TRACE}" pivot_repeat_trace)
foreach(text IN ITEMS "${pivot_trace}" "${pivot_repeat_trace}")
    foreach(pattern
        "BLOCK block-1 DIRECT_ACCEPT"
        "ATTEMPT \"gmres-ilut-cpu-v1\" \"rejected\" \"invalid GMRES input\""
        "ATTEMPT \"gmres-ilu0-cpu-v1\" \"rejected\" \"invalid GMRES input\""
        "SUMMARY direct=1 corrected=0 warm_start=0 fallback=0")
        string(FIND "${text}" "${pattern}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "sparse pivot trace evidence missing: ${pattern}")
        endif()
    endforeach()
    if(SPARSE_DIRECT_EVIDENCE_TEXT MATCHES "INDUSTRIAL_AVAILABLE 1")
        foreach(pattern
            "ATTEMPT \"accelerate-sparse-qr-cpu-v1\" \"accepted\""
            "backend=accelerate-sparse-qr-cpu-v1"
            "matrix_nnz=7"
            "rank_probe=passed")
            string(FIND "${text}" "${pattern}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "industrial sparse QR trace evidence missing: ${pattern}")
            endif()
        endforeach()
    else()
        foreach(pattern
            "ATTEMPT \"sparse-ordered-threshold-pivot-cpu-v2\" \"accepted\" \"runtime residual and constraints pass; ordering=amd-greedy"
            "row_swaps=2"
            "initial_nnz=7"
            "min_scaled_pivot="
            "column_order=")
            string(FIND "${text}" "${pattern}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "ordered sparse pivot trace evidence missing: ${pattern}")
            endif()
        endforeach()
    endif()
    if(text MATCHES "EXPERT pcg-|ATTEMPT \"pcg-")
        message(FATAL_ERROR "pivot plan retained an ineligible PCG backend")
    endif()
endforeach()
message(STATUS "Non-symmetric GMRES+ILUT, sparse pivoting, residual gate, and fallback invariants passed")
