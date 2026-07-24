# AGENTS.md

## Project: Archicad Layout Optimizer

---

### 1. Project Goal

Develop a C++ module for an Archicad Add-On that automatically optimizes the layout of drawing annotations:

- **Labels / Leader texts**
- **Dimension Lines & Texts**

**Objective:** Eliminate overlaps between text blocks, leader lines, and underlying drawing geometry while preserving drawing readability, maintaining layout stability, and ensuring 100% deterministic results.

---

### 2. Non-Negotiable Rules

1. **Zero Archicad API Leaks in Core:**
   - The core optimizer module (`LayoutOptimizer/`) must NOT include any Archicad SDK headers (`ACAPI_*.h`, `API_Guid`, `GS::Array`).
   - Core elements use primitive C++ types (`uint64_t`, `double`, `std::vector`).
2. **Absolute Determinism:**
   - Identical input must produce 100% identical output across runs and machines.
   - Never iterate over `std::unordered_map`, raw pointer addresses, or un-sorted containers in solver routines.
3. **Graceful Fallback (Never Fail):**
   - If a zero-collision solution is impossible, select the candidate with the lowest `LayoutCost`.
   - **Never crash, never throw unhandled exceptions, and never delete an annotation.**
4. **Respect User Layout:**
   - Never modify locked elements (`locked = true`).
   - Unconflicted existing positions must be preserved (_Layout Stability_).

---

### 3. Coordinate System & Boundary

- **Paper Space Millimeters (mm):** All coordinates and geometries inside the core optimizer use Paper Space mm.
- **Strict Adapter Boundary:**
  - All geometry entering `LayoutOptimizer` MUST already be converted by `ArchicadAdapter`.
  - The core optimizer NEVER performs coordinate transformations, handles model/story coordinates, or reads view scale factors.

---

### 4. Architecture

```text
[ Archicad API (ACAPI) ]
           │
           ▼
   [ ArchicadAdapter ]   ◄── Converts Scale/Matrix -> Paper Space mm, Handles Undo
           │
           ▼
    [ Layout Model ]     ◄── Isolated C++ types (No ACAPI dependencies)
           │
           ▼
  [ Candidate Generator ]
           │
           ▼
  [ Candidate Filter ]   ◄── Prunes out-of-bounds & invalid candidates early
           │
           ▼
   [ Collision Detector ] ◄── Symmetric Epsilon Check (1e-4 mm)
           │
           ▼
    [ Layout Solver ]    ◄── Deterministic Greedy / DFS Clusters / Fallback


### 5. Data Model (Immutable Geometry vs. Mutable Layout)

To prevent algorithms from accidentally modifying base measurement points, source geometry is strictly separated from mutable layout state.

#include <cstdint>
#include <vector>

using ElementId = uint64_t;

struct Point2D {
    double x = 0.0; // Paper Space mm
    double y = 0.0;
};

enum class ElementType {
    Leader,
    Dimension
};

// -----------------------------------------------------------------------------
// Leader Model
// -----------------------------------------------------------------------------
struct LeaderGeometry { // IMMUTABLE
    Point2D anchorPoint;
    double shoulderLength = 0.0;
};

struct LeaderLayout { // MUTABLE
    Point2D textPosition;
    Rect textBox;
    std::vector<Point2D> leaderPolyline;
    double leaderAngle = 0.0;
};

struct LeaderElement {
    ElementId id = 0;
    bool locked = false;
    LeaderGeometry geo; // Read-only source
    LeaderLayout layout; // Modifiable state
};

// -----------------------------------------------------------------------------
// Dimension Model
// -----------------------------------------------------------------------------
struct DimensionGeometry { // IMMUTABLE
    std::vector<Point2D> measurePoints;
    Point2D direction;
};

struct DimensionLayout { // MUTABLE
    Point2D textPosition;
    Rect textBox;
    double offset = 0.0;
};

struct DimensionElement {
    ElementId id = 0;
    bool locked = false;
    DimensionGeometry geo; // Read-only source
    DimensionLayout layout; // Modifiable state
};

### 6. Candidate Pipeline

Candidates are generated, filtered, and then evaluated.

struct Candidate {
    Point2D textPosition;
    Rect textBox;
    std::vector<Point2D> leaderPolyline;
    double offset = 0.0;
    double angle = 0.0;
    bool valid = true;
};

### Filtering Pipeline (Order of Candidate Pruning)

Before running the solver, invalidate impossible candidates:

- View Boundary Check: Text box falls outside the drawing/view limits.

- Forbidden Geometry Check: Text box lands directly over critical fixed geometry.

- Obstacle Line Crossing: Leader polyline crosses fixed structural boundaries.

Filtering reduces the solution space early, significantly speeding up the solver.

### 7. Collision Detection & Math Rules
Symmetric Epsilon AABB Intersection

struct Rect {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;

    bool Intersects(const Rect& other, double eps = 1e-4) const {
        return !(right < other.left - eps ||
                 other.right < left - eps ||
                 bottom < other.top - eps ||
                 other.bottom < top - eps);
    }
};

### 8. Cost Function

Candidates are compared lexicographically via std::tuple:
C++

#include <tuple>

struct LayoutCost {
    int textCollision = 0;        // Hard 1: Text-text overlaps
    int geometryCollision = 0;    // Hard 2: Text-geometry overlaps
    int lineCollision = 0;        // Hard 3: Polyline crossings
    double movement = 0.0;        // Soft 1: Distance from initial position
    double leaderLength = 0.0;    // Soft 2: Total line length
    double alignmentPenalty = 0.0;// Soft 3: Axis alignment deviation

    bool operator<(const LayoutCost& other) const {
        return std::tie(textCollision, geometryCollision, lineCollision,
                        movement, leaderLength, alignmentPenalty)
             < std::tie(other.textCollision, other.geometryCollision, other.lineCollision,
                        other.movement, other.leaderLength, other.alignmentPenalty);
    }
};

9. Solver Algorithm
Pipeline Execution Sequence (LayoutOptimizer::Optimize())
Collect Elements: Fetch data via Archicad Adapter.
Normalize Coordinates: Convert all elements to Paper Space mm.
Generate Candidates: Create discrete candidate positions.
Filter Invalid Candidates: Prune candidates failing boundary/obstacle checks.
Build Conflict Information: Construct graph edges between overlapping elements.
Solve Placement:Sort elements deterministically by ElementId.
Order placement priority by:Element Type Priority (Dimensions > Leaders).
Number of locked neighbors.
Number of invalid candidates (more constrained elements solved first).
Run Greedy Solver / DFS Cluster optimization.
Validate & Fallback: Ensure every element has a selected candidate (minimum cost fallback if 0-collision is unattainable).
Apply Changes: Pass new coordinates back to ArchicadAdapter inside an Undo Scope.
### 10. Implementation Order (Agent Must Follow)

The AI Agent MUST implement features strictly in this order.

Do not jump ahead to advanced features before completing Phase 6.

Phase 1: Geometry PrimitivesImplement Point2D, Rect with symmetric epsilon intersection.

- Write standalone C++ unit tests for geometry functions.

Phase 2: ArchicadAdapter & Leader DataConvert Archicad API_Guid $\to$ ElementId.

- Read Leaders and transform coordinates to Paper Space mm.

Phase 3: Candidate Generation & Filtering
 - Implement discrete candidate generation (8 angles $\times$ 3 distances).
 - Add filtering step (drawing boundary pruning).
Phase 4: Collision Detection
 - Implement BBox-vs-BBox and Segment-vs-Segment collision checks.
Phase 5: Greedy Solver & Fallback
I - mplement lexicographical cost evaluation.
 - Implement basic Greedy Solver with deterministic pre-sorting.
 - Add minimum cost fallback mechanism.

Phase 6: Write Back Changes
 - Apply modifications to Archicad elements wrapped in ACAPI_CallUndoableCommand.

Only after Phase 6 is complete and verified:

Phase 7: Add Dimensions (DimensionElement, chains).

Phase 8: Add DFS Clustering & Local Search.

Phase 9: Caching & Performance optimizations.

11. Testing & Logging Requirements
Every optimization pass must optionally emit diagnostic logs for debugging:

struct OptimizationLog {
    size_t elementCount = 0;
    size_t totalCandidatesGenerated = 0;
    size_t rejectedCandidates = 0;
    int finalConflictCount = 0;
    double executionTimeMs = 0.0;
};

Example Log Output:
[LayoutOptimizer] Elements: 124 | Candidates: 2968 | Filtered: 814 | Final Conflicts: 0 | Time: 87ms

12. Forbidden Approaches

    NO Archicad SDK includes in LayoutOptimizer/ core files.

    NO R-Tree or QuadTree until benchmarking >500 elements proves a bottleneck.

    NO Multithreading (std::thread, tbb) — Archicad API calls must remain single-threaded.

    NO Non-deterministic containers: Avoid raw iteration over std::unordered_map or sorting by pointer addresses.

    NO Silent Failures: Never leave an element without a chosen position or drop an element from the output list.
```
