#pragma once
#include <cstddef>
#include <random>
#include <vector>
#include "Army.h"
#include "UnitCatalog.h"

// Причина отказа, а не готовый текст: экран (консоль или окно) сам решает,
// как её показать.
enum class BuildResult { Ok, NotEnoughBudget, UnknownKind, BadPosition };

// Ручная закупка армии. Ничего не печатает и не читает ввод, только хранит
// состав и отвечает на вопросы "можно ли", поэтому подходит любому интерфейсу.
// Состав хранится записями, а не готовыми юнитами: порядок в строю можно менять,
// а настоящий Army строится один раз в конце.
class ArmyBuilder {
public:
    ArmyBuilder(UnitCatalog catalog, int costLimit);

    // Продолжает недособранную армию из сохранения. DataError, если состав
    // не помещается в лимит: такое сохранение повреждено или исправлено вручную.
    static ArmyBuilder restore(UnitCatalog catalog, int costLimit, std::vector<UnitRecord> units);

    // Покупает юнита выбранного типа в конец строя, имя берёт случайное
    BuildResult buy(UnitKind kind, std::mt19937& rng);
    BuildResult remove(std::size_t index);
    // Переставляет юнита на новое место, остальные сдвигаются
    BuildResult move(std::size_t from, std::size_t to);

    // Хватит ли денег на ещё одного юнита этого типа - для неактивных кнопок
    bool canBuy(UnitKind kind) const;

    const std::vector<UnitRecord>& units() const { return units_; }
    const UnitCatalog& catalog() const { return catalog_; }
    int limit() const { return costLimit_; }
    int spent() const { return spent_; }
    int remaining() const { return costLimit_ - spent_; }

    Army build() const;

private:
    UnitCatalog catalog_;
    int costLimit_;
    int spent_ = 0;
    std::vector<UnitRecord> units_;
};
