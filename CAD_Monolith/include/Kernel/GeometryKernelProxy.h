/**
 * @file GeometryKernelProxy.h
 * @brief Thread-safe interface for UI-to-Worker communication via message queues
 * @author Dr. Elias Voss
 * 
 * MANDATE: All kernel operations must go through this proxy.
 * Uses lock-free SPSC queues for zero-blocking communication.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include "Threading/HeartbeatMonitor.h"
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <cstdint>
#include <variant>
#include <optional>

namespace cad {

// Forward declarations
class BRepBody;
class NURBSCurve;
class NURBSSurface;
class ConstraintGraph;

/**
 * @brief Message types for kernel communication
 */
enum class MessageType : uint8_t {
    CREATE_BOX = 1,
    CREATE_CYLINDER,
    CREATE_SPHERE,
    CREATE_TORUS,
    EXTRUDE_SKETCH,
    REVOLVE_SKETCH,
    SWEEP_ALONG_PATH,
    LOFT_PROFILES,
    BOOLEAN_UNION,
    BOOLEAN_SUBTRACT,
    BOOLEAN_INTERSECT,
    MOVE_FACE,
    OFFSET_FACE,
    DELETE_FACE,
    FILLET_EDGE,
    CHAMFER_EDGE,
    CREATE_NURBS_CURVE,
    CREATE_NURBS_SURFACE,
    SOLVE_CONSTRAINTS,
    GET_BOUNDING_BOX,
    GET_MASS_PROPERTIES,
    EXPORT_TO_STEP,
    IMPORT_FROM_STEP,
    SANITIZE_GEOMETRY,
    SHUTDOWN
};

/**
 * @brief Command message structure
 */
struct KernelMessage {
    uint64_t id;
    MessageType type;
    std::vector<uint8_t> payload;  // Serialized parameters
    uint64_t timestamp;
    
    // For response correlation
    uint64_t correlation_id{0};
};

/**
 * @brief Response from kernel to UI
 */
template<typename T>
struct KernelResponse {
    uint64_t correlation_id;
    Result<T, ErrorCode> result;
    std::optional<T> data;
};

/**
 * @brief Lock-free single-producer single-consumer queue
 */
template<typename T, size_t Capacity = 1024>
class LockFreeSPSCQueue {
public:
    LockFreeSPSCQueue() noexcept : head_(0), tail_(0) {}
    
    bool push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % Capacity;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }
    
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_relaxed);
    }
    
    size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : (Capacity - head + tail);
    }
    
private:
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    T buffer_[Capacity];
};

/**
 * @brief Snapshot handle for rendering
 * 
 * Immutable view of kernel state that can be safely rendered
 * by UI thread without locking.
 */
struct RenderSnapshot {
    uint64_t version;
    std::vector<uint8_t> mesh_data;  // Serialized vertex/index buffers
    std::vector<uint8_t> transform_data;
    uint64_t timestamp;
    
    // Reference counted for safe sharing
    std::shared_ptr<void> shared_mesh_handle;
};

/**
 * @brief Main proxy for all kernel operations
 * 
 * Usage pattern:
 *   auto proxy = GeometryKernelProxy::create();
 *   proxy->start();
 *   
 *   // Send command (non-blocking)
 *   auto corr_id = proxy->sendCreateBox(width, height, depth);
 *   
 *   // Wait for response (with timeout)
 *   auto response = proxy->waitForResponse<BRepBody>(corr_id, timeout_ms);
 *   
 *   // Get snapshot for rendering (non-blocking)
 *   auto snapshot = proxy->getLatestSnapshot();
 *   renderer->draw(snapshot);
 */
class GeometryKernelProxy {
public:
    using ResponseCallback = std::function<void(uint64_t, const std::vector<uint8_t>&)>;
    
    static Result<std::unique_ptr<GeometryKernelProxy>, ErrorCode> create() noexcept;
    
    ~GeometryKernelProxy() noexcept;
    
    /**
     * @brief Start kernel worker thread
     */
    Result<void, ErrorCode> start() noexcept;
    
    /**
     * @brief Stop kernel worker thread
     */
    Result<void, ErrorCode> stop() noexcept;
    
    /**
     * @brief Check if kernel is running
     */
    bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }
    
    // ========== PRIMITIVE CREATION ==========
    
    Result<uint64_t> sendCreateBox(double width, double height, double depth) noexcept;
    Result<uint64_t> sendCreateCylinder(double radius, double height) noexcept;
    Result<uint64_t> sendCreateSphere(double radius) noexcept;
    Result<uint64_t> sendCreateTorus(double major_radius, double minor_radius) noexcept;
    
    // ========== SWEEP OPERATIONS ==========
    
    Result<uint64_t> sendExtrudeSketch(uint64_t sketch_id, double distance, double draft_angle) noexcept;
    Result<uint64_t> sendRevolveSketch(uint64_t sketch_id, double angle_degrees) noexcept;
    Result<uint64_t> sendSweepAlongPath(uint64_t profile_id, uint64_t path_id) noexcept;
    Result<uint64_t> sendLoftProfiles(const std::vector<uint64_t>& profile_ids) noexcept;
    
    // ========== BOOLEAN OPERATIONS ==========
    
    Result<uint64_t> sendBooleanUnion(uint64_t body_a, uint64_t body_b) noexcept;
    Result<uint64_t> sendBooleanSubtract(uint64_t target, uint64_t tool) noexcept;
    Result<uint64_t> sendBooleanIntersect(uint64_t body_a, uint64_t body_b) noexcept;
    
    // ========== DIRECT EDITING ==========
    
    Result<uint64_t> sendMoveFace(uint64_t body_id, uint64_t face_id, const double translation[3]) noexcept;
    Result<uint64_t> sendOffsetFace(uint64_t body_id, uint64_t face_id, double offset) noexcept;
    Result<uint64_t> sendDeleteFace(uint64_t body_id, uint64_t face_id) noexcept;
    Result<uint64_t> sendFilletEdge(uint64_t body_id, uint64_t edge_id, double radius) noexcept;
    
    // ========== NURBS ==========
    
    Result<uint64_t> sendCreateNURBSCurve(int degree, const std::vector<double>& control_points,
                                          const std::vector<double>& weights) noexcept;
    Result<uint64_t> sendCreateNURBSSurface(int u_degree, int v_degree,
                                            const std::vector<double>& control_points,
                                            const std::vector<double>& weights) noexcept;
    
    // ========== CONSTRAINTS ==========
    
    Result<uint64_t> sendSolveConstraints(uint64_t sketch_id) noexcept;
    
    // ========== QUERY OPERATIONS ==========
    
    Result<uint64_t> sendGetBoundingBox(uint64_t body_id) noexcept;
    Result<uint64_t> sendGetMassProperties(uint64_t body_id) noexcept;
    
    // ========== IMPORT/EXPORT ==========
    
    Result<uint64_t> sendExportToSTEP(uint64_t body_id, const std::string& filepath) noexcept;
    Result<uint64_t> sendImportFromSTEP(const std::string& filepath) noexcept;
    
    // ========== GEOMETRY SANITIZATION ==========
    
    Result<uint64_t> sendSanitizeGeometry(uint64_t body_id) noexcept;
    
    // ========== RESPONSE HANDLING ==========
    
    template<typename T>
    Result<KernelResponse<T>> waitForResponse(uint64_t correlation_id, 
                                               size_t timeout_ms = 5000) noexcept {
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            KernelResponse<T> response;
            
            {
                std::lock_guard<std::mutex> lock(response_mutex_);
                auto it = pending_responses_.find(correlation_id);
                if (it != pending_responses_.end()) {
                    response = std::move(it->second);
                    pending_responses_.erase(it);
                    return Result<KernelResponse<T>>::ok(std::move(response));
                }
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
                
            if (elapsed > timeout_ms) {
                return Result<KernelResponse<T>>::err(ErrorCode::GPU_TIMEOUT, 
                    "Response timeout after " + std::to_string(elapsed) + "ms");
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // ========== SNAPSHOT HANDLING ==========
    
    /**
     * @brief Get latest render snapshot (non-blocking)
     */
    std::shared_ptr<RenderSnapshot> getLatestSnapshot() const noexcept {
        return latest_snapshot_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Register callback for snapshot updates
     */
    void setSnapshotCallback(ResponseCallback callback) noexcept {
        snapshot_callback_ = std::move(callback);
    }
    
    /**
     * @brief Get heartbeat monitor reference
     */
    HeartbeatMonitor* getHeartbeatMonitor() noexcept {
        return &heartbeat_monitor_;
    }
    
private:
    GeometryKernelProxy() noexcept;
    
    void workerThreadMain() noexcept;
    void processMessage(const KernelMessage& msg) noexcept;
    void sendResponse(uint64_t correlation_id, const std::vector<uint8_t>& payload) noexcept;
    void updateSnapshot() noexcept;
    
    uint64_t generateCorrelationId() noexcept;
    
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    
    LockFreeSPSCQueue<KernelMessage> incoming_queue_;
    
    std::mutex response_mutex_;
    std::unordered_map<uint64_t, KernelResponse<std::vector<uint8_t>>> pending_responses_;
    
    std::atomic<std::shared_ptr<RenderSnapshot>> latest_snapshot_;
    ResponseCallback snapshot_callback_;
    
    HeartbeatMonitor heartbeat_monitor_;
    
    std::atomic<uint64_t> next_correlation_id_{1};
};

} // namespace cad
