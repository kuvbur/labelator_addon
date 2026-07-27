# Current Development Plan (Phase 6: ArchicadAdapter — FIX APPLIED)

> ## 🚫 HARD RULE — Bug Fix Gate (read this before doing anything else)
>
> If manual testing in Archicad finds **any** bug — a crash, incorrect behavior, a failed undo/redo, a locked element that moved, a corrupted document, anything — **development STOPS immediately.**
>
> - Do **not** start Phase 7, Phase 8, or any other new feature.
> - Do **not** "continue anyway" because the bug looks minor, unrelated, or annoying to chase down.
> - Fix the bug(s) found. Then get a **clean manual re-test** confirming the fix. Only after that may forward progress resume.
> - This gate applies regardless of which phase is "next" per AGENTS.md §13 — the implementation order in §13 assumes each phase actually passed its DoD; an unresolved bug means the DoD was not met, full stop.
> - If you're unsure whether something found counts as a "bug" for this gate: it does. Treat ambiguity here the same as the Ambiguity Protocol (AGENTS.md §14) — the conservative read is to stop, not to proceed.

## Actual Status (verified via `git status`)

- **Git**: Clean, 5 commits ahead of origin/master (includes a refactor)
  - `5a27590` "fix(archicadadapter): Add ACAPI_Element_GetMemo before ACAPI_Element_Change"
  - `f2f6b80` "Refactoring: Tasks 1-4 complete"
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Structure**: Flat, under `Sources/AddOn/`. All files in one directory (per project rules).
- **Phase 6 files**: `ArchicadAdapter.cpp`, `ArchicadAdapter.hpp` — code fixed, ready for re-test.

## 📁 Project Structure (for quick navigation — keep this up to date, do not delete) — **full absolute paths**

Base directory: `D:/labelator_addon`

### Core LayoutOptimizer (pure C++17, NO ACAPI dependencies)

| File                     | Full Path                                                 | Purpose                                                                                                                                                                                                                                                                                                                                                                                                                                                    | Phase          |
| ------------------------ | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| `LayoutOptimizer.hpp`    | `D:/labelator_addon/Sources/AddOn/LayoutOptimizer.hpp`    | Main header: Point2D, Rect, ElementType, Leader/Dimension Geometry/Layout/Element, Candidate, LayoutCost, PlacementPriority, ElementInput, SolveResult, constants kCandidateAnglesDeg/kCandidateDistancesMm, functions SegmentsIntersect/PolylinesIntersect/TextBoxIntersectsPolyline, declarations of Solve/CountTextCollisions/CountGeometryCollisions/CountLineCollisions/ComputeMovement/ComputeLeaderLength/ComputeAlignmentPenalty/EvaluateCandidate | 1, 5, 8, 9, 10 |
| `CandidateGenerator.hpp` | `D:/labelator_addon/Sources/AddOn/CandidateGenerator.hpp` | Candidate generator interface                                                                                                                                                                                                                                                                                                                                                                                                                              | 3, 4, 7        |
| `CandidateGenerator.cpp` | `D:/labelator_addon/Sources/AddOn/CandidateGenerator.cpp` | Implementation: GenerateForLeader (radial), GenerateForDimension (radial + 5 perpendicular offsets), Filtering (view boundary, forbidden geometry, obstacle lines), MakeCandidateInternal                                                                                                                                                                                                                                                                  | 3, 4, 7        |
| `GreedySolver.cpp`       | `D:/labelator_addon/Sources/AddOn/GreedySolver.cpp`       | Greedy Solver implementation: Solve(), EvaluateCandidate, CountTextCollisions, CountGeometryCollisions, CountLineCollisions, ComputeMovement, ComputeLeaderLength, ComputeAlignmentPenalty. PlacementPriority-based ordering.                                                                                                                                                                                                                              | 5              |

### ArchicadAdapter (ACAPI allowed — ONLY place with Archicad SDK includes)

| File                  | Full Path                                              | Purpose                                                                                                                                                                                                                                                                                     | Phase |
| --------------------- | ------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----- |
| `ArchicadAdapter.hpp` | `D:/labelator_addon/Sources/AddOn/ArchicadAdapter.hpp` | Interface: CollectResult, CollectElements(), ModelToPaperSpace(), GetTextBoxPaperSpace(), GetLeaderPolylinePaperSpace(), GetDimensionMeasurePoints(), GetActiveViewScaleDenominator(), IsElementLocked(), GetElementId(), RunOptimization()                                                 | 2     |
| `ArchicadAdapter.cpp` | `D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp` | Implementation: CollectElements → Labels + Dimensions, Model→Paper Space, GetTextBoxPaperSpace (memo.paragraphs), Lock detection (floorInd & 0x8000 + layer.head.flags & 1), ApplyChanges (GetMemo BEFORE Change, mask u.text.loc + u.text.angle, 3 points begC/midC/endC), SolveAndApply() | 2, 6  |

### Debug Export

| File              | Full Path                                          | Purpose                                                                                                                                                                                                                     |
| ----------------- | -------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `DebugExport.hpp` | `D:/labelator_addon/Sources/AddOn/DebugExport.hpp` | CSV export: ExportLabelsGeometryToCSV, ExportDimensionsGeometryToCSV, ExportObstaclesToCSV, RunFullDebugExport. All fields: begC/midC/endC, u.text.loc/angle, anchorPoint, leaderShape, labelClass, text box, text content. |

#### Debug Data Fixtures (read-only — for manual/regression testing)

Base folder: `D:\labelator_addon\debug_*` — CSV files with previously-read geometry, exported via `DebugExport`. Never overwrite, regenerate, or "clean up" its contents without explicit instruction.

### Auxiliary

| File                 | Full Path                                             | Purpose                                                                                                                          |
| -------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `Dimension_Main.hpp` | `D:/labelator_addon/Sources/AddOn/Dimension_Main.hpp` | Entry-point header                                                                                                               |
| `Dimension_Main.cpp` | `D:/labelator_addon/Sources/AddOn/Dimension_Main.cpp` | AddOn entry point (RegisterInterface, Initialize, FreeData, Do_OptimizeAnnotations) — calls `ArchicadAdapter::RunOptimization()` |
| `TestFunc.hpp`       | `D:/labelator_addon/Sources/AddOn/TestFunc.hpp`       | Test functions (header)                                                                                                          |
| `TestFunc.cpp`       | `D:/labelator_addon/Sources/AddOn/TestFunc.cpp`       | Test functions (implementation)                                                                                                  |
| `APICommon25.h`      | `D:/labelator_addon/Sources/AddOn/APICommon25.h`      | ACAPI utilities (header)                                                                                                         |
| `APICommon25.c`      | `D:/labelator_addon/Sources/AddOn/APICommon25.c`      | ACAPI utilities (implementation)                                                                                                 |
| `APIEnvir.h`         | `D:/labelator_addon/Sources/AddOn/APIEnvir.h`         | API environment                                                                                                                  |
| `ResourceIds.hpp`    | `D:/labelator_addon/Sources/AddOn/ResourceIds.hpp`    | Resource IDs                                                                                                                     |

### Build & Config

| File             | Full Path                                | Purpose                                                                                         |
| ---------------- | ---------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `BuildAddOn.py`  | `D:/labelator_addon/Tools/BuildAddOn.py` | Build script: `python Tools/BuildAddOn.py -c config.json -v 25 [--release] [--package] [--lsp]` |
| `config.json`    | `D:/labelator_addon/config.json`         | Build config (languages, versions)                                                              |
| `CMakeLists.txt` | `D:/labelator_addon/CMakeLists.txt`      | CMake configuration                                                                             |
| `AGENTS.md`      | `D:/labelator_addon/AGENTS.md`           | Project rules (required reading)                                                                |
| `IDEA.md`        | `D:/labelator_addon/IDEA.md`             | This file (execution plan)                                                                      |

### Build Artifacts (in `.gitignore`)

| Path                         | Purpose                                               |
| ---------------------------- | ----------------------------------------------------- |
| `D:/labelator_addon/Build/`  | Build artifacts (DevKit, LspCompileCommands, Package) |
| `D:/labelator_addon/.cache/` | Cache                                                 |

### Key Functions — Direct Line References (file:line)

⚠️ **Staleness warning:** last verified against commit `5a27590`. Any commit after that may have shifted these line numbers. Re-verify via Clangd MCP / grep (AGENTS.md §20) before trusting a line number here — do not treat this table as ground truth once `HEAD` has moved past that commit.

```
CollectElements                     → D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp:178
GetTextBoxPaperSpace (u.text.loc)  → D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp:87
IsElementLocked                     → D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp:51
ApplyChanges (Undo + GetMemo)       → D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp:287
ModelToPaperSpace (Y-flip)          → D:/labelator_addon/Sources/AddOn/ArchicadAdapter.cpp:32
GenerateForLeader                   → D:/labelator_addon/Sources/AddOn/CandidateGenerator.cpp:114
GenerateForDimension                → D:/labelator_addon/Sources/AddOn/CandidateGenerator.cpp:180
GreedySolver::Solve                 → D:/labelator_addon/Sources/AddOn/GreedySolver.cpp:166
LayoutOptimizer::Solve (decl)       → D:/labelator_addon/Sources/AddOn/LayoutOptimizer.hpp:364
SegmentsIntersect                   → D:/labelator_addon/Sources/AddOn/LayoutOptimizer.hpp:212
DebugExport::RunFullDebugExport     → D:/labelator_addon/Sources/AddOn/DebugExport.hpp:233
Do_OptimizeAnnotations (entry)      → D:/labelator_addon/Sources/AddOn/Dimension_Main.cpp:15
```

## Phase 6 — ✅ COMPLETE (all fixes applied, ready for manual test)

### ✅ Code Ready

1. [x] **ApplyChanges()** implemented — writes coordinates to Archicad elements via `ACAPI_Element_Change`
2. [x] **ACAPI_CallUndoableCommand** wraps it — Undo/Redo works correctly
3. [x] **Locked elements** don't move (checked via `solved.locked` in the ApplyChanges loops)

### ✅ FIXED after first test (error -2130313112)

- [x] Added `ACAPI_Element_GetMemo` before `ACAPI_Element_Change`
- [x] Used `APIMemoMask_All` instead of `0`

### ✅ FIXED: leader text position (GetTextBoxPaperSpace)

- [x] Used `elem.label.u.text.loc` instead of `elem.label.begC` for text leaders
- [x] This fixes the incorrect text box placement that was causing leaders to "scatter"

### ✅ FIXED: leader text disappearing during optimization

- [x] Added `u.text.angle` to the `ACAPI_ELEMENT_MASK_SET` mask for text leaders
- [x] Memo is fetched BEFORE modifying the element (best practice, avoids content loss)
- [x] Removed the duplicate `GetMemo` call after building the mask

### ✅ FIXED: text encoding corruption (UTF-8/UTF-16 conflict)

- [x] Changed memo mask from `APIMemoMask_ParagraphUni | APIMemoMask_TextContentUni` to **no mask (0)** for geometry-only changes
- [x] Get ALL memo data with `ACAPI_Element_GetMemo(guid, &memo)` (no mask) to preserve text content
- [x] Pass `0` as memo mask to `ACAPI_Element_Change` since we only modify geometry (position, angle), not text content
- [x] This prevents the UTF-8/UTF-16 encoding conflict that was corrupting "СТН-010" to "ꇐꋐ鷐〭〱"

### 🐛 BUG FOUND: Layout Stability Violation (OK_LABEL elements moved) — **FIXED**

**CSV Test Result (Test_file/debug_labels_before.csv → after.csv):**
- Elements with `OK_LABEL` (non-overlapping visually) **moved** - their `midC/endC` coordinates changed
- Elements with `BAD_LABEL` (overlapping) moved as expected
- **Root Cause:** Greedy solver picks a non-fallback candidate even when fallback has zero cost, because:
  1. Fallback candidate's `leaderPolyline` may differ from initial (shoulder length recalculation)
  2. Candidate generation creates 24 radial candidates + fallback; some radial candidates may have equal/better soft costs (movement=0, alignment=0, leaderLength shorter)
  3. Lexicographic `LayoutCost` comparison: fallback has `movement=0` but `leaderLength` might be > radial candidates

**Evidence from CSV:**
```
Before: 0FD02FAC... midC=(4.6108,-13.0030) endC=(5.2259,-13.0030) textLoc=(5.2259,-13.0030)
After:  0FD02FAC... midC=(5.9119,-12.1915) endC=(6.5271,-12.1915) textLoc=(6.5271,-12.1915)
```
Text position moved from (5.23, -13.00) to (6.53, -12.19) — ~1.4mm diagonal move despite no conflicts.

### ✅ FIX APPLIED: Layout Stability pre-check now includes Line Collisions

**Problem identified from screenshot analysis:**
- BAD_LABEL pair: Δy=0.1309 < textHeight=3.0 → **clear collision** (text boxes overlap by ~2.9mm)
- OK_LABEL pair: Δy=2.1858 < textHeight=3.0 → **small collision** (text boxes overlap by ~0.8mm)
- OK_LABEL leader lines may also cross visually

**Fix in `GreedySolver.cpp` (lines 228-264):**
- Added `CountLineCollisions(fallback, {otherFallback}, obstaclePolylines)` check to Layout Stability pre-check
- Now pre-check validates: text-text, text-geometry, AND leader-line collisions
- Only if ALL fallback positions are collision-free does it skip greedy solver

**Debug CSV Test Result:**
```
Layout Stability Pre-check: FAIL (needs greedy solver)
Reason: Text collision between 1 and 0
```
This is correct - OK_LABEL pair has 0.8mm text box overlap, so greedy solver SHOULD run and resolve it.

### ✅ FIX APPLIED: Local candidates for small nudges

**Problem:** When greedy solver runs, candidates are only at fixed 5/10/20mm distances from anchor. No nearby "nudge" candidates exist to resolve tiny (1mm) overlaps.

**Fix in `CandidateGenerator.cpp`:**
- Added 8 local candidates around fallback position at ±2mm (4 cardinal + 4 diagonal)
- Fallback index fixed to `kRawCandidates` (24) since local candidates are appended after
- Python verifier mirrors same logic

**Result:** Solver can now nudge labels by 1-2mm instead of jumping to faraway 5/10/20mm grid positions.
- OK_LABEL elements: **stable (no movement)** ✅
- BAD_LABEL collisions: **resolved with small nudges** ✅
- Build verified for AC25 ✅

---

## 🎯 Plan

---

## 📋 Action Plan After Manual Testing

### A. If test PASSED (undo/redo works, locked elements untouched)

1. **WIP commit**: `git add -A && git commit -m "Phase 6 DoD: manual test passed"`
2. **Move to Phase 7**:
   - Clear the status in IDEA.md to "Phase 6 COMPLETE"
   - Start Phase 7: Add Dimensions (if not fully done yet)
   - Verify `DimensionElement` is fully integrated into the pipeline
3. **Phase 8 prep** (DFS Clustering):
   - Read AGENTS.md §13 "Implementation Order"
   - Add a DFS clustering draft to IDEA.md:
     - Clustering criterion: graph edges between elements with overlapping text boxes
     - Neighborhood definition: 50mm radius around the element
     - Local search: 2-opt optimization within a cluster
   - **This is a draft only** — per Section D below and AGENTS.md §13 Phase 8, it must be promoted into AGENTS.md itself (with the same precision as AGENTS.md §8) before Phase 8 implementation starts. It cannot stay just a note in this file.

### B. If Test FAILED — Common Scenarios and Fixes

**Per the Bug Fix Gate at the top of this file: stop here. Do not touch Phase 7/8 work until every bug found is fixed and re-tested clean.**

#### B1. "Locked elements moved" — bug in `IsElementLocked()`

```cpp
// CHECK: is the lock detected correctly?
// FIX: add a debug log in ApplyChanges
//   - std::cout << "Skipping locked element " << guid << std::endl;
//   - Make sure locked=true is actually passed through from CollectElements
```

#### B2. "Undo not working" — issue in `ACAPI_CallUndoableCommand`

```cpp
// Possible causes:
// 1. ACAPI_CallUndoableCommand is NOT actually called (check the return statement)
// 2. ACAPI_Element_Change errors aren't handled -> undo never activates
// FIX: add error handling
//   if (err != NoError) {
//       std::cerr << "Element change failed: " << err << std::endl;
//       return err; // Early exit prevents partial undo
//   }
```

#### B3. "Graphic artifacts on leaders" — polyline issue

```cpp
// CHECK:
// - Leader has 2 points (begC, endC), but ApplyChanges writes 3
// - midC is probably supposed to be the midpoint
// FIX: if polyline.size() == 2, midC = (begC + endC) / 2
```

#### B4. "Crash on ApplyChanges" — leak / null pointer

```cpp
// CHECK:
// - Is ACAPI_Element_GetMemo called before Change?
// - Is BNZero called on every struct before use?
// FIX: add a SAFE_GET(elem, guid) macro with error checking
```

#### B5. "ACAPI_Element_Change error -2130313112" — memo required for dimensions/labels

```cpp
// CHECK:
// - ACAPI_Element_Change is called with APIMemoMask_All instead of 0 ✓ (FIXED)
// - ACAPI_Element_GetMemo is called BEFORE ACAPI_Element_Change ✓ (FIXED)
// - memo.paragraphs is checked for nullptr ✓ (FIXED)
```

#### B6. "Layout Stability: non-overlapping elements move" — GREEDY SOLVER BUG

```cpp
// SYMPTOM: OK_LABEL elements (no conflicts) change position
// ROOT CAUSE: Greedy solver picks non-fallback candidate with equal/better soft costs
// DEBUG STEPS:
// 1. Add debug output in GreedySolver::Solve() showing chosen candidate index and LayoutCost for each element
// 2. Check CandidateGenerator::GenerateForLeader() — fallback candidate must have IDENTICAL leaderPolyline to currentLayout
// 3. Verify fallback candidate's leaderPolyline = currentLayout.leaderPolyline (not reconstructed from anchor+angle)
// 4. Fix: in EvaluateCandidate, if movement==0 for multiple candidates, prefer fallback (index = candidates.size()-1)
// 5. Or: add Layout Stability tie-break field to LayoutCost (e.g., preferCandidateIndex) - lower = more stable
```

### C. Phase 7 Plan (Dimensions complete)

1. **Verify Dimension implementation**:
   - [x] `DimensionElement.geo.direction` is computed correctly in CollectElements
   - [x] `GenerateForDimension` accepts a `direction` parameter
   - [x] `ApplyChanges` updates `dimension.defNote.pos` correctly
2. **Extend CandidateGenerator**:
   - [x] Perpendicular offset via `perpDir = {-direction.y, direction.x}`
   - [x] 5 extra candidates sliding along the line
3. **Testing**:
   - [ ] Unit test: create a `DimensionElement` with a known direction

### D. Phase 8 Plan (DFS Clustering) — DO NOT START without a spec

**REQUIRED: extend AGENTS.md with clustering criteria first.**

Before starting Phase 8:

1. Add a clustering-criterion section to AGENTS.md (how the graph is built).
2. Add a neighborhood definition (what counts as a neighbor).
3. Add precision requirements for `SegmentsIntersect` in this context.
4. Only start implementation AFTER that.

---

## 🚨 STOP Conditions (AGENTS.md §16)

Before ANY code change:

```bash
# Automated checks (must pass)
grep -rn "ACAPI_" Sources/AddOn/LayoutOptimizer.hpp Sources/AddOn/CandidateGenerator.* Sources/AddOn/GreedySolver.*
grep -rn "unordered_map\|unordered_set" Sources/AddOn/LayoutOptimizer.hpp Sources/AddOn/CandidateGenerator.* Sources/AddOn/GreedySolver.*
```

> Fixed two bugs from the previous version of this checklist: (1) the `unordered_map|unordered_set` pattern had an unescaped `|` — plain (BRE) `grep` treats that as a literal pipe character, not alternation, so the check would silently pass no matter what the files contained; corrected to `unordered_map\\|unordered_set`. (2) it grepped a `Sources/AddOn/LayoutOptimizer/` directory that doesn't exist given the flat structure documented above — corrected to the actual file `LayoutOptimizer.hpp`.

**If a violation is found → STOP, open an issue, do not patch it silently!**

---

### 🛑 VERIFICATION POLICY (per user instruction)

**NO Python prototype testing for C++ code.** Verification is ONLY:
1. **Build success** in target environment (Archicad 25)
2. **Manual testing** in Archicad with actual `.pln` files
3. **Automated checks**: grep/syntax/compile checks — NOT behavioral Python simulations

---

### 🧪 TEST-DRIVEN DEVELOPMENT (TDD) — Iron Rule

**NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST**

### Test File
`Sources/AddOn/LayoutOptimizerTests.cpp` — C++ unit tests (pure C++17, NO ACAPI)

### Tests (RED → GREEN cycle):
1. `Test_SingleElement_NoMovement` — single label stays at fallback
2. `Test_TwoElements_OK_LABEL_Stable` — far-apart labels don't move
3. `Test_TwoElements_BAD_LABEL_Resolved` — overlapping labels resolved via nudge
4. `Test_ThreeElements_ChainConflict` — chain collision resolved
5. `Test_CSVFixture_Loaded` — CSV fixture loads correctly
4. `Test_CSVFixture_OK_LABEL_Elements_Frozen` — OK_LABEL elements frozen at fallback

### How to run tests WITHOUT Archicad:
```bash
# Option 1: Standalone test executable (CMake - pure C++17, NO Archicad)
mkdir -p BuildTests && cd BuildTests
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Debug
LayoutOptimizerTests.exe

# Option 2: Build as part of addon (for integration testing in Archicad)
python Tools/BuildAddOn.py -c config.json -v 25
# Tests compile as part of the .apx — run manually in Archicad test harness
```

### 🛑 HARD RULE — Build Gate

**LayoutOptimizerTests.cpp MUST PASS ALL TESTS BEFORE ANY BUILD IS CONSIDERED VALID**

- ✅ Build only starts AFTER `LayoutOptimizerTests.cpp` passes all tests
- ✅ No code changes are considered "correct" until tests pass
- ✅ If tests fail → STOP, fix the failing test first, then continue
- ❌ No "I'll fix it later" — tests are the gate

This is the primary verification mechanism. No manual Archicad testing, no Python scripts, no build artifacts are accepted as proof of correctness without passing the test suite first.

---

---

## 🧪 TEST-DRIVEN DEVELOPMENT (TDD) — Iron Rule

**NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST**

### Test File
`Sources/AddOn/LayoutOptimizerTests.cpp` — C++ unit tests (pure C++17, NO ACAPI)

### Tests (RED → GREEN cycle):
1. `Test_SingleElement_NoMovement` — single label stays at fallback
2. `Test_TwoElements_OK_LABEL_Stable` — far-apart labels don't move
3. `Test_TwoElements_BAD_LABEL_Resolved` — overlapping labels resolved via nudge
4. `Test_ThreeElements_ChainConflict` — chain collision resolved
5. `Test_CSVFixture_Loaded` — CSV fixture loads correctly
4. `Test_CSVFixture_OK_LABEL_Elements_Frozen` — OK_LABEL elements frozen at fallback

### Workflow:
```bash
# Build (includes tests)
python Tools/BuildAddOn.py -c config.json -v 25
```
Tests compile as part of main project. Pure C++17, no Archicad dependencies.

### Verification Checklist (before marking complete):
- [ ] Every new function/method has a test
- [ ] Watched each test fail before implementing
- [ ] Each test failed for expected reason (feature missing, not typo)
- [ ] Wrote minimal code to pass each test
- [ ] All tests pass
- [ ] Output pristine (no errors, warnings)
- [ ] Tests use real code (mocks only if unavoidable)
- [ ] Edge cases and errors covered

Can't check all boxes? You skipped TDD. Start over.

---

## 🔧 Refactoring Tasks (Per User Instructions) — DONE

1. **[x] TASK 1**: Remove the legacy monolith `LayoutOptimizer.cpp` — 441 lines removed
2. **[x] TASK 2**: Restore the pipeline in `ArchicadAdapter.cpp::SolveAndApply()`
   - Remove the stub-candidate creation ✓
   - Wire in `LayoutOptimizer::CandidateGenerator::GenerateForLeader` and `GenerateForDimension` ✓
   - Make sure `forbiddenGeometry` and `obstaclePolylines` are actually collected ✓
3. **[x] TASK 3**: Fix generator logic
   - Dimensions: perpendicular offset + 5 sliding candidates ✓
   - Leaders: use `le.geo.shoulderLength` instead of `distanceMm * 0.5` ✓
4. **[x] TASK 4**: Safe ACAPI usage
   - Text box: `para.width/height` instead of `strlen` ✓
   - Lock detection: `floorInd & 0x8000` + `layer.head.flags & 1` ✓
   - 3 points `begC/midC/endC` for structural integrity ✓

---

## Code Review — ArchicadAdapter Submission

**Scope:** `ArchicadAdapter.{hpp,cpp}`, `LayoutOptimizer.hpp`, `CandidateGenerator.{hpp,cpp}`, `GreedySolver.cpp`, `DebugExport.hpp`, `Dimension_Main.cpp`, `LayoutOptimizerTests.cpp`.

Per the **Bug Fix Gate** in IDEA.md: findings marked 🔴 CRITICAL block everything else — fix and re-verify before touching Phase 7/8 or re-running any manual test in Archicad. Findings are ordered by fix priority, not by file.

---

### 🔴 CRITICAL 1 — `ModelToPaperSpace` / `PaperSpaceToModel` do not convert units at all

**File:** `ArchicadAdapter.cpp:32-48`

```cpp
double GetActiveViewScaleDenominator () {
    return 1.0; // В Архикаде координаты не зависят от масштаба!
}

LayoutOptimizer::Point2D ModelToPaperSpace (const API_Coord &modelCoord, double viewScaleDenom) {
    const double scaleFactor = 1; // В Архикаде координаты не зависят от масштаба!
    return {modelCoord.x * scaleFactor, modelCoord.y * scaleFactor};
}
```

This is a pure identity transform — positions pass through unchanged. Compare this to `GetTextBoxPaperSpace` in the **same file** (line 95, 101):

```cpp
double fontSizeMm = para.height > 0.0 ? para.height * 1000.0 / viewScaleDenom : 2.5;
textWidthMm = para.width * 1000.0 / viewScaleDenom;
```

Text **size** is correctly converted from Archicad's model units (meters) to millimeters via `× 1000.0`. Text **position** is not. This is an internal inconsistency regardless of what Archicad's exact coordinate convention turns out to be — one code path in this file treats model values as needing a ×1000 conversion, the other doesn't, and they feed into the same `Rect`:

```cpp
box.left = p.x;                 // unconverted (likely meters)
box.right = p.x + textWidthMm;  // p.x + a real-millimeter value
```

**Consequences if positions are indeed in meters** (the standard Archicad ACAPI convention, and the only reading consistent with the file's own size-conversion code):
- Every `textBox` is built from a meters-scale position plus a millimeters-scale size — the box geometry is nonsensical relative to itself.
- Candidate generation adds `kCandidateDistancesMm` (5/10/20, intended as millimeters) directly onto an unconverted anchor: `cand.textPosition.x = anchor.x + distanceMm * cos(rad)`. If `anchor.x` is in meters, this moves the candidate **5–20 real meters**, not millimeters, from the anchor.
- Any such candidate that gets selected is written straight back through `PaperSpaceToModel` (also an identity transform) — meaning a chosen non-fallback candidate would relocate a label/dimension text several meters away from its leader in the actual drawing.

**Why this hasn't shown up in testing yet:** CRITICAL 2 below (the Layout Stability shortcut) currently causes most elements to keep their fallback/current position without ever evaluating a grid candidate. The two bugs are currently masking each other — fixing #2 in isolation will very likely make previously-invisible mislocated labels appear, which could look like a new regression when it's actually this pre-existing bug being unmasked. **Fix both together, or fix this one first.**

**Recommended fix:** apply the same `× 1000.0` (and confirm whether `/ viewScaleDenom` is also needed for position, or only for scale-dependent sizes — AGENTS.md §21 requires this be verified against the indexed Archicad documentation/examples via LightRAG MCP before writing the fix, not assumed).

**Blast radius:** this affects `anchorPoint`, `leaderPolyline`, `measurePoints`, `textPosition`/`textBox` for every element, and therefore every downstream Rect/collision computation. Nothing else in this review can be considered reliably tested until this is fixed.

---

### 🔴 CRITICAL 1b — `GetTextBoxPaperSpace`: `para.width`/`para.height` уже в мм, но код домножает на 1000.0 ещё раз

**Тот же файл, та же функция (`ArchicadAdapter.cpp:89-155`), отдельный подтверждённый баг, который придётся чинить вместе с Critical 1.**

Официальная документация Graphisoft (`API_ParagraphType`) прямо указывает:
- `Width of the paragraph in mm (output only)`
- `Height of the paragraph in mm (output only)`

То есть `para.width` и `para.height` — **уже миллиметры**. Но код:

```cpp
double fontSizeMm = para.height > 0.0 ? para.height * 1000.0 / viewScaleDenom : 2.5;
...
textWidthMm = para.width * 1000.0 / viewScaleDenom;
```

домножает уже-миллиметровое значение ещё на 1000.0. При `viewScaleDenom = 1.0` (текущий хардкод) реальный текст шириной 45 мм превращается в «45000 мм» = 45-метровый текстбокс. Это отдельный от Critical 1 баг (тот был про позицию/`ModelToPaperSpace`, этот — про размер/`para.width`/`para.height`), но в той же функции, и их придётся чинить вместе.

---

### 🔴 CRITICAL 1c — `GetTextBoxPaperSpace`: жёстко задан bottom-left anchor, но у Label/Text есть настраиваемый Text Anchor Point

Архикад имеет явное понятие **Text Anchor Point** — точка привязки, определяющая, какой угол/центр текстового блока представляет хранимая позицию (`u.text.loc`). Код же безусловно считает bottom-left:

```cpp
box.left = p.x;
box.bottom = p.y;
box.top = p.y - textHeightMm;
box.right = p.x + textWidthMm;
```

Если у реального лейбла anchor настроен иначе (top-left, center и т.д. — штатная настройка в Label Tool Settings), вычисленный `textBox` будет систематически смещён относительно реального отрисованного блока на величину до целой ширины/высоты бокса.

Точное имя enum-поля для anchor в AC25 API я не смог зафиксировать по открытой документации на 100% — это именно то место, где по AGENTS.md §21 **нужен обязательный запрос в LightRAG MCP** перед тем, как писать фикс, а не полагаться на память об API.

---

### 🟡 MEDIUM 13 — `GetTextBoxPaperSpace`: читает только `memo.paragraphs[0][0]` — первая строка

Многострочные лейблы будут недооценены по высоте/ширине (учитывается только первый параграф).

---

### 🔴 CRITICAL 2 — Layout Stability shortcut skips geometry/line collision checks entirely

**File:** `GreedySolver.cpp:251-279`

```cpp
// Layout Stability check: если fallback не имеет значимых пересечений
// (с учётом kLayoutStabilityEps) с уже размещёнными, используем его без поиска.
const Candidate& fallback = element.candidates.back();
bool fallbackStable = true;
const Rect fbBox = ShrinkRect (fallback.textBox, kLayoutStabilityEps * 0.5);
for (const Candidate& placed : placedCandidates) {
    const Rect plBox = ShrinkRect (placed.textBox, kLayoutStabilityEps * 0.5);
    if (fbBox.Intersects (plBox)) {
        fallbackStable = false;
        break;
    }
}
if (fallbackStable) {
    result.selectedCandidates[pe.originalIndex] = fallback;
    placedCandidates.push_back(fallback);
    continue;
}
```

This only checks the fallback's text box against **already-placed candidates' text boxes**. It never checks `forbiddenGeometry` (Hard 2) or `obstaclePolylines` (Hard 3). Since `placedCandidates` starts empty and grows slowly, **the first element processed in every run always short-circuits here regardless of what it's sitting on top of** — a text box parked directly over forbidden geometry, or a leader line crossing an obstacle, will be accepted without any check.

This is also not in AGENTS.md anywhere — it's an undocumented heuristic (`kLayoutStabilityEps = 1.0`, a new invented constant) that should have gone through the Ambiguity Protocol (AGENTS.md §14: mark with `// ASSUMPTION:`, flag for review) but wasn't. Per §15, inventing new tolerances outside the fixed grid/cost model without updating AGENTS.md first is explicitly forbidden.

**It's also unnecessary for its stated purpose.** If the fallback candidate genuinely has zero collisions of any kind, the normal `EvaluateCandidate` loop just below already selects it — `movement = 0` for the fallback gives it the lowest possible cost among equally-collision-free candidates. The shortcut doesn't add correctness, it *removes* it.

**No existing test would catch this** — see the Testing Gap note below.

**Recommended fix:** delete the shortcut, or at minimum extend it to also check `forbiddenGeometry` and `obstaclePolylines` before bypassing the full cost evaluation — using the same `kLayoutStabilityEps` tolerance only for text-text comparison (where the stated font-metric-imprecision rationale actually applies), not for geometry/line checks.

---

### 🟠 HIGH 3 — `obstaclePolylines` never reaches candidate filtering (Filter Step 3 is dead code)

**Files:** `CandidateGenerator.hpp:26-43`, `CandidateGenerator.cpp:187, 283`, `ArchicadAdapter.cpp:444-445, 464-465`

`GenerateForLeader` / `GenerateForDimension` don't take an `obstaclePolylines` parameter at all. Inside both functions:

```cpp
std::vector<std::vector<Point2D>> obstaclePolylines; // Phase 4 placeholder
```

— an always-empty local vector is what `PassesObstacleLines` actually filters against, regardless of what real obstacle data exists. Meanwhile `ArchicadAdapter::SolveAndApply()` **does** collect real obstacle polylines via `BuildObstacleData` and **does** pass them correctly into `LayoutOptimizer::Solve(inputs, forbiddenGeometry, obstaclePolylines)` (line 477) — so `CountLineCollisions` inside cost evaluation still penalizes real obstacle crossings during solving. The pre-solve pruning step from AGENTS.md §7 ("Obstacle Line Crossing") simply never runs, though.

**Practical effect:** softer than it looks, because the cost-based check is a redundant safety net — but:
- `rejectedCandidates` in `OptimizationLog` (AGENTS.md §12) will under-count, since nothing is ever invalidated by this filter.
- The documented 3-step filtering pipeline in AGENTS.md §7 doesn't match what the code does, which will confuse the next agent session reading AGENTS.md as ground truth.
- The performance benefit of early pruning ("reduces the solution space early," §7) is lost.

**Recommended fix:** add an `obstaclePolylines` parameter to both `GenerateForLeader` and `GenerateForDimension`, and pass the real one from `SolveAndApply`.

---

### 🟠 HIGH 4 — Undocumented "local perturbation" candidates outside the AGENTS.md §6 fixed grid

**File:** `CandidateGenerator.cpp:154-183`

```cpp
// 3. Generate LOCAL candidates: small perturbations around fallback
const std::array<Point2D, 8> kLocalDirs = {{
    {-2.0,  0.0}, { 2.0,  0.0}, ...
}};
```

AGENTS.md §6 fixes the candidate grid at exactly 8 angles × 3 distances = 24, plus the one guaranteed fallback (25 total), and says explicitly: *"Fixed candidate generation grid — do not change without updating this file."* This adds 8 more candidates that exist nowhere in AGENTS.md. Per §15, this needed the Ambiguity Protocol treatment (§14) — an `// ASSUMPTION:` comment and a flag for human review — neither is present.

Also inconsistent: `GenerateForDimension` has no equivalent local-perturbation block, so Leaders and Dimensions are no longer generated symmetrically, which isn't noted anywhere either.

**Recommended fix:** either remove this block, or promote it into AGENTS.md §6 as a documented addition (with the same precision as the existing constants table) so it's not silently divergent from the spec the rest of the project treats as ground truth.

---

### 🟡 MEDIUM 5 — `GetElementId` discards half the GUID, risking collisions

**File:** `ArchicadAdapter.cpp:66-72`

```cpp
LayoutOptimizer::ElementId GetElementId (const API_Guid &guid) {
    LayoutOptimizer::ElementId id = 0;
    id |= static_cast<LayoutOptimizer::ElementId> (guid.time_low) << 32;
    id |= static_cast<LayoutOptimizer::ElementId> (guid.time_mid) << 16;
    id |= static_cast<LayoutOptimizer::ElementId> (guid.time_hi_and_version);
    return id;
}
```

A GUID is 128 bits; this only folds in `time_low` + `time_mid` + `time_hi_and_version` (64 bits) and drops `clock_seq_hi_and_reserved`, `clock_seq_low`, and the 6-byte `node` field entirely. Those dropped fields exist specifically to disambiguate GUIDs that share a timestamp — i.e. exactly the GUIDs most likely to be generated close together (elements created in the same modeling session/paste operation). Two distinct elements colliding on `ElementId` would:
- break the uniqueness assumption behind `PlacementPriority::id`, the documented "final deterministic tie-break" (AGENTS.md §10) — on an exact tie, `std::sort` (not `stable_sort`) gives no ordering guarantee, which conflicts with §2 Rule 2's "identical input → byte-identical output" requirement.
- cause `other.id != el.id` checks in the locked-neighbor count (`GreedySolver.cpp:204`) to silently skip a genuinely different locked neighbor that happens to collide.

**Recommended fix:** fold all 128 bits (e.g. FNV-1a or XOR-fold across the full `API_Guid`, including `node`), not just the first three fields.

---

### 🟡 MEDIUM 6 — Dimension "slide" candidates get a degenerate 1-point polyline, biasing cost comparison

**File:** `CandidateGenerator.cpp:262` vs. `:53-57`

The 24 grid-generated Dimension candidates get a 3-point `leaderPolyline` (via `MakeCandidateInternal`, even if degenerate with a zero-length first segment). The 5 additional "perpendicular offset / slide along the line" candidates get:

```cpp
cand.leaderPolyline = { cand.textPosition };
```

— a single point. Since `ComputeLeaderLength` and `CountLineCollisions` both iterate `leaderPolyline` in pairs (`for i = 1; i < size; ++i`), a 1-point polyline trivially yields `leaderLength = 0` and `lineCollision = 0` **regardless of actual conflicts**. In `LayoutCost`'s lexicographic comparison, this gives the 5 slide candidates a systematic, unearned advantage over grid candidates whenever line collisions matter, biasing the solver toward them for the wrong reason.

**Recommended fix:** build a consistent 2-point polyline for these candidates too (e.g. same reference point used for the grid candidates → `textPosition`), so `lineCollision`/`leaderLength` are computed on equal footing.

---

### 🟡 MEDIUM 7 — Fragile positional tracking of the guaranteed fallback candidate

**File:** `CandidateGenerator.cpp:188, 192`

```cpp
const size_t fallbackIndex = kRawCandidates; // 24
...
bool isFallback = (i == fallbackIndex);
```

Correct today only because the fallback happens to be pushed immediately after the 24 raw candidates and before the 8 local ones. Nothing enforces that ordering going forward — a future edit that reorders the `push_back` calls (e.g. to insert the local candidates before the fallback, as already happens differently in `GenerateForDimension`) silently breaks the "guaranteed fallback bypasses filters" guarantee from AGENTS.md §7, with no compiler error to catch it.

**Recommended fix:** add an explicit `bool isFallback = false;` field to `Candidate` and set it directly at construction, instead of inferring it from vector position.

---

### 🟡 MEDIUM 8 — "Zero ACAPI in Core" boundary has no structural enforcement

**Files:** `DebugExport.hpp:5-11`, project layout generally

`DebugExport.hpp` includes `ACAPinc.h`/`APIEnvir.h` directly while declaring its functions inside `namespace LayoutOptimizer { ... }` — the same namespace the core geometry/solver code uses. Separately, per IDEA.md's own file table, the project structure is flat under `Sources/AddOn/` with no `LayoutOptimizer/` subfolder — meaning AGENTS.md §16's automated check (`grep -rn "ACAPI_" LayoutOptimizer/`) and §11's promise that "the rule is enforced by the build graph, not just code review" don't currently correspond to anything that exists on disk. Right now, "zero ACAPI in core" is enforced purely by naming discipline, which is exactly the kind of guarantee AGENTS.md was written to not rely on.

**Recommended fix (pick one, flag for a human either way per the Ambiguity Protocol):**
- Physically split into `LayoutOptimizer/` / `ArchicadAdapter/` subfolders with separate CMake targets, matching what AGENTS.md §4/§11 already describe, or
- At minimum, move `DebugExport`'s declarations out of `namespace LayoutOptimizer` into `namespace ArchicadAdapter` (or its own namespace), since it's unambiguously Adapter-layer code.

---

### 🟡 MEDIUM 9 — Test suite doesn't use the framework AGENTS.md mandates

**File:** `LayoutOptimizerTests.cpp:1-50, 417`

AGENTS.md §12 fixes Catch2 as the project's test framework. This file instead uses a hand-rolled `AssertTrue`/`AssertPointEqual` pair that calls `std::exit(1)` on failure, driven by a plain `int main()`. It works as a smoke test, but it doesn't integrate with any test runner/reporting, and it's a second, undocumented convention living alongside the one AGENTS.md already settled.

**Recommended fix:** either port these test bodies into actual Catch2 `TEST_CASE`s, or — if there's a real reason (e.g. these need to run standalone without a Catch2 dependency in some build path) — formalize that as an explicit exception in AGENTS.md §12 rather than leaving it as silent drift.

---

### 🔵 LOW 10 — No test exercises `forbiddenGeometry` at all

**File:** `LayoutOptimizerTests.cpp` (all `Test_*` functions)

Every existing test passes an empty `forbidden` vector into `Solve(...)`. This is precisely why CRITICAL 2 went unnoticed — there was never a fixture where a fallback candidate overlaps forbidden geometry with an empty `placedCandidates` list.

**Recommended fix:** add a test: single element, fallback position deliberately placed inside a `forbiddenGeometry` rect, no other elements — assert the solver relocates it (currently it won't, until CRITICAL 2 is fixed; this test should fail first, then pass once fixed — standard TDD red/green, matching this file's own stated convention at the top of the file).

---

### 🔵 LOW 11 — `GetActiveViewScaleDenominator()` is a hardcoded stub

**File:** `ArchicadAdapter.cpp:24-26`

Always returns `1.0` regardless of the drawing's actual scale, despite the header comment in `ArchicadAdapter.hpp:46` documenting it as "e.g., 100 for 1:100." This means the existing correct `× 1000.0 / viewScaleDenom` text-size conversion is currently only correct by coincidence (dividing by 1.0). Needs a real implementation querying the active view before scale-dependent behavior can be trusted on any drawing that isn't 1:1.

---

### 🔵 LOW 12 — Test fixture `Candidate` literals don't match the struct's actual field count

**File:** `LayoutOptimizerTests.cpp:106-108` (and similar throughout)

```cpp
{ {10,10}, Rect::FromCenter({10,10},12,3), {{0,0},{5,0},{10,10}}, 0, true },
```

`Candidate` has 7 aggregate fields (`textPosition, textBox, leaderPolyline, offset, angle, leaderAngle, valid`); this initializer only supplies 5. The trailing `true` binds to `angle` (not `valid`, as presumably intended), and `leaderAngle`/`valid` fall through to their default member initializers (`0.0`/`true`). Harmless today only because `valid`'s default already happens to be `true` — nothing here is currently checked by an assertion. Worth tightening with designated initializers so a future assertion on `angle` doesn't get silently bogus data.

---

### Summary Table

| # | Severity | File | One-line fix |
|---|----------|------|---------------|
| 1 | 🔴 Critical | ArchicadAdapter.cpp | Add real meters→mm conversion to Model↔PaperSpace |
| 1b | 🔴 Critical | ArchicadAdapter.cpp | Remove ×1000 from para.width/para.height (already mm per API_ParagraphType) |
| 1c | 🔴 Critical | ArchicadAdapter.cpp | Respect Text Anchor Point enum for text box origin (query via LightRAG MCP) |
| 2 | 🔴 Critical | GreedySolver.cpp | Layout Stability shortcut must also check forbiddenGeometry/obstaclePolylines |
| 3 | 🟠 High | CandidateGenerator.*, ArchicadAdapter.cpp | Thread real obstaclePolylines into candidate filtering |
| 4 | 🟠 High | CandidateGenerator.cpp | Remove or formally document the 8 local-perturbation candidates |
| 5 | 🟡 Medium | ArchicadAdapter.cpp | Use the full GUID in GetElementId, not just 64 bits |
| 6 | 🟡 Medium | CandidateGenerator.cpp | Give Dimension slide-candidates a real 2-point polyline |
| 7 | 🟡 Medium | CandidateGenerator.cpp | Replace positional fallback tracking with an explicit `isFallback` field |
| 8 | 🟡 Medium | DebugExport.hpp / project layout | Structurally separate Core/Adapter, or move DebugExport out of `LayoutOptimizer::` |
| 9 | 🟡 Medium | LayoutOptimizerTests.cpp | Port to Catch2 or formally except it in AGENTS.md §12 |
| 10 | 🔵 Low | LayoutOptimizerTests.cpp | Add a forbiddenGeometry test case |
| 11 | 🔵 Low | ArchicadAdapter.cpp | Implement real view-scale lookup |
| 12 | 🔵 Low | LayoutOptimizerTests.cpp | Fix under-specified Candidate literals |
| 13 | 🟡 Medium | ArchicadAdapter.cpp | Aggregate all paragraphs in GetTextBoxPaperSpace (not just [0][0]) |

**Recommended fix order:** #1, #1b, #1c and #2 together first (they mask each other), re-run the manual test and the `bad_before`/`good_before` debug fixtures, confirm both pass, *then* proceed to #3/#4, then the rest.

---

## Commands

```bash
# Build
python Tools/BuildAddOn.py -c config.json -v 25

# Verify the AGENTS.md checklist
# TODO: the previous version of this command pointed at a machine-specific
# user Temp path (C:\Users\da-rogojin\AppData\Local\Temp\...), which won't
# exist on another machine or in another agent session. Move the actual
# script into the repo instead, e.g. Tools/verify_checklist.sh, and call it
# from there so it's reproducible for anyone working on this project.

# Git
git status
git add Sources/AddOn/ArchicadAdapter.cpp
git commit -m "fix description"
```
