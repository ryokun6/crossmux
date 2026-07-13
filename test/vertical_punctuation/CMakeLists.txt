add_executable(VerticalPunctuationTest
  VerticalPunctuationTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(VerticalPunctuationTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(VerticalPunctuationTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(VerticalPunctuationTest)
