# Needed parameter: URUSC, FILE, EXPECTED, INCLUDE_DIR, WORKING_DIR

get_filename_component(TEST_NAME ${FILE} NAME_WE)
set(C_FILE "${WORKING_DIR}/${TEST_NAME}_tmp.c")
set(EXE_FILE "${WORKING_DIR}/${TEST_NAME}_tmp.exe")
set(ACTUAL_OUT "${WORKING_DIR}/${TEST_NAME}_actual.txt")

# Compile urusc --emit-c
execute_process(
    COMMAND ${URUSC} ${FILE} --emit-c
    OUTPUT_FILE ${C_FILE}
    RESULT_VARIABLE URUS_RES
    ERROR_VARIABLE ERR_OUT
)

if(NOT URUS_RES EQUAL 0)
    message(FATAL_ERROR "Urus compilation failed:\n${ERR_OUT}")
endif()

# Compile --emit-c output to executable
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -std=c11 -O2 -I${INCLUDE_DIR} ${C_FILE} -o ${EXE_FILE} -lm
    RESULT_VARIABLE GCC_RES
    ERROR_VARIABLE ERR_OUT
)

if(NOT GCC_RES EQUAL 0)
    message(FATAL_ERROR "C compilation failed:\n${ERR_OUT}")
endif()

# Compare output
execute_process(
    COMMAND ${EXE_FILE}
    OUTPUT_FILE ${ACTUAL_OUT}
    RESULT_VARIABLE RUN_RES
    ERROR_VARIABLE ERR_OUT
)

if(NOT RUN_RES EQUAL 0)
    message(FATAL_ERROR "Execution failed:\n${ERR_OUT}")
endif()

file(READ ${ACTUAL_OUT} ACTUAL_CONTENT)
file(READ ${EXPECTED} EXPECTED_CONTENT)

# Remove \r for unix/windows compability
string(REPLACE "\r" "" ACTUAL_CONTENT "${ACTUAL_CONTENT}")
string(REPLACE "\r" "" EXPECTED_CONTENT "${EXPECTED_CONTENT}")
string(STRIP "${ACTUAL_CONTENT}" ACTUAL_CONTENT)
string(STRIP "${EXPECTED_CONTENT}" EXPECTED_CONTENT)

if(NOT "${ACTUAL_CONTENT}" STREQUAL "${EXPECTED_CONTENT}")
    message(FATAL_ERROR "Output mismatch!\nExpected:\n${EXPECTED_CONTENT}\nGot:\n${ACTUAL_CONTENT}")
endif()

# Cleanup
file(REMOVE ${C_FILE} ${EXE_FILE} ${ACTUAL_OUT})
