#include "Geometry/ConstraintGraph.h"
#include <iostream>
#include <algorithm>
#include <queue>

namespace Cad {

// ============================================================================
// ConstraintGraph Implementation
// ============================================================================

ConstraintGraph::ConstraintGraph()
    : m_nodes()
    , m_edges()
    , m_constraints()
    , m_isSolved(false)
    , m_iterationCount(0)
    , m_maxIterations(100)
    , m_tolerance(GEOMETRY_EPSILON) {
}

ConstraintGraph::~ConstraintGraph() noexcept = default;

Result<size_t, ErrorCode> ConstraintGraph::addNode(std::unique_ptr<ConstraintNode> node) {
    if (!node) {
        return Result<size_t, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER,
            "Null constraint node provided");
    }
    
    size_t nodeId = m_nodes.size();
    node->setId(nodeId);
    m_nodes.push_back(std::move(node));
    
    m_isSolved = false; // Invalidate solution when graph changes
    
    return Result<size_t, ErrorCode>::success(nodeId);
}

Result<void, ErrorCode> ConstraintGraph::addConstraint(std::unique_ptr<GeometricConstraint> constraint) {
    if (!constraint) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER,
            "Null constraint provided");
    }
    
    // Validate that referenced nodes exist
    auto [nodeA, nodeB] = constraint->getReferencedNodes();
    if (nodeA >= m_nodes.size() || nodeB >= m_nodes.size()) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_REFERENCE,
            "Constraint references non-existent node");
    }
    
    size_t constraintId = m_constraints.size();
    constraint->setId(constraintId);
    
    // Add edge to graph
    m_edges.emplace_back(nodeA, nodeB);
    
    m_constraints.push_back(std::move(constraint));
    m_isSolved = false;
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> ConstraintGraph::solve() {
    if (m_isSolved) {
        return Result<void, ErrorCode>::success(); // Already solved
    }
    
    std::cout << "[CONSTRAINT] Starting solver with " << m_nodes.size() 
              << " nodes and " << m_constraints.size() << " constraints\n";
    
    // Check for over-constrained system
    Result<void, ErrorCode> DOFCheck = checkDegreesOfFreedom();
    if (!DOFCheck.isSuccess()) {
        return DOFCheck;
    }
    
    // Detect cycles in constraint graph
    if (hasCycles()) {
        std::cout << "[CONSTRAINT] Warning: Constraint graph contains cycles\n";
        // Cycles are allowed but may require iterative solving
    }
    
    // Solve using iterative Newton-Raphson method
    m_iterationCount = 0;
    double maxError = m_tolerance * 2.0;
    
    while (maxError > m_tolerance && m_iterationCount < m_maxIterations) {
        maxError = solveIteration();
        m_iterationCount++;
    }
    
    if (maxError > m_tolerance) {
        std::cerr << "[CONSTRAINT] Solver did not converge after " 
                  << m_iterationCount << " iterations. Max error: " << maxError << "\n";
        return Result<void, ErrorCode>::failure(ErrorCode::SOLVER_DID_NOT_CONVERGE,
            "Constraint solver failed to converge");
    }
    
    m_isSolved = true;
    std::cout << "[CONSTRAINT] Solver converged in " << m_iterationCount 
              << " iterations. Max error: " << maxError << "\n";
    
    return Result<void, ErrorCode>::success();
}

double ConstraintGraph::solveIteration() {
    double maxError = 0.0;
    
    // Build Jacobian matrix and residual vector
    // Simplified implementation - real version would use sparse matrices
    
    for (auto& constraint : m_constraints) {
        double error = constraint->evaluateError();
        maxError = std::max(maxError, std::abs(error));
        
        // Apply correction to nodes
        Result<void, ErrorCode> correctionResult = constraint->applyCorrection(m_tolerance);
        if (!correctionResult.isSuccess()) {
            std::cerr << "[CONSTRAINT] Failed to apply correction for constraint " 
                      << constraint->getId() << "\n";
        }
    }
    
    return maxError;
}

Result<void, ErrorCode> ConstraintGraph::checkDegreesOfFreedom() const {
    size_t totalDOF = 0;
    size_t constrainedDOF = 0;
    
    for (const auto& node : m_nodes) {
        totalDOF += node->getDegreesOfFreedom();
    }
    
    for (const auto& constraint : m_constraints) {
        constrainedDOF += constraint->getConstrainedDOF();
    }
    
    if (constrainedDOF > totalDOF) {
        return Result<void, ErrorCode>::failure(ErrorCode::OVER_CONSTRAINED,
            "System is over-constrained: " + std::to_string(constrainedDOF) + 
            " constraints > " + std::to_string(totalDOF) + " DOF");
    }
    
    if (constrainedDOF < totalDOF) {
        std::cout << "[CONSTRAINT] System is under-constrained by " 
                  << (totalDOF - constrainedDOF) << " DOF\n";
        // Under-constrained is OK - just means some DOF are free
    }
    
    return Result<void, ErrorCode>::success();
}

bool ConstraintGraph::hasCycles() const {
    // DFS-based cycle detection
    std::vector<bool> visited(m_nodes.size(), false);
    std::vector<bool> recStack(m_nodes.size(), false);
    
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (!visited[i]) {
            if (hasCyclesDFS(i, visited, recStack)) {
                return true;
            }
        }
    }
    
    return false;
}

bool ConstraintGraph::hasCyclesDFS(size_t node, std::vector<bool>& visited, 
                                    std::vector<bool>& recStack) const {
    visited[node] = true;
    recStack[node] = true;
    
    // Find all neighbors
    for (const auto& edge : m_edges) {
        size_t neighbor = (edge.first == node) ? edge.second : 
                          (edge.second == node) ? edge.first : SIZE_MAX;
        
        if (neighbor != SIZE_MAX) {
            if (!visited[neighbor]) {
                if (hasCyclesDFS(neighbor, visited, recStack)) {
                    return true;
                }
            } else if (recStack[neighbor]) {
                return true;
            }
        }
    }
    
    recStack[node] = false;
    return false;
}

std::vector<size_t> ConstraintGraph::topologicalSort() const {
    // Return nodes in dependency order for solving
    std::vector<size_t> result;
    std::vector<bool> visited(m_nodes.size(), false);
    
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (!visited[i]) {
            topologicalSortDFS(i, visited, result);
        }
    }
    
    std::reverse(result.begin(), result.end());
    return result;
}

void ConstraintGraph::topologicalSortDFS(size_t node, std::vector<bool>& visited,
                                          std::vector<size_t>& result) const {
    visited[node] = true;
    
    for (const auto& edge : m_edges) {
        size_t neighbor = (edge.first == node) ? edge.second : 
                          (edge.second == node) ? edge.first : SIZE_MAX;
        
        if (neighbor != SIZE_MAX && !visited[neighbor]) {
            topologicalSortDFS(neighbor, visited, result);
        }
    }
    
    result.push_back(node);
}

void ConstraintGraph::clear() {
    m_nodes.clear();
    m_edges.clear();
    m_constraints.clear();
    m_isSolved = false;
    m_iterationCount = 0;
}

bool ConstraintGraph::isSolved() const {
    return m_isSolved;
}

size_t ConstraintGraph::getNodeCount() const {
    return m_nodes.size();
}

size_t ConstraintGraph::getConstraintCount() const {
    return m_constraints.size();
}

size_t ConstraintGraph::getIterationCount() const {
    return m_iterationCount;
}

// ============================================================================
// ConstraintNode Implementation
// ============================================================================

ConstraintNode::ConstraintNode(NodeType type)
    : m_id(0)
    , m_type(type)
    , m_position(0.0, 0.0)
    , m_dof(2) // Default: 2 DOF for 2D point (x, y)
    , m_isFixed(false) {
}

ConstraintNode::~ConstraintNode() noexcept = default;

void ConstraintNode::setPosition(const Vector2& pos) {
    m_position = pos;
}

const Vector2& ConstraintNode::getPosition() const {
    return m_position;
}

void ConstraintNode::setId(size_t id) {
    m_id = id;
}

size_t ConstraintNode::getId() const {
    return m_id;
}

size_t ConstraintNode::getDegreesOfFreedom() const {
    return m_isFixed ? 0 : m_dof;
}

void ConstraintNode::setFixed(bool fixed) {
    m_isFixed = fixed;
}

bool ConstraintNode::isFixed() const {
    return m_isFixed;
}

ConstraintNode::NodeType ConstraintNode::getType() const {
    return m_type;
}

// ============================================================================
// GeometricConstraint Implementation
// ============================================================================

GeometricConstraint::GeometricConstraint(ConstraintType type)
    : m_id(0)
    , m_type(type)
    , m_nodeA(SIZE_MAX)
    , m_nodeB(SIZE_MAX)
    , m_value(0.0) {
}

GeometricConstraint::~GeometricConstraint() noexcept = default;

void GeometricConstraint::setId(size_t id) {
    m_id = id;
}

size_t GeometricConstraint::getId() const {
    return m_id;
}

std::pair<size_t, size_t> GeometricConstraint::getReferencedNodes() const {
    return {m_nodeA, m_nodeB};
}

void GeometricConstraint::setNodes(size_t nodeA, size_t nodeB) {
    m_nodeA = nodeA;
    m_nodeB = nodeB;
}

void GeometricConstraint::setValue(double value) {
    m_value = value;
}

double GeometricConstraint::getValue() const {
    return m_value;
}

double GeometricConstraint::evaluateError() const {
    // Placeholder - real implementation depends on constraint type
    return 0.0;
}

size_t GeometricConstraint::getConstrainedDOF() const {
    switch (m_type) {
        case ConstraintType::COINCIDENT: return 2;
        case ConstraintType::DISTANCE: return 1;
        case ConstraintType::HORIZONTAL: return 1;
        case ConstraintType::VERTICAL: return 1;
        case ConstraintType::PARALLEL: return 1;
        case ConstraintType::PERPENDICULAR: return 1;
        case ConstraintType::TANGENT: return 1;
        default: return 0;
    }
}

Result<void, ErrorCode> GeometricConstraint::applyCorrection(double tolerance) {
    // Placeholder - real implementation would compute and apply corrections
    return Result<void, ErrorCode>::success();
}

GeometricConstraint::ConstraintType GeometricConstraint::getType() const {
    return m_type;
}

} // namespace Cad
