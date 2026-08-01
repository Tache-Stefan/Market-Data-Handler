cmake_minimum_required(VERSION 3.20)
project(Market-Handler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MH_NATIVE_ARCH "Build with -march=native" ON)
option(MH_SANITIZE   "Build with ASan + UBSan"   OFF)

add_library(mh_flags INTERFACE)
target_include_directories(mh_flags INTERFACE ${CMAKE_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(mh_flags INTERFACE /W4)
else()
    target_compile_options(mh_flags INTERFACE -Wall -Wextra -fno-omit-frame-pointer)
    if(MH_NATIVE_ARCH)
        target_compile_options(mh_flags INTERFACE -march=native)
    endif()
    if(MH_SANITIZE)
        target_compile_options(mh_flags INTERFACE -fsanitize=address,undefined -g)
        target_link_options(mh_flags    INTERFACE -fsanitize=address,undefined)
    endif()
endif()

add_executable(Market-Handler src/main.cpp)

target_compile_definitions(Market-Handler PRIVATE ASIO_STANDALONE)

if(WIN32)
    target_link_libraries(Market-Handler PRIVATE ws2_32)
endif()

include(FetchContent)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.5
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(bench_orderbook benchmarks/bench_orderbook.cpp)
add_executable(bench_worker benchmarks/bench_worker.cpp)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) 
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_orderbook tests/test_orderbook.cpp)

target_link_libraries(Market-Handler  PRIVATE mh_flags)
target_link_libraries(bench_orderbook PRIVATE mh_flags benchmark::benchmark benchmark::benchmark_main)
target_link_libraries(bench_worker    PRIVATE mh_flags benchmark::benchmark benchmark::benchmark_main)
target_link_libraries(test_orderbook  PRIVATE mh_flags gtest_main)

add_test(NAME OrderBookTests COMMAND test_orderbook)