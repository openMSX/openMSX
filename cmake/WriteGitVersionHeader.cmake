# Get Git version string
execute_process(
    COMMAND git describe --tags --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# If failed (e.g., not a Git repo)
if(NOT GIT_VERSION)
    set(GIT_VERSION "0.0.0-unknown")
endif()

# New file content
set(FILE_CONTENT "#define GIT_VERSION \"${GIT_VERSION}\"\n")

# Only overwrite if necessary
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" OLD_CONTENT)
else()
    set(OLD_CONTENT "")
endif()
if(NOT OLD_CONTENT STREQUAL FILE_CONTENT)
    file(WRITE "${OUTPUT_FILE}" "${FILE_CONTENT}")
endif()
