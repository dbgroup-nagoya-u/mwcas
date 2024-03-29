cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  mwcas
  VERSION "1.1.1"
  DESCRIPTION "An open-source MwCAS library."
  LANGUAGES "CXX"
)

# define source directories
set(MWCAS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# this library uses multi-threads
find_package(Threads)

# check SSE support for spinlock hints
include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("xmmintrin.h" MWCAS_HAS_SPINLOCK_HINT)

#--------------------------------------------------------------------------------------#
# BzTree configurations
#--------------------------------------------------------------------------------------#

set(MWCAS_CAPACITY "4" CACHE STRING "The maximum number of target words for MwCAS.")
set(MWCAS_RETRY_THRESHOLD "10" CACHE STRING "The maximum number of retries for preventing busy loops.")
set(MWCAS_SLEEP_TIME "10" CACHE STRING "A sleep time for preventing busy loops [us].")

#--------------------------------------------------------------------------------------#
# Build targets
#--------------------------------------------------------------------------------------#

if(NOT TARGET dbgroup::mwcas)
  add_library(mwcas INTERFACE)
  add_library(dbgroup::mwcas ALIAS mwcas)
  target_compile_features(mwcas INTERFACE
    "cxx_std_17"
  )
  target_include_directories(mwcas INTERFACE
    "${MWCAS_SOURCE_DIR}/include"
  )
  target_compile_definitions(mwcas INTERFACE
    MWCAS_CAPACITY=${MWCAS_CAPACITY}
    MWCAS_RETRY_THRESHOLD=${MWCAS_RETRY_THRESHOLD}
    MWCAS_SLEEP_TIME=${MWCAS_SLEEP_TIME}
    $<$<BOOL:${MWCAS_HAS_SPINLOCK_HINT}>:MWCAS_HAS_SPINLOCK_HINT>
  )
  target_link_libraries(mwcas INTERFACE
    Threads::Threads
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
