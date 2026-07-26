# Текущий план разработки (Phase 6: ArchicadAdapter — ✅ COMPLETE)

## Реальный статус (проверено git status)
- **Git**: Clean, 4 commits ahead of origin/master
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
bash C:\\\\Users\\\\da-rogojin\\\\AppData\\\\Local\\\\Temp\\\\hermes-verify-build-archicad-adapter.sh

# Git
git status
git add Sources/AddOn/ArchicadAdapter.cpp Sources/AddOn/ArchicadAdapter.hpp
git commit -m "описание"
```

---

## Следующие этапы (AGENTS.md Section 13)

### Phase 7 — Add Dimensions (`DimensionElement`, chains) — 🔧 PLANNING

#### Цель Phase 7 (AGENTS.md Section 13):
- Реализовать поддержку цепочек размеров (Dimension chains)
- Полная интеграция `DimensionElement` в LayoutOptimizer
- Обновление ArchicadAdapter для сбора и применения цепочек размеров

#### 📋 Требования к реализации (AGENTS.md Sections 5, 6, 7, 9, 10):

**Data Model (Section 5):**
- `DimensionGeometry` — immutable: `measurePoints[]`, `direction`
- `DimensionLayout` — mutable: `textPosition`, `textBox`, `offset`
- `DimensionElement` — `id`, `locked`, `geo`, `layout`

**Candidate Generation (Section 6):**
- Fixed grid: 8 angles × 3 distances = 24 raw candidates + fallback
- Для Dimensions: offset вдоль перпендикуляра к dimension line
- Offset values: -20, -10, 0, 10, 20 mm (per LayoutOptimizer.cpp:212)

**Filtering Pipeline (Section 7):**
- View boundary check
- Forbidden geometry check (locked elements as obstacles)
- Obstacle line crossing

**Collision Detection (Section 8):**
- `Rect::Intersects` (eps=1e-4) for text boxes
- `SegmentsIntersect` for leader polylines vs obstacles

**Cost Function (Section 9):**
- Lexicographic `LayoutCost`: textCollision, geometryCollision, lineCollision, movement, leaderLength, alignmentPenalty

**Solver (Section 10):**
- `PlacementPriority`: typeRank=0 (Dimensions first), lockedNeighborCount, invalidCandidateCount, id
- Greedy solver ordered by priority

#### 🔧 Tactical Plan for Phase 7:

**Phase 7.1 — LayoutOptimizer Core Updates (Pure C++)**
- [ ] 7.1.1 Extend `GenerateDimensionCandidates` to use proper fixed grid (8 angles × 3 distances) + perpendicular offsets
- [ ] 7.1.2 Ensure `DimensionElement` candidate generation follows Section 6 constants exactly
- [ ] 7.1.3 Verify `EvaluateCandidate` works for Dimensions (no leaderPolyline, use offset for leaderLength)
- [ ] 7.1.4 Add `CollectResult` / `ApplyChanges` support for dimension chains in LayoutOptimizer API

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

#### 📋 Декомпозиция задач для реализации:

| Задача | Описание | Файлы | Приоритет |
|--------|----------|-------|-----------|
| 7.1 | LayoutOptimizer: Dimension candidates с fixed grid | `LayoutOptimizer.cpp`, `CandidateGenerator.cpp` | HIGH |
| 7.2 | LayoutOptimizer: Cost evaluation для Dimensions | `LayoutOptimizer.cpp`, `GreedySolver.cpp` | HIGH |
| 7.3 | ArchicadAdapter: Collect dimension chains | `ArchicadAdapter.cpp` | HIGH |
| 7.4 | ArchicadAdapter: Apply dimension changes | `ArchicadAdapter.cpp` | HIGH |
| 7.5 | Unit tests для Dimension logic | `*_test.cpp` (catch2) | MEDIUM |
| 7.6 | Golden-file tests (3 сценария) | test fixtures + expected outputs | MEDIUM |
| 7.7 | Build + manual test | `BuildAddOn.py` + Archicad | HIGH |

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