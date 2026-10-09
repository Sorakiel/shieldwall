#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "ArmyGenerator.h"
#include "ConsoleUI.h"
#include "ConsoleView.h"
#include "Menu.h"
#if SHIELDWALL_GRAPHICS
#include "SfmlView.h"
#endif
#include "UnitCatalog.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

std::filesystem::path executableDirectory(const char* argument) {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size > 0 && size < path.size()) {
        path.resize(size);
        return std::filesystem::path(path).parent_path();
    }
#endif
    return std::filesystem::absolute(argument).parent_path();
}

std::filesystem::path resource(const std::filesystem::path& directory,
                               const std::filesystem::path& relative) {
    if (std::filesystem::exists(directory / relative)) return directory / relative;
    return relative;
}

std::vector<BattleEvent> demoEvents() {
    return {
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
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    DWORD outputMode = 0;
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(output, &outputMode)) {
        SetConsoleMode(output, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    try {
        bool console = false;
        bool demoLog = false;
        bool demoView = false;
        std::string catalogPath;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help") {
                std::cout << "Shieldwall: закупка, бой по ходам, автосохранение\n"
                          << "shieldwall [--console] [путь к data/units.txt]\n"
                          << "shieldwall --demo-log   — пример журнала в консоли\n"
                          << "shieldwall --demo-view  — пример графического вида и журнала\n";
                return 0;
            }
            if (argument == "--console") console = true;
            else if (argument == "--demo-log") demoLog = true;
            else if (argument == "--demo-view") demoView = true;
            else if (argument.rfind("--", 0) == 0 || !catalogPath.empty()) {
                throw std::runtime_error("Неизвестный аргумент: " + argument + ". См. --help");
            } else catalogPath = argument;
        }
        if ((demoLog && demoView) || (demoView && console)) {
            throw std::runtime_error("Выберите один режим демонстрации");
        }
        if (demoView) {
#if !SHIELDWALL_GRAPHICS
            throw std::runtime_error("Графика отключена. Соберите с -DSHIELDWALL_GRAPHICS=ON");
#endif
        }
        if (demoLog) {
            ConsoleUI ui(std::cin, std::cout);
            ui.message("Демонстрация журнала: заранее заданные события, не настоящий бой.");
            std::mt19937 rng(42);
            ui.showEvents(demoEvents(), rng);
            return 0;
        }
        const auto directory = executableDirectory(argv[0]);
        const auto path = catalogPath.empty() ? resource(directory, "data/units.txt") :
                                               std::filesystem::path(catalogPath);
        const UnitCatalog catalog = UnitCatalog::loadFromFile(path.string());
        std::unique_ptr<View> view;
        if (console) view = std::make_unique<ConsoleView>(std::cin, std::cout);
#if SHIELDWALL_GRAPHICS
        else view = std::make_unique<SfmlView>(resource(directory, "assets/NotoSans-Regular.ttf"));
#else
        else view = std::make_unique<ConsoleView>(std::cin, std::cout);
#endif
        ConsoleUI ui(*view);
        if (demoView) {
            ui.setStage(ViewStage::Recruitment);
            ArmyGenerator generator(catalog);
            std::mt19937 armyRng(42), logRng(42);
            Army a = generator.generate(120, armyRng);
            Army b = generator.generate(120, armyRng);
            ui.showArmies(a, b);
            ui.message("Демонстрация вида: армии и события показаны как отдельные примеры.");
            ui.showEvents(demoEvents(), logRng);
            ui.choose({{0, "Выход"}});
        } else {
            Menu(ui, catalog).run();
        }
        return 0;
    } catch (const ViewClosed&) {
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
