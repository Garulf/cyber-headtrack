#pragma once

#include <atomic>
#include <limits>

// Tracks whether something has happened recently: alive for timeoutSeconds after each Beat.
// Times are seconds on any monotonic clock. Safe to beat and query from different threads.
class Heartbeat {
public:
    explicit Heartbeat(double timeoutSeconds) : timeoutSeconds_(timeoutSeconds) {}

    void Beat(double nowSeconds) { lastBeat_.store(nowSeconds, std::memory_order_relaxed); }

    bool Alive(double nowSeconds) const {
        return nowSeconds - lastBeat_.load(std::memory_order_relaxed) <= timeoutSeconds_;
    }

private:
    double timeoutSeconds_;
    std::atomic<double> lastBeat_{-std::numeric_limits<double>::infinity()};
};
