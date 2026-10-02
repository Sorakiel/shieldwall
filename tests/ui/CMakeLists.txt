cmake_minimum_required(VERSION 3.16)
project(shieldwall_ui_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(CoreSources)
# Те же временные реализации, что в main и тестах данных; после интеграции
# ядра автоматически проверяем оболочку с настоящими классами.
foreach(Pair "Unit|LightUnit::LightUnit|UnitStub" "Army|Army::add|ArmyStub"
             "BattleEngine|BattleEngine::nextTurn|EngineStub")
    string(REPLACE "|" ";" Parts "${Pair}")
    list(GET Parts 0 Name)
    list(GET Parts 1 Signature)
    list(GET Parts 2 Stub)
    file(READ "${Root}/src/${Name}.cpp" Source)
    string(FIND "${Source}" "${Signature}" Position)
    if(Position EQUAL -1)
        list(APPEND CoreSources "${Root}/tests/stubs/${Stub}.cpp")
    else()
        list(APPEND CoreSources "${Root}/src/${Name}.cpp")
    endif()
endforeach()

add_library(test_support STATIC ${CoreSources}
    "${Root}/src/UnitCatalog.cpp" "${Root}/src/UnitFactory.cpp"
    "${Root}/src/ArmyGenerator.cpp" "${Root}/src/SaveService.cpp"
    "${Root}/src/BattleReplay.cpp")
target_include_directories(test_support PUBLIC "${Root}/include")

add_executable(ui_tests UiTests.cpp
    "${Root}/src/EventFormatter.cpp" "${Root}/src/ConsoleUI.cpp"
    "${Root}/src/Menu.cpp" "${Root}/src/BattleRunner.cpp")
target_sources(ui_tests PRIVATE "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp")
target_link_libraries(ui_tests PRIVATE test_support)
add_executable(data_tests "${Root}/tests/DataTests.cpp")
target_link_libraries(data_tests PRIVATE test_support)

foreach(Target test_support ui_tests data_tests)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

enable_testing()
add_test(NAME ui COMMAND ui_tests)
add_test(NAME data COMMAND data_tests)
set_tests_properties(ui data PROPERTIES WORKING_DIRECTORY "${Root}")
