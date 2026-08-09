#include "Geometry/BRepBody.h"
#include <iostream>

namespace Cad {

BRepBody::BRepBody()
    : m_bodyId(0)
    , m_faces()
    , m_edges()
    , m_vertices()
    , m_isValid(false)
    , m_volume(0.0)
    , m_surfaceArea(0.0) {
}

BRepBody::~BRepBody() noexcept = default;

Result<void, ErrorCode> BRepBody::addFace(std::unique_ptr<Face> face) {
    if (!face) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER, "Null face provided");
    }
    
    // Validate face geometry before adding
    Result<void, ErrorCode> validation = GeometrySanitizer::validateFace(*face);
    if (!validation.isSuccess()) {
        return validation;
    }
    
    face->setId(m_faces.size());
    m_faces.push_back(std::move(face));
    m_isValid = false; // Invalidate cached properties
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> BRepBody::addEdge(std::unique_ptr<Edge> edge) {
    if (!edge) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER, "Null edge provided");
    }
    
    edge->setId(m_edges.size());
    m_edges.push_back(std::move(edge));
    
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> BRepBody::addVertex(std::unique_ptr<Vertex> vertex) {
    if (!vertex) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER, "Null vertex provided");
    }
    
    vertex->setId(m_vertices.size());
    m_vertices.push_back(std::move(vertex));
    
    return Result<void, ErrorCode>::success();
}

Result<double, ErrorCode> BRepBody::computeVolume() {
    if (m_isValid && m_volume > 0) {
        return Result<double, ErrorCode>::success(m_volume);
    }
    
    // Compute volume using divergence theorem
    double totalVolume = 0.0;
    
    for (const auto& face : m_faces) {
        double faceVolume = computeFaceVolumeContribution(*face);
        totalVolume += faceVolume;
    }
    
    m_volume = std::abs(totalVolume);
    m_isValid = true;
    
    return Result<double, ErrorCode>::success(m_volume);
}

Result<double, ErrorCode> BRepBody::computeSurfaceArea() {
    if (m_isValid && m_surfaceArea > 0) {
        return Result<double, ErrorCode>::success(m_surfaceArea);
    }
    
    double totalArea = 0.0;
    
    for (const auto& face : m_faces) {
        double faceArea = computeFaceArea(*face);
        totalArea += faceArea;
    }
    
    m_surfaceArea = totalArea;
    m_isValid = true;
    
    return Result<double, ErrorCode>::success(m_surfaceArea);
}

double BRepBody::computeFaceVolumeContribution(const Face& face) const {
    // Simplified volume contribution calculation
    // Real implementation would integrate over the face surface
    return 0.0; // Placeholder
}

double BRepBody::computeFaceArea(const Face& face) const {
    // Simplified area calculation
    // Real implementation would integrate over parametric domain
    return 0.0; // Placeholder
}

Result<BoundingBox, ErrorCode> BRepBody::computeBoundingBox() const {
    if (m_vertices.empty()) {
        return Result<BoundingBox, ErrorCode>::failure(ErrorCode::EMPTY_BODY, 
            "Cannot compute bounding box of empty body");
    }
    
    Vector3 minPt(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    
    for (const auto& vertex : m_vertices) {
        const auto& pos = vertex->getPosition();
        minPt.x() = std::min(minPt.x(), pos.x());
        minPt.y() = std::min(minPt.y(), pos.y());
        minPt.z() = std::min(minPt.z(), pos.z());
        
        maxPt.x() = std::max(maxPt.x(), pos.x());
        maxPt.y() = std::max(maxPt.y(), pos.y());
        maxPt.z() = std::max(maxPt.z(), pos.z());
    }
    
    return Result<BoundingBox, ErrorCode>::success(BoundingBox{minPt, maxPt});
}

Result<void, ErrorCode> BRepBody::transform(const Matrix4x4& transformation) {
    // Apply transformation to all vertices
    for (auto& vertex : m_vertices) {
        Vector3 newPos = transformation * vertex->getPosition();
        vertex->setPosition(newPos);
    }
    
    m_isValid = false; // Invalidate cached properties
    
    return Result<void, ErrorCode>::success();
}

Result<std::vector<Vector3>, ErrorCode> BRepBody::samplePoints(size_t count) const {
    // Return sample points on the surface for visualization
    std::vector<Vector3> points;
    points.reserve(count);
    
    // Placeholder implementation
    for (size_t i = 0; i < count && i < m_vertices.size(); ++i) {
        points.push_back(m_vertices[i]->getPosition());
    }
    
    return Result<std::vector<Vector3>, ErrorCode>::success(points);
}

bool BRepBody::isValid() const {
    return m_isValid;
}

size_t BRepBody::getFaceCount() const {
    return m_faces.size();
}

size_t BRepBody::getEdgeCount() const {
    return m_edges.size();
}

size_t BRepBody::getVertexCount() const {
    return m_vertices.size();
}

uint64_t BRepBody::getBodyId() const {
    return m_bodyId;
}

void BRepBody::setBodyId(uint64_t id) {
    m_bodyId = id;
}

// ============================================================================
// Face, Edge, Vertex Implementations
// ============================================================================

Face::Face()
    : m_id(0)
    , m_surface(nullptr)
    , m_loops()
    , m_normal() {
}

Face::~Face() noexcept = default;

Edge::Edge()
    : m_id(0)
    , m_curve(nullptr)
    , m_startVertex(nullptr)
    , m_endVertex(nullptr)
    , m_paramRange{0.0, 1.0} {
}

Edge::~Edge() noexcept = default;

Vertex::Vertex()
    : m_id(0)
    , m_position(0.0, 0.0, 0.0) {
}

Vertex::~Vertex() noexcept = default;

void Vertex::setPosition(const Vector3& pos) {
    m_position = pos;
}

const Vector3& Vertex::getPosition() const {
    return m_position;
}

void Vertex::setId(size_t id) {
    m_id = id;
}

size_t Vertex::getId() const {
    return m_id;
}

void Edge::setId(size_t id) {
    m_id = id;
}

size_t Edge::getId() const {
    return m_id;
}

void Face::setId(size_t id) {
    m_id = id;
}

size_t Face::getId() const {
    return m_id;
}

} // namespace Cad
