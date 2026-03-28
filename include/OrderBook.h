#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include "PriceBitSet.h"

namespace market_handler {

    #pragma pack(push, 1)
    struct Order {
        uint64_t id;
        Order *prev = nullptr;
        Order *next = nullptr;
        uint32_t price;

        uint32_t quantity : 31;
        uint32_t is_buy   : 1;
    };
    #pragma pack(pop)

    struct PriceLevel {
        Order *head = nullptr;
        Order *tail = nullptr;
        uint64_t total_quantity = 0;
    };

    class OrderPool {
    public:
        explicit OrderPool(const size_t capacity) {
            _pool.resize(capacity);
            _free_list.reserve(capacity);

            for (size_t i = 0; i < capacity; ++i) {
                _free_list.push_back(&_pool[i]);
            }
        }

        [[nodiscard]] inline Order* allocate() noexcept {
            if (_free_list.empty()) [[unlikely]] {
                return nullptr;
            }
            Order* order = _free_list.back();
            _free_list.pop_back();
            return order;
        }

        inline void deallocate(Order* order) noexcept {
            order->prev = nullptr;
            order->next = nullptr;
            _free_list.push_back(order);
        }

    private:
        std::vector<Order> _pool;
        std::vector<Order*> _free_list;
    };

    class OrderBook {
    public:
        static constexpr size_t MAX_TICKS = 262144;

        explicit OrderBook(const size_t max_orders = 1000000) : _pool(max_orders) {
            _order_map.reserve(max_orders);
            _bids.reserve(MAX_TICKS);
            _asks.reserve(MAX_TICKS);
        }

        void add_order(const uint64_t id, const uint32_t price, const uint32_t qty, const bool is_buy) noexcept;
        void cancel_order(const uint64_t id) noexcept;
        void modify_order(const uint64_t id, const uint32_t new_price, const uint32_t new_qty) noexcept;
        [[nodiscard]] uint32_t match_order(const uint32_t price, const uint32_t qty, const bool is_buy);

        [[nodiscard]] inline uint32_t get_best_bid() const noexcept { return _best_bid; }
        [[nodiscard]] inline uint32_t get_best_ask() const noexcept { return _best_ask; }

    private:
        OrderPool _pool;

        std::vector<PriceLevel> _bids;
        std::vector<PriceLevel> _asks;
        std::unordered_map<uint64_t, Order*> _order_map;

        PriceBitSet<MAX_TICKS> _bid_bits;
        PriceBitSet<MAX_TICKS> _ask_bits;

        uint32_t _best_bid = 0;
        uint32_t _best_ask = UINT32_MAX;
    };

    void OrderBook::add_order(const uint64_t id, const uint32_t price, const uint32_t qty, const bool is_buy) noexcept {
        Order *order = _pool.allocate();
        if (!order) [[unlikely]] {
            return;
        }

        int remaining_qty = match_order(price, qty, is_buy);
        if (remaining_qty == 0) {
            _pool.deallocate(order);
            return;
        }

        order->id = id;
        order->price = price;
        order->quantity = remaining_qty;
        order->is_buy = is_buy;

        _order_map[id] = order;
        PriceLevel &level = is_buy ? _bids[price] : _asks[price];

        if (level.total_quantity == 0) {
            if (is_buy) _bid_bits.set(price);
            else _ask_bits.set(price);
        }

        level.total_quantity += remaining_qty;

        if (!level.head) {
            level.head = level.tail = order;
        } else {
            order->prev = level.tail;
            level.tail->next = order;
            level.tail = order;
        }

        _best_bid = _bid_bits.get_highest();
        _best_ask = _ask_bits.get_lowest();
    }

    void OrderBook::cancel_order(const uint64_t id) noexcept {
        auto it = _order_map.find(id);
        if (it == _order_map.end()) [[unlikely]] {
            return;
        }

        Order *order = it->second;
        PriceLevel &level = order->is_buy ? _bids[order->price] : _asks[order->price];
        level.total_quantity -= order->quantity;

        if (order->prev) order->prev->next = order->next;
        else level.head = order->next;

        if (order->next) order->next->prev = order->prev;
        else level.tail = order->prev;

        if (level.total_quantity == 0) {
            if (order->is_buy) _bid_bits.clear(order->price);
            else _ask_bits.clear(order->price);

            _best_bid = _bid_bits.get_highest();
            _best_ask = _ask_bits.get_lowest();
        }

        _order_map.erase(it);
        _pool.deallocate(order);
    }

    void OrderBook::modify_order(const uint64_t id, const uint32_t new_price, const uint32_t new_qty) noexcept {
        auto it = _order_map.find(id);
        if (it == _order_map.end()) [[unlikely]] {
            return;
        }

        Order *order = it->second;

        if (new_price != order->price || new_qty >= order->quantity) {
            const bool is_buy = order->is_buy;
            cancel_order(id);
            add_order(id, new_price, new_qty, is_buy);
            return;    
        }

        const uint32_t qty_reduction = order->quantity - new_qty;
        if (new_qty == 0) {
            cancel_order(id);
            return;
        }

        order->quantity = new_qty;

        PriceLevel &level = order->is_buy ? _bids[order->price] : _asks[order->price];
        level.total_quantity -= (qty_reduction);
    }

    uint32_t OrderBook::match_order(const uint32_t price, const uint32_t qty, const bool is_buy) {
        uint32_t remaining_qty = qty;

        while (remaining_qty > 0) {
            bool is_match = is_buy ? (price >= _best_ask) : (price <= _best_bid);
            if (!is_match) {
                break;
            }

            uint32_t match_price = is_buy ? _best_ask : _best_bid;
            PriceLevel &level = is_buy ? _asks[match_price] : _bids[match_price];

            Order *resting_order = level.head;
            while (resting_order && remaining_qty > 0) {
                uint32_t trade_qty = std::min(static_cast<uint32_t>(resting_order->quantity), remaining_qty);
                
                remaining_qty -= trade_qty;
                resting_order->quantity -= trade_qty;
                level.total_quantity -= trade_qty;

                if (resting_order->quantity == 0) {
                    Order *next_order = resting_order->next;
                    
                    if (resting_order->prev) resting_order->prev->next = resting_order->next;
                    else level.head = resting_order->next;

                    if (resting_order->next) resting_order->next->prev = resting_order->prev;
                    else level.tail = resting_order->prev;

                    _order_map.erase(resting_order->id);
                    _pool.deallocate(resting_order);

                    resting_order = next_order;
                }
            }

            if (level.total_quantity == 0) {
                if (is_buy) _ask_bits.clear(match_price);
                else _bid_bits.clear(match_price);

                _best_bid = _bid_bits.get_highest();
                _best_ask = _ask_bits.get_lowest();
            }
        }

        return remaining_qty;
    }

}
