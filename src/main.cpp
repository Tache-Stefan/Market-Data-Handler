#include <iostream>
#include <thread>
#include <atomic>
#include <asio.hpp>
#include "Protocol.h"
#include "FeedHandler.h"
#include "OrderBook.h"
#include "SPSCQueue.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

using namespace market_handler;

std::atomic<bool> running{true};

void run_matching_engine(SPSCQueue<PacketPayload> &queue, OrderBook &book) {
    std::cout << "[Worker Thread] Matching engine started. Polling queue...\n";

    PacketPayload packet;
    uint64_t messages_processed = 0;

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
            ++messages_processed;
        } else {
            _mm_pause();
        }
    }

    std::cout << "[Worker Thread] Matching engine stopped. Total messages processed: " << messages_processed << "\n";
}

int main() {
    try {
        SPSCQueue<PacketPayload> queue(1048576);
        OrderBook book;

        std::thread worker_thread(run_matching_engine, std::ref(queue), std::ref(book));

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
