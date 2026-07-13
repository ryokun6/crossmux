add_executable(CjkKinsokuTest
  CjkKinsokuTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(CjkKinsokuTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(CjkKinsokuTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CjkKinsokuTest)

add_executable(CjkKinsokuJaTest
  CjkKinsokuTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(CjkKinsokuJaTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/EpdFont
)

target_compile_definitions(CjkKinsokuJaTest PRIVATE ENABLE_JAPANESE_VERSION ENABLE_CJK_VERSION)

target_link_libraries(CjkKinsokuJaTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CjkKinsokuJaTest)
