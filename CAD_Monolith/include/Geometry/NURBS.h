/**
 * @file NURBS.h
 * @brief NURBS curve and surface representation with tolerance guards
 * @author Dr. Elias Voss
 * 
 * MANDATE: All NURBS operations must validate knot vectors and control points.
 * Prevent division by zero in basis function evaluation.
 */

#pragma once

#include "Core/Result.h"
#include "Core/MemoryGuard.h"
#include "Geometry/BRepBody.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace cad {

/**
 * @brief NURBS basis function evaluator with numerical stability guards
 */
class NURBSBasis {
public:
    /**
     * @brief Cox-de Boor recursion formula with epsilon protection
     */
    static double evaluateBasis(int i, int p, const std::vector<double>& knot_vector, 
                                double u) noexcept;
    
    /**
     * @brief Evaluate all non-zero basis functions at parameter u
     */
    static std::vector<double> evaluateAllBasis(int p, const std::vector<double>& knot_vector,
                                                 double u) noexcept;
    
    /**
     * @brief Compute derivative of basis function
     */
    static double evaluateDerivative(int i, int p, const std::vector<double>& knot_vector,
                                     double u, int deriv_order = 1) noexcept;
    
private:
    static constexpr double EPSILON = 1e-12;
    
    static int findSpan(int p, double u, const std::vector<double>& knot_vector) noexcept;
};

/**
 * @brief NURBS Curve representation
 * 
 * A NURBS curve is defined by:
 * - Degree p
 * - Control points P_i (with weights w_i)
 * - Knot vector U = {u_0, u_1, ..., u_m}
 * 
 * C(u) = sum(N_i,p(u) * w_i * P_i) / sum(N_i,p(u) * w_i)
 */
class NURBSCurve {
public:
    /**
     * @brief Create NURBS curve from degree, control points, weights, and knot vector
     */
    static Result<std::unique_ptr<NURBSCurve>, ErrorCode>
    create(int degree, 
           const std::vector<Point3D>& control_points,
           const std::vector<double>& weights,
           const std::vector<double>& knot_vector) noexcept;
    
    /**
     * @brief Create interpolating curve through given points
     */
    static Result<std::unique_ptr<NURBSCurve>, ErrorCode>
    interpolateThroughPoints(const std::vector<Point3D>& points, 
                             int degree = 3) noexcept;
    
    /**
     * @brief Create Bezier curve (special case: single segment, no internal knots)
     */
    static Result<std::unique_ptr<NURBSCurve>, ErrorCode>
    createBezier(const std::vector<Point3D>& control_points) noexcept;
    
    explicit NURBSCurve(uint64_t id) noexcept : id_(id) {}
    
    uint64_t getId() const noexcept { return id_; }
    
    // ========== EVALUATION ==========
    
    /**
     * @brief Evaluate curve at parameter u in [0, 1]
     */
    Point3D evaluate(double u) const noexcept;
    
    /**
     * @brief Evaluate curve and its derivatives at parameter u
     */
    struct CurveEval {
        Point3D point;
        Point3D first_derivative;   // Tangent (not normalized)
        Point3D second_derivative;  // Curvature-related
        double curvature;           // |C' x C''| / |C'|^3
        double torsion;             // For 3D curves
    };
    
    CurveEval evaluateWithDerivatives(double u, int max_deriv_order = 2) const noexcept;
    
    /**
     * @brief Get point on curve (convenience method)
     */
    Point3D pointOnCurve(double u) const noexcept {
        return evaluate(u);
    }
    
    /**
     * @brief Get tangent vector at parameter u (normalized)
     */
    Point3D tangent(double u) const noexcept;
    
    /**
     * @brief Find parameter u for closest point to given position
     */
    Result<double, ErrorCode> findClosestParameter(const Point3D& point,
                                                    double initial_guess = 0.5,
                                                    int max_iterations = 20) const noexcept;
    
    /**
     * @brief Project point onto curve, returns (u, distance)
     */
    Result<std::pair<double, double>, ErrorCode> 
    projectPoint(const Point3D& point) const noexcept;
    
    // ========== MODIFICATION ==========
    
    /**
     * @brief Insert knot at parameter u
     */
    Result<void, ErrorCode> insertKnot(double u, int multiplicity = 1) noexcept;
    
    /**
     * @brief Remove knot if possible without changing shape
     */
    Result<void, ErrorCode> removeKnot(double u, int target_multiplicity = 0) noexcept;
    
    /**
     * @brief Elevate degree by 1
     */
    Result<void, ErrorCode> elevateDegree() noexcept;
    
    /**
     * @brief Refine curve by inserting multiple knots
     */
    Result<void, ErrorCode> refine(const std::vector<double>& parameters) noexcept;
    
    /**
     * @brief Reduce control points while maintaining tolerance
     */
    Result<void, ErrorCode> reduceControlPoints(double tolerance) noexcept;
    
    /**
     * @brief Split curve at parameter u into two curves
     */
    Result<std::pair<std::unique_ptr<NURBSCurve>, std::unique_ptr<NURBSCurve>>, ErrorCode>
    split(double u) const noexcept;
    
    // ========== ACCESSORS ==========
    
    int getDegree() const noexcept { return degree_; }
    size_t getControlPointCount() const noexcept { return control_points_.size(); }
    size_t getKnotCount() const noexcept { return knot_vector_.size(); }
    
    const std::vector<Point3D>& getControlPoints() const noexcept { return control_points_; }
    const std::vector<double>& getWeights() const noexcept { return weights_; }
    const std::vector<double>& getKnotVector() const noexcept { return knot_vector_; }
    
    double getStartParam() const noexcept { return knot_vector_[degree_]; }
    double getEndParam() const noexcept { return knot_vector_[knot_vector_.size() - degree_ - 1]; }
    
    // ========== VALIDATION ==========
    
    Result<void, ErrorCode> validate() const noexcept;
    
    bool isValid() const noexcept {
        return validate().isSuccess();
    }
    
    // ========== SERIALIZATION ==========
    
    std::vector<uint8_t> serialize() const noexcept;
    static Result<std::unique_ptr<NURBSCurve>, ErrorCode>
    deserialize(const std::vector<uint8_t>& data) noexcept;
    
    /**
     * @brief Convert to discrete polyline for rendering
     */
    std::vector<Point3D> tessellate(int num_segments = 32) const noexcept;
    
private:
    NURBSCurve() noexcept = default;
    
    Result<void, ErrorCode> validateKnotVector() const noexcept;
    Result<void, ErrorCode> validateControlPoints() const noexcept;
    void normalizeWeights() noexcept;
    
    uint64_t id_{0};
    int degree_{3};
    std::vector<Point3D> control_points_;
    std::vector<double> weights_;
    std::vector<double> knot_vector_;
    
    mutable uint64_t next_sub_id_{1};
};

/**
 * @brief NURBS Surface representation
 * 
 * A NURBS surface is defined by:
 * - Degrees (p, q) in U and V directions
 * - Control net P_ij (with weights w_ij)
 * - Knot vectors U and V
 * 
 * S(u,v) = sum(sum(N_i,p(u) * N_j,q(v) * w_ij * P_ij)) / sum(sum(N_i,p(u) * N_j,q(v) * w_ij))
 */
class NURBSSurface {
public:
    /**
     * @brief Create NURBS surface from degrees, control net, weights, and knot vectors
     */
    static Result<std::unique_ptr<NURBSSurface>, ErrorCode>
    create(int u_degree, int v_degree,
           const std::vector<std::vector<Point3D>>& control_net,
           const std::vector<std::vector<double>>& weights,
           const std::vector<double>& u_knot_vector,
           const std::vector<double>& v_knot_vector) noexcept;
    
    /**
     * @brief Create surface by sweeping curve along path
     */
    static Result<std::unique_ptr<NURBSSurface>, ErrorCode>
    createSweepSurface(const NURBSCurve& profile, const NURBSCurve& path) noexcept;
    
    /**
     * @brief Create surface by lofting through profiles
     */
    static Result<std::unique_ptr<NURBSSurface>, ErrorCode>
    createLoftSurface(const std::vector<NURBSCurve*>& profiles) noexcept;
    
    /**
     * @brief Create surface by revolving curve around axis
     */
    static Result<std::unique_ptr<NURBSSurface>, ErrorCode>
    createRevolvedSurface(const NURBSCurve& profile, 
                          const Point3D& axis_origin,
                          const Point3D& axis_direction,
                          double angle_degrees = 360.0) noexcept;
    
    explicit NURBSSurface(uint64_t id) noexcept : id_(id) {}
    
    uint64_t getId() const noexcept { return id_; }
    
    // ========== EVALUATION ==========
    
    /**
     * @brief Evaluate surface at parameters (u, v)
     */
    Point3D evaluate(double u, double v) const noexcept;
    
    /**
     * @brief Evaluate surface and its derivatives
     */
    struct SurfaceEval {
        Point3D point;
        Point3D du;           // Partial derivative w.r.t u
        Point3D dv;           // Partial derivative w.r.t v
        Point3D normal;       // Normalized (du x dv)
        Point3D duu;          // Second partial w.r.t u
        Point3D dvv;          // Second partial w.r.t v
        Point3D duv;          // Mixed partial
        double gaussian_curvature;
        double mean_curvature;
    };
    
    SurfaceEval evaluateWithDerivatives(double u, double v) const noexcept;
    
    /**
     * @brief Get normal vector at (u, v)
     */
    Point3D normal(double u, double v) const noexcept;
    
    /**
     * @brief Find parameters (u, v) for closest point to given position
     */
    Result<std::pair<double, double>, ErrorCode>
    findClosestParameters(const Point3D& point,
                          double initial_u = 0.5, double initial_v = 0.5,
                          int max_iterations = 20) const noexcept;
    
    // ========== ACCESSORS ==========
    
    int getUDegree() const noexcept { return u_degree_; }
    int getVDegree() const noexcept { return v_degree_; }
    
    size_t getUControlPointCount() const noexcept { return control_net_.size(); }
    size_t getVControlPointCount() const noexcept { 
        return control_net_.empty() ? 0 : control_net_[0].size(); 
    }
    
    const std::vector<std::vector<Point3D>>& getControlNet() const noexcept { return control_net_; }
    const std::vector<double>& getUKnotVector() const noexcept { return u_knot_vector_; }
    const std::vector<double>& getVKnotVector() const noexcept { return v_knot_vector_; }
    
    // ========== TESSELLATION ==========
    
    /**
     * @brief Generate mesh for rendering
     */
    struct MeshData {
        std::vector<Point3D> vertices;
        std::vector<uint32_t> indices;  // Triangle list
        std::vector<Point3D> normals;
    };
    
    MeshData tessellate(int u_segments = 20, int v_segments = 20) const noexcept;
    
    // ========== VALIDATION ==========
    
    Result<void, ErrorCode> validate() const noexcept;
    
    // ========== SERIALIZATION ==========
    
    std::vector<uint8_t> serialize() const noexcept;
    static Result<std::unique_ptr<NURBSSurface>, ErrorCode>
    deserialize(const std::vector<uint8_t>& data) noexcept;
    
private:
    NURBSSurface() noexcept = default;
    
    uint64_t id_{0};
    int u_degree_{3};
    int v_degree_{3};
    std::vector<std::vector<Point3D>> control_net_;
    std::vector<std::vector<double>> weights_;
    std::vector<double> u_knot_vector_;
    std::vector<double> v_knot_vector_;
};

} // namespace cad
