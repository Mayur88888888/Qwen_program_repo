# CAD MONOLITH - PERFORMANCE OPTIMIZATION GUIDE
## Dr. Elias Voss - Large Assembly Handling Strategy

---

## 1. OVERVIEW: HANDLING 10,000+ PART ASSEMBLIES

**Target:** Maintain 60 FPS UI responsiveness with assemblies exceeding 10,000 parts.

**Core Strategies:**
1. **GPU Instancing** - Single mesh draw call, multiple transforms
2. **Level of Detail (LOD)** - Progressive detail based on camera distance
3. **Spatial Partitioning** - BVH for O(log N) intersection tests
4. **Progressive Loading** - Stream geometry on-demand
5. **Frustum Culling** - Only render what's visible

---

## 2. GPU INSTANCING ARCHITECTURE

### 2.1 Concept

Instead of drawing each part individually:
```cpp
// NAIVE APPROACH (SLOW - O(N) draw calls)
for (const auto& part : assembly.parts) {
    meshCache_.get(part.mesh_id).draw();  // 10,000 draw calls!
}
```

Use instanced rendering:
```cpp
// INSTANCED APPROACH (FAST - O(1) draw calls per unique mesh)
std::unordered_map<MeshId, std::vector<Transform>> instances;

// Group instances by mesh type
for (const auto& part : assembly.parts) {
    instances[part.mesh_id].push_back(part.transform);
}

// One draw call per unique mesh
for (const auto& [mesh_id, transforms] : instances) {
    auto& mesh = meshCache_.get(mesh_id);
    mesh.drawInstanced(transforms);  // GPU draws all instances in one call
}
```

### 2.2 Implementation

```cpp
/**
 * @brief Instance data for GPU instancing
 */
struct InstanceData {
    glm::mat4 transform_matrix;      // Model matrix
    uint32_t material_id;            // Material index
    uint32_t picking_id;             // For selection
    float lod_distance;              // LOD switch distance
};

/**
 * @brief Instanced mesh renderer
 */
class InstancedMeshRenderer {
public:
    struct Config {
        size_t max_instances_per_batch = 10000;
        bool enable_frustum_culling = true;
        bool enable_occlusion_culling = false;
    };
    
    /**
     * @brief Register instance for rendering
     */
    void addInstance(uint64_t mesh_id, 
                     const Transform& transform,
                     uint32_t material_id,
                     uint32_t picking_id) noexcept;
    
    /**
     * @brief Render all instances with frustum culling
     */
    void render(const Camera& camera) noexcept {
        // Step 1: Group by mesh
        auto batches = groupByMeshId();
        
        // Step 2: Frustum cull each batch
        for (auto& [mesh_id, instances] : batches) {
            if (config_.enable_frustum_culling) {
                instances = cullToFrustum(instances, camera.frustum);
            }
            
            if (instances.empty()) continue;
            
            // Step 3: Update instance buffer
            updateInstanceBuffer(mesh_id, instances);
            
            // Step 4: Draw instanced
            meshCache_.get(mesh_id).drawInstanced(instances.size());
        }
    }
    
    /**
     * @brief Get picking result from GPU selection buffer
     */
    uint32_t pickInstance(int screen_x, int screen_y) noexcept;
    
private:
    std::vector<InstanceData> cullToFrustum(
        const std::vector<InstanceData>& instances,
        const Frustum& frustum) noexcept;
    
    void updateInstanceBuffer(uint64_t mesh_id, 
                              const std::vector<InstanceData>& instances) noexcept;
    
    Config config_;
    std::unordered_map<uint64_t, std::vector<InstanceData>> instance_batches_;
    
    // GPU resources
    GLuint instance_vbo_{0};
    GLuint instance_vao_{0};
};
```

---

## 3. LEVEL OF DETAIL (LOD) SYSTEM

### 3.1 LOD Hierarchy

Each part has multiple representations:

```
LOD 0: Full B-Rep (100,000+ triangles) - < 1m from camera
LOD 1: Medium Mesh (10,000 triangles)   - 1m to 10m
LOD 2: Low Mesh (1,000 triangles)       - 10m to 50m
LOD 3: Convex Hull (100 triangles)      - 50m to 200m
LOD 4: Bounding Box (12 triangles)      - > 200m
```

### 3.2 LOD Selection Algorithm

```cpp
/**
 * @brief Select appropriate LOD based on camera distance
 */
LODLevel selectLOD(double camera_distance, double screen_size_threshold) noexcept {
    if (camera_distance < 1.0) {
        return LODLevel::FULL_BREP;
    } else if (camera_distance < 10.0) {
        return LODLevel::MEDIUM;
    } else if (camera_distance < 50.0) {
        return LODLevel::LOW;
    } else if (camera_distance < 200.0) {
        return LODLevel::CONVEX_HULL;
    } else {
        return LODLevel::BOUNDING_BOX;
    }
}

/**
 * @brief Calculate screen-space size of object
 * More accurate than raw distance for LOD selection
 */
double calculateScreenSize(const BoundingBox& bbox, 
                           const Camera& camera) noexcept {
    // Project bounding box corners to screen space
    glm::vec4 corners[8];
    bbox.getCorners(corners);
    
    float min_screen_size = FLT_MAX;
    
    for (const auto& corner : corners) {
        glm::vec4 projected = camera.projection * camera.view * corner;
        if (projected.w > 0.0f) {
            float screen_x = projected.x / projected.w;
            float screen_y = projected.y / projected.w;
            // Calculate approximate screen coverage
            min_screen_size = std::min(min_screen_size, 
                                       std::abs(screen_x) + std::abs(screen_y));
        }
    }
    
    return min_screen_size;
}
```

### 3.3 LOD Generation Pipeline

```cpp
/**
 * @brief Generate LOD chain for a mesh during import/loading
 */
Result<LODChain, ErrorCode> generateLODs(const Mesh& original_mesh) noexcept {
    LODChain chain;
    
    // LOD 0: Original
    chain.lod_0 = original_mesh;
    
    // LOD 1: Decimate to 50%
    chain.lod_1 = meshDecimator.decimate(original_mesh, 0.5);
    
    // LOD 2: Decimate to 10%
    chain.lod_2 = meshDecimator.decimate(original_mesh, 0.1);
    
    // LOD 3: Convex hull approximation
    chain.lod_3 = computeConvexHull(original_mesh.vertices);
    
    // LOD 4: Bounding box
    chain.lod_4 = createBoundingBoxMesh(original_mesh.bbox);
    
    return Result<LODChain, ErrorCode>::ok(std::move(chain));
}

/**
 * @brief LOD chain storage
 */
struct LODChain {
    Mesh lod_0;  // Full detail
    Mesh lod_1;  // 50% triangles
    Mesh lod_2;  // 10% triangles
    Mesh lod_3;  // Convex hull
    Mesh lod_4;  // Bounding box
    
    /**
     * @brief Get mesh for specific LOD level
     */
    const Mesh& getLOD(LODLevel level) const noexcept {
        switch (level) {
            case LODLevel::FULL_BREP: return lod_0;
            case LODLevel::MEDIUM: return lod_1;
            case LODLevel::LOW: return lod_2;
            case LODLevel::CONVEX_HULL: return lod_3;
            case LODLevel::BOUNDING_BOX: return lod_4;
            default: return lod_0;
        }
    }
};
```

---

## 4. SPATIAL PARTITIONING WITH BVH

### 4.1 Bounding Volume Hierarchy Structure

```cpp
/**
 * @brief BVH node for spatial acceleration
 */
struct BVHNode {
    BoundingBox bounds;
    uint32_t start_index{0};      // Start index in primitive list
    uint32_t primitive_count{0};  // Number of primitives (leaf only)
    uint32_t left_child{0};       // Index of left child (internal nodes)
    uint32_t right_child{0};      // Index of right child
    bool is_leaf{false};
    
    /**
     * @brief Check if ray intersects this node
     */
    bool intersectsRay(const Ray& ray) const noexcept {
        return bounds.intersectRay(ray);
    }
    
    /**
     * @brief Check if node is in view frustum
     */
    bool isInFrustum(const Frustum& frustum) const noexcept {
        return frustum.contains(bounds);
    }
};

/**
 * @brief BVH accelerator structure
 */
class BVHAccelerator {
public:
    /**
     * @brief Build BVH from list of instances
     */
    void build(const std::vector<InstanceData>& instances) noexcept;
    
    /**
     * @brief Query visible instances from camera view
     */
    std::vector<uint32_t> queryVisible(const Camera& camera) const noexcept {
        std::vector<uint32_t> visible_indices;
        queryFrustum(root_node_, camera.frustum, visible_indices);
        return visible_indices;
    }
    
    /**
     * @brief Ray-pick instance
     */
    Result<uint32_t, ErrorCode> pickInstance(const Ray& ray) const noexcept;
    
private:
    void queryFrustum(uint32_t node_idx, 
                      const Frustum& frustum,
                      std::vector<uint32_t>& results) const noexcept;
    
    void subdivide(BVHNode& node, 
                   std::vector<uint32_t>& primitive_indices) noexcept;
    
    std::vector<BVHNode> nodes_;
    std::vector<uint32_t> primitive_indices_;  // Indices into instance array
};
```

### 4.2 BVH Construction

```cpp
/**
 * @brief Build BVH using Surface Area Heuristic (SAH)
 */
void BVHAccelerator::build(const std::vector<InstanceData>& instances) noexcept {
    if (instances.empty()) return;
    
    // Create initial primitive list
    std::vector<uint32_t> indices(instances.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // Compute bounding boxes for all instances
    std::vector<BoundingBox> prim_boxes(instances.size());
    for (size_t i = 0; i < instances.size(); ++i) {
        prim_boxes[i] = instances[i].original_bbox.transform(
            instances[i].transform_matrix);
    }
    
    // Build tree recursively
    nodes_.clear();
    nodes_.reserve(instances.size() * 2);  // Estimate
    
    BVHNode root;
    subdivide(root, indices);
    nodes_.push_back(root);
}

/**
 * @brief Subdivide node using SAH
 */
void BVHAccelerator::subdivide(BVHNode& node,
                                std::vector<uint32_t>& indices) noexcept {
    // Compute bounding box for this node
    node.bounds = BoundingBox::empty();
    for (uint32_t idx : indices) {
        node.bounds.extend(prim_boxes_[idx]);
    }
    
    // Leaf node threshold
    if (indices.size() <= 4) {
        node.is_leaf = true;
        node.primitive_count = indices.size();
        node.start_index = primitive_indices_.size();
        
        for (uint32_t idx : indices) {
            primitive_indices_.push_back(idx);
        }
        return;
    }
    
    // Find best split plane using SAH
    SplitInfo best_split = findBestSplit(indices);
    
    if (best_split.cost >= node.bounds.surfaceArea() * indices.size()) {
        // No good split found - make leaf
        node.is_leaf = true;
        node.primitive_count = indices.size();
        node.start_index = primitive_indices_.size();
        
        for (uint32_t idx : indices) {
            primitive_indices_.push_back(idx);
        }
        return;
    }
    
    // Partition primitives
    std::vector<uint32_t> left_indices, right_indices;
    partitionIndices(indices, best_split.axis, best_split.position,
                    left_indices, right_indices);
    
    // Create children
    node.left_child = nodes_.size();
    nodes_.emplace_back();
    subdivide(nodes_.back(), left_indices);
    
    node.right_child = nodes_.size();
    nodes_.emplace_back();
    subdivide(nodes_.back(), right_indices);
}
```

---

## 5. PROGRESSIVE LOADING STRATEGY

### 5.1 Background Loading Thread

```cpp
/**
 * @brief Progressive loader for large assemblies
 */
class ProgressiveLoader {
public:
    struct Config {
        size_t target_frame_time_ms = 16;  // 60 FPS
        size_t load_batch_size = 100;       // Parts per frame
        size_t priority_queue_size = 1000;
    };
    
    /**
     * @brief Start loading assembly progressively
     */
    void beginLoading(const std::string& filepath) noexcept {
        loading_ = true;
        
        // Parse file header and feature tree first
        auto header = parseFileHeader(filepath);
        auto feature_tree = parseFeatureTree(filepath);
        
        // Create placeholder instances immediately
        display_placeholder_.createFromFeatureTree(feature_tree);
        
        // Queue detailed geometry for background loading
        for (const auto& part : feature_tree.parts) {
            load_queue_.push(PartLoadRequest{
                .part_id = part.id,
                .filepath = part.filepath,
                .priority = calculatePriority(part, camera_)
            });
        }
        
        // Start worker thread
        worker_thread_ = std::thread([this]() {
            workerLoop();
        });
    }
    
    /**
     * @brief Update loading progress (called from main thread)
     */
    LoadingStatus update() noexcept {
        LoadingStatus status;
        status.total_parts = total_parts_;
        status.loaded_parts = loaded_parts_.load();
        status.loading = loading_.load();
        status.progress = static_cast<float>(loaded_parts_) / total_parts_;
        return status;
    }
    
private:
    void workerLoop() noexcept {
        while (loading_ && !load_queue_.empty()) {
            // Process batch
            auto batch = load_queue_.popBatch(config_.load_batch_size);
            
            for (const auto& request : batch) {
                auto mesh = loadPartGeometry(request.filepath);
                
                if (mesh.isSuccess()) {
                    // Generate LODs
                    auto lods = generateLODs(mesh.value());
                    
                    // Update display
                    display_.replacePlaceholderWithMesh(
                        request.part_id, lods);
                    
                    loaded_parts_++;
                }
            }
            
            // Yield to avoid starving UI thread
            std::this_thread::yield();
        }
        
        loading_ = false;
    }
    
    float calculatePriority(const PartInfo& part, const Camera& camera) noexcept {
        double distance = part.bbox.distanceTo(camera.position);
        
        // Priority: closer parts first, larger parts first
        return 1.0f / (distance + 1.0) * part.bbox.volume();
    }
    
    std::atomic<bool> loading_{false};
    std::atomic<size_t> loaded_parts_{0};
    size_t total_parts_{0};
    
    PriorityQueueload_queue_;
    std::thread worker_thread_;
    
    PlaceholderDisplay display_placeholder_;
    MeshDisplay display_;
};
```

### 5.2 Loading Status UI

```cpp
/**
 * @brief Display loading progress to user
 */
void renderLoadingOverlay(const LoadingStatus& status) noexcept {
    if (!status.loading) return;
    
    float progress = status.progress * 100.0f;
    
    ImGui::Begin("Loading Assembly", nullptr, 
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Loading: %zu / %zu parts", 
                status.loaded_parts, status.total_parts);
    
    ImGui::ProgressBar(progress / 100.0f, ImVec2(300, 20),
                       (std::to_string((int)progress) + "%").c_str());
    
    if (ImGui::Button("Cancel")) {
        loader.cancel();
    }
    
    ImGui::End();
}
```

---

## 6. PERFORMANCE METRICS & TARGETS

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Frame Rate | 60 FPS | GPU frame timer |
| Draw Calls | < 1000/frame | OpenGL debug output |
| Instance Count | 10,000+ | Per-frame statistics |
| BVH Query Time | < 1ms | CPU profiler |
| LOD Transition | Seamless | Visual inspection |
| Memory Usage | < 4GB typical | System monitor |
| Load Time (10k parts) | < 30s interactive | Progress timer |

---

## 7. IMPLEMENTATION CHECKLIST

- [ ] GPU instancing support in renderer
- [ ] LOD generation pipeline (decimation, convex hull, bbox)
- [ ] BVH construction with SAH
- [ ] Frustum culling integration
- [ ] Progressive loading thread
- [ ] Placeholder geometry system
- [ ] Picking with instance IDs
- [ ] Performance monitoring overlay
- [ ] Automatic LOD switching based on camera
- [ ] Batch instance buffer updates

---

## 8. PROFILING TOOLS INTEGRATION

```cpp
/**
 * @brief Performance monitoring overlay
 */
class PerformanceMonitor {
public:
    void beginFrame() noexcept {
        frame_timer_.start();
    }
    
    void endFrame() noexcept {
        frame_times_.push_back(frame_timer_.elapsedMs());
        if (frame_times_.size() > 60) {
            frame_times_.pop_front();
        }
        
        fps_ = 1000.0 / getAverageFrameTime();
    }
    
    void renderOverlay() noexcept {
        ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("FPS: %.1f", fps_);
        ImGui::Text("Frame Time: %.2f ms", getAverageFrameTime());
        ImGui::Text("Draw Calls: %d", draw_call_count_);
        ImGui::Text("Instances Rendered: %d", instance_count_);
        ImGui::Text("BVH Nodes Traversed: %d", bvh_query_count_);
        ImGui::Text("Visible Parts: %d / %d", visible_count_, total_count_);
        
        ImGui::End();
    }
    
private:
    double getAverageFrameTime() const noexcept {
        double sum = 0.0;
        for (double t : frame_times_) sum += t;
        return sum / frame_times_.size();
    }
    
    Timer frame_timer_;
    std::deque<double> frame_times_;
    double fps_{0.0};
    
    int draw_call_count_{0};
    int instance_count_{0};
    int bvh_query_count_{0};
    int visible_count_{0};
    int total_count_{0};
};
```
