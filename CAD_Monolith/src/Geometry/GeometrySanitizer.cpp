#include "Geometry/GeometrySanitizer.h"
#include <iostream>
#include <algorithm>

namespace Cad {

// Static epsilon value for geometric comparisons
constexpr double GEOMETRY_EPSILON = 1e-9;

Result<void, ErrorCode> GeometrySanitizer::validateFace(const Face& face) {
    // Check for degenerate geometry
    
    // 1. Verify face has valid loops
    // (Placeholder - real implementation would check loop connectivity)
    
    // 2. Check surface normal is well-defined
    // (Placeholder - would evaluate surface at sample points)
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> GeometrySanitizer::validateEdge(const Edge& edge) {
    // Check for zero-length edges
    
    if (!edge.m_curve) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_GEOMETRY,
            "Edge has null curve");
    }
    
    // Check if start and end vertices are coincident (zero-length)
    if (edge.m_startVertex && edge.m_endVertex) {
        double distSq = (edge.m_startVertex->m_position - edge.m_endVertex->m_position).squaredNorm();
        if (distSq < GEOMETRY_EPSILON * GEOMETRY_EPSILON) {
            return Result<void, ErrorCode>::failure(ErrorCode::DEGENERATE_EDGE,
                "Edge has zero length");
        }
    }
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> GeometrySanitizer::removeDegenerateTriangles(std::vector<Triangle>& triangles) {
    size_t originalCount = triangles.size();
    
    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(),
            [](const Triangle& tri) {
                // Check for zero-area triangles
                Vector3 edge1 = tri.v1 - tri.v0;
                Vector3 edge2 = tri.v2 - tri.v0;
                Vector3 cross = edge1.cross(edge2);
                
                double areaSquared = cross.squaredNorm();
                return areaSquared < GEOMETRY_EPSILON * GEOMETRY_EPSILON;
            }),
        triangles.end()
    );
    
    size_t removedCount = originalCount - triangles.size();
    if (removedCount > 0) {
        std::cout << "[SANITIZER] Removed " << removedCount << " degenerate triangles\n";
    }
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> GeometrySanitizer::removeZeroLengthEdges(BRepBody& body) {
    // Mark edges for removal
    std::vector<bool> keepEdge(body.getEdgeCount(), true);
    
    // Would iterate through edges and mark zero-length ones
    // Real implementation would rebuild topology after removal
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> GeometrySanitizer::removeNonManifoldVertices(BRepBody& body) {
    // Detect and remove non-manifold vertices
    // A vertex is non-manifold if it connects more than two shells
    
    // Placeholder implementation
    // Real version would use half-edge data structure to detect
    
    return Result<void, ErrorCode>::success();
}

Result<BRepBody, ErrorCode> GeometrySanitizer::sanitizeBody(const BRepBody& input) {
    BRepBody output = input;
    
    // Run all sanitization passes
    auto result = removeDegenerateTrianglesFromBody(output);
    if (!result.isSuccess()) {
        return Result<BRepBody, ErrorCode>::failure(result.getError());
    }
    
    result = removeZeroLengthEdges(output);
    if (!result.isSuccess()) {
        return Result<BRepBody, ErrorCode>::failure(result.getError());
    }
    
    result = removeNonManifoldVertices(output);
    if (!result.isSuccess()) {
        return Result<BRepBody, ErrorCode>::failure(result.getError());
    }
    
    return Result<BRepBody, ErrorCode>::success(output);
}

Result<void, ErrorCode> GeometrySanitizer::removeDegenerateTrianglesFromBody(BRepBody& body) {
    // Convert body faces to triangles and sanitize
    // Placeholder - real implementation would tessellate faces first
    
    return Result<void, ErrorCode>::success();
}

double GeometrySanitizer::computeEdgeLength(const Edge& edge) {
    if (!edge.m_startVertex || !edge.m_endVertex) {
        return 0.0;
    }
    
    return (edge.m_startVertex->m_position - edge.m_endVertex->m_position).norm();
}

double GeometrySanitizer::computeTriangleArea(const Triangle& tri) {
    Vector3 edge1 = tri.v1 - tri.v0;
    Vector3 edge2 = tri.v2 - tri.v0;
    Vector3 cross = edge1.cross(edge2);
    
    return 0.5 * cross.norm();
}

bool GeometrySanitizer::isPointOnLine(const Vector3& point, const Vector3& lineStart, 
                                       const Vector3& lineEnd, double tolerance) {
    Vector3 lineVec = lineEnd - lineStart;
    Vector3 pointVec = point - lineStart;
    
    double lineLenSq = lineVec.squaredNorm();
    if (lineLenSq < GEOMETRY_EPSILON) {
        // Line is effectively a point
        return (point - lineStart).squaredNorm() < tolerance * tolerance;
    }
    
    // Project point onto line
    double t = pointVec.dot(lineVec) / lineLenSq;
    
    // Clamp to segment
    t = std::max(0.0, std::min(1.0, t));
    
    Vector3 closestPoint = lineStart + t * lineVec;
    double distSq = (point - closestPoint).squaredNorm();
    
    return distSq < tolerance * tolerance;
}

bool GeometrySanitizer::arePointsCoincident(const Vector3& p1, const Vector3& p2, 
                                             double tolerance) {
    double distSq = (p1 - p2).squaredNorm();
    return distSq < tolerance * tolerance;
}

} // namespace Cad
