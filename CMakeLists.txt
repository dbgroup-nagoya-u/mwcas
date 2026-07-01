cmake_minimum_required(VERSION "3.11")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  mwcas
  VERSION "2.2.0"
  DESCRIPTION "An open-source MwCAS library."
  LANGUAGES "CXX"
)

if(NOT TARGET dbgroup::${PROJECT_NAME})
  # enable features
  include(FetchContent)

  #----------------------------------------------------------------------------#
  # Build parameters
  #----------------------------------------------------------------------------#

  set(
    MWCAS_CAPACITY
    "4" CACHE STRING
    "The maximum number of target words for MwCAS."
  )

  set(
    MWCAS_VALUE_BIT_NUM
    "48" CACHE STRING
    "The maximum number of bits for representing values."
  )

  set(
    MWCAS_RETRY_THRESHOLD
    "10" CACHE STRING
    "The maximum number of retries for preventing busy loops."
  )

  set(
    MWCAS_BACKOFF_TIME
    "10" CACHE STRING
    "A back-off time for preventing busy loops [us]."
  )

  #----------------------------------------------------------------------------#
  # Configurations
  #----------------------------------------------------------------------------#

  # use our C++ utilities
  FetchContent_Declare(
    cpp-utility
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/cpp-utility"
    GIT_TAG "1b790bf1a89cf29b5d759e4dd84ce814e727b06d" # v0.17.1
  )
  FetchContent_MakeAvailable(cpp-utility)

   # use our GC
  FetchContent_Declare(
    memory-manager
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/memory-manager"
    GIT_TAG "5982e394ff15d14db39c8ecd0c4516d65faf4074" # v0.5.1
  )
  FetchContent_MakeAvailable(memory-manager)

  #----------------------------------------------------------------------------#
  # Build targets
  #----------------------------------------------------------------------------#

  add_library(${PROJECT_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/deadlock_free/mwcas_descriptor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lock_free/mwcas_descriptor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lock_free/casn_descriptor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lock_free/aopt_descriptor.cpp"
  )
  add_library(dbgroup::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
  target_compile_features(${PROJECT_NAME} PUBLIC
    "cxx_std_20"
  )
  target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<STREQUAL:"${CMAKE_BUILD_TYPE}","Release">:-march=native>
    $<$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">:-g3>
    -Wall
    -Wextra
  )
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    MWCAS_CAPACITY=${MWCAS_CAPACITY}
    MWCAS_VALUE_BIT_NUM=${MWCAS_VALUE_BIT_NUM}
    MWCAS_RETRY_THRESHOLD=${MWCAS_RETRY_THRESHOLD}
    MWCAS_BACKOFF_TIME=${MWCAS_BACKOFF_TIME}
    $<$<BOOL:${MWCAS_HAS_SPINLOCK_HINT}>:MWCAS_HAS_SPINLOCK_HINT>
  )
  target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(${PROJECT_NAME} PUBLIC
    dbgroup::cpp_utility
    dbgroup::memory_manager
  )

  if(DEFINED ENV{CI})
    target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
  endif()

  #----------------------------------------------------------------------------#
  # Build Unit Tests
  #----------------------------------------------------------------------------#

  option(MWCAS_BUILD_TESTS, "Build unit tests for this library" OFF)
  if(${MWCAS_BUILD_TESTS})
    enable_testing()
    # include unit tests
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
  endif()
endif()
