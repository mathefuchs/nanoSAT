project(nanosat-tests VERSION 0.1.0)

# Unit tests
enable_testing()
include_directories(../src)
add_executable(nanosat-test
  main.cpp
  nanosat_parse_test.cpp
  nanosat_sat_test.cpp
)
target_link_libraries(nanosat-test PUBLIC gtest_main)
add_test(nanosat-test nanosat_test_suite)
