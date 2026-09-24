#pragma once
#include <memory>
#include <string>
#include "Unit.h"
#include "UnitCatalog.h"

// Единственное место, где тип юнита превращается в конкретный класс.
// Новый тип - новая ветка здесь плюс запись в файле данных, остальной код не меняется.
class UnitFactory {
public:
    static std::unique_ptr<Unit> create(const UnitSpec& spec, std::string name);
    // Для разбора строковых типов из файлов: "light" -> LightUnit и т.д.
    static std::unique_ptr<Unit> create(const std::string& kindToken, const UnitSpec& stats,
                                        std::string name);
};
