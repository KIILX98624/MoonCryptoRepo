execute_process(
    COMMAND "${MONC}" run "${FIXTURE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "MonC float fixture failed (exit ${result}): ${error_output}")
endif()

string(STRIP "${actual}" actual)
set(expected "sum=3.75
scaled=7.5
exp=25
float-ok")
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Unexpected float output.\nExpected:\n${expected}\nActual:\n${actual}")
endif()
