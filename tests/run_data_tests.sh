#!/bin/sh
# Сборка и запуск тестов данных из корня репозитория: sh tests/run_data_tests.sh
# Пока ядро (Unit.cpp, Army.cpp, BattleEngine.cpp) не написано, вместо него
# подставляются заглушки из tests/stubs; как только реализация появится, берётся она.
set -e

out=$(mktemp -d)/data_tests
files="tests/DataTests.cpp src/UnitCatalog.cpp"
for f in src/UnitFactory.cpp src/ArmyGenerator.cpp src/SaveService.cpp src/BattleReplay.cpp; do
    [ -f "$f" ] && files="$files $f"
done

if grep -q 'LightUnit::LightUnit' src/Unit.cpp; then files="$files src/Unit.cpp"; else files="$files tests/stubs/UnitStub.cpp"; fi
if grep -q 'Army::add' src/Army.cpp; then files="$files src/Army.cpp"; else files="$files tests/stubs/ArmyStub.cpp"; fi
if grep -q 'BattleEngine::nextTurn' src/BattleEngine.cpp; then
    files="$files src/BattleEngine.cpp"
elif [ -f tests/stubs/EngineStub.cpp ]; then
    files="$files tests/stubs/EngineStub.cpp"
fi

${CXX:-c++} -std=c++17 -Wall -Wextra -Iinclude $files -o "$out"
"$out"
