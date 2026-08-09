#include "Core/TransactionManager.h"
#include <iostream>
#include <algorithm>

namespace Cad {

TransactionManager* TransactionManager::s_instance = nullptr;

TransactionManager& TransactionManager::getInstance() {
    if (!s_instance) {
        s_instance = new TransactionManager();
    }
    return *s_instance;
}

void TransactionManager::destroyInstance() {
    delete s_instance;
    s_instance = nullptr;
}

TransactionManager::TransactionManager()
    : m_undoStack()
    , m_redoStack()
    , m_currentTransaction(nullptr)
    , m_maxHistorySize(100) // Default: 100 operations
    , m_isRecording(false) {
}

TransactionManager::~TransactionManager() noexcept {
    clearAll();
}

Result<void, ErrorCode> TransactionManager::beginTransaction(const std::string& name) {
    if (m_isRecording) {
        return Result<void, ErrorCode>::failure(ErrorCode::TRANSACTION_NESTED, 
            "Cannot begin transaction while another is active");
    }
    
    try {
        m_currentTransaction = std::make_unique<Transaction>(name);
        m_isRecording = true;
        return Result<void, ErrorCode>::success();
        
    } catch (const std::bad_alloc&) {
        return Result<void, ErrorCode>::failure(ErrorCode::MEMORY_EXHAUSTED, 
            "Failed to allocate transaction object");
    }
}

Result<void, ErrorCode> TransactionManager::commitTransaction() {
    if (!m_isRecording || !m_currentTransaction) {
        return Result<void, ErrorCode>::failure(ErrorCode::NO_ACTIVE_TRANSACTION,
            "No active transaction to commit");
    }
    
    if (m_currentTransaction->getDeltas().empty()) {
        // Empty transaction - just discard
        m_currentTransaction.reset();
        m_isRecording = false;
        return Result<void, ErrorCode>::success();
    }
    
    try {
        // Move transaction to undo stack
        m_undoStack.push_back(std::move(m_currentTransaction));
        m_currentTransaction.reset();
        m_isRecording = false;
        
        // Clear redo stack (new action invalidates redo history)
        m_redoStack.clear();
        
        // Enforce memory limit
        enforceMemoryLimit();
        
        return Result<void, ErrorCode>::success();
        
    } catch (const std::bad_alloc&) {
        return Result<void, ErrorCode>::failure(ErrorCode::MEMORY_EXHAUSTED,
            "Failed to commit transaction - out of memory");
    }
}

Result<void, ErrorCode> TransactionManager::rollbackTransaction() {
    if (!m_isRecording || !m_currentTransaction) {
        return Result<void, ErrorCode>::failure(ErrorCode::NO_ACTIVE_TRANSACTION,
            "No active transaction to rollback");
    }
    
    // Execute inverse deltas in reverse order
    auto& deltas = m_currentTransaction->getDeltas();
    for (auto it = deltas.rbegin(); it != deltas.rend(); ++it) {
        Result<void, ErrorCode> result = it->applyInverse();
        if (!result.isSuccess()) {
            std::cerr << "[TRANSACTION] Rollback partial failure: " << result.getError().message << "\n";
            // Continue rolling back other deltas even if one fails
        }
    }
    
    m_currentTransaction.reset();
    m_isRecording = false;
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> TransactionManager::addDelta(std::unique_ptr<IDelta> delta) {
    if (!m_isRecording || !m_currentTransaction) {
        return Result<void, ErrorCode>::failure(ErrorCode::NO_ACTIVE_TRANSACTION,
            "Cannot add delta outside of transaction");
    }
    
    try {
        m_currentTransaction->addDelta(std::move(delta));
        return Result<void, ErrorCode>::success();
        
    } catch (const std::exception& e) {
        return Result<void, ErrorCode>::failure(ErrorCode::DELTA_ADD_FAILED, e.what());
    }
}

Result<void, ErrorCode> TransactionManager::undo() {
    if (m_undoStack.empty()) {
        return Result<void, ErrorCode>::failure(ErrorCode::UNDO_STACK_EMPTY,
            "No operations to undo");
    }
    
    try {
        // Get last transaction
        auto transaction = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        
        // Execute inverse deltas in reverse order
        auto& deltas = transaction->getDeltas();
        for (auto it = deltas.rbegin(); it != deltas.rend(); ++it) {
            Result<void, ErrorCode> result = it->applyInverse();
            if (!result.isSuccess()) {
                std::cerr << "[UNDO] Partial failure: " << result.getError().message << "\n";
            }
        }
        
        // Move to redo stack
        m_redoStack.push_back(std::move(transaction));
        
        return Result<void, ErrorCode>::success();
        
    } catch (const std::exception& e) {
        return Result<void, ErrorCode>::failure(ErrorCode::UNDO_FAILED, e.what());
    }
}

Result<void, ErrorCode> TransactionManager::redo() {
    if (m_redoStack.empty()) {
        return Result<void, ErrorCode>::failure(ErrorCode::REDO_STACK_EMPTY,
            "No operations to redo");
    }
    
    try {
        // Get last transaction
        auto transaction = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        
        // Execute deltas in forward order
        auto& deltas = transaction->getDeltas();
        for (auto& delta : deltas) {
            Result<void, ErrorCode> result = delta.apply();
            if (!result.isSuccess()) {
                std::cerr << "[REDO] Partial failure: " << result.getError().message << "\n";
            }
        }
        
        // Move back to undo stack
        m_undoStack.push_back(std::move(transaction));
        
        return Result<void, ErrorCode>::success();
        
    } catch (const std::exception& e) {
        return Result<void, ErrorCode>::failure(ErrorCode::REDO_FAILED, e.what());
    }
}

bool TransactionManager::canUndo() const {
    return !m_undoStack.empty();
}

bool TransactionManager::canRedo() const {
    return !m_redoStack.empty();
}

size_t TransactionManager::getUndoStackSize() const {
    return m_undoStack.size();
}

size_t TransactionManager::getRedoStackSize() const {
    return m_redoStack.size();
}

void TransactionManager::setMaxHistorySize(size_t maxOperations) {
    m_maxHistorySize = maxOperations;
    enforceMemoryLimit();
}

void TransactionManager::enforceMemoryLimit() {
    while (m_undoStack.size() > m_maxHistorySize) {
        m_undoStack.pop_front();
    }
}

void TransactionManager::clearAll() noexcept {
    m_undoStack.clear();
    m_redoStack.clear();
    m_currentTransaction.reset();
    m_isRecording = false;
}

// ============================================================================
// Transaction Implementation
// ============================================================================

Transaction::Transaction(const std::string& name)
    : m_name(name)
    , m_timestamp(std::chrono::steady_clock::now())
    , m_deltas() {
}

Transaction::~Transaction() noexcept = default;

void Transaction::addDelta(std::unique_ptr<IDelta> delta) {
    m_deltas.push_back(std::move(delta));
}

const std::vector<std::unique_ptr<IDelta>>& Transaction::getDeltas() const {
    return m_deltas;
}

std::vector<std::unique_ptr<IDelta>>& Transaction::getDeltas() {
    return m_deltas;
}

const std::string& Transaction::getName() const {
    return m_name;
}

std::chrono::steady_clock::time_point Transaction::getTimestamp() const {
    return m_timestamp;
}

size_t Transaction::getDeltaCount() const {
    return m_deltas.size();
}

} // namespace Cad
