# define function to add unit tests in the same format
function(ADD_MWCAS_TEST MWCAS_TEST_TARGET)
  add_executable(${MWCAS_TEST_TARGET}
    "${CMAKE_CURRENT_SOURCE_DIR}/${MWCAS_TEST_TARGET}.cpp"
  )
  target_compile_features(${MWCAS_TEST_TARGET} PRIVATE "cxx_std_17")
  target_compile_options(${MWCAS_TEST_TARGET} PRIVATE
    -Wall
    -Wextra
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  target_include_directories(${MWCAS_TEST_TARGET} PRIVATE
    "${MWCAS_SOURCE_DIR}/src"
    "${MWCAS_SOURCE_DIR}/include"
    "${GOOGLETEST_SOURCE_DIR}/include"
  )
  target_link_libraries(${MWCAS_TEST_TARGET} PRIVATE
    pthread
    gtest_main
  )
  add_dependencies(${MWCAS_TEST_TARGET}
    gtest_main
  )
  add_test(NAME ${MWCAS_TEST_TARGET} COMMAND $<TARGET_FILE:${MWCAS_TEST_TARGET}>)
endfunction()

# add unit tests to build targets
ADD_MWCAS_TEST("rdcss_target_word_test")
