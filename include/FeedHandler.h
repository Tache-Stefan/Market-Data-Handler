#pragma once

#include <asio.hpp>
#include <iostream>
#include "Protocol.h"
#include "OrderBook.h"
#include "SPSCQueue.h"

namespace market_handler {

    class FeedHandler {
    public:
        FeedHandler(asio::io_context& io_context, short port, SPSCQueue<PacketPayload> &queue)
            : _socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)), 
              _queue(queue) {
            asio::socket_base::receive_buffer_size option(8 * 1024 * 1024);
            _socket.set_option(option);

            std::cout << "[Network Thread] Listening on UDP port " << port << "...\n";
            start_receive();
        }

        uint64_t dropped_packets = 0;

    private:
        asio::ip::udp::socket _socket;
        asio::ip::udp::endpoint _remote_endpoint;

        PacketPayload _recv_buffer;
        SPSCQueue<PacketPayload> &_queue;

        void start_receive();
    };

    void FeedHandler::start_receive() {
        _socket.async_receive_from(
            asio::buffer(&_recv_buffer, sizeof(PacketPayload)),
            _remote_endpoint,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd == sizeof(PacketPayload)) {
                    if (!_queue.push(_recv_buffer)) [[unlikely]] {
                        ++dropped_packets;
                    }
                } else if (ec) {
                    std::cerr << "Receive error: " << ec.message() << "\n";
                }

                start_receive();
            }
        );
    }

}
