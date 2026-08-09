/**
 * @file BRepBody.h
 * @brief Boundary Representation solid modeling core class
 * @author Dr. Elias Voss
 * 
 * MANDATE: All B-Rep operations must have tolerance guards.
 * GeometrySanitizer removes degenerate elements post-operation.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <array>
#include <string>

namespace cad {

// Geometric tolerance constants
constexpr double GEOMETRY_EPSILON = 1e-9;
constexpr double MIN_EDGE_LENGTH = 1e-6;
constexpr double MIN_TRIANGLE_AREA = 1e-12;

/**
 * @brief 3D point with tolerance-aware comparisons
 */
struct Point3D {
    double x, y, z;
    
    Point3D() : x(0), y(0), z(0) {}
    Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    
    bool operator==(const Point3D& other) const noexcept {
        return std::abs(x - other.x) < GEOMETRY_EPSILON &&
               std::abs(y - other.y) < GEOMETRY_EPSILON &&
               std::abs(z - other.z) < GEOMETRY_EPSILON;
    }
    
    Point3D operator+(const Point3D& other) const noexcept {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }
    
    Point3D operator-(const Point3D& other) const noexcept {
        return Point3D(x - other.x, y - other.y, z - other.z);
    }
    
    Point3D operator*(double scale) const noexcept {
        return Point3D(x * scale, y * scale, z * scale);
    }
    
    double dot(const Point3D& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }
    
    Point3D cross(const Point3D& other) const noexcept {
        return Point3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    double length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    Point3D normalized() const noexcept {
        double len = length();
        if (len < GEOMETRY_EPSILON) return Point3D(0, 0, 0);
        return Point3D(x / len, y / len, z / len);
    }
};

/**
 * @brief Topological entity types
 */
enum class TopoType : uint8_t {
    VERTEX = 1,
    EDGE = 2,
    WIRE = 3,
    FACE = 4,
    SHELL = 5,
    SOLID = 6
};

/**
 * @brief Unique ID for topological entities
 */
using TopoID = uint64_t;

/**
 * @brief Vertex in B-Rep structure
 */
struct Vertex {
    TopoID id;
    Point3D point;
    std::vector<TopoID> incident_edges;
    
    bool isDegenerate() const noexcept {
        return std::abs(point.x) > 1e15 || std::abs(point.y) > 1e15 || std::abs(point.z) > 1e15;
    }
};

/**
 * @brief Edge with geometric and topological data
 */
struct Edge {
    TopoID id;
    TopoID start_vertex;
    TopoID end_vertex;
    std::vector<Point3D> curve_points;  // Discretized curve
    std::vector<TopoID> adjacent_faces;
    
    double getLength() const noexcept {
        if (curve_points.size() < 2) return 0.0;
        double len = 0.0;
        for (size_t i = 1; i < curve_points.size(); ++i) {
            len += (curve_points[i] - curve_points[i-1]).length();
        }
        return len;
    }
    
    bool isDegenerate() const noexcept {
        return getLength() < MIN_EDGE_LENGTH;
    }
};

/**
 * @brief Face with surface representation
 */
struct Face {
    TopoID id;
    std::vector<TopoID> boundary_wires;
    std::vector<TopoID> interior_wires;  // Holes
    std::vector<Point3D> surface_mesh;   // Discretized surface
    Point3D normal;
    double area{0.0};
    
    bool isDegenerate() const noexcept {
        return area < MIN_TRIANGLE_AREA;
    }
};

/**
 * @brief Shell as collection of faces
 */
struct Shell {
    TopoID id;
    std::vector<TopoID> faces;
    bool is_closed{false};
};

/**
 * @brief Solid body with full B-Rep structure
 */
class BRepBody {
public:
    explicit BRepBody(TopoID id) noexcept : id_(id) {}
    
    TopoID getId() const noexcept { return id_; }
    
    // ========== PRIMITIVE CREATION ==========
    
    static Result<std::unique_ptr<BRepBody>, ErrorCode> 
    createBox(double width, double height, double depth) noexcept;
    
    static Result<std::unique_ptr<BRepBody>, ErrorCode>
    createCylinder(double radius, double height, int segments = 32) noexcept;
    
    static Result<std::unique_ptr<BRepBody>, ErrorCode>
    createSphere(double radius, int segments = 32) noexcept;
    
    static Result<std::unique_ptr<BRepBody>, ErrorCode>
    createTorus(double major_radius, double minor_radius, int segments = 32) noexcept;
    
    // ========== SWEEP OPERATIONS ==========
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    extrude(double distance, double draft_angle = 0.0) noexcept;
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    revolve(double angle_degrees, const Point3D& axis_origin, const Point3D& axis_direction) noexcept;
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    sweepAlongPath(const BRepBody& path_body) noexcept;
    
    static Result<std::unique_ptr<BRepBody>, ErrorCode>
    loft(const std::vector<BRepBody*>& profiles) noexcept;
    
    // ========== BOOLEAN OPERATIONS ==========
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    uniteWith(const BRepBody& other) noexcept;
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    subtract(const BRepBody& tool) noexcept;
    
    Result<std::unique_ptr<BRepBody>, ErrorCode>
    intersectWith(const BRepBody& other) noexcept;
    
    // ========== DIRECT EDITING ==========
    
    Result<void, ErrorCode> moveFace(TopoID face_id, const Point3D& translation) noexcept;
    Result<void, ErrorCode> offsetFace(TopoID face_id, double offset) noexcept;
    Result<void, ErrorCode> deleteFace(TopoID face_id, bool heal = true) noexcept;
    Result<void, ErrorCode> filletEdge(TopoID edge_id, double radius) noexcept;
    Result<void, ErrorCode> chamferEdge(TopoID edge_id, double distance) noexcept;
    
    // ========== QUERY OPERATIONS ==========
    
    Point3D getBoundingBoxMin() const noexcept;
    Point3D getBoundingBoxMax() const noexcept;
    
    struct MassProperties {
        double volume{0.0};
        Point3D centroid;
        std::array<double, 6> inertia_tensor;  // Ixx, Iyy, Izz, Ixy, Ixz, Iyz
    };
    
    MassProperties calculateMassProperties(double density = 1.0) const noexcept;
    
    // ========== VALIDATION & SANITIZATION ==========
    
    /**
     * @brief Check if B-Rep is valid (manifold, no degenerates)
     */
    Result<void, ErrorCode> validate() const noexcept;
    
    /**
     * @brief Remove degenerate triangles, zero-length edges, non-manifold vertices
     */
    Result<void, ErrorCode> sanitize() noexcept;
    
    /**
     * @brief Get count of degenerate elements found
     */
    size_t getDegenerateCount() const noexcept {
        return degenerate_count_;
    }
    
    // ========== SERIALIZATION ==========
    
    std::vector<uint8_t> serialize() const noexcept;
    static Result<std::unique_ptr<BRepBody>, ErrorCode> 
    deserialize(const std::vector<uint8_t>& data) noexcept;
    
    // ========== ACCESSORS ==========
    
    const std::vector<Vertex>& getVertices() const noexcept { return vertices_; }
    const std::vector<Edge>& getEdges() const noexcept { return edges_; }
    const std::vector<Face>& getFaces() const noexcept { return faces_; }
    const std::vector<Shell>& getShells() const noexcept { return shells_; }
    
    size_t getVertexCount() const noexcept { return vertices_.size(); }
    size_t getEdgeCount() const noexcept { return edges_.size(); }
    size_t getFaceCount() const noexcept { return faces_.size(); }
    
private:
    TopoID id_;
    std::vector<Vertex> vertices_;
    std::vector<Edge> edges_;
    std::vector<Face> faces_;
    std::vector<Shell> shells_;
    
    size_t degenerate_count_{0};
    mutable TopoID next_topo_id_{1};
    
    // Helper methods
    TopoID allocateVertexId() noexcept { return next_topo_id_++; }
    TopoID allocateEdgeId() noexcept { return next_topo_id_++; }
    TopoID allocateFaceId() noexcept { return next_topo_id_++; }
    TopoID allocateShellId() noexcept { return next_topo_id_++; }
    
    Result<void, ErrorCode> validateManifold() const noexcept;
    Result<void, ErrorCode> validateOrientations() const noexcept;
    void removeDegenerateElements() noexcept;
};

} // namespace cad
