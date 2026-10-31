#include "BuffGiver.h"
#include <vector>

namespace {

const BuffKind AllBuffs[] = { BuffKind::Helmet, BuffKind::Shield, BuffKind::Spear, BuffKind::Horse };

// Виды бонусов, которых у юнита ещё нет, в порядке перечисления BuffKind
std::vector<BuffKind> missingBuffs(const Unit& unit) {
    std::vector<BuffKind> missing;
    for (BuffKind buff : AllBuffs) {
        if (!hasBuff(unit, buff)) missing.push_back(buff);
    }
    return missing;
}

}  // namespace

BuffGiver::Result BuffGiver::tryGrant(Army& army, std::size_t lightIndex, std::mt19937& rng) {
    Result result;
    if (usedThisTurn_ || lightIndex >= army.size()) return result;

    Unit* giver = army.at(lightIndex);
    if (!giver->isAlive() || giver->kind() != UnitKind::Light) return result;

    std::vector<std::size_t> heavies;
    if (lightIndex > 0) heavies.push_back(lightIndex - 1);
    if (lightIndex + 1 < army.size()) heavies.push_back(lightIndex + 1);
    std::vector<std::size_t> neighbours;
    for (std::size_t i : heavies) {
        Unit* unit = army.at(i);
        // Тяжёлый, у которого уже есть все бонусы, получателем не считается
        if (unit->isAlive() && unit->kind() == UnitKind::Heavy && !missingBuffs(*unit).empty()) {
            neighbours.push_back(i);
        }
    }
    if (neighbours.empty()) return result;

    // Бросок шанса делаем только когда есть кому давать: иначе зря тратится случайность
    if (static_cast<int>(rng() % 100) >= chance_) return result;

    const std::size_t target = neighbours[rng() % neighbours.size()];
    // Выбираем только из бонусов, которых у получателя ещё нет: один вид не вешается дважды
    const std::vector<BuffKind> missing = missingBuffs(*army.at(target));
    const BuffKind buff = missing[rng() % missing.size()];

    result.giver = giver->name();
    result.receiver = army.at(target)->name();
    result.buff = buff;
    // Самая обёртка: вынули тяжёлого, завернули в бонус, поставили на то же место
    std::unique_ptr<Unit> old = army.replace(target, nullptr);
    army.replace(target, decorate(std::move(old), buff));
    result.granted = true;
    usedThisTurn_ = true;
    return result;
}
