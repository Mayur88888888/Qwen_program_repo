#include "Threading/HeartbeatMonitor.h"
#include <iostream>
#include <thread>

namespace Cad {

HeartbeatMonitor::HeartbeatMonitor(std::chrono::milliseconds timeout)
    : m_timeout(timeout)
    , m_target(nullptr)
    , m_running(false)
    , m_monitorThread(nullptr)
    , m_lastHeartbeat(std::chrono::steady_clock::now()) {
}

HeartbeatMonitor::~HeartbeatMonitor() noexcept {
    stop();
}

void HeartbeatMonitor::setTarget(IHeartbeatTarget* target) {
    m_target = target;
    // Reset heartbeat time when setting new target
    m_lastHeartbeat = std::chrono::steady_clock::now();
}

void HeartbeatMonitor::start() {
    if (m_running) {
        return; // Already running
    }
    
    m_running = true;
    m_monitorThread = std::make_unique<std::thread>(&HeartbeatMonitor::monitorLoop, this);
    
    std::cout << "[HEARTBEAT] Monitor started with " << m_timeout.count() << "ms timeout\n";
}

void HeartbeatMonitor::stop() noexcept {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_stopSignal.store(true, std::memory_order_release);
    
    if (m_monitorThread && m_monitorThread->joinable()) {
        m_monitorThread->join();
        m_monitorThread.reset();
    }
    
    std::cout << "[HEARTBEAT] Monitor stopped\n";
}

void HeartbeatMonitor::pulse() noexcept {
    // Called by the worker thread to signal it's alive
    m_lastHeartbeat = std::chrono::steady_clock::now();
}

void HeartbeatMonitor::monitorLoop() {
    while (!m_stopSignal.load(std::memory_order_acquire)) {
        // Wait for a short interval before checking again
        std::this_thread::sleep_for(m_timeout / 4);
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeartbeat);
        
        if (elapsed > m_timeout) {
            handleTimeout();
        }
    }
}

void HeartbeatMonitor::handleTimeout() {
    std::cerr << "[HEARTBEAT] TIMEOUT DETECTED! Target frozen for > " << m_timeout.count() << "ms\n";
    
    if (m_target) {
        std::cerr << "[HEARTBEAT] Initiating emergency restart of target...\n";
        
        try {
            // 1. Notify target of impending termination
            m_target->onPreKill();
            
            // 2. Force kill and restart (implementation depends on target)
            m_target->onKillAndRestart();
            
            // 3. Reset heartbeat timer after successful restart
            m_lastHeartbeat = std::chrono::steady_clock::now();
            
            std::cerr << "[HEARTBEAT] Target restarted successfully\n";
            
        } catch (const std::exception& e) {
            std::cerr << "[HEARTBEAT] CRITICAL: Failed to restart target: " << e.what() << "\n";
            // In a real implementation, this would trigger application-level recovery
        }
    }
}

bool HeartbeatMonitor::isRunning() const {
    return m_running;
}

std::chrono::milliseconds HeartbeatMonitor::getLastBeatTime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeartbeat);
}

// ============================================================================
// GeometryKernelWorker Implementation (The Worker Thread)
// ============================================================================

GeometryKernelWorker::GeometryKernelWorker()
    : m_running(true)
    , m_workerThread(nullptr)
    , m_messageQueue()
    , m_kernelState(KernelState::INITIALIZING) {
    
    startWorkerThread();
}

GeometryKernelWorker::~GeometryKernelWorker() noexcept {
    requestStop();
}

void GeometryKernelWorker::startWorkerThread() {
    m_workerThread = std::make_unique<std::thread>(&GeometryKernelWorker::workerLoop, this);
}

void GeometryKernelWorker::workerLoop() {
    std::cout << "[KERNEL] Worker thread started\n";
    
    m_kernelState = KernelState::READY;
    
    while (m_running) {
        // Send heartbeat pulse
        if (m_heartbeatTarget) {
            m_heartbeatTarget->pulse();
        }
        
        // Process messages from UI thread
        KernelMessage msg;
        if (m_messageQueue.try_dequeue(msg)) {
            processMessage(msg);
        } else {
            // No messages - sleep briefly to save CPU
            std::this_thread::sleep_for(1ms);
        }
    }
    
    m_kernelState = KernelState::SHUTDOWN;
    std::cout << "[KERNEL] Worker thread exiting\n";
}

void GeometryKernelWorker::processMessage(const KernelMessage& msg) {
    switch (msg.type) {
        case MessageType::CREATE_BODY:
            handleCreateBody(msg);
            break;
        case MessageType::MODIFY_BODY:
            handleModifyBody(msg);
            break;
        case MessageType::BOOLEAN_OP:
            handleBooleanOp(msg);
            break;
        case MessageType::COMPUTE_INTERSECTION:
            handleIntersection(msg);
            break;
        case MessageType::GET_SNAPSHOT:
            handleGetSnapshot(msg);
            break;
        default:
            std::cerr << "[KERNEL] Unknown message type: " << static_cast<int>(msg.type) << "\n";
    }
}

void GeometryKernelWorker::handleCreateBody(const KernelMessage& msg) {
    // Implementation would create geometry here
    // For now, just acknowledge
    std::cout << "[KERNEL] Received CREATE_BODY request\n";
}

void GeometryKernelWorker::handleModifyBody(const KernelMessage& msg) {
    std::cout << "[KERNEL] Received MODIFY_BODY request\n";
}

void GeometryKernelWorker::handleBooleanOp(const KernelMessage& msg) {
    std::cout << "[KERNEL] Received BOOLEAN_OP request\n";
}

void GeometryKernelWorker::handleIntersection(const KernelMessage& msg) {
    std::cout << "[KERNEL] Received INTERSECTION request\n";
}

void GeometryKernelWorker::handleGetSnapshot(const KernelMessage& msg) {
    std::cout << "[KERNEL] Received GET_SNAPSHOT request\n";
}

void GeometryKernelWorker::requestStop() noexcept {
    m_running = false;
    
    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
        m_workerThread.reset();
    }
}

bool GeometryKernelWorker::isAlive() const {
    return m_running && m_workerThread && m_workerThread->joinable();
}

KernelSnapshot GeometryKernelWorker::getSnapshot() const {
    // Return current kernel state snapshot
    return KernelSnapshot{};
}

void GeometryKernelWorker::restoreSnapshot(const KernelSnapshot& snapshot) {
    // Restore kernel state from snapshot
    std::cout << "[KERNEL] Restoring from snapshot\n";
}

void GeometryKernelWorker::onPreKill() noexcept {
    std::cout << "[KERNEL] Pre-kill notification received\n";
}

void GeometryKernelWorker::onKillAndRestart() {
    std::cout << "[KERNEL] Kill and restart initiated\n";
    // In real implementation: save state, kill thread, restart
}

void GeometryKernelWorker::setHeartbeatTarget(HeartbeatMonitor* target) {
    m_heartbeatTarget = target;
}

} // namespace Cad
