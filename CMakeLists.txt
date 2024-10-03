cmake_minimum_required(VERSION "3.11")

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

  # use our utility
  FetchContent_Declare(
    cpp-utility
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/cpp-utility.git"
    GIT_TAG "14008188d31fd77c99efec12c368b0aa874c1c2b"
  )
  FetchContent_MakeAvailable(cpp-utility)

  # use our GC
  FetchContent_Declare(
    memory-manager
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/memory-manager.git"
    GIT_TAG "89b58245bf22bb7a6ce05f9e108a19961a7f1f79"
  )
  FetchContent_MakeAvailable(memory-manager)

  #----------------------------------------------------------------------------#
  # Build targets
  #----------------------------------------------------------------------------#

  add_library(${PROJECT_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/deadlock_free/mwcas_descriptor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lock_free/aopt_descriptor.cpp"
  )
  add_library(dbgroup::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
  target_compile_features(${PROJECT_NAME} PUBLIC
    "cxx_std_20"
  )
  target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    MWCAS_CAPACITY=${MWCAS_CAPACITY}
    MWCAS_RETRY_THRESHOLD=${MWCAS_RETRY_THRESHOLD}
    MWCAS_BACKOFF_TIME=${MWCAS_BACKOFF_TIME}
    $<$<BOOL:${MWCAS_HAS_SPINLOCK_HINT}>:MWCAS_HAS_SPINLOCK_HINT>
  )
  target_link_libraries(${PROJECT_NAME} PUBLIC
    dbgroup::cpp_utility
    dbgroup::memory_manager
  )

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
