#include <benchmark/benchmark.h>
#include <vector>
#include <random>

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

static void BM_Worker_Throughput(benchmark::State& state) {
    const size_t burst_size = state.range(0);
    
    SPSCQueue<PacketPayload> queue(65536); 
    
    auto burst = generate_market_burst(burst_size);

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book;
        
        for (const auto& packet : burst) {
            queue.push(packet);
        }
        state.ResumeTiming();

        PacketPayload current_packet;
        size_t processed = 0;
        
        while (processed < burst_size) {
            if (queue.pop(current_packet)) {
                switch (current_packet.header.message_type) {
                    case 'A':
                        book.add_order(current_packet.add_msg.order_id, 
                                       current_packet.add_msg.price, 
                                       current_packet.add_msg.quantity, 
                                       current_packet.add_msg.is_buy);
                        break;
                    case 'X':
                        book.cancel_order(current_packet.cancel_msg.order_id);
                        break;
                    case 'M':
                        book.modify_order(current_packet.modify_msg.order_id,
                                          current_packet.modify_msg.new_price,
                                          current_packet.modify_msg.new_quantity);
                        break;
                }
                processed++;
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * burst_size);
}

BENCHMARK(BM_Worker_Throughput)->Arg(1000)->Arg(10000)->Arg(20000);

BENCHMARK_MAIN();
