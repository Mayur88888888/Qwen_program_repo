/**
 * @file SessionManager.h
 * @brief Global lifecycle management and recovery coordination
 * @author Dr. Elias Voss
 * 
 * MANDATE: Handles application startup, shutdown, and crash recovery.
 * Coordinates with TransactionManager and PersistenceManager for state preservation.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include "Threading/HeartbeatMonitor.h"
#include "Persistence/PersistenceManager.h"
#include <atomic>
#include <string>
#include <memory>

namespace cad {

// Forward declarations
class TransactionManager;
class GeometryKernelProxy;
class CommandManager;

/**
 * @brief Application session states
 */
enum class SessionState {
    UNINITIALIZED = 0,
    STARTING,
    RUNNING,
    RECOVERING,
    LOW_MEMORY_MODE,
    SHUTTING_DOWN,
    TERMINATED
};

/**
 * @brief Session configuration
 */
struct SessionConfig {
    std::string recovery_directory = "./recovery";
    std::string temp_directory = "./tmp";
    size_t auto_save_interval_seconds = 60;
    size_t max_undo_transactions = 100;
    size_t kernel_heartbeat_timeout_ms = 500;
    bool enable_debug_logging = false;
    size_t memory_threshold_bytes = 1024 * 1024 * 1024; // 1GB trigger for low memory mode
};

/**
 * @brief Main session orchestrator
 * 
 * Responsibilities:
 * - Initialize all subsystems in correct order
 * - Monitor global state (memory, thread health)
 * - Coordinate graceful shutdown
 * - Handle crash recovery on startup
 */
class SessionManager {
public:
    /**
     * @brief Get singleton instance
     */
    static SessionManager& instance() noexcept {
        static SessionManager inst;
        return inst;
    }
    
    /**
     * @brief Initialize the CAD system
     * 
     * Startup sequence:
     * 1. Create recovery directory
     * 2. Initialize memory system
     * 3. Start geometry kernel worker thread
     * 4. Initialize heartbeat monitor
     * 5. Load transaction history (if recovering)
     * 6. Set state to RUNNING
     */
    Result<void, ErrorCode> initialize(const SessionConfig& config = SessionConfig()) noexcept;
    
    /**
     * @brief Graceful shutdown
     * 
     * Shutdown sequence:
     * 1. Set state to SHUTTING_DOWN
     * 2. Flush all pending transactions
     * 3. Trigger final auto-save
     * 4. Stop heartbeat monitor
     * 5. Signal kernel thread to terminate
     * 6. Wait for kernel thread completion
     * 7. Clean up resources
     * 8. Set state to TERMINATED
     */
    Result<void, ErrorCode> shutdown() noexcept;
    
    /**
     * @brief Check if system is ready for operations
     */
    bool isReady() const noexcept {
        return state_.load(std::memory_order_acquire) == SessionState::RUNNING;
    }
    
    /**
     * @brief Get current session state
     */
    SessionState getState() const noexcept {
        return state_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if in low memory mode
     */
    bool isLowMemoryMode() const noexcept {
        return state_.load(std::memory_order_relaxed) == SessionState::LOW_MEMORY_MODE;
    }
    
    /**
     * @brief Enter low memory mode gracefully
     */
    void enterLowMemoryMode() noexcept;
    
    /**
     * @brief Request immediate auto-save
     */
    Result<void, ErrorCode> triggerAutoSave() noexcept;
    
    /**
     * @brief Get recovery manager for crash recovery operations
     */
    PersistenceManager* getRecoveryManager() noexcept {
        return recovery_mgr_.get();
    }
    
    /**
     * @brief Get transaction manager
     */
    TransactionManager* getTransactionManager() noexcept {
        return transaction_mgr_.get();
    }
    
    /**
     * @brief Get geometry kernel proxy
     */
    GeometryKernelProxy* getKernelProxy() noexcept {
        return kernel_proxy_.get();
    }
    
    /**
     * @brief Get session uptime in seconds
     */
    uint64_t getUptimeSeconds() const noexcept;
    
    /**
     * @brief Get last auto-save timestamp
     */
    uint64_t getLastAutoSaveTimestamp() const noexcept {
        return last_auto_save_timestamp_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if recovery data is available from previous crash
     */
    bool hasRecoveryData() const noexcept;
    
    /**
     * @brief Attempt recovery from last checkpoint
     */
    Result<void, ErrorCode> recoverFromCrash() noexcept;
    
private:
    SessionManager() noexcept = default;
    ~SessionManager() noexcept;
    
    // Delete copy/move
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    Result<void, ErrorCode> initializeSubsystems() noexcept;
    Result<void, ErrorCode> startKernelThread() noexcept;
    Result<void, ErrorCode> startHeartbeatMonitor() noexcept;
    void cleanupRecoveryFiles() noexcept;
    
    std::atomic<SessionState> state_{SessionState::UNINITIALIZED};
    SessionConfig config_;
    
    std::unique_ptr<PersistenceManager> recovery_mgr_;
    std::unique_ptr<TransactionManager> transaction_mgr_;
    std::unique_ptr<GeometryKernelProxy> kernel_proxy_;
    std::unique_ptr<HeartbeatMonitor> heartbeat_monitor_;
    
    std::atomic<uint64_t> session_start_time_{0};
    std::atomic<uint64_t> last_auto_save_timestamp_{0};
    
    std::string recovery_file_path_;
};

} // namespace cad
