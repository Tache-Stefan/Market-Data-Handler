# Market-Data-Handler

![C++20](https://img.shields.io/badge/C++-20-blue.svg)
![Architecture](https://img.shields.io/badge/Architecture-Lock--Free-orange.svg)
![Latency](https://img.shields.io/badge/Median_Latency-<100ns-success.svg)
![Throughput](https://img.shields.io/badge/Throughput-17M_msg%2Fs-success.svg)

This Market Data Handler is a single-threaded, zero-allocation HFT Limit Order Book (LOB) and UDP Feed Handler written in C++20. It features a concurrent, two-thread architecture: a dedicated network ingress thread, and a strictly **single-threaded**, lock-free matching engine to guarantee zero-contention state mutation.

Engineered strictly for ultra-low latency, the system bypasses standard C++ library overhead in favor of hardware-aware data structures, L1 cache-line packing, and OS-level thread affinity. It is capable of processing continuous UDP micro-bursts at over **17 million messages per second** with a median tick-to-trade latency of **~98 nanoseconds**.

## Core Architecture & Optimizations

This engine achieves sub-microsecond execution times by aggressively optimizing for the CPU pipeline and memory hierarchy:

* **Zero-Copy UDP Ingress:** Network payloads are parsed directly from the OS socket buffer into hardware-aligned structures.
* **Lock-Free SPSC Queue:** Cross-thread communication utilizes a custom Single-Producer/Single-Consumer ring buffer. It uses hardware destructive interference size (`alignas(64)`) to strictly prevent false sharing across CPU cores.
* **$O(1)$ Hardware Bit-Scanning:** The OrderBook maintains a 64-bit integer bitmask of active price levels. Finding the Best Bid and Offer (BBO) uses bit operations to find the highest/lowest set bit in a single clock cycle.
* **L1 Cache-Line Packing:** The `Order` struct is strictly packed to exactly 32 bytes using bitfields, guaranteeing exactly two complete orders fit inside a single 64-byte L1 Cache Line without straddling boundaries.
* **Hash-Free Direct Memory Access:** `std::unordered_map` was replaced with a pre-allocated flat `std::vector` array. Order lookups execute in a single clock cycle via absolute pointer arithmetic, completely eliminating hash collisions and TLB cache misses.
* **OS CPU Pinning:** Threads are permanently locked to isolated physical CPU cores.

## Performance Metrics

Performance is tracked using Google Benchmark and hardware `__rdtsc()` (Read Time-Stamp Counter) instructions to measure true end-to-end user-space latency. 

*Tested on an Intel i5-11320H CPU @ 3.18GHz*

### Latency (RDTSC)
Measures the time from the exact nanosecond the Asio socket receives the UDP packet, across the lock-free queue thread boundary, through the OrderBook spread execution, to the final match.

| Percentile | Cycles | Nanoseconds |
| :--- | :--- | :--- |
| **Median** | 313 | **97.9 ns** |
| **99th Percentile** | 2,647 | **828.5 ns** |
| **Max (OS Jitter)** | 291,301 | **91.1 µs** |

### Matching Engine Throughput (Micro-Burst)
Measures the raw execution speed of the Worker Thread processing a pre-generated burst of random limit orders, cancels, and matches.

| Burst Size | Throughput |
| :--- | :--- |
| 1,000 Messages | ~1.00 Million msgs/sec |
| 10,000 Messages | ~10.00 Million msgs/sec |
| **20,000 Messages** | **~17.00 Million msgs/sec** |

## Build Instructions

### Prerequisites
* CMake
* A C++20 compatible compiler
* Asio (Standalone)

### Compiling

Google Test and Google Benchmark are managed automatically via CMake `FetchContent`.

```bash
# Clone the repository
git clone [https://github.com/Tache-Stefan/Market-Data-Handler.git](https://github.com/Tache-Stefan/Market-Data-Handler.git)
cd Market-Data-Handler

# Configure CMake
cmake -S . -B build

# Build strictly in Release mode for accurate performance
cmake --build build --config Release
```

### Running the Engine

```bash
# Start the Feed Handler (Listens on UDP 12345)
./build/Release/Market-Handler
```

### Running the Simulator and Tests

```bash
# In a separate terminal, blast the engine with 1,000,000 binary UDP packets
python scripts/simulator.py

# Run the Google Benchmark suite
./build/bench_worker

# Run the Unit Tests
ctest --test-dir build -C Release
```
