add_executable(Utf8ComposeTest
  Utf8ComposeTest.cpp
<<<<<<< HEAD
  Utf8CjkWhitespaceTest.cpp
=======
>>>>>>> upstream/master
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(Utf8ComposeTest PRIVATE
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(Utf8ComposeTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(Utf8ComposeTest)
