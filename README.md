# Shieldwall — автобой на C++

Пошаговый автобой двух армий (лабораторная по ППА).

## Сборка
```bash
cmake -B build
cmake --build build
./build/shieldwall
```

## Команда и ветки
| Роль | Кто | Ветка | Файлы |
|---|---|---|---|
| Ядро боя | Ксюша (интегратор) | `feature/core-battle` | `Unit*.cpp`, `Army.cpp`, `BattleEngine.cpp`, `TargetSelector.*` |
| Армии и данные | Никита | `feature/data-army` | `data/units.json`, `UnitCatalog.*`, `UnitFactory.*`, `ArmyGenerator.*`, `SaveService.*` |
| Оболочка и логи | Коля | `feature/ui-log` | `EventFormatter.*`, `ConsoleUI.*`, `Menu.*`, `BattleRunner.*`, `main.cpp` |

## Правила
- В `main` напрямую не коммитим — только PR, один апрув. `main` всегда собирается.
- У файла один владелец. Общие заголовки (`Unit.h`, `Army.h`, `BattleEngine.h`, `Event.h`) — только по согласованию в чате, отдельным коммитом.
- Ветки: латиница, строчные, через дефис (`feature/core-archer-range`, `fix/...`). Живут пару дней.
- Каждое утро: `git pull --rebase origin main`.
- Коммиты: `core:`, `data:`, `ui:`, `fix:`, `build:`, `docs:` + короткое описание без точки.
- Стиль: C++17, `#pragma once`, 4 пробела, PascalCase классы, camelCase методы, поля `hp_`.
- Случайность только через `std::mt19937` с seed, никакого `rand()`.
- `CMakeLists.txt`: свои файлы дописываем в конец списка.
