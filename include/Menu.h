#pragma once
#include "ConsoleUI.h"
#include "UnitCatalog.h"

class Menu {
public:
    Menu(ConsoleUI& ui, const UnitCatalog& catalog) : ui_(ui), catalog_(catalog) {}
    void run();

private:
    enum class State { Setup, Recruitment, Battle, Result, Exit };

    ConsoleUI& ui_;
    const UnitCatalog& catalog_;
};
