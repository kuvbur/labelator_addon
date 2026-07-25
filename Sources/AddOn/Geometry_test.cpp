// Phase 1 Geometry Primitives — Unit Tests
// Tests per AGENTS.md Section 12, Phase 1 DoD:
// - non-overlapping rect pairs
// - touching-at-epsilon-boundary rect pairs
// - fully-overlapping rect pairs
// - collinear-overlap segment pair (Section 8.2)
// - near-parallel non-intersecting pair at epsilon boundary (Section 8.2)

#include "LayoutOptimizer.hpp"
#include <cmath>
#include <iostream>
#include <vector>

using namespace LayoutOptimizer;

int passed = 0, failed = 0;

#define PASS(msg) do { ++passed; std::cout << "[PASS] " << msg << "\n"; } while(0)
#define FAIL(msg) do { ++failed; std::cout << "[FAIL] " << msg << "\n"; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(msg); else FAIL(msg); } while(0)

bool eq(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }
bool pt_eq(Point2D a, Point2D b) { return eq(a.x, b.x) && eq(a.y, b.y); }
bool rect_eq(Rect a, Rect b) { return eq(a.left,b.left) && eq(a.top,b.top) && eq(a.right,b.right) && eq(a.bottom,b.bottom); }

int main() {
    std::cout << "=== Phase 1 Geometry Primitives Unit Tests ===\n\n";
    const double eps = 1e-4;

    // --- Rect::Intersects tests (Section 8.1) ---
    // 1. Non-overlapping
    { Rect a{0,0,10,10}, b{20,20,30,30};
      CHECK(!a.Intersects(b,eps), "Non-overlapping rects should not intersect");
      CHECK(!b.Intersects(a,eps), "Intersection symmetric (non-overlap)"); }

    // 2. Fully overlapping
    { Rect a{0,0,10,10}, b{0,0,10,10};
      CHECK(a.Intersects(b,eps), "Identical rects should intersect");
      CHECK(b.Intersects(a,eps), "Intersection symmetric (identical)"); }

    // 3. Partially overlapping
    { Rect a{0,0,10,10}, b{5,5,15,15};
      CHECK(a.Intersects(b,eps), "Partially overlapping rects should intersect");
      CHECK(b.Intersects(a,eps), "Intersection symmetric (partial)"); }

    // 4. Touching at epsilon boundary
    { Rect a{0,0,10,10}, b{10-eps,0,20,10};
      CHECK(a.Intersects(b,eps), "Touching at eps boundary should intersect (a->b)");
      CHECK(b.Intersects(a,eps), "Intersection symmetric (eps boundary)"); }

    // 5. Just outside epsilon (should NOT intersect)
    { Rect a{0,0,10,10}, c{10+2*eps,0,20,10};
      CHECK(!a.Intersects(c,eps), "Just outside eps boundary should NOT intersect");
      CHECK(!c.Intersects(a,eps), "Intersection symmetric (outside eps)"); }

    // 6. Touching at top/bottom edges (Y direction)
    { Rect a{0,0,10,10}, b{0,10-eps,10,20};
      CHECK(a.Intersects(b,eps), "Touching at Y eps boundary should intersect"); }

    // 7. Normalized rects (left>right, top>bottom)
    { Rect a{10,10,0,0}; a.Normalize(); Rect b{0,0,10,10};
      CHECK(a.Intersects(b,eps), "Normalized rect should intersect correctly"); }

    std::cout << "\n--- Rect tests done ---\n\n";

    // --- Segment intersection tests (Section 8.2) ---
    // 8. Collinear overlap
    { Point2D p1{0,0}, p2{10,0}, p3{5,0}, p4{15,0};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "Collinear overlap should intersect");
      CHECK(SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: collinear overlap"); }

    // 9. Collinear touch at endpoint
    { Point2D p1{0,0}, p2{10,0}, p3{10,0}, p4{20,0};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "Collinear endpoint touch should intersect");
      CHECK(SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: endpoint touch"); }

    // 10. Collinear disjoint (gap > eps)
    { Point2D p1{0,0}, p2{10,0}, p3{10+2*eps,0}, p4{20,0};
      CHECK(!SegmentsIntersect(p1,p2,p3,p4,eps), "Collinear disjoint (gap > eps) should not intersect");
      CHECK(!SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: collinear disjoint"); }

    // 11. Proper intersection (crossing)
    { Point2D p1{0,0}, p2{10,10}, p3{0,10}, p4{10,0};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "Crossing segments should intersect");
      CHECK(SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: crossing"); }

    // 12. T-intersection
    { Point2D p1{0,5}, p2{10,5}, p3{5,0}, p4{5,10};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "T-intersection should intersect");
      CHECK(SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: T-intersection"); }

    // 13. Near-parallel non-intersecting at epsilon boundary
    { Point2D p1{0,0}, p2{10,0}, p3{0,eps}, p4{10,eps};
      CHECK(!SegmentsIntersect(p1,p2,p3,p4,eps), "Near-parallel at eps distance should NOT intersect");
      CHECK(!SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: near-parallel"); }

    // 14. Near-parallel at > eps
    { Point2D p1{0,0}, p2{10,0}, p3{0,2*eps}, p4{10,2*eps};
      CHECK(!SegmentsIntersect(p1,p2,p3,p4,eps), "Near-parallel at 2*eps should NOT intersect"); }

    // 15. Segment completely inside another (collinear containment)
    { Point2D p1{0,0}, p2{10,0}, p3{2,0}, p4{8,0};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "Collinear containment should intersect");
      CHECK(SegmentsIntersect(p3,p4,p1,p2,eps), "Symmetric: containment"); }

    // 16. Endpoint on segment interior (collinear)
    { Point2D p1{0,0}, p2{10,0}, p3{5,0}, p4{5,5};
      CHECK(SegmentsIntersect(p1,p2,p3,p4,eps), "Endpoint on segment interior should intersect"); }

    std::cout << "\n--- Segment tests done ---\n\n";

    // --- Polyline intersection tests ---
    // 17. Polyline vs polyline crossing
    { std::vector<Point2D> a = {{0,0},{5,5},{10,0}};
      std::vector<Point2D> b = {{0,10},{5,5},{10,10}};
      CHECK(PolylinesIntersect(a,b,eps), "Polylines crossing should intersect");
      CHECK(PolylinesIntersect(b,a,eps), "Symmetric: polylines"); }

    // 18. Disjoint polylines
    { std::vector<Point2D> a = {{0,0},{5,0}};
      std::vector<Point2D> b = {{0,10},{5,10}};
      CHECK(!PolylinesIntersect(a,b,eps), "Disjoint polylines should not intersect"); }

    // 19. TextBox vs Polyline intersection
    { Rect box{0,0,10,10};
      std::vector<Point2D> poly = {{-5,5},{5,5},{15,5}};
      CHECK(TextBoxIntersectsPolyline(box,poly,eps), "Polyline through box should intersect"); }

    // 20. TextBox away from polyline
    { Rect box{0,0,10,10};
      std::vector<Point2D> poly = {{-5,-5},{-1,-1}};
      CHECK(!TextBoxIntersectsPolyline(box,poly,eps), "Polyline away from box should not intersect"); }

    // 21. TextBox corner touch at eps
    { Rect box{0,0,10,10};
      std::vector<Point2D> poly = {{10-eps,10-eps},{20,20}};
      CHECK(TextBoxIntersectsPolyline(box,poly,eps), "Polyline touching box corner at eps should intersect"); }

    // --- Point2D operators ---
    { Point2D a{1,2}, b{3,4};
      CHECK(pt_eq(a+b, Point2D{4,6}), "Point2D addition");
      CHECK(pt_eq(a-b, Point2D{-2,-2}), "Point2D subtraction");
      CHECK(pt_eq(a*2.0, Point2D{2,4}), "Point2D scalar multiplication");
      CHECK(pt_eq(2.0*a, Point2D{2,4}), "Point2D scalar multiplication (reverse)"); }

    // --- Rect utilities ---
    { Rect r{0,0,10,10};
      CHECK(eq(r.Width(),10.0), "Rect Width");
      CHECK(eq(r.Height(),10.0), "Rect Height");
      CHECK(pt_eq(r.Center(), Point2D{5,5}), "Rect Center"); }

    // --- LayoutCost lexicographic ordering ---
    { LayoutCost a{0,0,0,1.0,0,0}, b{0,0,0,2.0,0,0};
      CHECK(a < b, "LayoutCost: movement is 4th key, smaller wins");
      LayoutCost c{1,0,0,0,0,0}, d{0,5,0,0,0,0};
      CHECK(d < c, "LayoutCost: textCollision (1st key) dominates");
      LayoutCost e{0,1,0,0,0,0}, f{0,0,5,0,0,0};
      CHECK(f < e, "LayoutCost: geometryCollision (2nd) > lineCollision (3rd)"); }

    // --- PlacementPriority ordering ---
    { PlacementPriority p1{0,2,1,100}, p2{1,2,1,50};
      CHECK(p1 < p2, "PlacementPriority: Dimensions (typeRank=0) before Leaders (typeRank=1)");
      PlacementPriority p3{0,3,1,100}, p4{0,2,1,100};
      CHECK(p4 < p3, "Raw operator<: lower lockedNeighborCount sorts first (sign flip at construction)"); }

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}