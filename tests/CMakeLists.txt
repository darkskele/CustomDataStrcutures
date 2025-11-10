set(MCDS_TEST_SOURCES
    test_circular_buffer.cpp
    test_revolving_recency_buffer.cpp
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
