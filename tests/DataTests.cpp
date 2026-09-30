// Тесты зоны "Армии и данные". Запуск: sh tests/run_data_tests.sh из корня репозитория.
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include "ArmyGenerator.h"
#include "SaveService.h"
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

const UnitSpec& specOf(const UnitCatalog& catalog, UnitKind kind) {
    for (const UnitSpec& s : catalog.specs()) {
        if (s.kind == kind) return s;
    }
    throw DataError("в тестовом каталоге нет нужного типа");
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void testCatalogFile() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");
    CHECK(catalog.specs().size() == 3);

    const UnitSpec& light = specOf(catalog, UnitKind::Light);
    CHECK(light.maxHp == 20 && light.melee == 7 && light.defense == 1 && light.cost == 10);

    const UnitSpec& heavy = specOf(catalog, UnitKind::Heavy);
    CHECK(heavy.maxHp == 45 && heavy.melee == 9 && heavy.defense == 5 && heavy.cost == 30);

    const UnitSpec& archer = specOf(catalog, UnitKind::Archer);
    CHECK(archer.maxHp == 16 && archer.melee == 4 && archer.ranged == 11);
    CHECK(archer.range == 3 && archer.defense == 0 && archer.cost == 18);

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

void testFactory() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");

    auto light = UnitFactory::create(specOf(catalog, UnitKind::Light), "Тиль");
    CHECK(light->kind() == UnitKind::Light && light->name() == "Тиль");
    CHECK(light->hp() == 20 && light->maxHp() == 20 && light->meleeAttack() == 7);
    CHECK(light->defense() == 1 && light->cost() == 10);

    auto heavy = UnitFactory::create(specOf(catalog, UnitKind::Heavy), "Гурм");
    CHECK(heavy->kind() == UnitKind::Heavy && heavy->hp() == 45 && heavy->cost() == 30);

    auto unit = UnitFactory::create(specOf(catalog, UnitKind::Archer), "Вейн");
    CHECK(unit->kind() == UnitKind::Archer);
    const Archer* archer = dynamic_cast<const Archer*>(unit.get());
    CHECK(archer != nullptr);
    if (archer != nullptr) {
        CHECK(archer->meleeAttack() == 4 && archer->rangedAttack() == 11 && archer->range() == 3);
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

void testManualPurchaseRule() {
    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");
    const UnitSpec& heavy = specOf(catalog, UnitKind::Heavy);
    const UnitSpec& light = specOf(catalog, UnitKind::Light);

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

bool sameRecords(const std::vector<UnitRecord>& x, const std::vector<UnitRecord>& y) {
    if (x.size() != y.size()) return false;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const UnitSpec& p = x[i].spec;
        const UnitSpec& q = y[i].spec;
        if (x[i].name != y[i].name || p.kind != q.kind || p.maxHp != q.maxHp ||
            p.melee != q.melee || p.ranged != q.ranged || p.range != q.range ||
            p.defense != q.defense || p.cost != q.cost) {
            return false;
        }
    }
    return true;
}

SaveData makeSave(std::uint32_t armySeed, std::uint32_t battleSeed, int turn, int limit) {
    ArmyGenerator generator(UnitCatalog::loadFromFile("data/units.txt"));
    std::mt19937 rng(armySeed);
    Army a = generator.generate(limit, rng);
    Army b = generator.generate(limit, rng);
    SaveData data;
    data.seed = battleSeed;
    data.turn = turn;
    data.armyA = SaveService::snapshot(a);
    data.armyB = SaveService::snapshot(b);
    return data;
}

std::string saveError(const std::string& text) {
    std::istringstream in(text);
    try {
        SaveService::read(in, "save.txt");
    } catch (const DataError& e) {
        return e.what();
    }
    return "";
}

void testSnapshot() {
    SaveData data = makeSave(5, 1, 0, 200);
    CHECK(!data.armyA.empty() && !data.armyB.empty());

    UnitCatalog catalog = UnitCatalog::loadFromFile("data/units.txt");
    Army army;
    army.add(UnitFactory::create(specOf(catalog, UnitKind::Archer), "Вейн"));
    army.add(UnitFactory::create(specOf(catalog, UnitKind::Heavy), "Гурм"));
    army.at(1)->takeDamage(10);   // в записи остаётся maxHp, а не текущее hp
    std::vector<UnitRecord> records = SaveService::snapshot(army);
    CHECK(records.size() == 2);
    CHECK(records[0].name == "Вейн" && records[0].spec.kind == UnitKind::Archer);
    CHECK(records[0].spec.ranged == 11 && records[0].spec.range == 3 && records[0].spec.melee == 4);
    CHECK(records[1].spec.maxHp == 45 && records[1].spec.ranged == 0);
}

void testSaveRoundTrip() {
    SaveData data = makeSave(9, 4000000000u, 12, 300);

    std::ostringstream first;
    SaveService::write(data, first);
    std::istringstream in(first.str());
    SaveData loaded = SaveService::read(in, "save.txt");

    CHECK(loaded.seed == 4000000000u);
    CHECK(loaded.turn == 12);
    CHECK(sameRecords(loaded.armyA, data.armyA));
    CHECK(sameRecords(loaded.armyB, data.armyB));

    std::ostringstream second;
    SaveService::write(loaded, second);
    CHECK(first.str() == second.str());

    SaveData empty;
    std::ostringstream emptyText;
    SaveService::write(empty, emptyText);
    std::istringstream emptyIn(emptyText.str());
    SaveData emptyLoaded = SaveService::read(emptyIn, "save.txt");
    CHECK(emptyLoaded.armyA.empty() && emptyLoaded.armyB.empty());
}

void testSaveFile() {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "shieldwall_data_tests";
    std::filesystem::create_directories(dir);
    std::string path = (dir / "save.txt").string();
    SaveData data = makeSave(3, 77, 5, 150);
    SaveService::saveToFile(data, path);
    SaveData loaded = SaveService::loadFromFile(path);
    CHECK(loaded.seed == 77 && loaded.turn == 5);
    CHECK(sameRecords(loaded.armyA, data.armyA));
    std::filesystem::remove_all(dir);

    try {
        SaveService::loadFromFile((dir / "missing.txt").string());
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "не удалось открыть"));
    }
}

void testBrokenSave() {
    const std::string army0 = "army;0;1\nlight;Тиль;20;7;0;0;1;10\n";
    const std::string army1 = "army;1;0\n";
    const std::string good = "shieldwall-save;1\nseed;5\nturn;3\n" + army0 + army1;
    CHECK(saveError(good).empty());

    CHECK(contains(saveError(""), "файл закончился"));
    CHECK(contains(saveError("version;1\n"), "не сохранение"));
    CHECK(contains(saveError("shieldwall-save;9\nseed;5\n"), "не поддерживается"));
    CHECK(contains(saveError("shieldwall-save;1\ntrash;5\n"), "«seed»"));
    CHECK(contains(saveError("shieldwall-save;1\nseed;-5\n"), "32 бита"));
    CHECK(contains(saveError("shieldwall-save;1\nseed;99999999999\n"), "32 бита"));
    CHECK(contains(saveError("shieldwall-save;1\nseed;5\nturn;-1\n"), "отрицательным"));
    CHECK(contains(saveError("shieldwall-save;1\nseed;5\nturn;x\n"), "номер хода"));

    std::string head = "shieldwall-save;1\nseed;5\nturn;3\n";
    CHECK(contains(saveError(head + "army;1;0\n"), "по порядку"));
    CHECK(contains(saveError(head + "army;0;2\nlight;Тиль;20;7;0;0;1;10\n"), "файл закончился"));
    CHECK(contains(saveError(head + "army;0;1\nlight;Тиль;20;7;0;1;10\n"), "8 полей"));
    CHECK(contains(saveError(head + "army;0;1\nlight;;20;7;0;0;1;10\n" + army1), "пустое имя"));
    CHECK(contains(saveError(head + "army;0;1\ncavalry;Х;20;7;0;0;1;10\n" + army1), "cavalry"));
    CHECK(contains(saveError(head + "army;0;1\nlight;Тиль;0;7;0;0;1;10\n" + army1), "hp"));
    CHECK(contains(saveError(good + "army;2;0\n"), "лишние строки"));

    std::string err = saveError(head + army0 + "army;1;1\narcher;Вейн;16;4;11;0;0;18\n");
    CHECK(contains(err, "save.txt, строка 7") && contains(err, "лучника"));

    // Имя с разделителем испортило бы файл, поэтому запись отказывает сразу
    SaveData data = makeSave(1, 1, 0, 50);
    data.armyA[0].name = "Вейн;Гурм";
    std::ostringstream out;
    try {
        SaveService::write(data, out);
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "нельзя записать"));
    }
}

// Картина боя одной строкой: кто стоит в строю и сколько у него hp
std::string picture(Army& a, Army& b) {
    std::ostringstream out;
    for (Army* army : {&a, &b}) {
        for (std::size_t i = 0; i < army->size(); ++i) {
            out << army->at(i)->name() << "=" << army->at(i)->hp() << " ";
        }
        out << "| ";
    }
    return out.str();
}

void testReplayMatchesOriginalBattle() {
    for (std::uint32_t battleSeed : {1u, 42u, 2024u}) {
        SaveData data = makeSave(battleSeed + 100, battleSeed, 0, 120);

        // Оригинальный бой идёт в одной сессии, а с каждого хода проверяем восстановление
        BattleSession original = SaveService::replay(data);
        int turn = 0;
        while (!original.engine->finished() && turn < 200) {
            original.engine->nextTurn();
            ++turn;

            SaveData saved = data;
            saved.turn = turn;
            std::ostringstream text;
            SaveService::write(saved, text);
            std::istringstream in(text.str());
            BattleSession restored = SaveService::replay(SaveService::read(in, "save.txt"));

            CHECK(restored.turn == turn);
            CHECK(picture(*restored.armyA, *restored.armyB) ==
                  picture(*original.armyA, *original.armyB));
        }
        CHECK(turn > 3);   // бой успел пройти несколько ходов
    }
}

void testReplayPastEnd() {
    SaveData data = makeSave(1, 1, 5000, 30);
    try {
        SaveService::replay(data);
        CHECK(false);
    } catch (const DataError& e) {
        CHECK(contains(e.what(), "сохранение повреждено"));
    }
}

}  // namespace

int main() {
    testCatalogFile();
    testBrokenCatalog();
    testFactory();
    testGeneratorLimit();
    testGeneratorDeterminism();
    testGeneratorSharesOneRng();
    testManualPurchaseRule();
    testSnapshot();
    testSaveRoundTrip();
    testSaveFile();
    testBrokenSave();
    testReplayMatchesOriginalBattle();
    testReplayPastEnd();

    std::cout << "проверок: " << checks << ", провалено: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
