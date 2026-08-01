#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <chrono>
#include <memory>

#include "OrderBook.h"
#include "SPSCQueue.h"
#include "Protocol.h"

using namespace market_handler;

std::vector<PacketPayload> generate_market_burst(size_t count) {
    std::vector<PacketPayload> burst;
    burst.reserve(count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> price_dist(100000, 100500);
    std::uniform_int_distribution<uint32_t> qty_dist(10, 100);
    std::uniform_int_distribution<int> type_dist(1, 100);

    for (size_t i = 1; i <= count; ++i) {
        PacketPayload packet;
        int roll = type_dist(rng);

        if (roll <= 70) {
            packet.header.message_type = 'A';
            packet.add_msg.order_id = i;
            packet.add_msg.price = price_dist(rng);
            packet.add_msg.quantity = qty_dist(rng);
            packet.add_msg.is_buy = (roll % 2 == 0) ? 'B' : 'S';
        } else if (roll <= 85) {
            packet.header.message_type = 'X';
            packet.cancel_msg.order_id = std::max<uint64_t>(1, i - 5);
        } else {
            packet.header.message_type = 'M';
            packet.modify_msg.order_id = std::max<uint64_t>(1, i - 5);
            packet.modify_msg.new_price = price_dist(rng);
            packet.modify_msg.new_quantity = 50;
        }
        burst.push_back(packet);
    }
    return burst;
}

static inline void dispatch(OrderBook& book, const PacketPayload& p) noexcept {
    switch (p.header.message_type) {
        case 'A':
            book.add_order(p.add_msg.order_id, p.add_msg.price,
                           p.add_msg.quantity, p.add_msg.is_buy == 'B');
            break;
        case 'X':
            book.cancel_order(p.cancel_msg.order_id);
            break;
        case 'M':
            book.modify_order(p.modify_msg.order_id, p.modify_msg.new_price,
                              p.modify_msg.new_quantity);
            break;
        default:
            break;
    }
}

static void BM_Worker_Throughput(benchmark::State& state) {
    const size_t burst_size = static_cast<size_t>(state.range(0));
    const auto burst = generate_market_burst(burst_size);

    SPSCQueue<PacketPayload> queue(1 << 17);
    auto book = std::make_unique<OrderBook>();

    for (auto _ : state) {
        for (const auto& p : burst) queue.push(p);

        PacketPayload pkt;
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < burst_size; ++i) {
            while (!queue.pop(pkt)) {}
            dispatch(*book, pkt);
        }
        const auto t1 = std::chrono::steady_clock::now();

        state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
        benchmark::ClobberMemory();

        book = std::make_unique<OrderBook>();   // untimed reset; pool would otherwise exhaust
    }
    state.SetItemsProcessed(state.iterations() * burst_size);
}

BENCHMARK(BM_Worker_Throughput)->Arg(1000)->Arg(10000)->Arg(20000)->UseManualTime();

BENCHMARK_MAIN();
