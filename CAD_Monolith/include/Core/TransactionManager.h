/**
 * @file TransactionManager.h
 * @brief Undo/Redo system with delta compression and atomic transactions
 * @author Dr. Elias Voss
 * 
 * MANDATE: Store deltas, not full states. Wrap all user actions in transactions.
 * Support atomic rollback on failure.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace cad {

// Forward declarations
class Command;
class Transaction;

/**
 * @brief Transaction state machine
 */
enum class TransactionState {
    PENDING,      // Commands being collected
    COMMITTED,    // Ready for undo/redo
    ROLLED_BACK,  // Failed and reverted
    EXECUTING     // Currently executing
};

/**
 * @brief Delta representation for memory-efficient undo/redo
 * 
 * Instead of storing full object states, store only what changed:
 * - Object ID
 * - Property name
 * - Old value (for undo)
 * - New value (for redo)
 */
struct PropertyDelta {
    uint64_t object_id;
    std::string property_name;
    std::vector<uint8_t> old_value;  // Serialized
    std::vector<uint8_t> new_value;  // Serialized
    
    Result<void, ErrorCode> apply(bool forward) const noexcept;
};

/**
 * @brief Command delta container
 */
class CommandDelta {
public:
    using DeltaList = std::vector<PropertyDelta>;
    
    void addDelta(const PropertyDelta& delta) noexcept {
        deltas_.push_back(delta);
    }
    
    Result<void, ErrorCode> applyForward() noexcept {
        for (const auto& delta : deltas_) {
            auto res = delta.apply(true);
            if (res.isError()) return res;
        }
        return Result<void, ErrorCode>::ok();
    }
    
    Result<void, ErrorCode> applyBackward() noexcept {
        // Apply in reverse order
        for (auto it = deltas_.rbegin(); it != deltas_.rend(); ++it) {
            auto res = it->apply(false);
            if (res.isError()) return res;
        }
        return Result<void, ErrorCode>::ok();
    }
    
    size_t getDeltaCount() const noexcept {
        return deltas_.size();
    }
    
    void clear() noexcept {
        deltas_.clear();
    }
    
private:
    DeltaList deltas_;
};

/**
 * @brief Atomic transaction grouping multiple commands
 */
class Transaction {
public:
    explicit Transaction(uint64_t id) noexcept 
        : id_(id), state_(TransactionState::PENDING) {}
    
    uint64_t getId() const noexcept { return id_; }
    TransactionState getState() const noexcept { return state_; }
    
    void addCommand(std::unique_ptr<CommandDelta> cmd) noexcept {
        commands_.push_back(std::move(cmd));
    }
    
    Result<void, ErrorCode> commit() noexcept;
    Result<void, ErrorCode> execute() noexcept;
    Result<void, ErrorCode> undo() noexcept;
    Result<void, ErrorCode> redo() noexcept;
    Result<void, ErrorCode> rollback() noexcept;
    
    void setTimestamp(uint64_t ts) noexcept { timestamp_ = ts; }
    uint64_t getTimestamp() const noexcept { return timestamp_; }
    
    void setDescription(const std::string& desc) noexcept {
        description_ = desc;
    }
    
    const std::string& getDescription() const noexcept {
        return description_;
    }
    
    size_t getCommandCount() const noexcept {
        return commands_.size();
    }
    
private:
    uint64_t id_;
    TransactionState state_;
    uint64_t timestamp_{0};
    std::string description_;
    std::vector<std::unique_ptr<CommandDelta>> commands_;
};

/**
 * @brief Undo/Redo stack manager
 * 
 * Features:
 * - Delta-compressed storage
 * - Transaction grouping
 * - Memory limit enforcement
 * - Automatic flushing in low-memory mode
 */
class TransactionManager {
public:
    struct Config {
        size_t max_transactions = 100;
        size_t max_memory_bytes = 256 * 1024 * 1024; // 256MB
        bool enable_compression = true;
    };
    
    explicit TransactionManager(Config config = Config()) noexcept;
    ~TransactionManager() noexcept;
    
    /**
     * @brief Begin a new transaction
     */
    Transaction* beginTransaction(const std::string& description = "") noexcept;
    
    /**
     * @brief Commit current transaction
     */
    Result<void, ErrorCode> commitTransaction() noexcept;
    
    /**
     * @brief Rollback current transaction
     */
    Result<void, ErrorCode> rollbackTransaction() noexcept;
    
    /**
     * @brief Undo last committed transaction
     */
    Result<void, ErrorCode> undo() noexcept;
    
    /**
     * @brief Redo previously undone transaction
     */
    Result<void, ErrorCode> redo() noexcept;
    
    /**
     * @brief Check if undo is available
     */
    bool canUndo() const noexcept {
        return undo_stack_index_ > 0;
    }
    
    /**
     * @brief Check if redo is available
     */
    bool canRedo() const noexcept {
        return undo_stack_index_ < undo_stack_.size();
    }
    
    /**
     * @brief Clear all undo/redo history
     */
    void clearHistory() noexcept;
    
    /**
     * @brief Get current transaction (if active)
     */
    Transaction* getCurrentTransaction() noexcept {
        return current_transaction_.get();
    }
    
    /**
     * @brief Flush history to stay within memory limits
     */
    void flushToMemoryLimit() noexcept;
    
    /**
     * @brief Enable low-memory mode (reduce history depth)
     */
    void enterLowMemoryMode() noexcept {
        low_memory_mode_ = true;
        max_transactions_low_mem_ = 10; // Keep only last 10
        flushToMemoryLimit();
    }
    
    /**
     * @brief Get memory usage in bytes
     */
    size_t getMemoryUsageBytes() const noexcept;
    
    /**
     * @brief Serialize transaction stack for persistence
     */
    std::vector<uint8_t> serialize() const noexcept;
    
    /**
     * @brief Deserialize transaction stack from persistence
     */
    Result<void, ErrorCode> deserialize(const std::vector<uint8_t>& data) noexcept;
    
private:
    Result<void, ErrorCode> pushTransaction(std::unique_ptr<Transaction> txn) noexcept;
    void trimUndoStack() noexcept;
    
    Config config_;
    std::vector<std::unique_ptr<Transaction>> undo_stack_;
    size_t undo_stack_index_{0};
    std::unique_ptr<Transaction> current_transaction_;
    uint64_t next_transaction_id_{1};
    
    bool low_memory_mode_{false};
    size_t max_transactions_low_mem_{10};
    
    mutable size_t cached_memory_usage_{0};
};

} // namespace cad
