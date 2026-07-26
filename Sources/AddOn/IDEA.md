# Текущий план разработки (Phase 6: ArchicadAdapter — ✅ COMPLETE)

## Реальный статус (проверено git status)
- **Git**: Clean, 3 commits ahead of origin/master
  - `e1be291` "Phase 5: Fix GreedySolver - locked fallback + conflict counting + priority heuristic"
  - `c0465e9` "chore: ignore build artifacts"
  - `894dc1c` "Fix ArchicadAdapter.cpp: add missing includes, fix ACAPI_Element_GetElemList, fix typeID access"
- **Структура**: Плоская в `Sources/AddOn/`. Все файлы в одной папке (согласно правилам)
- **Файлы Phase 6**: `ArchicadAdapter.cpp`, `ArchicadAdapter.hpp` — восстановлены и исправлены

## Phase 6 — ✅ COMPLETE (ArchicadAdapter)

### ✅ Выполнено в этой сессии
1. **Восстановлены** `ArchicadAdapter.cpp` / `ArchicadAdapter.hpp` из `.bak`
2. **Исправлены критические ошибки компиляции**:
   - Добавлены недостающие includes: `ACAPinc.h`, `APIEnvir.h`
   - Исправлена сигнатура `ACAPI_Element_GetElemList` (параметр — указатель на `GS::Array<API_Guid>`)
   - Исправлен доступ к `typeID` в `API_Elem_Head` (убраны каскадные ошибки)
   - Исправлено предупреждение C4018 (signed/unsigned mismatch) — `Int32` → `UInt32`
   - Исправлены поля `API_TextType` (`loc` вместо `pos`/`textPosition`)
   - Исправлен API LayoutOptimizer::Candidate (`textPosition` вместо `placement`)
3. **Верификация**: `Build succeeded!` подтверждено скриптом `hermes-verify-build-archicad-adapter.sh`
4. **Интеграция LayoutOptimizer::Solve()** в adapter flow — **РЕАЛИЗОВАНО** в `SolveAndApply()` и `RunOptimization()`
5. **Undo/Redo** — **РЕАЛИЗОВАНО** через `ACAPI_CallUndoableCommand` в `ApplyChanges()`
6. **Locked elements** — **РЕАЛИЗОВАНО** (пропускаются в ApplyChanges, но учитываются как препятствия)
7. **Интеграция в Menu Handler** — **РЕАЛИЗОВАНО** в `Dimension_Main.cpp::Do_OptimizeAnnotations()`

### 🔧 Что осталось сделать в Phase 6
1. [ ] **Тест**: manual Archicad test — locked elements не двигаются, undo/redo работает

### Архитектура (AGENTS.md Section 4):
```
[Archicad API] → [ArchicadAdapter] → [LayoutOptimizer::Solve()] → [ArchicadAdapter::ApplyChanges()] → [ACAPI]
```

### Главный файл интеграции:
- `Dimension_Main.cpp` — entry point Add-On (уже есть, подключен adapter)

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