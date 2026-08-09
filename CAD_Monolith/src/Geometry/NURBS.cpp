#include "Geometry/NURBS.h"
#include <iostream>
#include <cmath>

namespace Cad {

// ============================================================================
// NURBSCurve Implementation
// ============================================================================

NURBSCurve::NURBSCurve()
    : m_degree(3)
    , m_controlPoints()
    , m_knotVector()
    , m_weights()
    , m_isValid(false) {
}

NURBSCurve::~NURBSCurve() noexcept = default;

Result<void, ErrorCode> NURBSCurve::initialize(size_t controlPointCount, size_t degree) {
    if (controlPointCount < 2) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER,
            "Need at least 2 control points");
    }
    
    if (degree > controlPointCount - 1) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER,
            "Degree cannot exceed control point count - 1");
    }
    
    m_degree = degree;
    m_controlPoints.resize(controlPointCount);
    m_weights.resize(controlPointCount, 1.0);
    
    // Initialize uniform knot vector
    size_t knotCount = controlPointCount + degree + 1;
    m_knotVector.resize(knotCount);
    
    for (size_t i = 0; i < knotCount; ++i) {
        if (i <= degree) {
            m_knotVector[i] = 0.0;
        } else if (i >= controlPointCount) {
            m_knotVector[i] = 1.0;
        } else {
            m_knotVector[i] = static_cast<double>(i - degree) / 
                             static_cast<double>(controlPointCount - degree);
        }
    }
    
    m_isValid = true;
    return Result<void, ErrorCode>::success();
}

Result<Vector3, ErrorCode> NURBSCurve::evaluate(double u) const {
    if (!m_isValid) {
        return Result<Vector3, ErrorCode>::failure(ErrorCode::INVALID_CURVE,
            "Curve not properly initialized");
    }
    
    if (u < m_knotVector.front() || u > m_knotVector.back()) {
        return Result<Vector3, ErrorCode>::failure(ErrorCode::PARAMETER_OUT_OF_RANGE,
            "Parameter u out of valid range");
    }
    
    // Cox-de Boor recursion formula
    size_t span = findSpan(u);
    
    std::vector<double> basisFunctions = computeBasisFunctions(u, span);
    
    Vector3 result(0.0, 0.0, 0.0);
    double weightSum = 0.0;
    
    for (size_t i = 0; i <= m_degree; ++i) {
        size_t idx = span - m_degree + i;
        double weight = m_weights[idx] * basisFunctions[i];
        result += m_controlPoints[idx] * weight;
        weightSum += weight;
    }
    
    if (std::abs(weightSum) < GEOMETRY_EPSILON) {
        return Result<Vector3, ErrorCode>::failure(ErrorCode::DEGENERATE_CURVE,
            "Curve evaluation resulted in zero weight sum");
    }
    
    result /= weightSum;
    
    return Result<Vector3, ErrorCode>::success(result);
}

Result<Vector3, ErrorCode> NURBSCurve::evaluateDerivative(double u, size_t derivativeOrder) const {
    if (derivativeOrder == 0) {
        return evaluate(u);
    }
    
    // Placeholder for derivative computation
    // Real implementation would use algorithm A2.3 from The NURBS Book
    return Result<Vector3, ErrorCode>::success(Vector3(0.0, 0.0, 0.0));
}

size_t NURBSCurve::findSpan(double u) const {
    if (u >= m_knotVector.back()) {
        return m_knotVector.size() - m_degree - 2;
    }
    
    size_t low = m_degree;
    size_t high = m_knotVector.size() - 1;
    size_t mid = (low + high) / 2;
    
    while (u < m_knotVector[mid] || u >= m_knotVector[mid + 1]) {
        if (u < m_knotVector[mid]) {
            high = mid;
        } else {
            low = mid;
        }
        mid = (low + high) / 2;
    }
    
    return mid;
}

std::vector<double> NURBSCurve::computeBasisFunctions(double u, size_t span) const {
    std::vector<double> basis(m_degree + 1, 0.0);
    std::vector<std::vector<double>> left(m_degree + 1, std::vector<double>(m_degree + 1));
    std::vector<std::vector<double>> right(m_degree + 1, std::vector<double>(m_degree + 1));
    
    basis[0] = 1.0;
    
    for (size_t j = 1; j <= m_degree; ++j) {
        left[j] = std::vector<double>(m_degree + 1);
        right[j] = std::vector<double>(m_degree + 1);
        
        double saved = 0.0;
        
        for (size_t r = 0; r < j; ++r) {
            double tempL = m_knotVector[span + 1 + r] - m_knotVector[span - j + 1 + r];
            double tempR = m_knotVector[span + 1] - m_knotVector[span + 1 - j + r];
            
            left[j][r] = (tempL == 0.0) ? 0.0 : (u - m_knotVector[span - j + 1 + r]) / tempL;
            right[j][r] = (tempR == 0.0) ? 0.0 : (m_knotVector[span + 1 + r] - u) / tempR;
            
            double temp = basis[r] * right[j][r];
            basis[r] = saved + basis[r] * left[j][r];
            saved = temp;
        }
        
        basis[j] = saved;
    }
    
    return basis;
}

Result<void, ErrorCode> NURBSCurve::setControlPoint(size_t index, const Vector3& point) {
    if (index >= m_controlPoints.size()) {
        return Result<void, ErrorCode>::failure(ErrorCode::INDEX_OUT_OF_RANGE,
            "Control point index out of range");
    }
    
    m_controlPoints[index] = point;
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> NURBSCurve::setWeight(size_t index, double weight) {
    if (index >= m_weights.size()) {
        return Result<void, ErrorCode>::failure(ErrorCode::INDEX_OUT_OF_RANGE,
            "Weight index out of range");
    }
    
    if (weight <= GEOMETRY_EPSILON) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_WEIGHT,
            "Weight must be positive");
    }
    
    m_weights[index] = weight;
    return Result<void, ErrorCode>::success();
}

Result<void, ErrorCode> NURBSCurve::insertKnot(double u, size_t multiplicity) {
    // Placeholder for knot insertion algorithm
    // Real implementation would use Algorithm 5.1 from The NURBS Book
    return Result<void, ErrorCode>::success();
}

bool NURBSCurve::isValid() const {
    return m_isValid;
}

size_t NURBSCurve::getDegree() const {
    return m_degree;
}

size_t NURBSCurve::getControlPointCount() const {
    return m_controlPoints.size();
}

const std::vector<Vector3>& NURBSCurve::getControlPoints() const {
    return m_controlPoints;
}

const std::vector<double>& NURBSCurve::getKnotVector() const {
    return m_knotVector;
}

// ============================================================================
// NURBSSurface Implementation
// ============================================================================

NURBSSurface::NURBSSurface()
    : m_degreeU(3)
    , m_degreeV(3)
    , m_controlPoints()
    , m_knotVectorU()
    , m_knotVectorV()
    , m_weights()
    , m_isValid(false) {
}

NURBSSurface::~NURBSSurface() noexcept = default;

Result<void, ErrorCode> NURBSSurface::initialize(size_t controlPointCountU, 
                                                  size_t controlPointCountV,
                                                  size_t degreeU,
                                                  size_t degreeV) {
    if (controlPointCountU < 2 || controlPointCountV < 2) {
        return Result<void, ErrorCode>::failure(ErrorCode::INVALID_PARAMETER,
            "Need at least 2x2 control points");
    }
    
    m_degreeU = degreeU;
    m_degreeV = degreeV;
    
    size_t totalPoints = controlPointCountU * controlPointCountV;
    m_controlPoints.resize(totalPoints);
    m_weights.resize(totalPoints, 1.0);
    
    // Initialize knot vectors
    size_t knotCountU = controlPointCountU + degreeU + 1;
    size_t knotCountV = controlPointCountV + degreeV + 1;
    
    m_knotVectorU.resize(knotCountU);
    m_knotVectorV.resize(knotCountV);
    
    // Uniform knot initialization (similar to curve)
    for (size_t i = 0; i < knotCountU; ++i) {
        if (i <= degreeU) {
            m_knotVectorU[i] = 0.0;
        } else if (i >= controlPointCountU) {
            m_knotVectorU[i] = 1.0;
        } else {
            m_knotVectorU[i] = static_cast<double>(i - degreeU) / 
                              static_cast<double>(controlPointCountU - degreeU);
        }
    }
    
    for (size_t i = 0; i < knotCountV; ++i) {
        if (i <= degreeV) {
            m_knotVectorV[i] = 0.0;
        } else if (i >= controlPointCountV) {
            m_knotVectorV[i] = 1.0;
        } else {
            m_knotVectorV[i] = static_cast<double>(i - degreeV) / 
                              static_cast<double>(controlPointCountV - degreeV);
        }
    }
    
    m_isValid = true;
    return Result<void, ErrorCode>::success();
}

Result<Vector3, ErrorCode> NURBSSurface::evaluate(double u, double v) const {
    if (!m_isValid) {
        return Result<Vector3, ErrorCode>::failure(ErrorCode::INVALID_SURFACE,
            "Surface not properly initialized");
    }
    
    // Placeholder surface evaluation
    // Real implementation would extend Cox-de Boor to 2D
    return Result<Vector3, ErrorCode>::success(Vector3(u, v, 0.0));
}

bool NURBSSurface::isValid() const {
    return m_isValid;
}

} // namespace Cad
