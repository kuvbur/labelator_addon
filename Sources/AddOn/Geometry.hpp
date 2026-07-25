//------------ kuvbur 2026 ------------
// Core geometry primitives for LayoutOptimizer (Phase 1)
// NO ACAPI/GSRoot includes allowed — pure C++17, stdlib only.
// See AGENTS.md Section 2 Rule 1, Section 3 (coordinate system), Section 8 (collision detection).

#pragma once

#include <cstdint>
#include <vector>
#include <tuple>
#include <algorithm>
#include <cmath>
#include <array>

namespace LayoutOptimizer {

// -----------------------------------------------------------------------------
// 3. Coordinate System (Paper Space mm, +X right, +Y down, angles clockwise from +X)
// -----------------------------------------------------------------------------
using ElementId = std::uint64_t;

struct Point2D {
    double x = 0.0;  // Paper Space mm, +X = right
    double y = 0.0;  // Paper Space mm, +Y = down (screen/document convention)

    // Required for std::map keys, deterministic ordering
    bool operator<(const Point2D& other) const {
        return std::tie(x, y) < std::tie(other.x, other.y);
    }
    bool operator==(const Point2D& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }
};

inline Point2D operator+(const Point2D& a, const Point2D& b) {
    return {a.x + b.x, a.y + b.y};
}
inline Point2D operator-(const Point2D& a, const Point2D& b) {
    return {a.x - b.x, a.y - b.y};
}
inline Point2D operator*(const Point2D& a, double s) {
    return {a.x * s, a.y * s};
}
inline Point2D operator*(double s, const Point2D& a) {
    return a * s;
}

// -----------------------------------------------------------------------------
// 8.1 Symmetric Epsilon AABB Intersection (eps = 1e-4 mm)
// -----------------------------------------------------------------------------
struct Rect {
    double left   = 0.0;
    double top    = 0.0;
    double right  = 0.0;
    double bottom = 0.0;

    // Ensure left <= right, top <= bottom (Y increases downward)
    void Normalize() {
        if (left > right)   std::swap(left, right);
        if (top > bottom)   std::swap(top, bottom);
    }

    // Section 8.1: symmetric epsilon intersection test
    // eps = 1e-4 mm — touching at epsilon boundary counts as intersecting
    bool Intersects(const Rect& other, double eps = 1e-4) const {
        return !(right  < other.left   - eps ||
                 other.right < left    - eps ||
                 bottom < other.top    - eps ||
                 other.bottom < top    - eps);
    }

    bool Contains(const Point2D& p, double eps = 1e-4) const {
        return p.x >= left - eps && p.x <= right + eps &&
               p.y >= top  - eps && p.y <= bottom + eps;
    }

    double Width()  const { return right - left; }
    double Height() const { return bottom - top; }
    Point2D Center() const { return {(left + right) * 0.5, (top + bottom) * 0.5}; }

    bool operator==(const Rect& other) const {
        return left == other.left && top == other.top &&
               right == other.right && bottom == other.bottom;
    }
};

// -----------------------------------------------------------------------------
// 5. Data Model (Immutable Geometry vs Mutable Layout)
// -----------------------------------------------------------------------------
enum class ElementType {
    Leader,
    Dimension
};

// --- Leader ---
struct LeaderGeometry {          // IMMUTABLE source geometry
    Point2D anchorPoint;         // Leader anchor (arrow head / attachment point)
    double shoulderLength = 0.0; // Shoulder segment length (mm)
};

struct LeaderLayout {            // MUTABLE layout state
    Point2D textPosition;        // Text box position (top-left or center per convention)
    Rect textBox;                // Text bounding box in Paper Space mm
    std::vector<Point2D> leaderPolyline; // Polyline from anchor -> shoulder -> text
    double leaderAngle = 0.0;    // Degrees, clockwise from +X (East), per Section 3
};

struct LeaderElement {
    ElementId id = 0;
    bool locked = false;         // If true: never repositioned; acts as obstacle only
    LeaderGeometry geo;          // Read-only
    LeaderLayout layout;         // Modifiable
};

// --- Dimension ---
struct DimensionGeometry {       // IMMUTABLE
    std::vector<Point2D> measurePoints; // Dimension line points
    Point2D direction;                  // Dimension direction vector (normalized)
};

struct DimensionLayout {         // MUTABLE
    Point2D textPosition;
    Rect textBox;
    double offset = 0.0;         // Offset from measure line
};

struct DimensionElement {
    ElementId id = 0;
    bool locked = false;
    DimensionGeometry geo;
    DimensionLayout layout;
};

// -----------------------------------------------------------------------------
// 6. Candidate Generation (Fixed Grid — Section 6 constants)
// -----------------------------------------------------------------------------
struct Candidate {
    Point2D textPosition;
    Rect textBox;
    std::vector<Point2D> leaderPolyline;
    double offset = 0.0;
    double angle = 0.0;          // Degrees, clockwise from +X
    bool valid = true;           // False after filtering (but fallback is always valid)
};

// Fixed candidate grid — Section 6 constants (DO NOT CHANGE without updating AGENTS.md)
inline constexpr double kCandidateAnglesDeg[8] = {
    0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0
};
inline constexpr double kCandidateDistancesMm[3] = {
    5.0, 10.0, 20.0
};

// -----------------------------------------------------------------------------
// 8.2 Deterministic Segment-vs-Segment Intersection (eps = 1e-4)
// -----------------------------------------------------------------------------
enum class Orientation { Collinear, Clockwise, CounterClockwise };

inline double Cross(const Point2D& o, const Point2D& a, const Point2D& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

inline Orientation GetOrientation(const Point2D& o, const Point2D& a,
                                   const Point2D& b, double eps = 1e-4) {
    double val = Cross(o, a, b);
    if (val > eps) return Orientation::CounterClockwise;
    if (val < -eps) return Orientation::Clockwise;
    return Orientation::Collinear;
}

inline bool OnSegment(const Point2D& p, const Point2D& a, const Point2D& b,
                       double eps = 1e-4) {
    return p.x >= std::min(a.x, b.x) - eps && p.x <= std::max(a.x, b.x) + eps &&
           p.y >= std::min(a.y, b.y) - eps && p.y <= std::max(a.y, b.y) + eps;
}

// Symmetric segment-segment intersection including collinear overlap
// Section 8.2: must satisfy SegmentsIntersect(A,B) == SegmentsIntersect(B,A)
inline bool SegmentsIntersect(const Point2D& p1, const Point2D& p2,
                               const Point2D& p3, const Point2D& p4,
                               double eps = 1e-4) {
    Orientation o1 = GetOrientation(p1, p2, p3, eps);
    Orientation o2 = GetOrientation(p1, p2, p4, eps);
    Orientation o3 = GetOrientation(p3, p4, p1, eps);
    Orientation o4 = GetOrientation(p3, p4, p2, eps);

    // General case: proper intersection
    if (o1 != o2 && o3 != o4) return true;

    // Special cases: collinear overlap
    if (o1 == Orientation::Collinear && OnSegment(p3, p1, p2, eps)) return true;
    if (o2 == Orientation::Collinear && OnSegment(p4, p1, p2, eps)) return true;
    if (o3 == Orientation::Collinear && OnSegment(p1, p3, p4, eps)) return true;
    if (o4 == Orientation::Collinear && OnSegment(p2, p3, p4, eps)) return true;

    return false;
}

// Polyline vs polyline intersection (for leader vs obstacle lines)
inline bool PolylinesIntersect(const std::vector<Point2D>& a,
                                const std::vector<Point2D>& b,
                                double eps = 1e-4) {
    if (a.size() < 2 || b.size() < 2) return false;
    for (size_t i = 1; i < a.size(); ++i) {
        for (size_t j = 1; j < b.size(); ++j) {
            if (SegmentsIntersect(a[i-1], a[i], b[j-1], b[j], eps)) {
                return true;
            }
        }
    }
    return false;
}

// Text box vs polyline intersection (text box treated as 4 segments)
inline bool TextBoxIntersectsPolyline(const Rect& box,
                                       const std::vector<Point2D>& poly,
                                       double eps = 1e-4) {
    if (poly.size() < 2) return false;
    // Box edges: left, right, top, bottom
    Point2D bl{box.left, box.bottom};
    Point2D tl{box.left, box.top};
    Point2D tr{box.right, box.top};
    Point2D br{box.right, box.bottom};
    std::array<Point2D, 4> boxPts = {bl, tl, tr, br};
    for (size_t i = 0; i < 4; ++i) {
        Point2D p1 = boxPts[i];
        Point2D p2 = boxPts[(i + 1) % 4];
        for (size_t j = 1; j < poly.size(); ++j) {
            if (SegmentsIntersect(p1, p2, poly[j-1], poly[j], eps)) {
                return true;
            }
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// 9. Cost Function — Lexicographic comparison via std::tuple
// -----------------------------------------------------------------------------
struct LayoutCost {
    int textCollision = 0;         // Hard 1: Text-text overlaps
    int geometryCollision = 0;     // Hard 2: Text-geometry overlaps
    int lineCollision = 0;         // Hard 3: Polyline crossings
    double movement = 0.0;         // Soft 1: Distance from initial position
    double leaderLength = 0.0;     // Soft 2: Total leader line length
    double alignmentPenalty = 0.0; // Soft 3: Axis alignment deviation

    bool operator<(const LayoutCost& other) const {
        return std::tie(textCollision, geometryCollision, lineCollision,
                        movement, leaderLength, alignmentPenalty)
             < std::tie(other.textCollision, other.geometryCollision, other.lineCollision,
                        other.movement, other.leaderLength, other.alignmentPenalty);
    }
    bool operator==(const LayoutCost& other) const {
        return textCollision == other.textCollision &&
               geometryCollision == other.geometryCollision &&
               lineCollision == other.lineCollision &&
               movement == other.movement &&
               leaderLength == other.leaderLength &&
               alignmentPenalty == other.alignmentPenalty;
    }
};

// -----------------------------------------------------------------------------
// 10. Placement Priority — Deterministic element ordering
// -----------------------------------------------------------------------------
struct PlacementPriority {
    int typeRank = 0;              // 0 = Dimension, 1 = Leader (Dimensions first)
    int lockedNeighborCount = 0;   // More locked neighbors = solved first (negate at construction)
    int invalidCandidateCount = 0; // More constrained = solved first (negate at construction)
    ElementId id = 0;              // Final deterministic tie-break, ascending

    bool operator<(const PlacementPriority& other) const {
        return std::tie(typeRank, lockedNeighborCount, invalidCandidateCount, id)
             < std::tie(other.typeRank, other.lockedNeighborCount,
                        other.invalidCandidateCount, other.id);
    }
};

// -----------------------------------------------------------------------------
// 12. Diagnostic Log
// -----------------------------------------------------------------------------
struct OptimizationLog {
    size_t elementCount = 0;
    size_t totalCandidatesGenerated = 0;
    size_t rejectedCandidates = 0;
    int finalConflictCount = 0;
    double executionTimeMs = 0.0;
};

} // namespace LayoutOptimizer