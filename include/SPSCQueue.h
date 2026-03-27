#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
   
template<typename T, typename Alloc = std::allocator<T>>
class SPSCQueue {
public:
    using allocator_traits = std::allocator_traits<Alloc>;
    using size_type = typename allocator_traits::size_type;

    explicit SPSCQueue(size_type capacity, Alloc const& alloc = Alloc{})
        : _alloc{alloc},
            _capacity(capacity),
            _index_mask(capacity > 0 ? (capacity - 1) : 0),
            _ring(capacity ? allocator_traits::allocate(_alloc, capacity) : nullptr) {
            assert(std::has_single_bit(capacity) && "capacity must be a power of two");
            }

    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;
    
    ~SPSCQueue() {
        while(!empty()) {
            const size_type idx = _pop_cursor.load(std::memory_order_acquire) & _index_mask;
            allocator_traits::destroy(_alloc, _ring + idx);
            _pop_cursor.store(_pop_cursor.load(std::memory_order_acquire) + 1, std::memory_order_release);
        }
        if (_ring) {
            allocator_traits::deallocate(_alloc, _ring, _capacity);
        }
    }

    auto size() const noexcept {
        auto push_cursor = _push_cursor.load(std::memory_order_acquire);
        auto pop_cursor = _pop_cursor.load(std::memory_order_acquire);

        assert(pop_cursor <= push_cursor);
        return push_cursor - pop_cursor;
    }
    auto empty() const noexcept { return size() == 0; }
    auto full() const noexcept { return size() == capacity(); }
    auto capacity() const noexcept { return _capacity; }

    bool push(const T& value);
    bool pop(T& value);

private:
    auto full(size_type push_cursor, size_type pop_cursor) const noexcept {
        return (push_cursor - pop_cursor) >= _capacity;
    }

    auto empty(size_type push_cursor, size_type pop_cursor) const noexcept {
        return push_cursor == pop_cursor;
    }

    Alloc _alloc;
    size_type _capacity;
    size_type _index_mask;
    T* _ring;

    using cursor_type = std::atomic<size_type>;
    static_assert(cursor_type::is_always_lock_free, "cursor_type must be lock-free");

    static constexpr auto hardware_destructive_interference_size = size_type{64};
    static_assert(sizeof(cursor_type) <= hardware_destructive_interference_size, "cursor_type must fit within a cache line");

    alignas(hardware_destructive_interference_size) cursor_type _push_cursor{0};
    alignas(hardware_destructive_interference_size) cursor_type _pop_cursor{0};

    // Exclusive to the pop thread
    alignas(hardware_destructive_interference_size) size_type _push_cursor_cached = 0;

    // Exclusive to the push thread
    alignas(hardware_destructive_interference_size) size_type _pop_cursor_cached = 0;

    char _padding[hardware_destructive_interference_size - sizeof(cursor_type)];
};

template<typename T, typename Alloc>
bool SPSCQueue<T, Alloc>::push(const T& value) {
    if (_capacity == 0) {
        return false;
    }

    auto push_cursor = _push_cursor.load(std::memory_order_relaxed);
    if (full(push_cursor, _pop_cursor_cached)) {
        _pop_cursor_cached = _pop_cursor.load(std::memory_order_acquire);
        if (full(push_cursor, _pop_cursor_cached)) {
            return false;
        }
    }

    const size_type idx = push_cursor & _index_mask;
    allocator_traits::construct(_alloc, _ring + idx, value);
    _push_cursor.store(push_cursor + 1, std::memory_order_release);
    return true;
}

template<typename T, typename Alloc>
bool SPSCQueue<T, Alloc>::pop(T& value) {
    auto pop_cursor = _pop_cursor.load(std::memory_order_relaxed);
    if (empty(_push_cursor_cached, pop_cursor)) {
        _push_cursor_cached = _push_cursor.load(std::memory_order_acquire);
        if (empty(_push_cursor_cached, pop_cursor)) {
            return false;
        }
    }

    const size_type idx = pop_cursor & _index_mask;
    value = std::move(_ring[idx]);
    allocator_traits::destroy(_alloc, _ring + idx);
    _pop_cursor.store(pop_cursor + 1, std::memory_order_release);
    return true;
}
