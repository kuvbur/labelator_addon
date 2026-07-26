# Текущий план разработки (Phase 6: ArchicadAdapter — IN PROGRESS)

## Реальный статус (проверено git status)
- **Git**: Clean, 3 commits ahead of origin/master
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)
- **Файлы Phase 6**: `ArchicadAdapter.cpp`, `ArchicadAdapter.hpp` — восстановлены и исправлены

## Phase 6 — 🔧 IN PROGRESS (ArchicadAdapter) — REFACTORING

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
   - [x] **Dimensions**: Восстановить логику перпендикулярного смещения. Свойство `cand.offset` отражает перпендикулярное расстояние от размерной линии (вычисляется через проекцию на перпендикуляр направлению). Добавлены кандидаты со смещением вдоль линии размеров.
   - [x] **Leaders**: Использовать оригинальную длину полки `le.geo.shoulderLength`, переданную из адаптера, вместо жестко закодированного `shoulderDist = distanceMm * 0.5`.
4. [x] **ЗАДАЧА 4**: Безопасная работа с ACAPI (`ArchicadAdapter.cpp`)
   - [x] Заменить эвристическую оценку ширины текстового бокса через `strlen` на точное извлечение из `paragraph.width` и `paragraph.height` в структуре `memo` выноски/размера.
   - [x] Убрать хардкодную битовую проверку блокировки (`floorInd & 0x8000`), использовать стандартные API-флаги и атрибуты слоев (`layer.head.flags & 1`).
   - [x] При обновлении геометрии выносок в `ApplyChanges` гарантировать структурную целостность элемента (гарантируем 3 точки: begC, midC, endC), чтобы избежать графических артефактов в Archicad.

### 🔧 Что осталось сделать в Phase 6 (после рефакторинга)
1. [x] **Реализовать** `ApplyChanges()` — записать новые координаты в Archicad элементы
2. [x] **Обернуть** в `ACAPI_CallUndoableCommand` для корректного Undo/Redo
3. [ ] **Тест**: manual Archicad test — locked elements не двигаются, undo/redo работает

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
