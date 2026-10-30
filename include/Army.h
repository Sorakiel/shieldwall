#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Unit.h"

class Army {
public:
    void add(std::unique_ptr<Unit> unit);   // в конец строя
    // Вставка на место index: тот, кто стоял там, и все за ним сдвигаются назад.
    // Для клонирования магом ("ставит копию перед собой"). index == size() - то же, что add.
    void insert(std::size_t index, std::unique_ptr<Unit> unit);
    // Подмена юнита на месте (для декоратора). Возвращает прежнего, чтобы обернуть его.
    std::unique_ptr<Unit> replace(std::size_t index, std::unique_ptr<Unit> unit);
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
