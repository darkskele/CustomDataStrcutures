set(MCDS_TEST_SOURCES
    test_b_tree.cpp
    test_flat_map.cpp
    test_ring_buffer.cpp
    test_spsc_ring_buffer.cpp
    test_slab.cpp
    test_memory_pool.cpp
)

add_executable(mcds_tests
    ${MCDS_TEST_SOURCES}
)

target_link_libraries(mcds_tests
    PRIVATE
        mcds 
        GTest::gtest_main 
)

target_include_directories(mcds_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} 
)

include(GoogleTest)
gtest_discover_tests(mcds_tests)
