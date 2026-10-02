#pragma once
#include <string>
#include <utility>
#include "UserInterface.h"
#include "UnitCatalog.h"

class Menu {
public:
    Menu(UserInterface& ui, const UnitCatalog& catalog,
         std::string autosavePath = "saves/autosave.txt")
        : ui_(ui), catalog_(catalog), autosavePath_(std::move(autosavePath)) {}
    void run();

private:
    enum class State { Welcome, Setup, Recruitment, Ready, Load, Battle, Result, Exit };
    UserInterface& ui_;
    const UnitCatalog& catalog_;
    std::string autosavePath_;
};
