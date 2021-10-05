cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  mwcas
  VERSION "1.0.0"
  DESCRIPTION "An open-source MwCAS library."
  LANGUAGES "CXX"
)

# define source directories
set(MWCAS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

#--------------------------------------------------------------------------------------#
# Build targets
#--------------------------------------------------------------------------------------#

add_library(mwcas INTERFACE)
target_compile_features(
  mwcas INTERFACE
  "cxx_std_17"
)
target_compile_options(
  mwcas INTERFACE
  -Wall
  -Wextra
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
)
target_include_directories(
  mwcas INTERFACE
  "${MWCAS_SOURCE_DIR}/include"
)
target_link_libraries(
  mwcas INTERFACE
  pthread
)

#--------------------------------------------------------------------------------------#
# Set Compile Options
#--------------------------------------------------------------------------------------#

if(DEFINED MWCAS_CAPACITY)
  target_compile_definitions(
    mwcas INTERFACE
    MWCAS_CAPACITY=${MWCAS_CAPACITY}
  )
endif()

#--------------------------------------------------------------------------------------#
# Build Unit Tests
#--------------------------------------------------------------------------------------#

option(MWCAS_BUILD_TESTS, "Build unit tests for a MwCAS library" OFF)
if(${MWCAS_BUILD_TESTS})
  enable_testing()
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()
