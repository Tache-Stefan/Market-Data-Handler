cmake_minimum_required(VERSION 3.20)
project(Market-Handler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /Ox /Ot /Oi)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native)
endif()

add_executable(Market-Handler src/main.cpp)

target_compile_definitions(Market-Handler PRIVATE ASIO_STANDALONE)
target_include_directories(Market-Handler PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.5
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(bench_orderbook benchmarks/bench_orderbook.cpp)
target_include_directories(bench_orderbook PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_orderbook PRIVATE benchmark::benchmark benchmark::benchmark_main)