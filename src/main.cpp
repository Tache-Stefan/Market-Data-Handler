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

static double estimate_ns_per_cycle(int sample_ms = 200) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    uint64_t tsc_start = __rdtsc();
    auto t_start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(sample_ms));
    auto t_end = std::chrono::high_resolution_clock::now();
    uint64_t tsc_end = __rdtsc();

    std::chrono::duration<double> elapsed = t_end - t_start;
    double seconds = elapsed.count();
    if (seconds <= 0.0) {
        return 0.313; // fallback to hardcoded value
    }

    double cycles = double(tsc_end - tsc_start);
    double hz = cycles / seconds;
    return 1e9 / hz; // ns per cycle
}

void run_matching_engine(SPSCQueue<PacketPayload> &queue, OrderBook &book) {
    pin_current_thread_to_core(4);

    std::cout << "[Worker Thread] Matching engine started. Polling queue...\n";

    PacketPayload packet;
    uint64_t messages_processed = 0;
    static constexpr size_t MAX_SAMPLES = 1'000'000;
    std::vector<uint64_t> cycle_deltas(MAX_SAMPLES);
    size_t sample_count = 0;
    uint64_t idle_polls = 0;

    while (running.load(std::memory_order_acquire)) {
        if (queue.pop(packet)) {
            idle_polls = 0;
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
            if (sample_count < MAX_SAMPLES) [[likely]] {
                cycle_deltas[sample_count++] = egress_tsc - packet.ingress_tsc;
            }

            ++messages_processed;
        } else {
           if (++idle_polls > 64) _mm_pause();
        }
    }

    std::cout << "[Worker Thread] Matching engine stopped. Total messages processed: " << messages_processed << "\n";

    if (sample_count > 0) {
        const auto begin = cycle_deltas.begin();
        const auto end = begin + sample_count;
        std::sort(begin, end);
        
        uint64_t median_cycles = cycle_deltas[sample_count / 2];
        uint64_t p99_cycles = cycle_deltas[static_cast<size_t>(sample_count * 0.99)];
        uint64_t max_cycles = cycle_deltas[sample_count - 1];

        double ns_per_cycle = estimate_ns_per_cycle();
        double measured_mhz = (1e9 / ns_per_cycle) / 1e6;
        std::cout << "(measured CPU freq: " << measured_mhz << " MHz)\n";

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
