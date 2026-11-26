execute_process(
    COMMAND "\${MONC}" run "\${FIXTURE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Python built-ins fixture failed (\${result}): \${error}")
endif()
string(STRIP "\${actual}" actual)
set(expected "12.5
1
-42
3
-0xff
3
15
2
30
10
4
4
true")
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Unexpected built-ins output.\nExpected:\n\${expected}\nActual:\n\${actual}")
endif()
