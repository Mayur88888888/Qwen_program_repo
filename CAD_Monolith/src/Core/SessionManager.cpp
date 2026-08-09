#include "Core/SessionManager.h"
#include <iostream>
#include <fstream>

namespace Cad {

SessionManager* SessionManager::s_instance = nullptr;

SessionManager& SessionManager::getInstance() {
    if (!s_instance) {
        s_instance = new SessionManager();
    }
    return *s_instance;
}

void SessionManager::destroyInstance() {
    delete s_instance;
    s_instance = nullptr;
}

SessionManager::SessionManager() 
    : m_sessionId(0)
    , m_isRecoveryMode(false)
    , m_kernelWorker(nullptr)
    , m_heartbeatMonitor(nullptr) {
    
    initialize();
}

SessionManager::~SessionManager() noexcept {
    shutdown();
}

Result<void, ErrorCode> SessionManager::initialize() {
    try {
        // 1. Install global memory handler FIRST
        MemoryGuard::installGlobalHandler();
        
        // 2. Generate session ID
        m_sessionId = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        
        // 3. Create recovery directory
        std::string recoveryPath = getRecoveryDirectory();
        #ifdef _WIN32
            CreateDirectoryA(recoveryPath.c_str(), nullptr);
        #else
            mkdir(recoveryPath.c_str(), 0755);
        #endif
        
        // 4. Start heartbeat monitor
        m_heartbeatMonitor = std::make_unique<HeartbeatMonitor>(500ms);
        
        // 5. Start geometry kernel worker thread
        m_kernelWorker = std::make_unique<GeometryKernelWorker>();
        
        // 6. Link heartbeat to kernel
        m_heartbeatMonitor->setTarget(m_kernelWorker.get());
        m_heartbeatMonitor->start();
        
        std::cout << "[SESSION] Initialized Session " << m_sessionId << "\n";
        return Result<void, ErrorCode>::success();
        
    } catch (const std::bad_alloc& e) {
        std::cerr << "[SESSION] FATAL: Cannot allocate session resources.\n";
        return Result<void, ErrorCode>::failure(ErrorCode::MEMORY_EXHAUSTED, "Failed to allocate session resources");
    } catch (const std::exception& e) {
        std::cerr << "[SESSION] FATAL: " << e.what() << "\n";
        return Result<void, ErrorCode>::failure(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

void SessionManager::shutdown() noexcept {
    std::cout << "[SESSION] Shutting down Session " << m_sessionId << "...\n";
    
    // 1. Stop heartbeat first
    if (m_heartbeatMonitor) {
        m_heartbeatMonitor->stop();
        m_heartbeatMonitor.reset();
    }
    
    // 2. Signal kernel to stop gracefully
    if (m_kernelWorker) {
        m_kernelWorker->requestStop();
        m_kernelWorker.reset();
    }
    
    // 3. Cleanup transaction manager
    TransactionManager::getInstance().clearAll();
    
    // 4. Remove memory handler
    MemoryGuard::removeGlobalHandler();
    
    std::cout << "[SESSION] Shutdown complete.\n";
}

Result<void, ErrorCode> SessionManager::startNewSession(const std::string& filePath) {
    if (m_kernelWorker && !m_kernelWorker->isAlive()) {
        std::cerr << "[SESSION] Kernel died unexpectedly. Attempting restart...\n";
        Result<void, ErrorCode> restartResult = restartKernel();
        if (!restartResult.isSuccess()) {
            return restartResult;
        }
    }
    
    m_currentFilePath = filePath;
    m_sessionStartTime = std::chrono::steady_clock::now();
    
    // Clear any previous recovery files for this session
    cleanupRecoveryFiles();
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> SessionManager::restartKernel() {
    try {
        // Preserve current state before killing
        auto preservedState = m_kernelWorker ? m_kernelWorker->getSnapshot() : nullptr;
        
        // Kill old worker
        m_kernelWorker.reset();
        
        // Create new worker
        m_kernelWorker = std::make_unique<GeometryKernelWorker>();
        m_heartbeatMonitor->setTarget(m_kernelWorker.get());
        
        // Restore state if possible
        if (preservedState) {
            m_kernelWorker->restoreSnapshot(*preservedState);
        }
        
        std::cout << "[SESSION] Kernel restarted successfully.\n";
        return Result<void, ErrorCode>::success();
        
    } catch (const std::exception& e) {
        std::cerr << "[SESSION] Kernel restart failed: " << e.what() << "\n";
        return Result<void, ErrorCode>::failure(ErrorCode::KERNEL_RESTART_FAILED, e.what());
    }
}

std::string SessionManager::getRecoveryDirectory() const {
    #ifdef _WIN32
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        return exePath.substr(0, lastSlash) + "\\recovery";
    #else
        return "./recovery";
    #endif
}

std::string SessionManager::getRecoveryFilePath() const {
    return getRecoveryDirectory() + "/session_" + std::to_string(m_sessionId) + ".autosave";
}

void SessionManager::cleanupRecoveryFiles() {
    // Delete old recovery files older than 24 hours
    // Implementation would iterate and check timestamps
}

bool SessionManager::isRecoveryAvailable() const {
    std::ifstream file(getRecoveryFilePath());
    return file.good();
}

Result<void, ErrorCode> SessionManager::triggerRecovery() {
    m_isRecoveryMode = true;
    std::cout << "[SESSION] Entering Recovery Mode...\n";
    
    // Delegate to PersistenceManager for actual recovery logic
    return Result<void, ErrorCode>::success();
}

uint64_t SessionManager::getSessionId() const {
    return m_sessionId;
}

bool SessionManager::isRecoveryMode() const {
    return m_isRecoveryMode;
}

} // namespace Cad
