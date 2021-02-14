cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(mwcas
  VERSION "0.1.0"
  DESCRIPTION "An open-source MwCAS library."
  LANGUAGES "CXX"
)

# define source directories
set(MWCAS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/googletest")

# build BzTree as a header-only library
add_library(mwcas INTERFACE)
target_compile_features(mwcas INTERFACE "cxx_std_17")
target_compile_options(mwcas INTERFACE
  -Wall
  -Wextra
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
)
target_include_directories(mwcas INTERFACE
  "${MWCAS_SOURCE_DIR}/src"
  "${MWCAS_SOURCE_DIR}/include"
)
target_link_libraries(mwcas INTERFACE
  pthread
)

# build unit tests if required
option(MWCAS_BUILD_TESTS, "Build unit tests for a MwCAS library" OFF)
if(${MWCAS_BUILD_TESTS})
  enable_testing()
  # use GoogleTest for unit tests
  add_subdirectory("${GOOGLETEST_SOURCE_DIR}")
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()