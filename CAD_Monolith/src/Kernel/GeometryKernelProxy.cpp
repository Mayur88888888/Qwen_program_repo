#include "Kernel/GeometryKernelProxy.h"
#include <iostream>

namespace Cad {

GeometryKernelProxy* GeometryKernelProxy::s_instance = nullptr;

GeometryKernelProxy& GeometryKernelProxy::getInstance() {
    if (!s_instance) {
        s_instance = new GeometryKernelProxy();
    }
    return *s_instance;
}

void GeometryKernelProxy::destroyInstance() {
    delete s_instance;
    s_instance = nullptr;
}

GeometryKernelProxy::GeometryKernelProxy()
    : m_worker()
    , m_heartbeatMonitor(std::make_unique<HeartbeatMonitor>(500ms))
    , m_requestCounter(0)
    , m_pendingRequests() {
    
    // Link heartbeat to worker
    m_heartbeatMonitor->setTarget(&m_worker);
    m_heartbeatMonitor->start();
    
    std::cout << "[PROXY] Geometry Kernel Proxy initialized\n";
}

GeometryKernelProxy::~GeometryKernelProxy() noexcept {
    m_heartbeatMonitor->stop();
}

Result<uint64_t, ErrorCode> GeometryKernelProxy::createBox(double width, double height, double depth) {
    KernelMessage msg;
    msg.type = MessageType::CREATE_BODY;
    msg.payload.boxParams = {width, height, depth};
    
    return sendRequest(msg);
}

Result<uint64_t, ErrorCode> GeometryKernelProxy::createSphere(double radius) {
    KernelMessage msg;
    msg.type = MessageType::CREATE_BODY;
    msg.payload.sphereParams = {radius};
    
    return sendRequest(msg);
}

Result<uint64_t, ErrorCode> GeometryKernelProxy::createCylinder(double radius, double height) {
    KernelMessage msg;
    msg.type = MessageType::CREATE_BODY;
    msg.payload.cylinderParams = {radius, height};
    
    return sendRequest(msg);
}

Result<void, ErrorCode> GeometryKernelProxy::booleanUnion(uint64_t bodyA, uint64_t bodyB) {
    KernelMessage msg;
    msg.type = MessageType::BOOLEAN_OP;
    msg.payload.booleanParams = {bodyA, bodyB, BooleanOp::UNION};
    
    auto result = sendRequest(msg);
    return result.isSuccess() ? Result<void, ErrorCode>::success() 
                              : Result<void, ErrorCode>::failure(result.getError());
}

Result<void, ErrorCode> GeometryKernelProxy::booleanSubtract(uint64_t bodyA, uint64_t bodyB) {
    KernelMessage msg;
    msg.type = MessageType::BOOLEAN_OP;
    msg.payload.booleanParams = {bodyA, bodyB, BooleanOp::SUBTRACT};
    
    auto result = sendRequest(msg);
    return result.isSuccess() ? Result<void, ErrorCode>::success() 
                              : Result<void, ErrorCode>::failure(result.getError());
}

Result<void, ErrorCode> GeometryKernelProxy::booleanIntersect(uint64_t bodyA, uint64_t bodyB) {
    KernelMessage msg;
    msg.type = MessageType::BOOLEAN_OP;
    msg.payload.booleanParams = {bodyA, bodyB, BooleanOp::INTERSECT};
    
    auto result = sendRequest(msg);
    return result.isSuccess() ? Result<void, ErrorCode>::success() 
                              : Result<void, ErrorCode>::failure(result.getError());
}

Result<std::vector<Vector3>, ErrorCode> GeometryKernelProxy::computeIntersection(
    uint64_t bodyA, uint64_t bodyB) {
    
    KernelMessage msg;
    msg.type = MessageType::COMPUTE_INTERSECTION;
    msg.payload.intersectionParams = {bodyA, bodyB};
    
    // In a real implementation, this would wait for response and return points
    // For now, return empty vector as placeholder
    auto result = sendRequest(msg);
    if (result.isSuccess()) {
        return Result<std::vector<Vector3>, ErrorCode>::success(std::vector<Vector3>{});
    } else {
        return Result<std::vector<Vector3>, ErrorCode>::failure(result.getError());
    }
}

Result<uint64_t, ErrorCode> GeometryKernelProxy::sendRequest(const KernelMessage& msg) {
    uint64_t requestId = ++m_requestCounter;
    
    // Create promise/future pair for synchronous response
    auto promise = std::make_shared<std::promise<Result<KernelResponse, ErrorCode>>>();
    m_pendingRequests[requestId] = promise;
    
    // Send message to worker thread
    if (!m_worker.postMessage(msg)) {
        m_pendingRequests.erase(requestId);
        return Result<uint64_t, ErrorCode>::failure(ErrorCode::QUEUE_FULL, 
            "Failed to send message to kernel - queue full");
    }
    
    // Wait for response with timeout
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(5)); // 5 second timeout
    
    if (status == std::future_status::timeout) {
        m_pendingRequests.erase(requestId);
        return Result<uint64_t, ErrorCode>::failure(ErrorCode::TIMEOUT, 
            "Kernel request timed out after 5 seconds");
    }
    
    auto result = future.get();
    if (!result.isSuccess()) {
        return Result<uint64_t, ErrorCode>::failure(result.getError());
    }
    
    // Extract body ID from response
    return Result<uint64_t, ErrorCode>::success(result.getValue().bodyId);
}

bool GeometryKernelProxy::isKernelAlive() const {
    return m_worker.isAlive();
}

void GeometryKernelProxy::forceRestart() {
    std::cout << "[PROXY] Forcing kernel restart...\n";
    m_worker.onKillAndRestart();
}

size_t GeometryKernelProxy::getPendingRequestCount() const {
    return m_pendingRequests.size();
}

// Explicit specialization for void return type handling
template<>
Result<void, ErrorCode> GeometryKernelProxy::sendRequest<void>(const KernelMessage& msg) {
    uint64_t requestId = ++m_requestCounter;
    
    auto promise = std::make_shared<std::promise<Result<KernelResponse, ErrorCode>>>();
    m_pendingRequests[requestId] = promise;
    
    if (!m_worker.postMessage(msg)) {
        m_pendingRequests.erase(requestId);
        return Result<void, ErrorCode>::failure(ErrorCode::QUEUE_FULL, 
            "Failed to send message to kernel - queue full");
    }
    
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(5));
    
    if (status == std::future_status::timeout) {
        m_pendingRequests.erase(requestId);
        return Result<void, ErrorCode>::failure(ErrorCode::TIMEOUT, 
            "Kernel request timed out after 5 seconds");
    }
    
    auto result = future.get();
    if (!result.isSuccess()) {
        return Result<void, ErrorCode>::failure(result.getError());
    }
    
    return Result<void, ErrorCode>::success();
}

} // namespace Cad
