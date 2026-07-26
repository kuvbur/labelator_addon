# Текущий план разработки (Phase 6: ArchicadAdapter — FIX APPLIED)

## Реальный статус (проверено git status)
- **Git**: Clean, 5 commits ahead of origin/master (включая рефакторинг)
  - `5a27590` "fix(archicadadapter): Add ACAPI_Element_GetMemo before ACAPI_Element_Change"
  - `f2f6b80` "Refactoring: Tasks 1-4 complete"
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)
- **Файлы Phase 6**: `ArchicadAdapter.cpp`, `ArchicadAdapter.hpp` — код исправлен, готов к повторному тестированию

## Phase 6 — 🛠️ FIX APPLIED (ожидает повторного теста)

### ✅ Код готов
1. [x] **ApplyChanges()** реализован — запись координат в Archicad элементы через `ACAPI_Element_Change`
2. [x] **ACAPI_CallUndoableCommand** обернут — Undo/Redo работает корректно
3. [x] **Locked elements** не двигаются (проверка `solved.locked` в циклах ApplyChanges)

### ✅ ИСПРАВЛЕНО после первого теста (error -2130313112)
- [x] Добавлен `ACAPI_Element_GetMemo` перед `ACAPI_Element_Change`
- [x] Использован `APIMemoMask_All` вместо `0`
- [x] Проверка `memo.paragraphs != nullptr` для dimensions

### ⏳ Ожидает ручного тестирования в Archicad
- [ ] **Manual test**: undo/redo round-trips, locked elements не меняются

---

## 📋 План действий после ручного тестирования

### A. Если тест PASSED (undo/redo работает, locked не тронуты)

1. **WIP commit**: `git add -A && git commit -m "Phase 6 DoD: manual test passed"`
2. **Phase 7 переход**:
   - Очистить статус в IDEA.md: "Phase 6 COMPLETE"
   - Начать Phase 7: Add Dimensions (если не доделаны полностью)
   - Проверить, что DimensionElement полностью интегрирован в конвейер

3. **Phase 8 подготовка** (DFS Clustering):
   - Прочитать AGENTS.md §13 "Implementation Order"
   - Добавить в IDEA.md план DFS clustering:
     - Clustering criterion: графовые связи между элементами с пересекающимися textBox
     - Neighborhood definition: радиус 50mm вокруг элемента
     - Local search: 2-opt оптимизация внутри кластера

### B. Если тест FAILED — типичные сценарии и исправления

#### B1. "Locked elements moved" — баг в `IsElementLocked()`
```cpp
// Проверить: правильно ли определяется lock
// ИСПРАВЛЕНИЕ: добавить debug-лог в ApplyChanges
//   - std::cout << "Skipping locked element " << guid << std::endl;
//   - Убедиться, что locked=true передаётся из CollectElements
```

#### B2. "Undo not working" — проблема в `ACAPI_CallUndoableCommand`
```cpp
// Возможные причины:
// 1. ACAPI_CallUndoableCommand НЕ вызывается (проверить return statement)
// 2. Ошибки ACAPI_Element_Change не обрабатываются → undo не активируется
// ИСПРАВЛЕНИЕ: добавить обработку ошибок
//   if (err != NoError) { 
//       std::cerr << "Element change failed: " << err << std::endl;
//       return err; // Early exit prevents partial undo
//   }
```

#### B3. "Graphic artifacts on leaders" — проблема с polyline
```cpp
// ПРОВЕРИТЬ:
// - Leader имеет 2 точки (begC, endC), но в ApplyChanges пишем 3
// - Возможно, midC должен вычисляться как средняя точка
// ИСПРАВЛЕНИЕ: если polyline.size() == 2, midC = (begC + endC) / 2
```

#### B4. "Crash on ApplyChanges" — утечка/нулевой указатель
```cpp
// ПРОВЕРИТЬ:
// - ACAPI_Element_GetMemo вызывается перед Change?
// - BNZero вызывается для всех структур перед использованием?
// ИСПРАВЛЕНИЕ: добавить макрос SAFE_GET(elem, guid) с проверкой err
```

#### B5. "ACAPI_Element_Change error -2130313112" — memo required для dimensions/labels
```cpp
// ПРОВЕРИТЬ:
// - ACAPI_Element_Change вызывается с APIMemoMask_All вместо 0 ✓ (ИСПРАВЛЕНО)
// - ACAPI_Element_GetMemo вызывается ПЕРЕД ACAPI_Element_Change ✓ (ИСПРАВЛЕНО)
// - memo.paragraphs проверяется на nullptr ✓ (ИСПРАВЛЕНО)
```

### C. План Phase 7 (Dimensions complete)
1. **Verify Dimension implementation**:
   - [x] `DimensionElement.geo.direction` вычисляется правильно в CollectElements
   - [x] `GenerateForDimension` принимает `direction` параметр
   - [x] `ApplyChanges` обновляет `dimension.defNote.pos` правильно

2. **Дополнить CandidateGenerator**:
   - [x] Perpendicular offset через `perpDir = {-direction.y, direction.x}`
   - [x] 5 дополнительных кандидатов со скольжением вдоль линии

3. **Тестирование**:
   - [ ] Unit test: создать DimensionElement с известным direction

### D. План Phase 8 (DFS Clustering) — НЕ ЗАПУСКАТЬ без спецификации

**ТРЕБУЕТСЯ: расширение AGENTS.md с критериями clustering**

Перед началом Phase 8:
1. Добавить в AGENTS.md секцию про clustering criterion (как формируется граф)
2. Добавить Neighborhood definition (что считается соседом)
3. Добавить precision требования для SegmentsIntersect
4. Только ПОСЛЕ этого начинать реализацию

---

## 🚨 STOP Conditions (AGENTS.md §16)

Перед ЛЮБЫМ изменением кода:
```bash
# Автоматические проверки (обязательно проходить)
grep -rn "ACAPI_" Sources/AddOn/LayoutOptimizer.hpp Sources/AddOn/CandidateGenerator.* Sources/AddOn/GreedySolver.*
grep -rn "unordered_map|unordered_set" Sources/AddOn/LayoutOptimizer/ Sources/AddOn/CandidateGenerator.* Sources/AddOn/GreedySolver.*
```

**Если обнаружено нарушение → СТОП, создать issue, не патчить!**

---

## 🔧 Refactoring Tasks (Per User Instructions) - ВЫПОЛНЕНО

1. **[x] ЗАДАЧА 1**: Удалить legacy-монолит `LayoutOptimizer.cpp` — 441 строк удалено
2. **[x] ЗАДАЧА 2**: Восстановить конвейер в `ArchicadAdapter.cpp::SolveAndApply()`
   - Удалить создание кандидата-заглушки ✓
   - Внедрить `LayoutOptimizer::CandidateGenerator::GenerateForLeader` и `GenerateForDimension` ✓
   - Обеспечить сбор `forbiddenGeometry` и `obstaclePolylines` ✓
3. **[x] ЗАДАЧА 3**: Исправить логику генератора
   - Dimensions: перпендикулярное смещение + 5 кандидатов скольжения ✓
   - Leaders: использовать `le.geo.shoulderLength` вместо `distanceMm * 0.5` ✓
4. **[x] ЗАДАЧА 4**: Безопасная работа с ACAPI
   - Text box: `para.width/height` вместо `strlen` ✓
   - Lock detection: `floorInd & 0x8000` + `layer.head.flags & 1` ✓
   - 3 точки `begC/midC/endC` для структурной целостности ✓

---

## Команды
```bash
# Сборка
python Tools/BuildAddOn.py -c config.json -v 25

# Верификация AGENTS.md checklist
bash C:\Users\da-rogojin\AppData\Local\Temp\hermes-verify-build-archicad-adapter.sh

# Git
git status
git add Sources/AddOn/ArchicadAdapter.cpp
git commit -m "описание исправления"
```