#include "UnitFactory.h"
#include "UnitCreator.h"

// Фасад над Factory Method: прежний вызов UnitFactory::create работает как раньше
// (его используют ArmyBuilder, BattleReplay и тесты), а выбор класса живёт в UnitCreator.
std::unique_ptr<Unit> UnitFactory::create(const UnitSpec& spec, std::string name) {
    return UnitCreator::forKind(spec.kind).create(spec, std::move(name));
}
