cmake_minimum_required(VERSION 3.24)
project(shieldwall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(SHIELDWALL_GRAPHICS "Build the SFML window" OFF)

# Русские строки в исходниках: без этого MSVC читает файлы в системной кодировке
if(MSVC)
    add_compile_options(/utf-8)
endif()

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


    src/EventFormatter.cpp
    src/ConsoleUI.cpp
    src/Menu.cpp
    src/BattleRunner.cpp
    src/View.cpp
    src/ConsoleView.cpp
    src/ArmyBuilder.cpp
    src/TargetSelector.cpp
    include/UserInterface.h
    src/SaveSlots.cpp
)

target_include_directories(shieldwall PRIVATE include)
target_compile_definitions(shieldwall PRIVATE SHIELDWALL_GRAPHICS=$<BOOL:${SHIELDWALL_GRAPHICS}>)

if(SHIELDWALL_GRAPHICS)
    target_sources(shieldwall PRIVATE src/SfmlView.cpp)
    # Архив с проверкой SHA-256: отдельно устанавливать SFML или Git не требуется.
    include(FetchContent)
    # FreeType 2.13 объявляет старый минимум; эта настройка нужна для CMake 4.
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static SFML libraries" FORCE)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "Audio is not used" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "Network is not used" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "SFML examples are not needed" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "Build only project tests" FORCE)
    set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "Fetch FreeType with SFML" FORCE)
    # Первое объявление имеет приоритет над Git-загрузкой внутри SFML.
    FetchContent_Declare(Freetype
        URL https://codeload.github.com/freetype/freetype/tar.gz/refs/tags/VER-2-13-2
        URL_HASH SHA256=427201f5d5151670d05c1f5b45bef5dda1f2e7dd971ef54f0feaaa7ffd2ab90c
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(SFML
        URL https://codeload.github.com/SFML/SFML/tar.gz/refs/tags/3.0.2
        URL_HASH SHA256=0034e05f95509e5d3fb81b1625713e06da7b068f210288ce3fd67106f8f46995
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SFML)
    target_link_libraries(shieldwall PRIVATE SFML::Graphics)

endif()

# Обновляем ресурсы при каждой сборке, даже если исходники C++ не менялись.
add_custom_target(shieldwall_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:shieldwall>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:shieldwall>/data"
    VERBATIM
)
add_dependencies(shieldwall shieldwall_resources)
