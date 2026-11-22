execute_process(
    COMMAND "${MONC}" run "${FIXTURE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "MonC C interop fixture failed (exit ${result}): ${error_output}")
endif()

string(STRIP "${actual}" actual)
set(expected "sqrt=5
pow=32
sin=0
abs=17")
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Unexpected C interop output.\nExpected:\n${expected}\nActual:\n${actual}")
endif()
