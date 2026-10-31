cmake_minimum_required(VERSION 3.16)
project(shieldwall_ui_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(CoreSources
    "${Root}/src/Unit.cpp"
    "${Root}/src/Army.cpp"
    "${Root}/src/BattleEngine.cpp")

add_library(test_support STATIC ${CoreSources}
    "${Root}/src/TargetSelector.cpp"
    "${Root}/src/UnitCatalog.cpp" "${Root}/src/UnitFactory.cpp"
    "${Root}/src/ArmyGenerator.cpp" "${Root}/src/SaveService.cpp"
    "${Root}/src/BattleReplay.cpp" "${Root}/src/ArmyBuilder.cpp"
    "${Root}/src/SaveSlots.cpp"
    "${Root}/src/Mage.cpp" "${Root}/src/Healer.cpp"
    "${Root}/src/UnitDecorator.cpp"
    "${Root}/src/BuffGiver.cpp" "${Root}/src/UnitCreator.cpp"
    "${Root}/src/MilitaryAcademy.cpp" "${Root}/src/ArmyFactory.cpp")
target_include_directories(test_support PUBLIC "${Root}/include")

add_executable(ui_tests UiTests.cpp
    "${Root}/src/EventFormatter.cpp" "${Root}/src/ConsoleUI.cpp"
    "${Root}/src/Menu.cpp" "${Root}/src/BattleRunner.cpp"
    "${Root}/include/UserInterface.h")
target_sources(ui_tests PRIVATE "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp")
target_link_libraries(ui_tests PRIVATE test_support)
add_executable(data_tests "${Root}/tests/DataTests.cpp")
target_link_libraries(data_tests PRIVATE test_support)

add_executable(session_tests SessionTests.cpp
    "${Root}/src/EventFormatter.cpp" "${Root}/src/ConsoleUI.cpp"
    "${Root}/src/Menu.cpp" "${Root}/src/BattleRunner.cpp"
    "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp"
    "${Root}/include/UserInterface.h")
target_link_libraries(session_tests PRIVATE test_support)

add_executable(save_ui_tests SaveUiTests.cpp
    "${Root}/src/EventFormatter.cpp" "${Root}/src/ConsoleUI.cpp"
    "${Root}/src/Menu.cpp" "${Root}/src/BattleRunner.cpp"
    "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp"
    "${Root}/src/TextInput.cpp" "${Root}/src/SaveLocation.cpp")
target_link_libraries(save_ui_tests PRIVATE test_support)

add_executable(pattern_tests "${Root}/tests/PatternTests.cpp")
target_link_libraries(pattern_tests PRIVATE test_support)

add_executable(ability_engine_tests "${Root}/tests/AbilityEngineTests.cpp"
    "${Root}/src/EventFormatter.cpp")
target_link_libraries(ability_engine_tests PRIVATE test_support)

add_executable(console_layout_tests ConsoleLayoutTests.cpp
    "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp")
target_include_directories(console_layout_tests PRIVATE "${Root}/include")

add_executable(battle_playback_tests BattlePlaybackTests.cpp)
target_link_libraries(battle_playback_tests PRIVATE test_support)

add_executable(flow_tests FlowTests.cpp
    "${Root}/src/ConsoleUI.cpp" "${Root}/src/View.cpp" "${Root}/src/ConsoleView.cpp"
    "${Root}/src/Menu.cpp" "${Root}/src/BattleRunner.cpp" "${Root}/src/EventFormatter.cpp")
target_link_libraries(flow_tests PRIVATE test_support)

foreach(Target test_support ui_tests data_tests session_tests save_ui_tests pattern_tests ability_engine_tests console_layout_tests battle_playback_tests flow_tests)
    if(MSVC)
        target_compile_options(${Target} PRIVATE /utf-8 /W4)
        if(NOT Target STREQUAL "test_support")
            target_sources(${Target} PRIVATE "${Root}/src/WindowsUtf8.manifest")
        endif()
    else()
        target_compile_options(${Target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

enable_testing()
add_test(NAME ui COMMAND ui_tests)
add_test(NAME data COMMAND data_tests)
add_test(NAME session COMMAND session_tests)
add_test(NAME save_ui COMMAND save_ui_tests)
add_test(NAME patterns COMMAND pattern_tests)
add_test(NAME ability_engine COMMAND ability_engine_tests)
add_test(NAME console_layout COMMAND console_layout_tests)
add_test(NAME battle_playback COMMAND battle_playback_tests)
add_test(NAME flow COMMAND flow_tests)
set_tests_properties(ui data session save_ui patterns ability_engine console_layout battle_playback flow PROPERTIES WORKING_DIRECTORY "${Root}")
