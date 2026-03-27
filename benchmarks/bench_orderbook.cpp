#include <benchmark/benchmark.h>
#include "OrderBook.h"
#include <random>

using namespace market_handler;

static void BM_OrderBook_AddSamePrice(benchmark::State& state) {
    OrderBook book(state.range(0));
    uint64_t id = 1;

    for (auto _ : state) {
        book.add_order(id++, 100500, 100, true);
        
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_OrderBook_AddSamePrice)->Arg(1000000); 

static void BM_OrderBook_AddAndCancel(benchmark::State& state) {
    OrderBook book(state.range(0));
    uint64_t id = 1;

    for (auto _ : state) {
        book.add_order(id, 100500, 100, true);
        book.cancel_order(id);
        
        id++;
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_OrderBook_AddAndCancel)->Arg(1000000);

static void BM_OrderBook_AddRandomPrice(benchmark::State& state) {
    OrderBook book(state.range(0));
    uint64_t id = 1;

    std::mt19937 rng(42); 
    std::uniform_int_distribution<uint32_t> dist(100000, 101000);

    for (auto _ : state) {
        uint32_t random_price = dist(rng);
        book.add_order(id++, random_price, 100, true);
        
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_OrderBook_AddRandomPrice)->Arg(1000000);

BENCHMARK_MAIN();
