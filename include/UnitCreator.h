#pragma once
#include <memory>
#include <string>
#include "Unit.h"
#include "UnitCatalog.h"

// Паттерн Factory Method: у каждого типа юнита свой создатель, который знает, какой
// конкретный класс породить. Тот, кто просит юнита, получает Unit и конкретных классов не знает.
class UnitCreator {
public:
    virtual ~UnitCreator() = default;
    virtual UnitKind kind() const = 0;
    virtual std::unique_ptr<Unit> create(const UnitSpec& spec, std::string name) const = 0;

    // Создатель нужного типа. DataError, если тип не зарегистрирован.
    static const UnitCreator& forKind(UnitKind kind);
};
