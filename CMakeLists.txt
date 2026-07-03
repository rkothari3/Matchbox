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
)
target_include_directories(matchbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(matchbox PRIVATE -Wall -Wextra)

# --- Tests (GoogleTest via FetchContent) -------------------------------------
include(FetchContent)
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
)
target_link_libraries(matchbox_tests PRIVATE matchbox GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(matchbox_tests)
