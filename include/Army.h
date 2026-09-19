#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Unit.h"

class Army {
public:
    void add(std::unique_ptr<Unit> unit);   // в конец строя
    Unit* at(std::size_t i);
    Unit* front();                          // units_[0] - даже если он мёртв
    std::size_t size() const;
    int  indexOf(const Unit* u) const;      // -1, если юнита нет в армии
    void removeDead();                      // вызывается ровно один раз за ход
    bool isDefeated() const;                // живых не осталось
    int  totalCost() const;

private:
    std::vector<std::unique_ptr<Unit>> units_;
};
