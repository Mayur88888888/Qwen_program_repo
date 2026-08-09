/**
 * @file HeartbeatMonitor.h
 * @brief Watchdog for kernel thread health monitoring
 * @author Dr. Elias Voss
 * 
 * MANDATE: Kill and restart kernel thread if frozen >500ms.
 * Preserves UI state during kernel recovery.
 */

#pragma once

#include "Core/Result.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>

namespace cad {

// Forward declaration
class GeometryKernelProxy;

/**
 * @brief Heartbeat monitor configuration
 */
struct HeartbeatConfig {
    size_t timeout_ms = 500;          // Kill threshold
    size_t check_interval_ms = 50;    // Polling frequency
    size_t restart_delay_ms = 100;    // Wait before restart
    size_t max_restart_attempts = 3;  // Give up after N failures
};

/**
 * @brief Monitors kernel thread health and triggers restart on freeze
 * 
 * Mechanism:
 * - Kernel thread must pulse heartbeat every N ms
 * - Monitor checks last heartbeat timestamp
 * - If elapsed > timeout_ms: kill thread, preserve UI state, restart
 * - Track restart attempts, give up after max failures
 */
class HeartbeatMonitor {
public:
    using KernelRestartFunc = std::function<Result<void, ErrorCode>()>;
    
    explicit HeartbeatMonitor(HeartbeatConfig config = HeartbeatConfig()) noexcept;
    ~HeartbeatMonitor() noexcept;
    
    // Delete copy/move
    HeartbeatMonitor(const HeartbeatMonitor&) = delete;
    HeartbeatMonitor& operator=(const HeartbeatMonitor&) = delete;
    
    /**
     * @brief Start monitoring
     */
    Result<void, ErrorCode> start() noexcept;
    
    /**
     * @brief Stop monitoring (called during shutdown)
     */
    void stop() noexcept;
    
    /**
     * @brief Register callback for kernel restart
     */
    void setRestartCallback(KernelRestartFunc callback) noexcept {
        restart_callback_ = std::move(callback);
    }
    
    /**
     * @brief Pulse heartbeat from kernel thread
     * Must be called regularly by kernel thread
     */
    void pulse() noexcept {
        last_heartbeat_.store(now(), std::memory_order_release);
    }
    
    /**
     * @brief Check if kernel is considered healthy
     */
    bool isKernelHealthy() const noexcept {
        auto elapsed = now() - last_heartbeat_.load(std::memory_order_acquire);
        return elapsed < config_.timeout_ms;
    }
    
    /**
     * @brief Get number of times kernel was restarted
     */
    size_t getRestartCount() const noexcept {
        return restart_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get number of consecutive failures
     */
    size_t getConsecutiveFailures() const noexcept {
        return consecutive_failures_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if monitor has given up on kernel
     */
    bool hasExhaustedRetries() const noexcept {
        return consecutive_failures_.load(std::memory_order_relaxed) >= config_.max_restart_attempts;
    }
    
    /**
     * @brief Reset failure counter (call after successful operation)
     */
    void resetFailureCounter() noexcept {
        consecutive_failures_.store(0, std::memory_order_relaxed);
    }
    
private:
    void monitorLoop() noexcept;
    Result<void, ErrorCode> handleTimeout() noexcept;
    uint64_t now() const noexcept;
    
    HeartbeatConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> last_heartbeat_{0};
    std::atomic<size_t> restart_count_{0};
    std::atomic<size_t> consecutive_failures_{0};
    
    std::thread monitor_thread_;
    KernelRestartFunc restart_callback_;
};

} // namespace cad
