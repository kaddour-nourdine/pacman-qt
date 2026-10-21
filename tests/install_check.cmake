# Checks that pacman_core can be installed and found again.
#
# The script installs the library into a temporary prefix, then configures
# and builds a small project outside the source tree against it with
# find_package(pacman_core). Nothing else proves that the export rules
# work: a broken export still builds the tree perfectly well.
#
# Run it through CTest:
#
#     add_test(NAME InstallCheck
#         COMMAND ${CMAKE_COMMAND}
#             -DBUILD_DIR=${CMAKE_BINARY_DIR}
#             -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
#             -P ${CMAKE_CURRENT_SOURCE_DIR}/install_check.cmake)

if(NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "install_check.cmake needs -DBUILD_DIR=<build dir> -DSOURCE_DIR=<source dir>")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/install-check")
set(prefix "${work}/prefix")

file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${prefix}")

# Runs a command and stops with its output when it fails.
function(run_step description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed (exit ${result}):\n${output}\n${error}")
    endif()
    set(step_output "${output}" PARENT_SCOPE)
endfunction()

run_step("building pacman_core"
    ${CMAKE_COMMAND} --build "${BUILD_DIR}" --target pacman_core)

run_step("installing pacman_core"
    ${CMAKE_COMMAND} --install "${BUILD_DIR}" --prefix "${prefix}")

# The consumer is built with the same flags as this build. Without that, a
# build that instruments the code (--coverage, -fsanitize=...) installs a
# library that needs a runtime the consumer does not link.
set(consumer_flags "")
if(DEFINED CONSUMER_CXX_FLAGS AND NOT CONSUMER_CXX_FLAGS STREQUAL "")
    list(APPEND consumer_flags "-DCMAKE_CXX_FLAGS=${CONSUMER_CXX_FLAGS}")
endif()
if(DEFINED CONSUMER_LINKER_FLAGS AND NOT CONSUMER_LINKER_FLAGS STREQUAL "")
    list(APPEND consumer_flags "-DCMAKE_EXE_LINKER_FLAGS=${CONSUMER_LINKER_FLAGS}")
endif()

run_step("configuring the consumer"
    ${CMAKE_COMMAND} -S "${SOURCE_DIR}/tests/install_consumer" -B "${work}/consumer"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    ${consumer_flags})

run_step("building the consumer"
    ${CMAKE_COMMAND} --build "${work}/consumer")

# The executable is in the build directory for a single-config generator and
# in a subdirectory for a multi-config one.
file(GLOB_RECURSE candidates
    "${work}/consumer/pacman_core_consumer"
    "${work}/consumer/pacman_core_consumer.exe"
    "${work}/consumer/*/pacman_core_consumer"
    "${work}/consumer/*/pacman_core_consumer.exe"
)
set(consumer "")
foreach(candidate IN LISTS candidates)
    if(EXISTS "${candidate}")
        set(consumer "${candidate}")
        break()
    endif()
endforeach()
if(consumer STREQUAL "")
    message(FATAL_ERROR "the consumer was built but its executable was not found")
endif()

execute_process(
    COMMAND "${consumer}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "the consumer failed (exit ${result}):\n${output}\n${error}")
endif()

message(STATUS "install check: ${output}")
