set(MCDS_BENCH_SOURCES
    # bench_ring_buffer.cpp
)

add_executable(mcds_benchmarks
    ${MCDS_BENCH_SOURCES}
)

target_link_libraries(mcds_benchmarks
    PRIVATE
        mcds  
        benchmark::benchmark 
)
