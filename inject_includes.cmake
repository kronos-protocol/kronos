file(READ "${INPUT_FILE}" CONTENT)

string(REPLACE ";" "\n" INCLUDES_TO_ADD "${HEADER_INCLUDES}")

string(REGEX MATCH "#include \"cmock\\.h\"" CMOCK_INCLUDE "${CONTENT}")

if(NOT CMOCK_INCLUDE)
    message(FATAL_ERROR "Could not find cmock.h include in ${INPUT_FILE}")
endif()

string(REGEX REPLACE
    "(#include \"cmock\\.h\")(\\n)"
    "\\1\n${INCLUDES_TO_ADD}\\2"
    CONTENT "${CONTENT}"
)

file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
message(STATUS "✓ Injected includes into mock: ${OUTPUT_FILE}")
