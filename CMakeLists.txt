cmake_minimum_required(VERSION 3.16)
project(shieldwall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Каждый дописывает свои файлы только в конец списка (см. README)
add_executable(shieldwall
    src/main.cpp
    src/Unit.cpp
    src/Army.cpp
    src/BattleEngine.cpp
    src/UnitCatalog.cpp
    src/UnitFactory.cpp
    src/ArmyGenerator.cpp
    src/SaveService.cpp
    src/BattleReplay.cpp
)

target_include_directories(shieldwall PRIVATE include)
