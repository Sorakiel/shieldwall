// Тесты зоны "Армии и данные". Запуск: sh tests/run_data_tests.sh из корня репозитория.
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include "ArmyGenerator.h"
#include "UnitCatalog.h"
#include "UnitFactory.h"

namespace {

int checks = 0;
int failures = 0;

void check(bool ok, const std::string& what, int line) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL (строка " << line << "): " << what << "\n";
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

UnitCatalog loadText(const std::string& text) {
    std::istringstream in(text);
    return UnitCatalog::loadFromStream(in, "test.txt");
}

// Возвращает текст ошибки или пустую строку, если загрузка прошла без ошибки
std::string loadError(const std::string& text) {
    try {
        loadText(text);
    } catch (const DataError& e) {
        return e.what();
    }
    return "";
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void testCatalogFile() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");
    CHECK(catalog.specs().size() == 3);

    const UnitSpec& light = catalog.spec(UnitKind::Light);
    CHECK(light.maxHp == 20 && light.melee == 7 && light.defense == 1 && light.cost == 10);

    const UnitSpec& heavy = catalog.spec(UnitKind::Heavy);
    CHECK(heavy.maxHp == 45 && heavy.melee == 9 && heavy.defense == 5 && heavy.cost == 30);

    const UnitSpec& archer = catalog.spec(UnitKind::Archer);
    CHECK(archer.maxHp == 16 && archer.melee == 4 && archer.ranged == 11);
    CHECK(archer.range == 3 && archer.defense == 0 && archer.cost == 18);

    CHECK(catalog.minCost() == 10);
}

void testCatalogTolerance() {
    // Комментарии, пустые строки, пробелы и CRLF не должны мешать чтению
    UnitCatalog catalog = loadText("\xEF\xBB\xBF" "version;1\r\n\r\n# комментарий\r\n  light ; 20;7;0;0;1;10 \r\n");
    CHECK(catalog.specs().size() == 1);
    CHECK(catalog.spec(UnitKind::Light).cost == 10);
}

void testBrokenCatalog() {
    const std::string good = "light;20;7;0;0;1;10\n";

    std::string err = loadError("");
    CHECK(contains(err, "test.txt") && contains(err, "version"));

    err = loadError(good);
    CHECK(contains(err, "строка 1") && contains(err, "version;N"));

    err = loadError("version;2\n" + good);
    CHECK(contains(err, "строка 1") && contains(err, "не поддерживается"));

    err = loadError("version;abc\n" + good);
    CHECK(contains(err, "версия") && contains(err, "abc"));

    err = loadError("version;1\n");
    CHECK(contains(err, "ни один тип"));

    err = loadError("version;1\nlight;20;7;0;0;1\n");
    CHECK(contains(err, "строка 2") && contains(err, "ожидалось"));

    err = loadError("version;1\nlight;20;7;0;0;1;10;99\n");
    CHECK(contains(err, "строка 2") && contains(err, "лишние"));

    err = loadError("version;1\n# комментарий\nlight;двадцать;7;0;0;1;10\n");
    CHECK(contains(err, "строка 3") && contains(err, "hp") && contains(err, "двадцать"));

    err = loadError("version;1\ncavalry;20;7;0;0;1;10\n");
    CHECK(contains(err, "строка 2") && contains(err, "cavalry"));

    err = loadError("version;1\n" + good + good);
    CHECK(contains(err, "строка 3") && contains(err, "уже описан"));

    err = loadError("version;1\nlight;0;7;0;0;1;10\n");
    CHECK(contains(err, "hp должно быть"));

    err = loadError("version;1\nlight;20;7;0;0;1;-10\n");
    CHECK(contains(err, "цена должна быть"));

    err = loadError("version;1\nlight;20;7;0;0;-1;10\n");
    CHECK(contains(err, "отрицательными"));

    err = loadError("version;1\narcher;16;4;11;0;0;18\n");
    CHECK(contains(err, "лучника"));

    err = loadError("version;1\nheavy;45;9;5;2;5;30\n");
    CHECK(contains(err, "без стрельбы"));

    err = loadError("version;1\nlight;20;7;0;0;1;99999999999\n");
    CHECK(contains(err, "цена") && contains(err, "не целое"));

    try {
        UnitCatalog::loadFromFile("data/нет_такого_файла.txt");
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "не удалось открыть"));
    }
}

void testMissingKind() {
    UnitCatalog catalog = loadText("version;1\nlight;20;7;0;0;1;10\n");
    try {
        catalog.spec(UnitKind::Archer);
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "archer"));
    }
}

void testFactory() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");

    auto light = UnitFactory::create(catalog.spec(UnitKind::Light), "Тиль");
    CHECK(light->kind() == UnitKind::Light && light->name() == "Тиль");
    CHECK(light->hp() == 20 && light->maxHp() == 20 && light->meleeAttack() == 7);
    CHECK(light->defense() == 1 && light->cost() == 10);

    auto heavy = UnitFactory::create(catalog.spec(UnitKind::Heavy), "Гурм");
    CHECK(heavy->kind() == UnitKind::Heavy && heavy->hp() == 45 && heavy->cost() == 30);

    auto unit = UnitFactory::create(catalog.spec(UnitKind::Archer), "Вейн");
    CHECK(unit->kind() == UnitKind::Archer);
    const Archer* archer = dynamic_cast<const Archer*>(unit.get());
    CHECK(archer != nullptr);
    if (archer != nullptr) {
        CHECK(archer->meleeAttack() == 4 && archer->rangedAttack() == 11 && archer->range() == 3);
    }

    // Строковый тип - тот же путь, что и у файла данных
    auto byToken = UnitFactory::create("heavy", catalog.spec(UnitKind::Heavy), "Брон");
    CHECK(byToken->kind() == UnitKind::Heavy);
    try {
        UnitFactory::create("cavalry", catalog.spec(UnitKind::Heavy), "Х");
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "cavalry"));
    }
}

// Состав армии одной строкой: по ней сравниваем армии, не завися от внутренностей Army
std::string describe(Army& army) {
    std::ostringstream out;
    for (std::size_t i = 0; i < army.size(); ++i) {
        const Unit* u = army.at(i);
        out << dataformat::kindToken(u->kind()) << ":" << u->name() << " ";
    }
    return out.str();
}

void testGeneratorLimit() {
    ArmyGenerator generator(UnitCatalog::loadFromFile("data/units.txt"));
    const int minCost = 10;

    for (int limit : {0, 1, 9, 10, 17, 18, 29, 30, 100, 250, 1000}) {
        for (std::uint32_t seed = 1; seed <= 60; ++seed) {
            std::mt19937 rng(seed);
            Army army = generator.generate(limit, rng);
            CHECK(army.totalCost() <= limit);
            // Добирает до конца: остаток меньше самого дешёвого юнита
            CHECK(limit - army.totalCost() < minCost);
            if (limit < minCost) {
                CHECK(army.size() == 0);
            }
        }
    }
}

void testGeneratorDeterminism() {
    ArmyGenerator generator(UnitCatalog::loadFromFile("data/units.txt"));

    std::mt19937 first(2024);
    std::mt19937 second(2024);
    Army a = generator.generate(300, first);
    Army b = generator.generate(300, second);
    CHECK(a.size() > 0);
    CHECK(describe(a) == describe(b));

    // Разные seed должны давать разные армии, иначе генератор ничего не выбирает
    std::string reference = describe(a);
    bool differs = false;
    for (std::uint32_t seed = 1; seed <= 20 && !differs; ++seed) {
        std::mt19937 rng(seed);
        Army other = generator.generate(300, rng);
        differs = describe(other) != reference;
    }
    CHECK(differs);
}

void testGeneratorSharesOneRng() {
    // Генератор продолжает чужую последовательность, а не стартует заново:
    // вторая армия от того же rng не совпадает с первой
    ArmyGenerator generator(UnitCatalog::loadFromFile("data/units.txt"));
    std::mt19937 rng(7);
    Army first = generator.generate(300, rng);
    Army second = generator.generate(300, rng);
    CHECK(describe(first) != describe(second));

    std::mt19937 again(7);
    Army firstAgain = generator.generate(300, again);
    Army secondAgain = generator.generate(300, again);
    CHECK(describe(first) == describe(firstAgain));
    CHECK(describe(second) == describe(secondAgain));
}

void testGeneratorNames() {
    ArmyGenerator generator(UnitCatalog::loadFromFile("data/units.txt"));
    std::mt19937 rng(11);
    Army army = generator.generate(2000, rng);
    CHECK(army.size() > 16);
    for (std::size_t i = 0; i < army.size(); ++i) {
        for (std::size_t j = i + 1; j < army.size(); ++j) {
            CHECK(army.at(i)->name() != army.at(j)->name());
        }
    }
}

void testManualPurchaseRule() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");
    const UnitSpec& heavy = catalog.spec(UnitKind::Heavy);
    const UnitSpec& light = catalog.spec(UnitKind::Light);

    CHECK(ArmyGenerator::canAfford(0, 30, 30));
    CHECK(!ArmyGenerator::canAfford(1, 30, 30));
    CHECK(!ArmyGenerator::canAfford(0, 30, -5));

    Army army;
    CHECK(ArmyGenerator::tryBuy(army, heavy, "Гурм", 45));
    CHECK(army.totalCost() == 30);
    CHECK(!ArmyGenerator::tryBuy(army, heavy, "Брон", 45));   // 60 > 45
    CHECK(army.size() == 1);                                   // отказ ничего не меняет
    CHECK(ArmyGenerator::tryBuy(army, light, "Тиль", 45));
    CHECK(ArmyGenerator::tryBuy(army, light, "Кай", 45) == false);
    CHECK(army.totalCost() == 40);
}

}  // namespace

int main() {
    testCatalogFile();
    testCatalogTolerance();
    testBrokenCatalog();
    testMissingKind();
    testFactory();
    testGeneratorLimit();
    testGeneratorDeterminism();
    testGeneratorSharesOneRng();
    testGeneratorNames();
    testManualPurchaseRule();

    std::cout << "проверок: " << checks << ", провалено: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
