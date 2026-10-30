// Тесты новых юнитов и паттернов зоны "Армии и данные". Запуск из корня репозитория:
// cmake -S tests/ui -B build/ui-tests && cmake --build build/ui-tests && ctest --test-dir build/ui-tests
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include "ArmyFactory.h"
#include "BuffGiver.h"
#include "Healer.h"
#include "Mage.h"
#include "MilitaryAcademy.h"
#include "SaveService.h"
#include "UnitCatalog.h"
#include "UnitCreator.h"
#include "UnitDecorator.h"
#include "UnitFactory.h"

namespace {

int failures = 0;
void check(bool ok, const char* what, int line) {
    if (!ok) { ++failures; std::cerr << "FAIL (строка " << line << "): " << what << "\n"; }
}
#define CHECK(cond) check((cond), #cond, __LINE__)

UnitCatalog loadDefault() { return UnitCatalog::loadFromFile("data/units.txt"); }

std::unique_ptr<Unit> make(const UnitCatalog& c, UnitKind kind, const std::string& name) {
    for (const UnitSpec& s : c.specs()) if (s.kind == kind) return UnitFactory::create(s, name);
    return nullptr;
}

void testCatalogHasNewTypes() {
    UnitCatalog c = loadDefault();
    CHECK(c.specs().size() == 5);
    auto mage = make(c, UnitKind::Mage, "Мерлин");
    CHECK(mage && mage->kind() == UnitKind::Mage);
    CHECK(dynamic_cast<IClonable*>(mage.get()) == nullptr);   // маг не клонируется
    CHECK(dynamic_cast<IHealable*>(mage.get()) == nullptr);   // и не лечится
    auto healer = make(c, UnitKind::Healer, "Ада");
    CHECK(dynamic_cast<IHealable*>(healer.get()) != nullptr);
    CHECK(dynamic_cast<IClonable*>(healer.get()) == nullptr);
    auto light = make(c, UnitKind::Light, "Тиль");
    CHECK(dynamic_cast<IClonable*>(light.get()) && dynamic_cast<IHealable*>(light.get()));
    auto heavy = make(c, UnitKind::Heavy, "Гурм");
    CHECK(dynamic_cast<IClonable*>(heavy.get()) == nullptr && dynamic_cast<IHealable*>(heavy.get()));
}

void testBadNewSpecs() {
    auto err = [](const std::string& line) {
        std::istringstream in("version;1\n" + line + "\n");
        try { UnitCatalog::loadFromStream(in, "t.txt"); } catch (const DataError&) { return true; }
        return false;
    };
    CHECK(err("mage;14;2;0;3;0;25"));        // нулевая сила
    CHECK(err("mage;14;2;150;3;0;25"));      // шанс больше 100
    CHECK(err("healer;18;2;8;0;0;22"));      // нулевой радиус
    CHECK(err("light;20;7;5;0;1;10"));       // выстрел у лёгкого
}

void testMage() {
    UnitCatalog c = loadDefault();
    std::mt19937 rng(7);
    Army a;
    a.add(make(c, UnitKind::Light, "Первый"));
    a.add(make(c, UnitKind::Mage, "Маг"));
    a.add(make(c, UnitKind::Heavy, "Тяжёлый"));
    Mage* mage = dynamic_cast<Mage*>(a.at(1));

    // Единственный клонируемый - лёгкий; тяжёлый и сам маг не подходят
    int successes = 0;
    for (int i = 0; i < 40; ++i) {
        Army b;
        b.add(make(c, UnitKind::Light, "Первый"));
        b.add(make(c, UnitKind::Mage, "Маг"));
        b.add(make(c, UnitKind::Heavy, "Тяжёлый"));
        auto r = dynamic_cast<Mage*>(b.at(1))->cast(b, 1, rng);
        CHECK(r.performed && r.target == "Первый");
        if (r.success) {
            ++successes;
            CHECK(b.size() == 4);
            CHECK(b.at(1)->name() == "Первый" && b.at(2)->kind() == UnitKind::Mage);   // копия перед магом
        } else {
            CHECK(b.size() == 3);
        }
    }
    CHECK(successes > 10 && successes < 40);   // шанс 60%

    // Первый ряд не колдует
    Army front;
    front.add(make(c, UnitKind::Mage, "Маг"));
    front.add(make(c, UnitKind::Light, "Л"));
    CHECK(!dynamic_cast<Mage*>(front.at(0))->cast(front, 0, rng).performed);
    CHECK(front.size() == 2);

    // Радиус: клонируемый вне радиуса недосягаем (радиус мага 3)
    Army far;
    far.add(make(c, UnitKind::Light, "Далёкий"));
    for (int i = 0; i < 4; ++i) far.add(make(c, UnitKind::Heavy, "Т"));
    far.add(make(c, UnitKind::Mage, "Маг"));
    CHECK(!dynamic_cast<Mage*>(far.at(5))->cast(far, 5, rng).performed);
    (void)mage;
}

void testHealer() {
    UnitCatalog c = loadDefault();
    std::mt19937 rng(3);
    Army a;
    a.add(make(c, UnitKind::Light, "Раненый"));
    a.add(make(c, UnitKind::Healer, "Ада"));
    a.add(make(c, UnitKind::Mage, "Маг"));
    a.at(0)->takeDamage(10);
    a.at(2)->takeDamage(5);   // маг ранен, но лечить его нельзя
    Healer* h = dynamic_cast<Healer*>(a.at(1));
    auto r = h->cast(a, 1, rng);
    CHECK(r.performed && r.target == "Раненый" && r.restored == 8);
    CHECK(a.at(0)->hp() == 18 && a.at(2)->hp() == 9);
    r = h->cast(a, 1, rng);
    CHECK(r.restored == 2 && a.at(0)->hp() == 20);          // не выше максимума
    r = h->cast(a, 1, rng);
    CHECK(!r.performed && a.at(2)->hp() == 9);              // остался только маг - никого не лечим

    Army front;
    front.add(make(c, UnitKind::Healer, "Ада"));
    front.add(make(c, UnitKind::Light, "Л"));
    front.at(1)->takeDamage(5);
    CHECK(!dynamic_cast<Healer*>(front.at(0))->cast(front, 0, rng).performed);
    CHECK(front.at(1)->hp() == 15);
}

void testDecorator() {
    UnitCatalog c = loadDefault();
    auto heavy = make(c, UnitKind::Heavy, "Гурм");
    auto helmeted = decorate(std::move(heavy), BuffKind::Helmet);
    CHECK(helmeted->defense() == 7 && helmeted->name() == "Гурм [шлем]");
    CHECK(helmeted->kind() == UnitKind::Heavy);
    auto spear = decorate(std::move(helmeted), BuffKind::Spear);   // обёртки складываются
    CHECK(spear->defense() == 7 && spear->meleeAttack() == 13);
    spear->takeDamage(5);
    CHECK(spear->hp() == 40);
    auto copy = spear->clone();
    CHECK(copy->hp() == 40 && copy->meleeAttack() == 13 && copy->name() == spear->name());
    copy->takeDamage(10);
    CHECK(spear->hp() == 40);   // копия независима
    dynamic_cast<IHealable*>(spear.get())->heal(100);
    CHECK(spear->hp() == 45);

    bool threw = false;
    try { decorate(make(c, UnitKind::Mage, "М"), BuffKind::Shield); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);   // мага нельзя лечить, значит и декорировать нельзя
}

void testBuffGiver() {
    UnitCatalog c = loadDefault();
    std::mt19937 rng(11);
    BuffGiver giver(100);   // шанс 100% - проверяем правило "один за ход"
    Army a;
    a.add(make(c, UnitKind::Heavy, "Т1"));
    a.add(make(c, UnitKind::Light, "Л1"));
    a.add(make(c, UnitKind::Heavy, "Т2"));
    a.add(make(c, UnitKind::Light, "Л2"));

    giver.newTurn();
    auto r1 = giver.tryGrant(a, 1, rng);
    CHECK(r1.granted && giver.usedThisTurn());
    auto r2 = giver.tryGrant(a, 3, rng);
    CHECK(!r2.granted);                     // второй бонус за тот же ход нельзя
    giver.newTurn();
    auto r3 = giver.tryGrant(a, 3, rng);
    CHECK(r3.granted && r3.receiver.rfind("Т2", 0) == 0);   // у Л2 единственный сосед-тяжёлый - Т2
    CHECK(a.at(0)->name().find('[') != std::string::npos || a.at(2)->name().find('[') != std::string::npos);
    CHECK(a.size() == 4);

    // Нет тяжёлого рядом - бонуса нет
    Army lone;
    lone.add(make(c, UnitKind::Light, "Л"));
    lone.add(make(c, UnitKind::Archer, "Лук"));
    giver.newTurn();
    CHECK(!giver.tryGrant(lone, 0, rng).granted);

    // Шанс 0 - бонуса нет
    BuffGiver never(0);
    CHECK(!never.tryGrant(a, 1, rng).granted);
}

void testFactoryMethodAndAcademy() {
    UnitCatalog c = loadDefault();
    CHECK(UnitCreator::forKind(UnitKind::Mage).kind() == UnitKind::Mage);
    MilitaryAcademy academy(c);
    CHECK(academy.available().size() == 5);
    CHECK(academy.offers(UnitKind::Healer) && academy.sample(UnitKind::Heavy)->maxHp() == 45);

    Army army;
    CHECK(academy.tryEnlist(army, UnitKind::Heavy, "А", 60));   // 30
    CHECK(academy.tryEnlist(army, UnitKind::Heavy, "Б", 60));   // 60
    CHECK(!academy.tryEnlist(army, UnitKind::Light, "В", 60));  // не помещается
    CHECK(army.size() == 2 && army.totalCost() == 60);
    auto u1 = academy.enlist(UnitKind::Archer, "Х");
    auto u2 = academy.enlist(UnitKind::Archer, "Х");
    CHECK(u1.get() != u2.get());   // каждый раз новый экземпляр, не образец

    // Академия только из части типов не продаёт чужих
    std::istringstream in("version;1\nlight;20;7;0;0;1;10\n");
    MilitaryAcademy small(UnitCatalog::loadFromStream(in, "t"));
    CHECK(!small.offers(UnitKind::Mage));
    CHECK(!small.tryEnlist(army, UnitKind::Mage, "М", 1000));
}

void testArmyFactories() {
    UnitCatalog c = loadDefault();
    for (ArmyAlgorithm algo : {ArmyAlgorithm::Compact, ArmyAlgorithm::Random}) {
        auto factory = makeArmyFactory(algo, c);
        for (int limit : {0, 9, 10, 100, 300}) {
            std::mt19937 rng(42);
            Army army = factory->createArmy(limit, rng);
            CHECK(army.totalCost() <= limit);
            CHECK(limit < 10 ? army.size() == 0 : army.size() > 0);
        }
    }
    // Сжатая фабрика: состав не зависит от seed, лимит выбирается плотно
    auto compact = makeArmyFactory(ArmyAlgorithm::Compact, c);
    std::mt19937 r1(1), r2(999);
    Army a = compact->createArmy(150, r1), b = compact->createArmy(150, r2);
    CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) CHECK(a.at(i)->kind() == b.at(i)->kind());
    CHECK(a.at(0)->kind() == UnitKind::Heavy);      // самый дорогой первым
    CHECK(150 - a.totalCost() < 10);                // остаток меньше самого дешёвого юнита
    // Случайная фабрика воспроизводима по seed
    auto random = makeArmyFactory(ArmyAlgorithm::Random, c);
    std::mt19937 s1(5), s2(5);
    Army x = random->createArmy(200, s1), y = random->createArmy(200, s2);
    CHECK(x.size() == y.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        CHECK(x.at(i)->kind() == y.at(i)->kind() && x.at(i)->name() == y.at(i)->name());
}

void testSaveRoundTrip() {
    UnitCatalog c = loadDefault();
    auto factory = makeArmyFactory(ArmyAlgorithm::Compact, c);
    std::mt19937 rng(8);
    Army a = factory->createArmy(200, rng), b = factory->createArmy(200, rng);
    SaveData data;
    data.phase = SavePhase::Battle; data.costLimit = 200; data.seed = 77; data.turn = 0;
    data.armyA = SaveService::snapshot(a);
    data.armyB = SaveService::snapshot(b);
    std::ostringstream out;
    SaveService::write(data, out);
    std::istringstream in(out.str());
    SaveData back = SaveService::read(in, "mem");
    CHECK(back.armyA.size() == a.size());
    bool same = true;
    for (std::size_t i = 0; i < back.armyA.size(); ++i) {
        const UnitSpec& s = back.armyA[i].spec;
        auto rebuilt = UnitFactory::create(s, back.armyA[i].name);
        const Unit* orig = a.at(i);
        same = same && rebuilt->kind() == orig->kind() && rebuilt->maxHp() == orig->maxHp() &&
               rebuilt->defense() == orig->defense() && rebuilt->meleeAttack() == orig->meleeAttack();
    }
    CHECK(same);
}

}  // namespace

int main() {
    testCatalogHasNewTypes();
    testBadNewSpecs();
    testMage();
    testHealer();
    testDecorator();
    testBuffGiver();
    testFactoryMethodAndAcademy();
    testArmyFactories();
    testSaveRoundTrip();
    if (failures == 0) { std::cout << "pattern tests: OK\n"; return 0; }
    std::cerr << failures << " проверок не прошло\n";
    return 1;
}
