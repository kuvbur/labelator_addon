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
   - `LayoutOptimizer/` must NOT include `ACAPI_*.h`, `API_Guid`, `GS::Array`, or any other ACAPI/GSRoot header.
   - Core elements use primitive C++ types (`uint64_t`, `double`, `std::vector`) only.
   - Enforced by the automated grep checklist in Section 16 — every PR touching `LayoutOptimizer/` must pass it before review.
2. **Absolute Determinism:**
   - Identical input must produce byte-identical output across runs and machines.
   - Never iterate over `std::unordered_map`/`std::unordered_set`, raw pointer addresses, or un-sorted containers in solver routines.
   - Compiler/toolchain requirements that make this achievable in practice are in Section 11 — determinism is a build-flag concern as much as an algorithm concern.
3. **Graceful Fallback (Never Fail):**
   - If a zero-collision solution is impossible, select the candidate with the lowest `LayoutCost`.
   - Every element is guaranteed at least one valid candidate at all times — see Section 7, "Guaranteed Fallback Candidate."
   - Never crash, never throw an unhandled exception, never delete an annotation, never drop an element from the output list.
4. **Respect User Layout:**
   - Never reposition a locked element (`locked == true`).
   - A locked element's current geometry still counts as an obstacle for other elements — see Section 5.
   - Unconflicted existing positions must be preserved (_Layout Stability_).
5. **No Silent Invention:**
   - If any rule in this document is ambiguous for the feature being implemented, follow the Ambiguity Protocol in Section 14. Do not guess silently.

---

### 3. Coordinate System & Boundary

- **Paper Space Millimeters (mm):** all coordinates and geometry inside the core optimizer use Paper Space mm.
- **Axis Convention (core-internal, mandatory):**
  - X increases rightward, Y increases **downward** (matches typical 2D document/screen layout coordinate systems).
  - Angles are measured in degrees, clockwise, with 0° = +X direction (East), 90° = +Y direction (South/down).
  - This is the core module's own internal convention. It does **not** need to match Archicad's native coordinate system — `ArchicadAdapter` is solely responsible for that translation (including any Y-flip).
- **Strict Adapter Boundary:**
  - All geometry entering `LayoutOptimizer` MUST already be converted by `ArchicadAdapter` to this convention.
  - The core optimizer NEVER performs coordinate transformations, handles model/story coordinates, reads view scale factors, or assumes any Archicad-native axis direction.

---

### 4. Architecture

```text
[ Archicad API (ACAPI) ]
           │
           ▼
   [ ArchicadAdapter ]    ◄── Converts Scale/Matrix -> Paper Space mm, Handles Undo
           │
           ▼
    [ Layout Model ]      ◄── Isolated C++ types (No ACAPI dependencies)
           │
           ▼
  [ Candidate Generator ] ◄── Fixed angle/distance grid (Section 6)
           │
           ▼
  [ Candidate Filter ]    ◄── Prunes out-of-bounds & invalid candidates early
           │
           ▼
   [ Collision Detector ] ◄── Symmetric Epsilon Check (1e-4 mm)
           │
           ▼
    [ Layout Solver ]     ◄── Deterministic Greedy / DFS Clusters / Fallback
```

---

### 5. Data Model (Immutable Geometry vs. Mutable Layout)

Source geometry is strictly separated from mutable layout state, so algorithms cannot accidentally corrupt base measurement points.

```cpp
#include <cstdint>
#include <vector>

using ElementId = uint64_t;

struct Point2D {
    double x = 0.0; // Paper Space mm
    double y = 0.0; // Paper Space mm, +Y = down (see Section 3)
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
    LeaderGeometry geo;   // Read-only source
    LeaderLayout layout;  // Modifiable state
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
    DimensionGeometry geo;   // Read-only source
    DimensionLayout layout;  // Modifiable state
};
```

**Locked elements as obstacles:** a locked element (`locked == true`) is never a placement target — it is excluded from the solver's search entirely. However, its _current_ `layout.textBox` / `layout.leaderPolyline` **must** be included as Forbidden Geometry / Obstacle Lines when generating and filtering candidates for every other (unlocked) element. A locked element can still appear in `finalConflictCount` if other elements cannot avoid it, but its own position never changes.

---

### 6. Candidate Generation (Deterministic Parameters)

Candidate positions are generated from a **fixed, documented grid** — never invented ad hoc, so runs stay reproducible and comparable across implementation sessions.

```cpp
// Fixed candidate generation grid — do not change without updating this file.
constexpr double kCandidateAnglesDeg[8] = {
    0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0
};
constexpr double kCandidateDistancesMm[3] = {
    5.0, 10.0, 20.0
};
// Total raw candidates per element = 8 * 3 = 24, before the current-position
// fallback candidate (Section 7) and filtering are applied.
```

Angle 0° = East, measured clockwise per Section 3. Distance is measured from the element's anchor point (`LeaderGeometry::anchorPoint` or the relevant `DimensionGeometry` point) to the candidate `textPosition`.

```cpp
struct Candidate {
    Point2D textPosition;
    Rect textBox;
    std::vector<Point2D> leaderPolyline;
    double offset = 0.0;
    double angle = 0.0;
    bool valid = true;
};
```

---

### 7. Candidate Filtering Pipeline

Order of pruning, applied after generation:

1. **View Boundary Check** — text box falls outside the drawing/view limits.
2. **Forbidden Geometry Check** — text box lands directly over critical fixed geometry (including locked-element layouts, per Section 5).
3. **Obstacle Line Crossing** — leader polyline crosses fixed structural boundaries.

Filtering reduces the solution space early, significantly speeding up the solver.

**Guaranteed Fallback Candidate:** before filtering runs, each element's _current, unmodified_ layout position is inserted into its candidate list as an unconditionally `valid = true` candidate — it bypasses steps 1–3 above. This guarantees every element has at least one candidate even when fully surrounded by obstacles, satisfying the "never drop an element" rule in Section 2. It participates normally in cost evaluation (Section 9) like any other candidate.

---

### 8. Collision Detection & Math Rules

#### 8.1 Symmetric Epsilon AABB Intersection

```cpp
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
```

#### 8.2 Deterministic Segment-vs-Segment Intersection

Used for leader-polyline vs. obstacle-line collisions (Phase 4). Must use the **same epsilon** (1e-4 mm) as `Rect::Intersects`, and must be symmetric under argument swap: `SegmentsIntersect(A, B) == SegmentsIntersect(B, A)`. Collinear overlap must be handled explicitly — do not rely on strict orientation tests alone.

```cpp
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

// Returns true if segment (p1,p2) intersects segment (p3,p4), including
// collinear overlap.
inline bool SegmentsIntersect(const Point2D& p1, const Point2D& p2,
                               const Point2D& p3, const Point2D& p4,
                               double eps = 1e-4) {
    Orientation o1 = GetOrientation(p1, p2, p3, eps);
    Orientation o2 = GetOrientation(p1, p2, p4, eps);
    Orientation o3 = GetOrientation(p3, p4, p1, eps);
    Orientation o4 = GetOrientation(p3, p4, p2, eps);

    if (o1 != o2 && o3 != o4) return true;

    if (o1 == Orientation::Collinear && OnSegment(p3, p1, p2, eps)) return true;
    if (o2 == Orientation::Collinear && OnSegment(p4, p1, p2, eps)) return true;
    if (o3 == Orientation::Collinear && OnSegment(p1, p3, p4, eps)) return true;
    if (o4 == Orientation::Collinear && OnSegment(p2, p3, p4, eps)) return true;

    return false;
}
```

---

### 9. Cost Function

Candidates are compared lexicographically via `std::tuple`:

```cpp
#include <tuple>

struct LayoutCost {
    int textCollision = 0;         // Hard 1: Text-text overlaps
    int geometryCollision = 0;     // Hard 2: Text-geometry overlaps
    int lineCollision = 0;         // Hard 3: Polyline crossings
    double movement = 0.0;         // Soft 1: Distance from initial position
    double leaderLength = 0.0;     // Soft 2: Total line length
    double alignmentPenalty = 0.0; // Soft 3: Axis alignment deviation

    bool operator<(const LayoutCost& other) const {
        return std::tie(textCollision, geometryCollision, lineCollision,
                        movement, leaderLength, alignmentPenalty)
             < std::tie(other.textCollision, other.geometryCollision, other.lineCollision,
                        other.movement, other.leaderLength, other.alignmentPenalty);
    }
};
```

Note: `LayoutCost` has no `ElementId` tie-break because it compares _candidates for a single element_, not elements against each other. Element-ordering tie-breaks belong to `PlacementPriority` (Section 10).

---

### 10. Solver Algorithm

**Pipeline Execution Sequence (`LayoutOptimizer::Optimize()`):**

1. Collect Elements — fetch data via `ArchicadAdapter`.
2. Normalize Coordinates — convert all elements to Paper Space mm (Adapter-side only, Section 3).
3. Generate Candidates — fixed grid (Section 6) + guaranteed fallback candidate (Section 7).
4. Filter Invalid Candidates — prune per Section 7's pipeline.
5. Build Conflict Information — construct graph edges between overlapping elements.
6. Solve Placement:
   - Sort elements deterministically using `PlacementPriority` (below).
   - Run Greedy Solver / DFS Cluster optimization (DFS clustering is not yet specified — see Section 13, Phase 8).
7. Validate & Fallback — ensure every element has a selected candidate; use minimum-`LayoutCost` fallback if zero-collision is unattainable.
8. Apply Changes — pass new coordinates back to `ArchicadAdapter` inside an Undo Scope.

**Deterministic Placement Priority** — element ordering must be a single, fully-ordered comparator, not a prose checklist, mirroring the `LayoutCost` pattern:

```cpp
struct PlacementPriority {
    int typeRank = 0;            // 0 = Dimension, 1 = Leader (Dimensions solved first)
    int lockedNeighborCount = 0; // more locked neighbors = solved first -> negate for sort if needed
    int invalidCandidateCount = 0; // more constrained = solved first -> negate for sort if needed
    ElementId id = 0;            // final deterministic tie-break, ascending

    bool operator<(const PlacementPriority& other) const {
        return std::tie(typeRank, lockedNeighborCount, invalidCandidateCount, id)
             < std::tie(other.typeRank, other.lockedNeighborCount,
                        other.invalidCandidateCount, other.id);
    }
};
```

Sort ascending by this comparator to get solve order (adjust field signs at construction time so "more constrained first" falls out of ascending order — do not embed sign-flipping logic inside `operator<` itself).

---

### 11. Build System & Toolchain (Determinism Requirements)

- **Language standard:** C++17, fixed for the whole project. Do not mix standards between `LayoutOptimizer/` and `ArchicadAdapter/`.
- **Build system:** CMake. `LayoutOptimizer/` builds as its own static library target with **zero** dependencies beyond the standard library, so the "no ACAPI headers" rule is enforced by the build graph, not just code review.
- **Floating-point determinism** — required compiler flags:
  - MSVC: `/fp:strict` (or at minimum `/fp:precise`); do not use `/fp:fast`.
  - GCC/Clang: `-ffp-contract=off`; do not use `-ffast-math` or `-Ofast`.
- **Sorting:** every `std::sort` / `std::stable_sort` in solver code must use a comparator that forms a strict weak ordering across the _entire_ key (see `PlacementPriority`, `LayoutCost` above) — never sort by a partial key and leave the rest to insertion order.
- **Forbidden containers in solver paths:** `std::unordered_map`, `std::unordered_set` — use `std::map`/`std::vector` with explicit sorting instead.
- This section governs correctness constraints on _how code compiles_. Section 17 covers the separate concern of _how to invoke the actual build script_ day-to-day — they don't overlap, but both apply on every build.

---

### 12. Testing & Logging Requirements

- **Unit test framework:** Catch2 (header-only, no external build dependency), fixed for the whole project. If the surrounding codebase already standardizes on another framework, substitute it here — but the choice must be locked in this document before Phase 1 begins, not decided per-phase.
- **Golden-file / snapshot tests (required, in addition to unit tests):** fixed input scenarios (element sets, as JSON or a plain struct-literal fixture) with a checked-in expected output, compared byte-for-byte on every CI run. Minimum required scenarios:
  1. No-conflict baseline (positions must not move — Layout Stability).
  2. Dense-conflict scenario requiring the greedy solver to actually relocate elements.
  3. Fully-obstructed scenario that forces the guaranteed-fallback-candidate path (Section 7).
     These are the primary defense against a future change silently breaking determinism or dropping an element.

**Diagnostic Log:**

```cpp
struct OptimizationLog {
    size_t elementCount = 0;
    size_t totalCandidatesGenerated = 0;
    size_t rejectedCandidates = 0;
    int finalConflictCount = 0;
    double executionTimeMs = 0.0;
};
```

Example: `[LayoutOptimizer] Elements: 124 | Candidates: 2968 | Filtered: 814 | Final Conflicts: 0 | Time: 87ms`

---

### 13. Implementation Order (Agent Must Follow)

Implement strictly in this order. Do not start a phase until the prior phase's Definition of Done (DoD) is met and its tests pass.

**Phase 1 — Geometry Primitives**

- Implement `Point2D`, `Rect` (Section 8.1).
- DoD: unit tests cover non-overlapping, touching-at-epsilon-boundary, and fully-overlapping rect pairs.

**Phase 2 — ArchicadAdapter & Leader Data**

- Convert Archicad `API_Guid` → `ElementId`.
- Read Leaders and transform coordinates to Paper Space mm per Section 3's convention.
- DoD: adapter output for a known fixture matches hand-computed Paper Space coordinates.

**Phase 3 — Candidate Generation & Filtering**

- Implement candidate generation using the fixed grid in Section 6 (no other values).
- Implement the guaranteed fallback candidate (Section 7).
- Implement the filtering pipeline (Section 7, steps 1–3; boundary check only if obstacle data isn't ready yet — obstacle/line checks may land in Phase 4).
- DoD: for a fixture with a fully-boxed-in element, exactly one candidate (the fallback) survives filtering.

**Phase 4 — Collision Detection**

- Implement `Rect::Intersects` usage end-to-end and `SegmentsIntersect` (Section 8.2).
- DoD: unit tests include a collinear-overlap segment pair and a near-parallel non-intersecting pair at the epsilon boundary.

**Phase 5 — Greedy Solver & Fallback**

- Implement `LayoutCost` lexicographic evaluation (Section 9).
- Implement `PlacementPriority`-ordered greedy solver (Section 10).
- Implement minimum-cost fallback mechanism.
- DoD: golden-file scenarios 1 and 2 (Section 12) pass byte-for-byte on repeated runs.

**Phase 6 — Write Back Changes**

- Apply modifications to Archicad elements wrapped in `ACAPI_CallUndoableCommand`.
- DoD: undo/redo round-trips correctly in a manual Archicad test; locked elements verifiably untouched.

_Only after Phase 6 is complete and verified:_

**Phase 7 — Add Dimensions** (`DimensionElement`, chains)

**Phase 8 — Add DFS Clustering & Local Search**

- **Not yet specified.** Before starting this phase, this document must be extended with an explicit clustering criterion (what forms a graph edge) and neighborhood definition, at the same precision as Section 8. Do not infer clustering behavior from the Phase 5 greedy solver by analogy — they are different algorithms with different correctness requirements.

**Phase 9 — Caching & Performance Optimizations**

---

### 14. Ambiguity Protocol

If any rule in this document is unclear for the specific feature being implemented:

1. Choose the most conservative interpretation (favors not moving elements, not crossing obstacles, not silently dropping data).
2. Mark the choice inline with a comment: `// ASSUMPTION: <what was assumed and why>`.
3. Call it out explicitly in the PR/commit description for human review.
4. Do not silently invent behavior not traceable to this document or an explicit assumption comment.

---

### 15. Forbidden Approaches

- NO Archicad SDK includes in `LayoutOptimizer/` core files.
- NO R-Tree or QuadTree until benchmarking >500 elements proves a bottleneck.
- NO Multithreading (`std::thread`, `tbb`) — Archicad API calls must remain single-threaded.
- NO Non-deterministic containers — no raw iteration over `std::unordered_map`, no sorting by pointer addresses, no `/fp:fast`/`-ffast-math`.
- NO Silent Failures — never leave an element without a chosen position, never drop an element from the output list.
- NO inventing candidate-generation constants, coordinate conventions, or tie-break rules not already fixed in this document — extend the document first (Section 14).

---

### 16. Code Review Checklist (Automated Checks)

Run before approving any PR touching `LayoutOptimizer/`:

```bash
# All of the following must return zero matches:
grep -rn "ACAPI_"        LayoutOptimizer/
grep -rn "API_Guid"      LayoutOptimizer/
grep -rn "GS::Array"     LayoutOptimizer/
grep -rn "std::thread"   LayoutOptimizer/
grep -rn "tbb::"         LayoutOptimizer/
grep -rn "ffast-math\|fp:fast" LayoutOptimizer/ CMakeLists.txt

# Manual review required if either of these has matches:
grep -rln "unordered_map\|unordered_set" LayoutOptimizer/
# -> confirm iteration order never affects output (e.g. only used for O(1)
#    lookup, always re-sorted before iteration).
```

---

### 17. Build & Packaging Workflow (`BuildAddOn.py`)

Complements Section 11 — Section 11 governs compiler/determinism constraints; this section covers the actual day-to-day build invocation.

**Target Archicad Version:** Archicad 25 (AC25).

Requires **Developer Command Prompt for VS** (provides `INCLUDE`/`LIB`/MSVC toolset).

```bash
python Tools\BuildAddOn.py -c config.json -v <version> [--release] [--package]
```

- `-v <version>` — e.g. `25`, `27`; multiple versions space-separated.
- Without `--release` → Debug build, single version, no language selection.
- `--release` → RelWithDebInfo for all languages from config.json (or `-l <LANG>`).
- `--package` → pack `.apx`/`.bundle` into `Build/Package`.
- DevKit auto-downloaded to `Build/DevKit/APIDevKit-<version>`; or supply local `-d <path>` (then `-v` must be single).

---

### 18. LSP (clangd) Configuration

Not the same as the normal build (Section 17). Normal build uses the VS generator (multi-config) → **no `compile_commands.json`**. For LSP, a separate Ninja + `clang-cl` config is used (MSVC `cl.exe` fails to extract system includes).

```bash
python Tools\BuildAddOn.py -c config.json -v <version> --lsp
```

Also run from the **Developer Command Prompt**. Generates `Build/LspCompileCommands/<version>/compile_commands.json` and copies it to the repo root.

Regenerate after: AC version change, `CMakeCommon.cmake` edits, new `.cpp` files added under `Sources/AddOn`.

Machine-specific artefacts from this step — `compile_commands.json`, `Build/LspCompileCommands/`, `Build/DevKit/` — fall under the general "build artifacts are never committed" rule in Section 21; make sure all three are covered by `.gitignore`.

---

### 19. Code Editing & Indentation Policy

To prevent indentation errors and issues with patching tools:

1. **Never rewrite large files entirely** just because of whitespace/indentation mismatches during patching.
2. **Auto-format edited files:** right after editing a C++ file (`.cpp`, `.hpp`, `.h`), run `clang-format` via terminal to ensure adherence to project style: `clang-format -i path/to/edited_file.cpp`. Keep ALL changes `clang-format` produces without manual rollback.
3. **Pre-formatting before patching (optional):** if a patch fails due to whitespace mismatches, run `clang-format -i` on the target file first, then re-apply the patch.
4. **Trust `clang-format` on include ordering:** do NOT manually revert or rearrange `#include` directives if `clang-format` reorders them.
   - Putting the paired header (e.g. `#include "TestFunc.hpp"` inside `TestFunc.cpp`) first is standard LLVM/Google/C++ practice, verifying header self-sufficiency.
5. **Do NOT fight `clang-format`:** accept its output as project truth — reordered lines or includes are not style violations.

---

### 20. C++ Code Navigation Priority

When looking for function definitions, classes, symbols, or architectural context in C++, follow this strict priority order:

1. **First choice — Clangd MCP:** find exact C++ symbols, definitions, and declarations using `clangd-mcp` tools first (`workspace_symbol_search`, `find_definition`, `find_references`). AST-based, no context noise.
2. **Second choice (fallback / context search) — LightRAG MCP:** if `clangd-mcp` fails to locate the symbol, or the request needs higher-level architectural context or conceptual understanding, call **LightRAG MCP** before forming a response.
   - **query:** a concise English description of the function/class names, modules/files, or key concepts (threading, memory, system architecture).
   - **mode:** `"hybrid"` unless explicitly instructed otherwise.
3. **Constraints:**
   - Base responses on retrieved context — if Clangd/LightRAG returns context or code, treat it as ground truth.
   - No hallucinated architecture, function signatures, or APIs that already exist in the codebase/context.
   - If neither tool yields sufficient context, say explicitly what's missing and ask for clarification — this is the same Ambiguity Protocol as Section 14, do not guess silently.
   - Never answer a C++ navigation/architecture question without attempting Clangd MCP first, then LightRAG MCP if needed.
   - This fallback ordering (Clangd first, LightRAG only if needed) applies to _navigating the project's own code_. For anything touching the Archicad SDK itself, Section 21 applies instead — there, LightRAG verification is mandatory, not a fallback.

---

### 21. Archicad API (ACAPI) Usage — Mandatory RAG Verification

Scope: any code inside `ArchicadAdapter/`, or anywhere else `ACAPI_*` functions, `API_Guid`, `GS::Array`, or other ACAPI/GSRoot types are used — this is the only layer permitted to touch ACAPI at all (Section 2, Rule 1).

- Before writing or modifying any call into the Archicad SDK, query **LightRAG MCP** against the indexed documentation, official examples, and open-source Archicad Add-On projects. Do not rely on memorized/trained knowledge of the API alone.
  - The ACAPI surface is versioned (this project targets AC25 — Section 17) and has non-obvious ownership, lifetime, and undo-scoping rules. A plausible-looking but wrong call is a common source of crashes and document corruption — exactly what Section 2, Rule 3 ("Graceful Fallback — Never Fail") exists to prevent.
- **This is mandatory verification, not a fallback.** Unlike the Clangd-first ordering in Section 20, ACAPI usage always goes through LightRAG regardless of whether Clangd can already resolve the symbol — Clangd confirms a function exists and its declared signature; only the documentation/example corpus confirms it's being called correctly.
- **Query convention:** same as Section 20 — a concise English query naming the exact ACAPI function/struct/pattern needed, `mode: "hybrid"` unless told otherwise.
- **If RAG has no relevant coverage** for a specific call, treat it as the Ambiguity Protocol (Section 14): do not guess from memory — flag the gap explicitly for human review instead of writing an unverified ACAPI call.
- Applies most heavily to Phase 2 (ArchicadAdapter & Leader Data) and Phase 6 (Write Back Changes) — the phases where ACAPI is actually touched.

---

### 22. Git & Version Control Discipline

Implementation spans many separate agent sessions with no shared memory between them. Git history — not assumptions about file contents — is the only reliable record of what is actually done.

- **Clean tree before editing:** run `git status` before making any change. If there are uncommitted changes not part of the current task, STOP — do not build on top of them, silently commit them, or discard them. Flag to a human instead.
- **Log before starting a phase:** run `git log` and confirm the previous phase's DoD commit (Section 13) actually exists before starting the next phase. Do not trust a phase is "done" just because the files look present.
- **One commit must equal one working state.** Every commit that lands on a branch tip must build and pass the current test suite (Section 12). WIP/broken intermediate states are fine to commit locally but must be squashed or clearly marked `WIP:` before being left as the tip — never leave a non-building commit as "done."
- **Commit messages cite this document.** Reference the phase and/or section, e.g. `Phase 4: implement SegmentsIntersect (Section 8.2)`. Keeps history greppable and lets a reviewer jump straight to the relevant rule.
- **Never rewrite shared history.** No `git commit --amend`, `git rebase -i`, or `git push --force` on any branch that has already been reviewed or shared. Fix mistakes with a new commit.
- **Never stage blindly.** No `git add .` / `git add -A` without first reading `git status`/`git diff`. Stage only files inside the current task's scope — this catches stray build artifacts or another session's unfinished edits.
- **Scope discipline mirrors the module boundary (Section 2):** a commit for a `LayoutOptimizer/` (core) task should not also touch `ArchicadAdapter/` files, and vice versa. Unrelated fixes belong in their own commit.
- **One branch per phase/feature** (e.g. `phase-4-collision-detection`), never commit directly to `main`/`master`. Merge only once the phase's DoD is met.
- **Build artifacts are never committed** — confirm `.gitignore` covers CMake build directories, compiled binaries, IDE files, and the LSP artefacts listed in Section 18, before the first commit of a new phase.

## Strict Operational Workflow & State Persistence

To ensure uninterrupted continuity, prevent hallucinated progress, and allow flawless recovery across session breaks or context resets, you **MUST** strictly adhere to the following operational loop before performing any actions or tool calls.

### File Roles & Hierarchy

- **`AGENTS.md`** (Read-Only Blueprint): Contains the macro-level strategy, high-level phases, and project milestones. Never alter this file unless explicitly instructed.
- **`IDEA.md`** (Dynamic Work Log & Execution Plan): The single source of truth for the current execution state, step-by-step tactical plan, completed work, and exact resume points.

---

### Execution Loop (Mandatory Sequence)

#### 1. Session Initialization / Resume Protocol

Before taking **ANY** step or calling any execution tool:

1. Read `AGENTS.md` to align with the active strategic milestone.
2. Read `IDEA.md` to restore the exact current state, completed steps, and pending tasks.
3. If `IDEA.md` does not exist or is empty, initialize it immediately based on `AGENTS.md`.

#### 2. Pre-Action Planning & Verification

- **Never execute blind actions.** Before executing any code, file modification, or complex command, update `IDEA.md` with your immediate, detailed tactical plan.
- Explicitly break down the upcoming task into small, verifiable sub-tasks.
- Verify that your planned action directly advances the current milestone in `AGENTS.md`.

#### 3. Execution & Progress Logging

- Execute the planned step.
- **Immediately log the outcome in `IDEA.md`**:
  - Mark finished sub-tasks as completed.
  - Record key decisions, architectural changes, file edits, or blocker details.
  - Explicitly write down the **Next Step / Current State** so work can be resumed instantly after a context break.

---

### Mandatory Rules for `IDEA.md` Structure

Your updates to `IDEA.md` MUST always include these core sections:

```markdown
# Current Execution Plan

## Active Milestone

[Current Phase / Goal from AGENTS.md]

## Current State & Resume Marker

- **Status:** [IN_PROGRESS / WAITING_FOR_TEST / BLOCKED]
- **Last Action Completed:** [Detailed summary of what was just done]
- **Immediate Next Step:** [Exact step to take next]

## Tactical Step-by-Step Plan

- [x] Completed task 1
- [x] Completed task 2
- [/] Active task in progress
- [ ] Upcoming task A
- [ ] Upcoming task B

## Execution Log & Decisions

- [Timestamp/Step ID]: Action taken, files modified, key outcomes or errors encountered.

CRITICAL RULE: Do not rely on conversational memory for project state. If an action or decision is not logged in IDEA.md, it did not happen. Always sync IDEA.md BEFORE and AFTER significant steps.
```
