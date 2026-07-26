add_executable(WordListTest
  WordListTest.cpp
)

target_include_directories(WordListTest PRIVATE
  ${REPO_ROOT}/lib/WordList
)

target_link_libraries(WordListTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WordListTest)
