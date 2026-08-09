/**
 * @file ConstraintGraph.h
 * @brief Parametric constraint solver with DAG dependency tracking
 * @author Dr. Elias Voss
 * 
 * MANDATE: Detect cyclic dependencies before solving.
 * Support incremental updates when single constraint changes.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include "Geometry/BRepBody.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <set>

namespace cad {

/**
 * @brief Constraint types for 2D sketching
 */
enum class ConstraintType : uint8_t {
    // Geometric constraints
    COINCIDENT = 1,      // Two points coincide
    PARALLEL = 2,        // Two lines parallel
    PERPENDICULAR = 3,   // Two lines perpendicular
    TANGENT = 4,         // Curve tangent to line/curve
    CONCENTRIC = 5,      // Two arcs/circles share center
    SYMMETRIC = 6,       // Points symmetric about line
    EQUAL_LENGTH = 7,    // Two lines equal length
    EQUAL_RADIUS = 8,    // Two arcs/circles equal radius
    MIDPOINT = 9,        // Point at line midpoint
    HORIZONTAL = 10,     // Line horizontal
    VERTICAL = 11,       // Line vertical
    FIXED = 12,          // Point/line fixed in space
    
    // Dimensional constraints
    DISTANCE = 100,      // Distance between points/lines
    ANGLE = 101,         // Angle between lines
    RADIUS = 102,        // Radius of arc/circle
    DIAMETER = 103,      // Diameter of circle
    LENGTH = 104         // Length of line
};

/**
 * @brief Constraint value (dimension or geometric parameter)
 */
struct ConstraintValue {
    double value{0.0};
    double tolerance{1e-6};
    bool is_driving{true};  // If false, constraint is reference only
    
    ConstraintValue() = default;
    explicit ConstraintValue(double v, double tol = 1e-6, bool driving = true)
        : value(v), tolerance(tol), is_driving(driving) {}
};

/**
 * @brief Entity being constrained (point, line, arc, etc.)
 */
struct ConstrainedEntity {
    uint64_t id;
    std::string name;
    
    enum class Type { POINT, LINE, CIRCLE, ARC, SPLINE } entity_type;
    
    // For points
    Point3D point;
    
    // For lines
    Point3D start_point;
    Point3D end_point;
    
    // For circles/arcs
    Point3D center;
    double radius{0.0};
    double start_angle{0.0};
    double end_angle{0.0};
    
    // Degrees of freedom count
    int dof{2};  // Points have 2 DOF in 2D
    
    bool is_fully_constrained() const noexcept {
        return dof <= 0;
    }
};

/**
 * @brief Single constraint in the graph
 */
class Constraint {
public:
    using EntityId = uint64_t;
    
    Constraint(uint64_t id, ConstraintType type, 
               const std::vector<EntityId>& entities,
               const ConstraintValue& value = ConstraintValue()) noexcept
        : id_(id), type_(type), entities_(entities), value_(value) {}
    
    uint64_t getId() const noexcept { return id_; }
    ConstraintType getType() const noexcept { return type_; }
    const std::vector<EntityId>& getEntities() const noexcept { return entities_; }
    const ConstraintValue& getValue() const noexcept { return value_; }
    
    void setValue(double new_value) noexcept {
        value_.value = new_value;
    }
    
    void setDriving(bool driving) noexcept {
        value_.is_driving = driving;
    }
    
    /**
     * @brief Calculate residual (how much constraint is violated)
     */
    double calculateResidual(const std::unordered_map<EntityId, ConstrainedEntity>& entities) const noexcept;
    
    /**
     * @brief Calculate Jacobian row for this constraint
     */
    std::vector<double> calculateJacobianRow(
        const std::unordered_map<EntityId, ConstrainedEntity>& entities) const noexcept;
    
private:
    uint64_t id_{0};
    ConstraintType type_;
    std::vector<EntityId> entities_;
    ConstraintValue value_;
};

/**
 * @brief Directed Acyclic Graph for dependency tracking
 * 
 * Used to:
 * - Detect cycles before adding constraints
 * - Determine solve order
 * - Enable incremental updates
 */
class DependencyGraph {
public:
    using NodeId = uint64_t;
    
    /**
     * @brief Add node to graph
     */
    void addNode(NodeId id) noexcept;
    
    /**
     * @brief Add directed edge (from -> to means 'from' depends on 'to')
     * @return Error if cycle would be created
     */
    Result<void, ErrorCode> addEdge(NodeId from, NodeId to) noexcept;
    
    /**
     * @brief Remove edge
     */
    void removeEdge(NodeId from, NodeId to) noexcept;
    
    /**
     * @brief Check if adding edge would create cycle
     */
    bool wouldCreateCycle(NodeId from, NodeId to) const noexcept;
    
    /**
     * @brief Get all nodes that depend on given node (for incremental update)
     */
    std::vector<NodeId> getDependents(NodeId node) const noexcept;
    
    /**
     * @brief Get topological sort order for solving
     */
    std::vector<NodeId> getTopologicalOrder() const noexcept;
    
    /**
     * @brief Check if graph has any cycles
     */
    bool hasCycle() const noexcept;
    
    /**
     * @brief Clear graph
     */
    void clear() noexcept;
    
private:
    bool hasCycleDFS(NodeId node, std::vector<bool>& visited, 
                     std::vector<bool>& rec_stack) const noexcept;
    
    std::unordered_map<NodeId, std::vector<NodeId>> adjacency_list_;
    std::set<NodeId> all_nodes_;
};

/**
 * @brief Main constraint solver using Newton-Raphson with damping
 */
class ConstraintSolver {
public:
    struct Config {
        int max_iterations{50};
        double convergence_tolerance{1e-8};
        double damping_factor{0.5};  // For Newton-Raphson stability
        bool enable_incremental_solve{true};
    };
    
    explicit ConstraintSolver(Config config = Config()) noexcept;
    
    /**
     * @brief Add entity to solver
     */
    Result<uint64_t, ErrorCode> addEntity(ConstrainedEntity entity) noexcept;
    
    /**
     * @brief Add constraint between entities
     */
    Result<uint64_t, ErrorCode> addConstraint(ConstraintType type,
                                               const std::vector<uint64_t>& entity_ids,
                                               const ConstraintValue& value = ConstraintValue()) noexcept;
    
    /**
     * @brief Remove constraint
     */
    Result<void, ErrorCode> removeConstraint(uint64_t constraint_id) noexcept;
    
    /**
     * @brief Update constraint value (triggers incremental solve)
     */
    Result<void, ErrorCode> updateConstraintValue(uint64_t constraint_id, 
                                                   double new_value) noexcept;
    
    /**
     * @brief Solve all constraints
     * @return Number of iterations taken, or error if unsolvable
     */
    Result<int, ErrorCode> solve() noexcept;
    
    /**
     * @brief Get current DOF count
     */
    int getRemainingDOF() const noexcept;
    
    /**
     * @brief Check if sketch is fully constrained
     */
    bool isFullyConstrained() const noexcept {
        return getRemainingDOF() <= 0;
    }
    
    /**
     * @brief Check if sketch is over-constrained
     */
    bool isOverConstrained() const noexcept {
        return over_constrained_;
    }
    
    /**
     * @brief Get conflicting constraints (if over-constrained)
     */
    std::vector<uint64_t> getConflictingConstraints() const noexcept {
        return conflicting_constraints_;
    }
    
    /**
     * @brief Get solved entities
     */
    const std::unordered_map<uint64_t, ConstrainedEntity>& getEntities() const noexcept {
        return entities_;
    }
    
    /**
     * @brief Get entity by ID
     */
    const ConstrainedEntity* getEntity(uint64_t id) const noexcept;
    
    /**
     * @brief Get dependency graph for analysis
     */
    const DependencyGraph& getDependencyGraph() const noexcept {
        return dependency_graph_;
    }
    
    /**
     * @brief Clear all entities and constraints
     */
    void clear() noexcept;
    
private:
    Result<void, ErrorCode> detectCycles() const noexcept;
    Result<void, ErrorCode> buildJacobian(std::vector<std::vector<double>>& J) const noexcept;
    Result<void, ErrorCode> buildResidualVector(std::vector<double>& r) const noexcept;
    Result<void, ErrorCode> solveLinearSystem(std::vector<std::vector<double>>& J,
                                               std::vector<double>& delta) noexcept;
    void updateEntities(const std::vector<double>& delta) noexcept;
    
    Config config_;
    std::unordered_map<uint64_t, ConstrainedEntity> entities_;
    std::unordered_map<uint64_t, Constraint> constraints_;
    DependencyGraph dependency_graph_;
    
    uint64_t next_entity_id_{1};
    uint64_t next_constraint_id_{1};
    
    bool over_constrained_{false};
    std::vector<uint64_t> conflicting_constraints_;
    
    int total_dof_{0};
};

/**
 * @brief High-level constraint graph for parametric modeling
 * 
 * Manages feature dependencies:
 * - Sketch -> Extrusion -> Fillet -> Pattern
 * - Changing sketch updates all dependent features
 */
class ConstraintGraph {
public:
    using FeatureId = uint64_t;
    
    ConstraintGraph() noexcept = default;
    
    /**
     * @brief Add feature with dependencies
     */
    Result<FeatureId, ErrorCode> addFeature(const std::string& name,
                                             const std::vector<FeatureId>& dependencies) noexcept;
    
    /**
     * @brief Suppress feature (and all children)
     */
    Result<void, ErrorCode> suppressFeature(FeatureId id) noexcept;
    
    /**
     * @brief Unsuppress feature
     */
    Result<void, ErrorCode> unsuppressFeature(FeatureId id) noexcept;
    
    /**
     * @brief Rebuild feature and all dependents
     */
    Result<void, ErrorCode> rebuildFeature(FeatureId id) noexcept;
    
    /**
     * @brief Get rebuild order (topological sort)
     */
    std::vector<FeatureId> getRebuildOrder() const noexcept;
    
    /**
     * @brief Check if feature is suppressed
     */
    bool isSuppressed(FeatureId id) const noexcept;
    
    /**
     * @brief Get all children of feature
     */
    std::vector<FeatureId> getChildren(FeatureId id) const noexcept;
    
    /**
     * @brief Get all parents of feature
     */
    std::vector<FeatureId> getParents(FeatureId id) const noexcept;
    
private:
    struct Feature {
        FeatureId id;
        std::string name;
        std::vector<FeatureId> dependencies;
        bool suppressed{false};
    };
    
    std::unordered_map<FeatureId, Feature> features_;
    DependencyGraph dependency_graph_;
    uint64_t next_feature_id_{1};
};

} // namespace cad
