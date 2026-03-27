#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace market_handler {

    template<size_t MAX_TICKS = 100000>
    class PriceBitSet {
    public:
        inline void set(const uint32_t price) noexcept;
        inline void clear(const uint32_t price) noexcept;

        [[nodiscard]] inline uint32_t get_highest() const noexcept;
        [[nodiscard]] inline uint32_t get_lowest() const noexcept;
 
    private:
        static constexpr size_t L0_SIZE = (MAX_TICKS / 64) + 1;
        static constexpr size_t L1_SIZE = (L0_SIZE / 64) + 1;

        std::array<uint64_t, L0_SIZE> _l0{};
        std::array<uint64_t, L1_SIZE> _l1{};
        uint64_t _l2 = 0;
    };

    template<size_t MAX_TICKS>
    void PriceBitSet<MAX_TICKS>::set(const uint32_t price) noexcept {
        const size_t l0_index = price / 64;
        const size_t l1_index = l0_index / 64;

        _l0[l0_index] |= (1ULL << (price % 64));
        _l1[l1_index] |= (1ULL << (l0_index % 64));
        _l2 |= (1ULL << l1_index);
    }

    template<size_t MAX_TICKS>
    void PriceBitSet<MAX_TICKS>::clear(const uint32_t price) noexcept {
        const size_t l0_index = price / 64;
        const size_t l1_index = l0_index / 64;

        _l0[l0_index] &= ~(1ULL << (price % 64));
        if (_l0[l0_index] == 0) {
            _l1[l1_index] &= ~(1ULL << (l0_index % 64));
            if (_l1[l1_index] == 0) {
                _l2 &= ~(1ULL << l1_index);
            }
        }
    }

    template<size_t MAX_TICKS>
    uint32_t PriceBitSet<MAX_TICKS>::get_highest() const noexcept {
        if (_l2 == 0) return 0;
        const int l2_bit = 63 - std::countl_zero(_l2);
        const int l1_bit = 63 - std::countl_zero(_l1[l2_bit]);
        const size_t l0_idx = (l2_bit * 64) + l1_bit;
        const int l0_bit = 63 - std::countl_zero(_l0[l0_idx]);
        return (l0_idx * 64) + l0_bit;
    }

    template<size_t MAX_TICKS>
    uint32_t PriceBitSet<MAX_TICKS>::get_lowest() const noexcept {
        if (_l2 == 0) return UINT32_MAX;
        const int l2_bit = std::countr_zero(_l2);
        const int l1_bit = std::countr_zero(_l1[l2_bit]);
        const size_t l0_idx = (l2_bit * 64) + l1_bit;
        const int l0_bit = std::countr_zero(_l0[l0_idx]);
        return (l0_idx * 64) + l0_bit;
    }
}
