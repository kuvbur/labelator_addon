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

### 🐛 BUG FOUND: Layout Stability Violation (OK_LABEL elements moved)

**CSV Test Result (Test_file/debug_labels_before.csv → after.csv):**
- Elements with `OK_LABEL` (non-overlapping visually) **moved** - their `midC/endC` coordinates changed
- Elements with `BAD_LABEL` (overlapping) moved as expected
- **Root Cause Candidate:** Greedy solver picks a non-fallback candidate even when fallback has zero cost, because:
  1. Fallback candidate's `leaderPolyline` may differ from initial (shoulder length recalculation)
  2. Candidate generation creates 24 radial candidates + fallback; some radial candidates may have equal/better soft costs (movement=0, alignment=0, leaderLength shorter)
  3. Lexicographic `LayoutCost` comparison: fallback has `movement=0` but `leaderLength` might be > radial candidates

**Evidence from CSV:**
```
Before: 0FD02FAC... midC=(4.6108,-13.0030) endC=(5.2259,-13.0030) textLoc=(5.2259,-13.0030)
After:  0FD02FAC... midC=(5.9119,-12.1915) endC=(6.5271,-12.1915) textLoc=(6.5271,-12.1915)
```
Text position moved from (5.23, -13.00) to (6.53, -12.19) — ~1.4mm diagonal move despite no conflicts.

### 🔧 FIX APPLIED: Layout Stability pre-check now includes Line Collisions

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

### ⏳ Next: Manual Archicad Test Required

- [ ] Build and load `labelator.apx` in Archicad 25
- [ ] Run "Optimize Annotations" on `test_25.pln`
- [ ] Verify: BAD_LABEL pair resolves overlap (moves apart to Δy >= 3mm)
- [ ] Verify: OK_LABEL pair - check if small movement is acceptable or if text box calculation needs adjustment
- [ ] Verify: Text content preserved (no "O" truncation or "ꇐꋐ鷐〭〱" corruption)
- [ ] Verify: Undo/Redo works, locked elements unchanged

**Test Artifacts Ready:**
- `Build/labelator/25/Debug/labelator.apx` — compiled addon
- `test_25.pln` / `test_25.bpn` — test project
- `Test_file/debug_labels_before.csv` — input fixture (4 labels: 2 OK, 2 BAD)
- `Tools/test_csv_optimizer.py` — automated CSV verification script

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

> Fixed two bugs from the previous version of this checklist: (1) the `unordered_map|unordered_set` pattern had an unescaped `|` — plain (BRE) `grep` treats that as a literal pipe character, not alternation, so the check would silently pass no matter what the files contained; corrected to `unordered_map\|unordered_set`. (2) it grepped a `Sources/AddOn/LayoutOptimizer/` directory that doesn't exist given the flat structure documented above — corrected to the actual file `LayoutOptimizer.hpp`.

**If a violation is found → STOP, open an issue, do not patch it silently!**

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
