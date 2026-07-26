# Текущий план разработки (Phase 6: ArchicadAdapter — ✅ COMPLETE)

## Реальный статус (проверено git status)
- **Git**: Clean, 5 commits ahead of origin/master
  - `3582088` "Phase 7 Planning: Detailed Phase 7 plan for Dimension chains"
  - `824950b` "Phase 6 Complete: ArchicadAdapter implementation with LayoutOptimizer integration"
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)

## Phase 6 — ✅ COMPLETE (ArchicadAdapter)

### ✅ Выполнено
1. **Восстановлены** `ArchicadAdapter.cpp` / `ArchicadAdapter.hpp` из `.bak`
2. **Исправлены критические ошибки компиляции**:
   - Добавлены недостающие includes: `ACAPinc.h`, `APIEnvir.h`
   - Исправлена сигнатура `ACAPI_Element_GetElemList` (параметр — указатель на `GS::Array<API_Guid>`)
   - Исправлен доступ к `typeID` в `API_Elem_Head` (убраны каскадные ошибки)
   - Исправлено предупреждение C4018 (signed/unsigned mismatch) — `Int32` → `UInt32`
   - Исправлены поля `API_TextType` (`loc` вместо `pos`/`textPosition`)
   - Исправлен API `LayoutOptimizer::Candidate` (`textPosition` вместо `placement`)
3. **Верификация**: `Build succeeded!` подтверждено скриптом `hermes-verify-build-archicad-adapter.sh`
4. **Интеграция LayoutOptimizer::Solve()** в adapter flow — **РЕАЛИЗОВАНО** в `SolveAndApply()` и `RunOptimization()`
5. **Undo/Redo** — **РЕАЛИЗОВАНО** через `ACAPI_CallUndoableCommand` в `ApplyChanges()`
6. **Locked elements** — **РЕАЛИЗОВАНО** (пропускаются в ApplyChanges, но учитываются как препятствия)
7. **Подключение к Menu Handler** в `Dimension_Main.cpp::Do_OptimizeAnnotations()` → вызывает `ArchicadAdapter::RunOptimization()`

### Архитектура (AGENTS.md Section 4):
```
[Archicad API] → [ArchicadAdapter] → [LayoutOptimizer::Solve()] → [ArchicadAdapter::ApplyChanges()] → [ACAPI]
```

### Главный файл интеграции:
- `Dimension_Main.cpp` — entry point Add-On (подключен adapter)

---

## Анализ ошибок в этой сессии

### ❌ Ошибки (согласно правилам)
1. **Игнорирование RAG** — пытался угадать поля `API_ElementMemo` (`textBlocks`, `polygon`) вместо проверки через lightrag
   - **Исправление**: Использовал lightrag для проверки правильных полей (`coords`, `paragraphs`, `textContent`)
   - **Урок**: Всегда использовать RAG для проверки структур SDK

2. **Неправильный `using`** — написал `using LO = LayoutOptimizer;` (такого в C++ нет)
   - **Исправление**: Убрал `using`, использую полные имена `LayoutOptimizer::`
   - **Урок**: Не изобретать синтаксис, проверять в стандарте C++

3. **Неправильная сигнатура `ACAPI_Element_GetElemList`** — передавал значение вместо указателя
   - **Исправление**: Исправил на `&guids` (указатель на `GS::Array<API_Guid>`)
   - **Урок**: Проверять сигнатуры функций через RAG или заголовочные файлы

4. **Отсутствие includes** — `ACAPinc.h` и `APIEnvir.h` не были включены
   - **Исправление**: Добавил в начало `ArchicadAdapter.cpp`
   - **Урок**: Соблюдать порядок includes: системные → проектные → свои

5. **Зацикливание на ошибке** — тратил время на исправление `typeID`, не проверив сначала инклуды
   - **Исправление**: Добавил нужные includes, ошибка исчезла
   - **Урок**: При каскадных ошибках проверять базовые вещи (includes, namespace)

### ✅ Правильные действия
1. **Соблюдение плоской структуры** — все файлы в `Sources/AddOn/`
2. **Обновление IDEA.md** — после каждого значимого шага
3. **Верификация через скрипт** — создал `hermes-verify-build-archicad-adapter.sh`
4. **Использование git** — коммит после каждого исправления

---

## Команды
```bash
# Сборка
python Tools/BuildAddOn.py -c config.json -v 25

# Верификация
bash C:\\Users\\da-rogojin\\AppData\\Local\\Temp\\hermes-verify-build-archicad-adapter.sh

# Git
git status
git add Sources/AddOn/ArchicadAdapter.cpp Sources/AddOn/ArchicadAdapter.hpp
git commit -m "описание"
```

---

## Code Review: LayoutOptimizer Core (Pre-Phase 7) — ✅ VERIFIED

### ✅ Compliant with AGENTS.md
| Check | Status | Notes |
|-------|--------|-------|
| No ACAPI/GSRoot includes in LayoutOptimizer | ✅ PASS | Pure C++17, stdlib only |
| Deterministic containers (no unordered_map/set in solver) | ✅ PASS | Uses `std::map`, `std::vector` with explicit sort |
| Fixed candidate grid constants (Section 6) | ✅ PASS | 8 angles × 3 distances = 24 + fallback |
| Guaranteed fallback candidate (Section 7) | ✅ PASS | Added last, `valid = true`, bypasses filters |
| Symmetric epsilon AABB (1e-4) | ✅ PASS | `Rect::Intersects` with `eps = 1e-4` |
| Deterministic segment intersection | ✅ PASS | `SegmentsIntersect` with collinear overlap handling |
| LayoutCost lexicographic tuple | ✅ PASS | 6-field tie-break: textCollision, geometryCollision, lineCollision, movement, leaderLength, alignmentPenalty |
| PlacementPriority ordering | ✅ PASS | typeRank=0(Dim), lockedNeighborCount, invalidCandidateCount, id |
| No R-Tree/QuadTree | ✅ PASS | O(n²) acceptable for <500 elements |
| No multithreading | ✅ PASS | Single-threaded |
| M_PI defined for MSVC | ✅ PASS | `#ifndef M_PI` guard added |

### ⚠️ Issues to Fix Before Phase 7

| Issue | File:Line | Severity | Fix |
|-------|-----------|----------|-----|
| `SolveLeadersGreedy` / `SolveDimensionsGreedy` unused | LayoutOptimizer.cpp:245, 333 | LOW | Remove or mark as legacy; `Solve()` is the canonical API |
| Duplicate cost evaluation logic | LayoutOptimizer.cpp:26-133 | MEDIUM | Consolidate `EvaluateCandidateCost` (per-element) with `EvaluateCandidate` (unified API) |
| **Dimension candidates don't use fixed grid (8×3)** | **LayoutOptimizer.cpp:196-240** | **HIGH** | **Section 6 mandates 8×3 grid for ALL elements; dimensions use 5 offsets instead** |
| Leader candidate shoulder length from geo (not fixed 0.5×dist) | LayoutOptimizer.cpp:161-162 | MEDIUM | AGENTS.md Section 6: distance from anchor to textPosition; shoulder = `geo.shoulderLength` |
| `CandidateGenerator.cpp` not integrated with LayoutOptimizer.cpp | CandidateGenerator.cpp | MEDIUM | Two parallel candidate generation systems; unify |
| `EvaluateCandidate` (GreedySolver) not used by `Solve()` | GreedySolver.cpp:125 | MEDIUM | `Solve()` uses per-element overloads; should use unified `EvaluateCandidate` |
| No golden-file test fixtures | - | **HIGH** | AGENTS.md Section 12 requires 3 golden-file scenarios |
| No Catch2 test infrastructure | - | MEDIUM | AGENTS.md Section 12 mandates Catch2 |

### 🔧 Critical Fixes Required for Phase 7

**1. Dimension Candidate Generation (AGENTS.md Section 6 violation)**
```cpp
// CURRENT (violates fixed grid):
constexpr double kOffsets[5] = {-20.0, -10.0, 0.0, 10.0, 20.0};

// REQUIRED (Section 6: 8 angles × 3 distances = 24 candidates):
for (double angDeg : kCandidateAnglesDeg) {
    for (double dist : kCandidateDistancesMm) {
        // Generate perpendicular offset candidate at angle/distance
    }
}
```

**2. Unify Candidate Generation**
- Remove `SolveLeadersGreedy` / `SolveDimensionsGreedy` (legacy)
- Use `CandidateGenerator::GenerateForLeader/Dimension` everywhere
- `Solve()` API already uses `ElementInput` with pre-generated candidates

**3. Add Catch2 + Golden-file Tests**
```cpp
// Test scenarios per AGENTS.md Section 12:
// 1. No-conflict baseline (Layout Stability)
// 2. Dense-conflict (greedy relocates)
// 3. Fully-obstructed (fallback candidate path)
```

### ✅ Verification (Ad-hoc)
- **Build**: `python Tools/BuildAddOn.py -c config.json -v 25` → **SUCCESS**
- **Script**: `hermes-verify-build-archicad-adapter.sh` → **SUCCESS**

### Phase 7 — Add Dimensions (`DimensionElement`, chains) — 🔧 PLANNING

#### Цель Phase 7 (AGENTS.md Section 13):
- Реализовать поддержку цепочек размеров (Dimension chains)
- Полная интеграция `DimensionElement` в LayoutOptimizer
- Обновление ArchicadAdapter для сбора и применения цепочек размеров

#### 🔧 Tactical Plan for Phase 7:

**Phase 7.1 — LayoutOptimizer Core Fixes (Pure C++)** — *Must do first*
- [ ] 7.1.1 Fix `GenerateDimensionCandidates` to use Section 6 fixed grid (8×3) + perpendicular offsets
- [ ] 7.1.2 Remove legacy `SolveLeadersGreedy` / `SolveDimensionsGreedy` 
- [ ] 7.1.3 Unify `EvaluateCandidateCost` overloads into single `EvaluateCandidate`
- [ ] 7.1.4 Integrate `CandidateGenerator.cpp` with `LayoutOptimizer::Solve()` flow
- [ ] 7.1.5 Add Catch2 unit tests + 3 golden-file test fixtures

**Phase 7.2 — ArchicadAdapter Updates (ACAPI allowed)**
- [ ] 7.2.1 Update `CollectElements()` to extract dimension chains (associated dimensions)
- [ ] 7.2.2 Implement `GetDimensionChainMeasurePoints()` for chained dimensions
- [ ] 7.2.3 Update `ApplyChanges()` to write back dimension text positions with proper masks
- [ ] 7.2.4 Handle `API_DimensionType` fields: `defNote.pos`, `refC`, `direction`, chain association

**Phase 7.3 — Integration & Testing**
- [ ] 7.3.1 Verify golden-file test: no-conflict baseline (Layout Stability)
- [ ] 7.3.2 Verify golden-file test: dense-conflict scenario (greedy actually relocates)
- [ ] 7.3.3 Verify golden-file test: fully-obstructed scenario (fallback candidate)
- [ ] 7.3.4 Build verification via `BuildAddOn.py`
- [ ] 7.3.5 Manual Archicad test: dimensions reposition correctly, chains preserved, undo/redo works

#### ⚠️ Важные заметки (AGENTS.md):
- **Section 2 Rule 1**: LayoutOptimizer core НЕ включает ACAPI headers
- **Section 2 Rule 2**: Absolute Determinism — no unordered_map/set in solver paths
- **Section 2 Rule 3**: Graceful Fallback — every element has fallback candidate
- **Section 2 Rule 4**: Respect User Layout — locked elements not moved, but count as obstacles
- **Section 6**: Candidate grid constants НЕ МЕНЯТЬ без обновления AGENTS.md
- **Section 11**: C++17, `/fp:strict` (MSVC), `-ffp-contract=off` (GCC/Clang)
- **Section 12**: Catch2 for unit tests, golden-file tests REQUIRED
- **Section 15**: NO R-Tree/QuadTree until >500 elements benchmarked

### Phase 8 — Add DFS Clustering & Local Search
- Не начато (требует расширения AGENTS.md с явным критерием кластеризации)

### Phase 9 — Caching & Performance Optimizations
- Не начато