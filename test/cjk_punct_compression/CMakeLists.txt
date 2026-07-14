add_executable(CjkPunctCompressionTest
  CjkPunctCompressionTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(CjkPunctCompressionTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(CjkPunctCompressionTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CjkPunctCompressionTest)

add_executable(CjkPunctCompressionScTest
  CjkPunctCompressionTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(CjkPunctCompressionScTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/EpdFont
)

target_compile_definitions(CjkPunctCompressionScTest PRIVATE
  ENABLE_CHINESE_VERSION
  CHINESE_UI_SIMPLIFIED
  ENABLE_CJK_VERSION
)

target_link_libraries(CjkPunctCompressionScTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CjkPunctCompressionScTest)
