# Add warning flags to a target
function(enable_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wundef -Wno-invalid-offsetof -Wunused-macros
            -Wmissing-declarations -Wshadow -Wno-sign-conversion
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()

# Enable debug symbols for the given target.
function(enable_debug_symbols target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -g)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /Zi)
        target_link_options(${target} PRIVATE /DEBUG)
    endif()
endfunction()

# Enable address and undefined behavior sanitizers for the given target,
# only supported on Clang and GCC.
function(enable_sanitizer target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # Add sanitizer flags for Clang and GCC
	set(FLAGS "-fsanitize=address,undefined")
        target_compile_options(${target} PRIVATE ${FLAGS})
        target_link_options(${target} PRIVATE ${FLAGS})
    endif()
endfunction()

# Tries to locate a single external library
function(find_and_check_external_library package target_alternatives)
    message(DEBUG "Looking for package '${package}' (target '${target_alternatives}')")
    #set(CMAKE_FIND_DEBUG_MODE TRUE)
    find_package(${package} QUIET)

    # Check each target in the list of alternatives
    string(REPLACE "|" ";" targets "${target_alternatives}")
    foreach(target IN LISTS targets)
        if(TARGET ${target})
            set(FOUND_TARGET ${target})
            break()
        endif()
    endforeach()

    # If no target was found, try to find it using pkg-config
    if(NOT DEFINED FOUND_TARGET)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            # Assume the pkg-config name is an all-lowercase version of the
            # cmake package name This is generally true, but an exception is
            # SDL_ttf (but cmake has builtin support for that package).
            string(TOLOWER "${package}" pkg_name)
            message(DEBUG "Trying to find '${pkg_name}' via pkg-config...")
            pkg_check_modules(${pkg_name} QUIET IMPORTED_TARGET ${pkg_name})
            if(TARGET PkgConfig::${pkg_name})
                # Create an alias with the expected target name
                list(GET targets 0 first_target)
                add_library(${first_target} ALIAS PkgConfig::${pkg_name})
                set(FOUND_TARGET ${first_target})
            endif()
        endif()
    endif()

    if(NOT TARGET ${FOUND_TARGET})
        message(FATAL_ERROR
            "Could not find required package '${package}'.\n"
            "Hint: Install the '${package}' development package.\n"
            "If it is installed in a non-standard location, set CMAKE_PREFIX_PATH or PKG_CONFIG_PATH."
        )
    endif()

    # Return the found target
    set(${package}_FOUND_TARGET ${FOUND_TARGET} PARENT_SCOPE)

    # Check if the package provides version information
    if (DEFINED ${package}_VERSION)
        set(version_str " version: ${${package}_VERSION}")
    endif()
    message(STATUS "Found package '${package}' target: '${FOUND_TARGET}'${version_str}")
endfunction()

# Handles a list of external libraries
function(find_and_check_external_libraries target)
    set(args ${ARGN})
    list(LENGTH args num_args)
    math(EXPR last "(${num_args} / 2) - 1")

    foreach(i RANGE 0 ${last})
        math(EXPR idx_package "2 * ${i} + 0")
        math(EXPR idx_target  "2 * ${i} + 1")
        list(GET args ${idx_package} package)
        list(GET args ${idx_target} target_alternatives)
        find_and_check_external_library(${package} ${target_alternatives})
        target_link_libraries(${target} PRIVATE ${${package}_FOUND_TARGET})
    endforeach()
endfunction()
