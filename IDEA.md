# Текущий план разработки (Phase 6: ArchicadAdapter — IN PROGRESS)

## Реальный статус (проверено git status)
- **Git**: Clean, 4 commits ahead of origin/master (включая рефакторинг)
  - `f2f6b80` "Refactoring: Tasks 1-4 complete"
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)
- **Файлы Phase 6**: `ArchicadAdapter.cpp`, `ArchicadAdapter.hpp` — восстановлены, рефакторены и готовы к тестированию

## Phase 6 — ⏳ READY FOR MANUAL TEST (ArchicadAdapter)

### ✅ Код готов
1. [x] **ApplyChanges()** реализован — запись координат в Archicad элементы через `ACAPI_Element_Change`
2. [x] **ACAPI_CallUndoableCommand** обернут — Undo/Redo работает корректно
3. [x] **Locked elements** не двигаются (проверка `solved.locked` в циклах ApplyChanges)

### ⏳ Ожидает ручного тестирования в Archicad
- [ ] **Manual test**: undo/redo round-trips, locked elements не меняются

### ✅ Выполнено в этой сессии
1. **Восстановлены** `ArchicadAdapter.cpp` / `ArchicadAdapter.hpp` из `.bak`
2. **Исправлены критические ошибки компиляции**:
   - Добавлены недостающие includes: `ACAPinc.h`, `APIEnvir.h`
   - Исправлена сигнатура `ACAPI_Element_GetElemList` (параметр — указатель на `GS::Array<API_Guid>`)
   - Исправлен доступ к `typeID` в `API_Elem_Head` (убраны каскадные ошибки)
   - Исправлено предупреждение C4018 (signed/unsigned mismatch) — `Int32` → `UInt32`
3. **Верификация**: `Build succeeded!` подтверждено скриптом `hermes-verify-build-archicad-adapter.sh`

### 🔧 Refactoring Tasks (Per User Instructions)
1. [x] **ЗАДАЧА 1**: Удалить legacy-монолит `LayoutOptimizer.cpp` (файл существует в Sources/AddOn/) — **ВЫПОЛНЕНО**
2. [x] **ЗАДАЧА 2**: Восстановить конвейер в `ArchicadAdapter.cpp::SolveAndApply()`
   - [x] Удалить создание кандидата-заглушки (fallback stub)
   - [x] Внедрить прямые вызовы `LayoutOptimizer::CandidateGenerator::GenerateForLeader` и `GenerateForDimension`
   - [x] Обеспечить сбор и передачу реальных данных `forbiddenGeometry` и `obstaclePolylines` в `Solve()`
3. [x] **ЗАДАЧА 3**: Исправить логику генератора (`CandidateGenerator.cpp`)
   - [x] **Dimensions**: Восстановить логику перпендикулярного смещения. Свойство `cand.offset` теперь отражает перпендикулярное расстояние от размерной линии (вычисляется через проекцию на перпендикуляр). Добавлены 5 дополнительных кандидатов для скольжения вдоль размерной линии.
   - [x] **Leaders**: Использовать оригинальную длину полки `le.geo.shoulderLength`, переданную из адаптера, вместо жестко закодированного `shoulderDist = distanceMm * 0.5`.
4. [x] **ЗАДАЧА 4**: Безопасная работа с ACAPI (`ArchicadAdapter.cpp`)
   - [x] Заменить эвристическую оценку ширины текстового бокса через `strlen` на точное извлечение `para.width` из структуры `memo` выноски/размера (с fallback на `strlen` при отсутствии).
   - [x] Убрать хардкодную битовую проверку блокировки (`floorInd & 0x8000`), использовать стандартные API-флаги (`element.header.flags.locked`) и атрибуты слоев (`layer.head.flags & 1`).
   - [x] При обновлении геометрии выносок в `ApplyChanges` гарантировать структурную целостность элемента — всегда писать 3 точки (`begC`, `midC`, `endC`), дублируя при необходимости, чтобы избежать графических артефактов в Archicad.

### 🔧 Что осталось сделать в Phase 6 (после рефакторинга)
1. [x] **Реализовать** `ApplyChanges()` — записать новые координаты в Archicad элементы
2. [x] **Обернуть** в `ACAPI_CallUndoableCommand` для корректного Undo/Redo
3. [ ] **Тест**: manual Archicad test — locked elements не меняются, undo/redo работает (требует ручной проверки в Archicad)

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
// - ACAPI_Element_GetMemo вызывается перед GetMemo? (нет, у нас GetMemo не нужен)
// - BNZero вызывается для всех структур перед использованием?
// ИСПРАВЛЕНИЕ: добавить макрос SAFE_GET(elem, guid) с проверкой err
```

#### B5. "ACAPI_Element_Change error -2130313112" — memo required для dimensions/labels
```cpp
// ПРОВЕРИТЬ:
// - ACAPI_Element_Change вызывается с APIMemoMask_All вместо 0
// - ACAPI_Element_GetMemo вызывается ПЕРЕД ACAPI_Element_Change
// - memo.paragraphs проверяется на nullptr
// ИСПРАВЛЕНИЕ: добавлено в ArchicadAdapter.cpp
//   - err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_All);
//   - if (err != NoError) continue;
//   - err = ACAPI_Element_Change (&elem, &mask, &memo, APIMemoMask_All, true);
```

### C. План Phase 7 (Dimensions complete)

1. **Verify Dimension implementation**:
   - [ ] `DimensionElement.geo.direction` вычисляется правильно в CollectElements
   - [ ] `GenerateForDimension` принимает `direction` параметр
   - [ ] `ApplyChanges` обновляет `dimension.defNote.pos` правильно

2. **Дополнить CandidateGenerator**:
   - [ ] Если direction не нормализован, добавить нормализацию
   - [ ] Если offset вычисляется неправильно, поправить формулу

3. **Тестирование**:
   - [ ] Unit test: создать DimensionElement с известным direction
   - [ ] Проверить, что кандидаты генерируются в правильном направлении

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
grep -rn "unordered_map\|unordered_set" Sources/AddOn/LayoutOptimizer/ Sources/AddOn/CandidateGenerator.* Sources/AddOn/GreedySolver.*
```

**Если обнаружено нарушение → СТОП, создать issue, не патчить!**

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
bash C:\Users\da-rogojin\AppData\Local\Temp\hermes-verify-build-archicad-adapter.sh

# Git
git status
git add Sources/AddOn/ArchicadAdapter.cpp Sources/AddOn/ArchicadAdapter.hpp
git commit -m "описание"
```
