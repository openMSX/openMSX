if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DEFAULT_ALSAMIDI TRUE)
else()
    set(DEFAULT_ALSAMIDI FALSE)
endif()

option(OPENMSX_UNITTEST "Build unittest" FALSE)
option(OPENMSX_SANITIZERS "Enable sanitizers" FALSE)
option(OPENMSX_DEBUG_OPTIONS "Enable debug options" FALSE)
option(COMPONENT_ALSAMIDI "Enable alsamidi support" ${DEFAULT_ALSAMIDI})
option(COMPONENT_LASERDISC "Enable laserdisc support" TRUE)
set(OPENMSX_INSTALL_BASE "/opt/openMSX" CACHE STRING "Directory to install to")

set(OPENMSX_SOURCES "")
set(OPENMSX_INCLUDE_DIRS "")
set(OPENMSX_DEFINES "")

# Generate components.hh
message(STATUS "Generating components.hh")
set (COMPONENTS_LIST "")
if (COMPONENT_ALSAMIDI)
    list(APPEND COMPONENTS_LIST "ALSA")
endif()
if (COMPONENT_LASERDISC)
    list(APPEND COMPONENTS_LIST "LASERDISC")
endif()
string(REPLACE ";" " " COMPONENTS_LIST "${COMPONENTS_LIST}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/components.hh.in
    ${CMAKE_CURRENT_BINARY_DIR}/components.hh
)

# Generate systemfuncs.hh
message(STATUS "Generating systemfuncs.hh")
include(CheckSymbolExists)
check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
check_symbol_exists(ftruncate "unistd.h" HAVE_FTRUNCATE)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/systemfuncs.hh.in
    ${CMAKE_CURRENT_BINARY_DIR}/systemfuncs.hh
)
list(APPEND OPENMSX_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}")

# Generate version.ii
message(STATUS "Generating Version.ii")
set(OPENMSX_RELEASE_BOOL false)
if (OPENMSX_RELEASE_FLAG)
    set(OPENMSX_RELEASE_BOOL true)
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.ii.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.ii
)

# Generate build-info.ii
message(STATUS "Generating build-info.hh")
## PLATFORM_ANDROID, hardcoded, fine
## {MIN,MAX}_SCALE_FACTOR, hardcoded, fine
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TARGET_PLATFORM)
set(OPENMSX_SET_WINDOW_ICON true)
if (TARGET_PLATFORM STREQUAL "darwin")
    set(OPENMSX_SET_WINDOW_ICON false)
endif()
set(DATADIR "${OPENMSX_INSTALL_BASE}/share")
set(DOCDIR "${OPENMSX_INSTALL_BASE}/doc")
set(BUILD_FLAVOUR "${CMAKE_BUILD_TYPE}")
if (BUILD_FLAVOUR STREQUAL "")
    set(BUILD_FLAVOUR "Debug")
endif()
set(TARGET_CPU "${CMAKE_SYSTEM_PROCESSOR}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build-info.hh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build-info.hh
)

# Collect all openMSX source files
function(openmsx_sources) # helper, called from "sources.cmake" files
    # Define the argument structure
    set(options) # No boolean options
    set(oneValueArgs) # No single-value arguments
    set(multiValueArgs SOURCES SUBDIRS)

    # Parse the arguments
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Add source files
    foreach(src IN LISTS ARGS_SOURCES)
        list(APPEND OPENMSX_SOURCES "${CMAKE_CURRENT_LIST_DIR}/${src}")
    endforeach()

    # Add current directory as an include directory
    list(APPEND OPENMSX_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")

    # Recurse into subdirectories
    foreach(dir IN LISTS ARGS_SUBDIRS)
        include("${CMAKE_CURRENT_LIST_DIR}/${dir}/sources.cmake")
    endforeach()

    # Export variables to the parent scope
    set(OPENMSX_SOURCES ${OPENMSX_SOURCES} PARENT_SCOPE)
    set(OPENMSX_INCLUDE_DIRS ${OPENMSX_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()
openmsx_sources(SUBDIRS .)

# Define the openMSX executable
add_executable(openmsx)
target_sources(openmsx PRIVATE ${OPENMSX_SOURCES})
if (OPENMSX_UNITTEST)
    # TODO, alternative: use FetchContent to download Catch2
    list(APPEND OPENMSX_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Contrib/catch2")
    list(APPEND OPENMSX_DEFINES "UNITTEST")
    set(OPENMSX_SANITIZERS TRUE)
    set(OPENMSX_DEBUG_OPTIONS TRUE)
endif()
if (OPENMSX_SANITIZERS)
    enable_sanitizer(openmsx)
endif()
if (OPENMSX_DEBUG_OPTIONS)
    list(APPEND OPENMSX_DEFINES "DEBUG" "_GLIBCXX_DEBUG" "_GLIBCXX_DEBUG_PEDANTIC")
    enable_debug_symbols(openmsx)
endif()
target_compile_definitions(openmsx PRIVATE ${OPENMSX_DEFINES})
list(REMOVE_DUPLICATES OPENMSX_INCLUDE_DIRS)
target_include_directories(openmsx PRIVATE ${OPENMSX_INCLUDE_DIRS})
target_compile_features(openmsx PRIVATE cxx_std_23)
enable_warnings(openmsx)

# Add library dependencies
set(EXTERNAL_LIBS
    ALSA ALSA::ALSA
    Freetype Freetype::Freetype
    OpenGL OpenGL::GL
    GLEW GLEW::GLEW
    PNG PNG::PNG
    Ogg Ogg::ogg
    SDL2 SDL2::SDL2|SDL2::SDL2main
    SDL2_ttf SDL2_ttf::SDL2_ttf|sdl_ttf::sdl_ttf
    TCL tcl::tcl
    THEORA theora::dec|theora::theora
    Vorbis Vorbis::vorbis|vorbis::vorbis
    ZLIB ZLIB::ZLIB
)
find_and_check_external_libraries(openmsx ${EXTERNAL_LIBS})

# Schedule generating git_version.ii during build
set(GIT_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/git_version.ii")
add_custom_target(generate_git_version
    COMMAND ${CMAKE_COMMAND} -D OUTPUT_FILE=${GIT_VERSION_HEADER} -P ${CMAKE_SOURCE_DIR}/cmake/WriteGitVersionHeader.cmake
)
add_dependencies(openmsx generate_git_version)
