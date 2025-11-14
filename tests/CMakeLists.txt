set(MCDS_TEST_SOURCES
    test_b_tree.cpp
)

add_executable(mcds_tests
    ${MCDS_TEST_SOURCES}
)

target_link_libraries(mcds_tests
    PRIVATE
        mcds 
        GTest::gtest_main 
)

include(GoogleTest)
gtest_discover_tests(mcds_tests)
