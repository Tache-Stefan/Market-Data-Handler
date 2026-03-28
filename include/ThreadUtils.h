#pragma once

#include <iostream>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <sched.h>
#endif

namespace market_handler {

    inline void pin_current_thread_to_core(int core_id) {
#ifdef _WIN32
        HANDLE this_thread = GetCurrentThread();
        if (SetThreadAffinityMask(this_thread, (1ULL << core_id)) == 0) {
            std::cerr << "Failed to pin to core " << core_id << ". Error: " << GetLastError() << "\n";
        }
        SetThreadPriority(this_thread, THREAD_PRIORITY_TIME_CRITICAL);

#elif defined(__linux__)
        // 1. Set CPU Affinity
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        pthread_t current_thread = pthread_self();
        if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
            std::cerr << "Failed to pin to core " << core_id << "\n";
        }

        // 2. Set Real-Time Priority
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(current_thread, SCHED_FIFO, &param) != 0) {
            std::cerr << "Warning: Failed to set SCHED_FIFO priority. Run as root or set CAP_SYS_NICE.\n";
        }
#endif
    }

}
