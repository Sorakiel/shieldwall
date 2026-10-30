#pragma once
#include <memory>
#include <random>
#include <string>
#include "Army.h"
#include "UnitCatalog.h"

// Паттерн Abstract Factory для автоматического набора армий. Обе фабрики возвращают
// только готовые армии и укладываются в лимит цены; различается алгоритм набора.
class ArmyFactory {
public:
    virtual ~ArmyFactory() = default;
    virtual Army createArmy(int costLimit, std::mt19937& rng) const = 0;
    virtual std::string title() const = 0;
};

// Фабрика 1, «сжатый» (жадный) алгоритм. Типы сортируются по цене от дорогих к дешёвым;
// кругами покупается по одному юниту каждого типа, пока что-то помещается. Состав не зависит
// от случая, лимит выбирается почти целиком; rng нужен только для имён. Дорогие идут в голову строя.
class CompactArmyFactory : public ArmyFactory {
public:
    explicit CompactArmyFactory(UnitCatalog catalog) : catalog_(std::move(catalog)) {}
    Army createArmy(int costLimit, std::mt19937& rng) const override;
    std::string title() const override { return "Сжатый набор"; }
private:
    UnitCatalog catalog_;
};

// Фабрика 2, случайный алгоритм: делегирует ArmyGenerator, чтобы правило лимита и порядок
// использования mt19937 остались прежними.
class RandomArmyFactory : public ArmyFactory {
public:
    explicit RandomArmyFactory(UnitCatalog catalog) : catalog_(std::move(catalog)) {}
    Army createArmy(int costLimit, std::mt19937& rng) const override;
    std::string title() const override { return "Случайный набор"; }
private:
    UnitCatalog catalog_;
};

enum class ArmyAlgorithm { Compact, Random };
std::unique_ptr<ArmyFactory> makeArmyFactory(ArmyAlgorithm algorithm, const UnitCatalog& catalog);
