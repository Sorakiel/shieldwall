cmake_minimum_required(VERSION 3.16)
project(battle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Каждый дописывает свои файлы только в конец списка (см. README)
add_executable(battle
    src/main.cpp
    src/Unit.cpp
    src/Army.cpp
    src/BattleEngine.cpp
)

target_include_directories(battle PRIVATE include)
