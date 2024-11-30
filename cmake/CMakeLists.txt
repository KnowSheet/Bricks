# TODO(dkorolev): Consider intelligently re-running `cmake` from the `Makefile` if the set of source files has changed.

cmake_minimum_required(VERSION 3.14.1)

project(c5t_user_project C CXX)

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

# Settings for `googletest`. It builds faste without `gmock`.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
option(BUILD_GMOCK OFF)

set_property(GLOBAL PROPERTY C5T_LIBRARIES_PROPERTY "Threads::Threads" "C5T")

# The helper to clone a dependency, or use it from a sibling dir if available.
function(UseGitOrCloneImpl remote use_custom_branch custom_branch)
  cmake_path(GET remote FILENAME dep)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${dep}" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/${dep}'.")
    add_subdirectory("${CMAKE_SOURCE_DIR}/${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/${dep}': Configured.")
    set("C5T_DEP_DIR_${dep}" "${CMAKE_SOURCE_DIR}/${dep}" CACHE INTERNAL "")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/../${dep}" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/../${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/../${dep}'.")
    add_subdirectory("${CMAKE_SOURCE_DIR}/../${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/../${dep}': Configured.")
    set("C5T_DEP_DIR_${dep}" "${CMAKE_SOURCE_DIR}/../${dep}" CACHE INTERNAL "")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_CURRENT_SOURCE_DIR/../${dep}'.")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_CURRENT_SOURCE_DIR/../${dep}': Configured.")
    set("C5T_DEP_DIR_${dep}" "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" CACHE INTERNAL "")
  else()
    if(NOT use_custom_branch)
      message(STATUS "Cloning `${dep}` from `${remote}` using the default branch ...")
      execute_process(OUTPUT_QUIET ERROR_QUIET COMMAND git clone --depth 1 ${remote} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
      message(STATUS "Cloning `${dep}` from `${remote}:${branch}` ...")
      execute_process(OUTPUT_QUIET ERROR_QUIET COMMAND git clone --depth 1 -b ${custom_branch} ${remote} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${dep}" ${dep})
    message(STATUS "Cloning `${dep}` from `${remote}`: Configured.")
    if("${C5T_PROJECT_ROOT}" STREQUAL "")
      file(TOUCH .gitignore)
      file(APPEND .gitignore "${dep}/\n")  # NOTE(dkorolev): This may add extra `.gitignore` lines, ignore for now.
    endif()
    set("C5T_DEP_DIR_${dep}" "${CMAKE_CURRENT_SOURCE_DIR}/${dep}" CACHE INTERNAL "")
  endif()
  # If the added repository contains the `.c5t_cmake_dependency` file,
  # then add this repo's C5T library name to all targets built by this `CMakeLists.txt`.
  if(EXISTS "${C5T_DEP_DIR_${dep}}/.c5t_cmake_dependency")
    string(TOUPPER ${dep} UPPERCASE_DEP)
    message(STATUS "Adding `C5T_${UPPERCASE_DEP}`.")
    get_property(tmp GLOBAL PROPERTY C5T_LIBRARIES_PROPERTY)
    list(APPEND tmp "C5T_${UPPERCASE_DEP}")
    set_property(GLOBAL PROPERTY C5T_LIBRARIES_PROPERTY ${tmp})
  endif()
endfunction()

function(UseGitOrClone remote)
  UseGitOrCloneImpl("${remote}" FALSE _)
endfunction()

function(UseGitOrCloneBranch remote branch)
  UseGitOrCloneImpl("${remote}" TRUE "${branch}")
endfunction()

UseGitOrCloneBranch(https://github.com/dkorolev/current use_current_deps_github_org)

# Set `C5T_NO_GTEST` in the `Makefile` to any non-empty value to speed up the builds that do not need `googletest`.
if(NOT TARGET gtest)
  if("$ENV{C5T_NO_GTEST}" STREQUAL "")
    UseGitOrClone(https://github.com/current-deps/googletest)
  else()
    message(STATUS "Skipping `googletest` as `C5T_NO_GTEST` is set.")
  endif()
endif()

set_property(GLOBAL PROPERTY C5T_PATH ${C5T_DEP_DIR_current})

set(C5T_DEPS_1 $ENV{C5T_DEPS})
if(NOT "${C5T_DEPS_1}" STREQUAL "")
  string(REPLACE ":" ";" C5T_DEPS_2 "${C5T_DEPS_1}")
  string(REPLACE " " ";" C5T_DEPS_3 "${C5T_DEPS_2}")
  foreach(C5T_DEP IN LISTS C5T_DEPS_3)
    UseGitOrClone("https://github.com/current-deps/${C5T_DEP}")
  endforeach()
endif()

get_property(C5T_LIBRARIES GLOBAL PROPERTY C5T_LIBRARIES_PROPERTY)

add_custom_target(C5T_CURRENT_BUILD_INFO_H_TARGET ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/inc/current_build_info.h")
add_custom_target(C5T_CUSTOM_PER_BUILD_SCRIPT_TARGET ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/inc/pre_build_sh_output.inl")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inc/current_build_info.h"
                   COMMAND "${C5T_DEP_DIR_current}/scripts/gen-current-build.sh"
                   ARGS "${CMAKE_CURRENT_BINARY_DIR}/inc/current_build_info.h"
                   DEPENDS inc/current_build_info.h.force_rebuild)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/inc")
if(EXISTS "${CMAKE_SOURCE_DIR}/src/pre_build.sh")
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inc/pre_build_sh_output.inl"
                     COMMAND "${CMAKE_SOURCE_DIR}/src/pre_build.sh"
                     ARGS "${CMAKE_CURRENT_BINARY_DIR}/inc/pre_build_sh_output.inl"
                     WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                     DEPENDS inc/pre_build_sh_output.inl.force_rebuild)
else()
  file(TOUCH "${CMAKE_CURRENT_BINARY_DIR}/inc/pre_build_sh_output.inl")
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/inc/pre_build_sh_output.inl" "#pragma once\n// No `src/pre_build.sh` found.\n")
endif()

add_custom_command(OUTPUT inc/current_build_info.h.force_rebuild COMMAND cmake -E echo >/dev/null)
add_custom_command(OUTPUT inc/pre_build_sh_output.inl.force_rebuild COMMAND cmake -E echo >/dev/null)

if("${C5T_PROJECT_ROOT}" STREQUAL "")
  set(C5T_PROJECT_ROOT_SRC "src")
else()
  set(C5T_PROJECT_ROOT_SRC "${C5T_PROJECT_ROOT}/src")
endif()

# Declare shared libraries as shared library targets. Do not link them against anything external.
file(GLOB_RECURSE BINARY_SOURCE_FILES "${C5T_PROJECT_ROOT_SRC}/dlib_*.cc")
foreach(SHARED_LIBRARY_SOURCE_FILE ${BINARY_SOURCE_FILES})
  get_filename_component(SHARED_LIBRARY_TARGET_NAME "${SHARED_LIBRARY_SOURCE_FILE}" NAME_WE)
  add_library(${SHARED_LIBRARY_TARGET_NAME} SHARED "${SHARED_LIBRARY_SOURCE_FILE}") 
  add_dependencies(${SHARED_LIBRARY_TARGET_NAME} C5T_CURRENT_BUILD_INFO_H_TARGET C5T_CUSTOM_PER_BUILD_SCRIPT_TARGET)
  # TODO(dkorolev): Might be worth it to `grep` this `dlib_*.cc` source file for required library dependencies.
  target_compile_definitions(${SHARED_LIBRARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
  target_compile_definitions(${SHARED_LIBRARY_TARGET_NAME} PRIVATE C5T_DLIB)
  target_include_directories(${SHARED_LIBRARY_TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inc" ${C5T_DEP_DIR_current})
  target_link_libraries(${SHARED_LIBRARY_TARGET_NAME} PRIVATE "${C5T_LIBRARIES}")
endforeach()

# Declare user-defined libraries as library targets.
set(USR_LIBRARIES)
file(GLOB_RECURSE LIBRARY_SOURCE_FILES "${C5T_PROJECT_ROOT_SRC}/lib_*.cc")
foreach(LIBRARY_SOURCE_FILE ${LIBRARY_SOURCE_FILES})
  get_filename_component(LIBRARY_TARGET_NAME "${LIBRARY_SOURCE_FILE}" NAME_WE)
  list(APPEND USR_LIBRARIES "${LIBRARY_TARGET_NAME}")
endforeach()

# Then build all the libraries, such that every library is linked against every other library.
file(GLOB_RECURSE LIBRARY_SOURCE_FILES "${C5T_PROJECT_ROOT_SRC}/lib_*.cc")
foreach(LIBRARY_SOURCE_FILE ${LIBRARY_SOURCE_FILES})
  get_filename_component(LIBRARY_TARGET_NAME "${LIBRARY_SOURCE_FILE}" NAME_WE)
  add_library(${LIBRARY_TARGET_NAME} STATIC "${LIBRARY_SOURCE_FILE}")
  add_dependencies(${LIBRARY_TARGET_NAME} C5T_CURRENT_BUILD_INFO_H_TARGET C5T_CUSTOM_PER_BUILD_SCRIPT_TARGET)
  target_compile_definitions(${LIBRARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
  target_include_directories(${LIBRARY_TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inc" ${C5T_DEP_DIR_current})
  set(USR_LIBRARIES_WO_SELF "${USR_LIBRARIES}")
  list(REMOVE_ITEM USR_LIBRARIES_WO_SELF ${LIBRARY_TARGET_NAME})
  target_link_libraries(${LIBRARY_TARGET_NAME} PRIVATE "${USR_LIBRARIES_WO_SELF}")
endforeach()

# Declare binaries as binary targets. And link them against all the libraries.
file(GLOB_RECURSE BINARY_SOURCE_FILES "${C5T_PROJECT_ROOT_SRC}/*.cc")
foreach(BINARY_SOURCE_FILE ${BINARY_SOURCE_FILES})
  get_filename_component(BINARY_TARGET_NAME "${BINARY_SOURCE_FILE}" NAME_WE)
  if(NOT (BINARY_TARGET_NAME MATCHES "^lib_.*$" OR BINARY_TARGET_NAME MATCHES "^test_.*$" OR BINARY_TARGET_NAME MATCHES "^dlib_.*$"))
    add_executable(${BINARY_TARGET_NAME} "${BINARY_SOURCE_FILE}") 
    add_dependencies(${BINARY_TARGET_NAME} C5T_CURRENT_BUILD_INFO_H_TARGET C5T_CUSTOM_PER_BUILD_SCRIPT_TARGET)
    target_compile_definitions(${BINARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
    target_include_directories(${BINARY_TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inc")
    target_link_libraries(${BINARY_TARGET_NAME} PRIVATE "${C5T_LIBRARIES}")
    target_link_libraries(${BINARY_TARGET_NAME} PRIVATE "${USR_LIBRARIES}")
  endif()
endforeach()

# Declare tests as test targets. And link them against all the libraries.
enable_testing()
file(GLOB_RECURSE TEST_SOURCE_FILES "${C5T_PROJECT_ROOT_SRC}/test_*.cc")
foreach(TEST_SOURCE_FILE ${TEST_SOURCE_FILES})
  get_filename_component(TEST_TARGET_NAME "${TEST_SOURCE_FILE}" NAME_WE)
  add_executable(${TEST_TARGET_NAME} "${TEST_SOURCE_FILE}") 
  add_dependencies(${TEST_TARGET_NAME} C5T_CURRENT_BUILD_INFO_H_TARGET C5T_CUSTOM_PER_BUILD_SCRIPT_TARGET)
  target_include_directories(${TEST_TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inc")
  target_link_libraries(${TEST_TARGET_NAME} PRIVATE gtest_main "${C5T_LIBRARIES}")
  target_link_libraries(${TEST_TARGET_NAME} PRIVATE "${USR_LIBRARIES}")
  add_test(NAME ${TEST_TARGET_NAME} COMMAND ${TEST_TARGET_NAME})
endforeach()
