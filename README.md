# Market-Data-Handler

![C++20](https://img.shields.io/badge/C++-20-blue.svg)
![Architecture](https://img.shields.io/badge/Architecture-Lock--Free-orange.svg)
![Latency](https://img.shields.io/badge/Median_Latency-135ns-success.svg)
![Throughput](https://img.shields.io/badge/Throughput-46M_msg%2Fs-success.svg)

This Market Data Handler is a zero-allocation HFT Limit Order Book (LOB) and UDP Feed Handler written in C++20. It uses a two-thread architecture: a dedicated network ingress thread, and a strictly **single-threaded** matching engine that owns all book state, so mutation requires no locks and no atomics. The two threads communicate over a custom lock-free SPSC ring buffer.

Engineered for ultra-low latency, the system bypasses standard C++ library overhead in favor of hardware-aware data structures, cache-line-aligned packets, and OS-level thread affinity. The matching engine sustains **46 million messages per second** (~21.5 ns, ~68 CPU cycles per message), with a **135 ns median internal latency** measured from Asio completion-handler entry to book update.

## Core Architecture & Optimizations

This engine achieves sub-microsecond execution times by aggressively optimizing for the CPU pipeline and memory hierarchy:

* **Single-Copy UDP Ingress:** Zero-allocation, single-copy ingress: the kernel writes directly into a hardware-aligned packet struct, which is then moved into the ring by value. True zero-copy needs a claim/commit queue API or kernel bypass.
* **Lock-Free SPSC Queue:** Cross-thread communication utilizes a custom Single-Producer/Single-Consumer ring buffer. It uses hardware destructive interference size (`alignas(64)`) to strictly prevent false sharing across CPU cores.
* **$O(1)$ Hardware Bit-Scanning:** BBO lookup is O(1): a 3-level hierarchical bitmap resolved by three dependent loads plus three lzcnt/tzcnt — roughly a dozen cycles, and constant regardless of book depth.
* **L1 Cache-Line Packing:** The `Order` struct is strictly 32 bytes and declared alignas(32), guaranteeing exactly two complete orders fit inside a single 64-byte L1 Cache Line without straddling boundaries.
* **Hash-Free Direct Memory Access:** `std::unordered_map` was replaced with a pre-allocated flat `std::vector` array. Order lookup is a single indexed load — no hashing, no pointer chasing, no allocation, and no rehash pauses. The 12 MB table still spans thousands of 4 KB pages, so the next step is huge pages to actually fold it into the TLB.
* **OS CPU Pinning:** Threads are permanently locked to distinct physical CPU cores.

## Performance Metrics

Measured on an Intel i5-11320H (Tiger Lake, 4C/8T) at ~3.22 GHz, Ubuntu on WSL2.
Built with `-O3 -march=native -DNDEBUG`. Network thread pinned to CPU 2, matching engine
to CPU 4 — distinct physical cores.

### Matching Engine Throughput

Google Benchmark, `UseManualTime`, 11 repetitions, medians reported. The queue is pre-filled and drained single-threaded, isolating matching-engine cost from the network path.

| Burst Size | Time / message | Throughput |
| :--- | :--- | :--- |
| 1,000 | 21.4 ns | 46.6 M msg/s |
| 10,000 | 21.8 ns | 46.0 M msg/s |
| **20,000** | **21.3 ns** | **47.0 M msg/s** |

### Order Book Micro-Benchmarks

| Operation | Median |
| :--- | :--- |
| Modify, quantity decrease (in place — keeps queue priority) | 7.60 ns |
| Modify, price change (cancel/replace — loses queue priority) | 12.7 ns |
| Aggressive order, full cross and fill | 7.84 ns |

A price-changing modify costs **1.67x** an in-place quantity reduction: the measured price of surrendering time priority.

### End-to-End Latency (RDTSC)

1,000,000 UDP messages, timestamped with `__rdtsc()` on entry to the Asio completion handler and again after the book update.

| Percentile | Cycles | Nanoseconds |
| :--- | :--- | :--- |
| **Median** | 435 | **134.8 ns** |
| 99th Percentile | 9,081 | 2,815 ns |
| Max | 1,800,837 | 558 us |

## Build instructions

### Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

### Tests

```bash
ctest --test-dir build --output-on-failure
```

Sanitizer build (ASan + UBSan):

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DMH_SANITIZE=ON
cmake --build build-asan -j"$(nproc)"
./build-asan/test_orderbook
```

### Running the Engine

```bash
# UDP socket buffer: the engine requests 8 MB, but Linux clamps to net.core.rmem_max
sudo sysctl -w net.core.rmem_max=16777216

# Real-time scheduling without running as root
sudo setcap cap_sys_nice+ep ./build/Market-Handler

# script(1) gives a PTY so diagnostics are line-buffered and captured
script -q -c './build/Market-Handler' latency.txt
```

In a second terminal, blast 1,000,000 binary UDP packets, then Ctrl+C the engine for the report:

```bash
python3 scripts/simulator.py
grep '^Udp:' /proc/net/snmp      # RcvbufErrors must be unchanged
```

### Benchmarks

```bash
taskset -c 4 ./build/bench_worker \
  --benchmark_min_time=100x --benchmark_repetitions=11 \
  --benchmark_report_aggregates_only=true

taskset -c 4 ./build/bench_orderbook \
  --benchmark_min_time=200000x --benchmark_repetitions=11 \
  --benchmark_report_aggregates_only=true
```
