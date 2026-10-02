#include <iostream>
#include <string>
#include "ConsoleUI.h"
#include "Menu.h"
#include "UnitCatalog.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    ConsoleUI ui(std::cin, std::cout);
    if (argc == 2 && std::string(argv[1]) == "--demo-log") {
        ui.message("Демонстрация журнала: заранее заданные события, не настоящий бой.");
        const std::vector<BattleEvent> events = {
            {EventType::Melee, 1, 0, "Тиль", "Гурм", 2, 43},
            {EventType::Shot, 1, 1, "Хорд", "Тиль", 10, 10},
            {EventType::OutOfRange, 1, 0, "Вейн"},
            {EventType::Miss, 2, 0, "Вейн"},
            {EventType::Melee, 2, 1, "Гурм", "Тиль", 10, 0},
            {EventType::Death, 2, 0, "Тиль"},
            {EventType::Promote, 2, 0, "Вейн"},
            {EventType::Shot, 3, 1, "Хорд", "Вейн", 16, 0},
            {EventType::Death, 3, 0, "Вейн"},
            {EventType::BattleEnd, 3, 1}
        };
        std::mt19937 rng(42);
        ui.showEvents(events, rng);
        return 0;
    }
    if (argc > 2 || (argc == 2 && std::string(argv[1]) == "--help")) {
        ui.message("Запуск из корня проекта: shieldwall [путь к data/units.txt]\n"
                   "Демонстрация журнала: shieldwall --demo-log");
        return argc > 2 ? 1 : 0;
    }
    try {
        const std::string path = argc == 2 ? argv[1] : "data/units.txt";
        const UnitCatalog catalog = UnitCatalog::loadFromFile(path);
        Menu(ui, catalog).run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
