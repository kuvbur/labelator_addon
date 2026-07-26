# Текущий план разработки (Phase 6: ArchicadAdapter — ⚠️ НУЖДАЕТСЯ В ДОРАБОТКЕ)

## Реальный статус (проверено git status)

- **Git**: Clean, 5 commits ahead of origin/master
  - `3582088` "Phase 7 Planning: Detailed Phase 7 plan for Dimension chains"
  - `824950b` "Phase 6 Complete: ArchicadAdapter implementation with LayoutOptimizer integration"
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)

## Phase 6 — ⚠️ ЧАСТИЧНО ВЫПОЛНЕНО (ArchicadAdapter)

### ✅ Выполнено (Синтаксис и компиляция)

1. **Восстановлены** `ArchicadAdapter.cpp` / `ArchicadAdapter.hpp` из `.bak`
2. **Исправлены критические ошибки компиляции**:
   - Добавлены недостающие includes: `ACAPinc.h`, `APIEnvir.h`
   - Исправлена сигнатура `ACAPI_Element_GetElemList`
   - Исправлен доступ к `typeID` в `API_Elem_Head`
   - Исправлено предупреждение C4018 (signed/unsigned mismatch)
   - Исправлены поля `API_TextType` и API `LayoutOptimizer::Candidate`
3. **Верификация**: `Build succeeded!` подтверждено скриптом
4. **Undo/Redo** — реализовано через `ACAPI_CallUndoableCommand`
5. **Locked elements** — пропускаются в ApplyChanges
6. **Подключение к Menu Handler** в `Dimension_Main.cpp::Do_OptimizeAnnotations()`

### ❌ Не выполнено (Логическая интеграция разорвана)

- `SolveAndApply()` передает в решатель кандидата-заглушку вместо вызова генератора.
- В оптимизатор передаются пустые векторы препятствий (`forbiddenGeometry`).
- Размеры текста вычисляются эвристически (через `strlen`), что небезопасно.

### Архитектура (AGENTS.md Section 4):

[Archicad API] → [ArchicadAdapter] → [LayoutOptimizer::Solve()] → [ArchicadAdapter::ApplyChanges()] → [ACAPI]

---

## Анализ ошибок в этой сессии

### ❌ Ошибки (согласно правилам)

1. **Иллюзия интеграции решателя** — функция `SolveAndApply()` не вызывает `CandidateGenerator`. Вместо этого жестко задается текущая позиция (`simplified: use current position as one candidate`). Из-за этого решатель работает вхолостую.
   - **Исправление**: Запланировано в Phase 7.0. Необходимо связать Адаптер с `CandidateGenerator`.
2. **Эвристика размеров текста (`strlen`)** — грубый подсчет ширины текста игнорирует многострочность и пропорциональные шрифты.
   - **Исправление**: Запланировано в Phase 7.0. Использовать реальный Bounding Box из `API_ElementMemo`.
3. **Игнорирование препятствий** — адаптер передает пустые `forbiddenGeometry` в `Solve()`.
   - **Исправление**: Запланировано в Phase 7.0.
4. **Игнорирование RAG** — пытался угадать поля `API_ElementMemo`.
   - **Исправление**: Использовал lightrag для проверки правильных полей.
5. **Неправильный `using`** — написал `using LO = LayoutOptimizer;` (такого в C++ нет).
   - **Исправление**: Убрал `using`, использую полные имена `LayoutOptimizer::`.

### ✅ Правильные действия

1. **Соблюдение плоской структуры** — все файлы в `Sources/AddOn/`
2. **Обновление IDEA.md** — после каждого значимого шага
3. **Верификация через скрипт** — создал `hermes-verify-build-archicad-adapter.sh`
4. **Использование git** — коммит после каждого исправления

---

## Code Review: LayoutOptimizer Core (Pre-Phase 7) — ✅ VERIFIED

### ⚠️ Issues to Fix Before Phase 7

| Issue                                                 | File:Line                       | Severity | Fix                                                                                               |
| ----------------------------------------------------- | ------------------------------- | -------- | ------------------------------------------------------------------------------------------------- |
| `SolveLeadersGreedy` / `SolveDimensionsGreedy` unused | LayoutOptimizer.cpp:245, 333    | LOW      | Remove legacy file `LayoutOptimizer.cpp` entirely; `Solve()` in GreedySolver is the canonical API |
| **Dimension candidates don't use fixed grid (8×3)**   | **LayoutOptimizer.cpp:196-240** | **HIGH** | **Section 6 mandates 8×3 grid for ALL elements; dimensions use 5 offsets instead**                |
| Leader candidate shoulder length from geo             | CandidateGenerator.cpp          | MEDIUM   | Use original `geo.shoulderLength` from adapter instead of `distanceMm * 0.5`                      |
| No golden-file test fixtures                          | -                               | **HIGH** | AGENTS.md Section 12 requires 3 golden-file scenarios                                             |

---

## Phase 7 — Add Dimensions (`DimensionElement`, chains) — 🔧 PLANNING

#### Цель Phase 7 (AGENTS.md Section 13):

- Восстановить разорванный конвейер оптимизации (Hotfixes)
- Реализовать поддержку цепочек размеров (Dimension chains)
- Полная интеграция `DimensionElement` в LayoutOptimizer

#### 🔧 Tactical Plan for Phase 7:

**Phase 7.0 — ArchicadAdapter Hotfixes (Must do first before Dimensions)**

- [ ] 7.0.1 В `SolveAndApply()` убрать ручную генерацию кандидата-заглушки и подключить реальные вызовы `CandidateGenerator::GenerateForLeader` / `GenerateForDimension`.
- [ ] 7.0.2 В `SolveAndApply()` реализовать сбор и передачу реальных `forbiddenGeometry` и `obstaclePolylines` (сейчас передаются пустые векторы).
- [ ] 7.0.3 В `GetTextBoxPaperSpace` убрать хардкод ширины текста через `strlen`, использовать точные данные Bounding Box из `API_ElementMemo`.
- [ ] 7.0.4 В `ApplyChanges()` добавить безопасную обработку изменения количества узлов в `leaderPolyline` (например, при переходе от 2 к 3 точкам) для предотвращения графических артефактов в Archicad.

**Phase 7.1 — LayoutOptimizer Core Fixes (Pure C++)**

- [ ] 7.1.1 Удалить устаревший монолит `LayoutOptimizer.cpp` (исходники решателя теперь в `GreedySolver.cpp`).
- [ ] 7.1.2 Fix `CandidateGenerator.cpp` для Dimensions: использовать сетку из Section 6 (8×3) + перпендикулярные смещения.
- [ ] 7.1.3 Fix `CandidateGenerator.cpp` для Leaders: использовать оригинальную длину полки `le.geo.shoulderLength`.
- [ ] 7.1.4 Интегрировать единый `EvaluateCandidate` и избавиться от дублирования стоимости.
- [ ] 7.1.5 Добавить Catch2 unit tests + 3 golden-file test fixtures (Layout Stability, Dense-conflict, Fully-obstructed).

**Phase 7.2 — ArchicadAdapter Updates (ACAPI allowed)**

- [ ] 7.2.1 Update `CollectElements()` to extract dimension chains (associated dimensions).
- [ ] 7.2.2 Implement `GetDimensionChainMeasurePoints()` for chained dimensions.
- [ ] 7.2.3 Update `ApplyChanges()` to write back dimension text positions with proper masks.
- [ ] 7.2.4 Handle `API_DimensionType` fields: `defNote.pos`, `refC`, `direction`, chain association.

**Phase 7.3 — Integration & Testing**

- [ ] 7.3.1 Verify golden-file test: no-conflict baseline (Layout Stability).
- [ ] 7.3.2 Verify golden-file test: dense-conflict scenario (greedy actually relocates).
- [ ] 7.3.3 Verify golden-file test: fully-obstructed scenario (fallback candidate).
- [ ] 7.3.4 Build verification via `BuildAddOn.py`.
- [ ] 7.3.5 Manual Archicad test: dimensions reposition correctly, chains preserved, undo/redo works.

#### ⚠️ Важные заметки (AGENTS.md):

- **Section 2 Rule 1**: LayoutOptimizer core НЕ включает ACAPI headers.
- **Section 2 Rule 2**: Absolute Determinism — no unordered_map/set in solver paths.
- **Section 2 Rule 3**: Graceful Fallback — every element has fallback candidate.
- **Section 11**: C++17, `/fp:strict` (MSVC), `-ffp-contract=off` (GCC/Clang).
- **Section 12**: Catch2 for unit tests, golden-file tests REQUIRED.
