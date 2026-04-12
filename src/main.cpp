#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <asio.hpp>
#include "Protocol.h"
#include "FeedHandler.h"
#include "OrderBook.h"
#include "SPSCQueue.h"
#include "ThreadUtils.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <cstring>
#endif

using namespace market_handler;

std::atomic<bool> running{true};

void run_matching_engine(SPSCQueue<PacketPayload> &queue, OrderBook &book) {
    pin_current_thread_to_core(3);

    std::cout << "[Worker Thread] Matching engine started. Polling queue...\n";

    PacketPayload packet;
    uint64_t messages_processed = 0;
    std::vector<uint64_t> cycle_deltas;
    cycle_deltas.reserve(1000000);

    while (running.load(std::memory_order_acquire)) {
        if (queue.pop(packet)) {
            switch (packet.header.message_type) {
                case 'A':
                    book.add_order(packet.add_msg.order_id, 
                                   packet.add_msg.price, 
                                   packet.add_msg.quantity, 
                                   packet.add_msg.is_buy == 'B');
                    break;
                case 'X':
                    book.cancel_order(packet.cancel_msg.order_id);
                    break;
                case 'M':
                    book.modify_order(packet.modify_msg.order_id, 
                                      packet.modify_msg.new_price, 
                                      packet.modify_msg.new_quantity);
                    break;
                default:
                    break;
            }

            uint64_t egress_tsc = __rdtsc();
            cycle_deltas.push_back(egress_tsc - packet.ingress_tsc);

            ++messages_processed;
        } else {
            _mm_pause();
        }
    }

    std::cout << "[Worker Thread] Matching engine stopped. Total messages processed: " << messages_processed << "\n";

    if (!cycle_deltas.empty()) {
        std::sort(cycle_deltas.begin(), cycle_deltas.end());
        
        uint64_t median_cycles = cycle_deltas[cycle_deltas.size() / 2];
        uint64_t p99_cycles = cycle_deltas[cycle_deltas.size() * 0.99];
        uint64_t max_cycles = cycle_deltas.back();

        // (3187 MHz CPU)
        const double ns_per_cycle = 0.313;

        std::cout << "\n=== LATENCY REPORT ===\n";
        std::cout << "Median Latency: " << (median_cycles * ns_per_cycle) << " ns (" << median_cycles << " cycles)\n";
        std::cout << "99th Percentile: " << (p99_cycles * ns_per_cycle) << " ns (" << p99_cycles << " cycles)\n";
        std::cout << "Max Latency:    " << (max_cycles * ns_per_cycle) << " ns (" << max_cycles << " cycles)\n";
        std::cout << "====================================\n";
    }
}

int main() {
    try {
#ifdef _WIN32
        SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#else
        struct sched_param sp;
        sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
            std::cerr << "[Warning] Failed to set real-time priority: " 
                      << std::strerror(errno) 
                      << " (Are you running as root/sudo?)\n";
        }
#endif

        SPSCQueue<PacketPayload> queue(1048576);
        OrderBook book;

        std::thread worker_thread(run_matching_engine, std::ref(queue), std::ref(book));

        pin_current_thread_to_core(2);

        asio::io_context io_context;
        FeedHandler feed_handler(io_context, 12345, queue);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            std::cout << "\n[Main Thread] Signal received, shutting down...\n";
            running.store(false, std::memory_order_release);
            io_context.stop();
        });

        io_context.run();

        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
