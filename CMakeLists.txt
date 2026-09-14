cmake_minimum_required(VERSION 3.16)
project(matchbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Core matching-engine library -------------------------------------------
# Header-only-ish: logic lives in src/, public headers in include/.
add_library(matchbox
    src/core/Order.cpp
    src/book/PriceLevel.cpp
    src/book/OrderBook.cpp
    src/io/CsvTradeLogger.cpp
    src/io/BookSnapshotWriter.cpp
)
target_include_directories(matchbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(matchbox PRIVATE -Wall -Wextra)

include(FetchContent)

# --- Benchmarks (Google Benchmark via FetchContent) --------------------------
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(matchbox_bench bench/OrderBookBenchmark.cpp)
target_link_libraries(matchbox_bench PRIVATE matchbox benchmark::benchmark)

add_executable(matchbox_throughput bench/ThroughputTest.cpp)
target_link_libraries(matchbox_throughput PRIVATE matchbox)

# --- Networking: TCP order-entry gateway, client, and round-trip benchmark -----
add_executable(matchbox_gateway src/net/Gateway.cpp)
target_link_libraries(matchbox_gateway PRIVATE matchbox)

add_executable(matchbox_client src/net/OrderClient.cpp)
target_link_libraries(matchbox_client PRIVATE matchbox)

add_executable(matchbox_netbench bench/NetBenchmark.cpp)
target_link_libraries(matchbox_netbench PRIVATE matchbox)

# --- Interactive CLI ---------------------------------------------------------
add_executable(matchbox_cli src/cli/CliRepl.cpp)
target_link_libraries(matchbox_cli PRIVATE matchbox)

# --- Tests (GoogleTest via FetchContent) -------------------------------------
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(matchbox_tests
    test/OrderBookTest.cpp
    test/OrderLifecycleTest.cpp
    test/PersistenceTest.cpp
)
target_link_libraries(matchbox_tests PRIVATE matchbox GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(matchbox_tests)
