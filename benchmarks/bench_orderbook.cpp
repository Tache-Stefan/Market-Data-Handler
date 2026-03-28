#include <benchmark/benchmark.h>
#include "OrderBook.h"

using namespace market_handler;

static void BM_Modify_InPlace(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;

    for (auto _ : state) {
        book.add_order(id, 100000, 100, true);
        
        book.modify_order(id, 100000, 50); 
        
        book.cancel_order(id); 
        
        id++;
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_Modify_InPlace);

static void BM_Modify_CancelReplace(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;

    for (auto _ : state) {
        book.add_order(id, 100000, 100, true);
        
        book.modify_order(id, 100005, 100); 
        
        book.cancel_order(id);
        
        id++;
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_Modify_CancelReplace);

static void BM_Match_AggressiveOrder(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;

    for (auto _ : state) {
        book.add_order(id++, 100500, 100, false); 
        
        book.add_order(id++, 100500, 100, true); 
        
        benchmark::DoNotOptimize(id);
    }
}
BENCHMARK(BM_Match_AggressiveOrder);

BENCHMARK_MAIN();
