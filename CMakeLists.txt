cmake_minimum_required(VERSION 3.14.1)
project(C5T LANGUAGES C CXX)

if (NOT TARGET C5T)
  message(STATUS "Add `C5T` into `target_link_libraries()` of your `CMakeLists.txt` to use `current` headers.")
  add_library(C5T INTERFACE)
  target_include_directories(C5T INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
