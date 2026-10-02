// Тесты зоны "Армии и данные". Запуск из корня репозитория: sh tests/run_data_tests.sh
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include "ArmyBuilder.h"
#include "ArmyGenerator.h"
#include "BattleReplay.h"
#include <chrono>
#include <fstream>
#include "SaveService.h"
#include "SaveSlots.h"
#include "UnitCatalog.h"
#include "UnitFactory.h"

namespace {

int failures = 0;

void check(bool ok, const char* what, int line) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL (строка " << line << "): " << what << "\n";
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

UnitCatalog loadDefault() { return UnitCatalog::loadFromFile("data/units.txt"); }

// Текст ошибки загрузки или пустая строка, если ошибки не было
template <typename Load>
std::string errorOf(Load load) {
    try {
        load();
    } catch (const DataError& e) {
        return e.what();
    }
    return "";
}

std::string catalogError(const std::string& text) {
    return errorOf([&] {
        std::istringstream in(text);
        UnitCatalog::loadFromStream(in, "test.txt");
    });
}

std::string saveError(const std::string& text) {
    return errorOf([&] {
        std::istringstream in(text);
        SaveService::read(in, "save.txt");
    });
}

// Состав армии одной строкой: по ней сравниваем армии
std::string describe(Army& army) {
    std::ostringstream out;
    for (std::size_t i = 0; i < army.size(); ++i) {
        out << army.at(i)->name() << "=" << army.at(i)->hp() << " ";
    }
    return out.str();
}

void testCatalogReadsFile() {
    UnitCatalog catalog = loadDefault();
    CHECK(catalog.specs().size() == 3);
    const UnitSpec& archer = catalog.specs()[2];
    CHECK(archer.kind == UnitKind::Archer && archer.maxHp == 16 && archer.melee == 4);
    CHECK(archer.ranged == 11 && archer.range == 3 && archer.defense == 0 && archer.cost == 18);
}

void testBrokenCatalog() {
    const std::string unit = "light;20;7;0;0;1;10\n";
    CHECK(contains(catalogError(""), "version"));
    CHECK(contains(catalogError(unit), "строка 1"));
    CHECK(contains(catalogError("version;2\n" + unit), "не поддерживается"));
    CHECK(contains(catalogError("version;1\nlight;20;7;0;0;1\n"), "строка 2"));
    CHECK(contains(catalogError("version;1\nlight;много;7;0;0;1;10\n"), "hp"));
    CHECK(contains(catalogError("version;1\ncavalry;20;7;0;0;1;10\n"), "cavalry"));
    CHECK(contains(catalogError("version;1\n" + unit + unit), "уже описан"));
    CHECK(contains(catalogError("version;1\nlight;20;7;0;0;1;-10\n"), "цена"));
    CHECK(contains(errorOf([] { UnitCatalog::loadFromFile("data/нет_файла.txt"); }), "открыть"));
}

void testFactory() {
    UnitCatalog catalog = loadDefault();
    auto heavy = UnitFactory::create(catalog.specs()[1], "Гурм");
    CHECK(heavy->kind() == UnitKind::Heavy && heavy->name() == "Гурм");
    CHECK(heavy->hp() == 45 && heavy->defense() == 5 && heavy->cost() == 30);

    auto unit = UnitFactory::create(catalog.specs()[2], "Вейн");
    const Archer* archer = dynamic_cast<const Archer*>(unit.get());
    CHECK(archer != nullptr && archer->rangedAttack() == 11 && archer->range() == 3);
}

void testGeneratorStaysWithinLimit() {
    ArmyGenerator generator(loadDefault());
    for (int limit : {0, 9, 10, 18, 100, 500}) {
        for (std::uint32_t seed = 1; seed <= 30; ++seed) {
            std::mt19937 rng(seed);
            Army army = generator.generate(limit, rng);
            CHECK(army.totalCost() <= limit);
            CHECK(limit - army.totalCost() < 10);   // добрал до конца: дешевле юнита нет
        }
    }
}

void testGeneratorIsDeterministic() {
    ArmyGenerator generator(loadDefault());
    std::mt19937 first(2024), second(2024), other(2025);
    Army a = generator.generate(300, first);
    Army b = generator.generate(300, second);
    Army c = generator.generate(300, other);
    CHECK(a.size() > 0);
    CHECK(describe(a) == describe(b));
    CHECK(describe(a) != describe(c));
}

void testManualPurchaseUsesSameRule() {
    UnitCatalog catalog = loadDefault();
    Army army;
    CHECK(ArmyGenerator::tryBuy(army, catalog.specs()[1], "Гурм", 45));    // 30 <= 45
    CHECK(!ArmyGenerator::tryBuy(army, catalog.specs()[1], "Брон", 45));   // 60 > 45
    CHECK(army.size() == 1 && army.totalCost() == 30);
    CHECK(!ArmyGenerator::canAfford(1, 30, 30));
}

void testArmyBuilder() {
    std::mt19937 rng(5);
    ArmyBuilder builder(loadDefault(), 60);
    CHECK(builder.buy(UnitKind::Heavy, rng) == BuildResult::Ok);     // 30
    CHECK(builder.buy(UnitKind::Archer, rng) == BuildResult::Ok);    // 18, всего 48
    CHECK(builder.spent() == 48 && builder.remaining() == 12);
    CHECK(builder.canBuy(UnitKind::Light) && !builder.canBuy(UnitKind::Heavy));

    // Отказ ничего не меняет, а границу лимита проверяет то же canAfford
    CHECK(builder.buy(UnitKind::Heavy, rng) == BuildResult::NotEnoughBudget);
    CHECK(builder.units().size() == 2 && builder.spent() == 48);
    CHECK(builder.buy(UnitKind::Light, rng) == BuildResult::Ok);     // 58
    CHECK(builder.buy(UnitKind::Light, rng) == BuildResult::NotEnoughBudget);

    // Порядок в строю: лучника в начало, потом убрать тяжёлого, деньги возвращаются
    CHECK(builder.move(1, 0) == BuildResult::Ok);
    CHECK(builder.units()[0].spec.kind == UnitKind::Archer);
    CHECK(builder.units()[1].spec.kind == UnitKind::Heavy);
    CHECK(builder.remove(1) == BuildResult::Ok);
    CHECK(builder.spent() == 28 && builder.canBuy(UnitKind::Heavy));
    CHECK(builder.remove(5) == BuildResult::BadPosition);
    CHECK(builder.move(0, 5) == BuildResult::BadPosition);

    // Готовая армия повторяет состав и цену
    Army army = builder.build();
    CHECK(army.size() == 2 && army.totalCost() == builder.spent());
    CHECK(army.at(0)->kind() == UnitKind::Archer && army.at(1)->kind() == UnitKind::Light);
    CHECK(army.at(0)->name() == builder.units()[0].name);

    std::istringstream onlyLight("version;1\nlight;20;7;0;0;1;10\n");
    ArmyBuilder light(UnitCatalog::loadFromStream(onlyLight, "test.txt"), 100);
    CHECK(light.buy(UnitKind::Archer, rng) == BuildResult::UnknownKind);
}

SaveData makeSave(std::uint32_t battleSeed, int turn) {
    ArmyGenerator generator(loadDefault());
    std::mt19937 rng(battleSeed + 100);
    Army a = generator.generate(120, rng);
    Army b = generator.generate(120, rng);
    SaveData data;
    data.phase = SavePhase::Battle;
    data.costLimit = 120;
    data.seed = battleSeed;
    data.turn = turn;
    data.armyA = SaveService::snapshot(a);
    data.armyB = SaveService::snapshot(b);
    return data;
}

std::string serialize(const SaveData& data) {
    std::ostringstream out;
    SaveService::write(data, out);
    return out.str();
}

void testSaveRoundTrip() {
    SaveData data = makeSave(4000000000u, 12);
    std::istringstream in(serialize(data));
    SaveData loaded = SaveService::read(in, "save.txt");
    CHECK(loaded.seed == 4000000000u && loaded.turn == 12);
    CHECK(loaded.phase == SavePhase::Battle && loaded.costLimit == 120);
    CHECK(serialize(loaded) == serialize(data));
    CHECK(!loaded.armyA.empty() && !loaded.armyB.empty());

    // Папка saves/ может ещё не существовать: автосохранение должно её создать
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "shieldwall_saves";
    std::string path = (dir / "auto" / "save.txt").string();
    SaveService::saveToFile(data, path);
    CHECK(serialize(SaveService::loadFromFile(path)) == serialize(data));
    std::filesystem::remove_all(dir);
}

void testBrokenSave() {
    const std::string head = "shieldwall-save;2\nphase;battle\nlimit;40\nseed;5\nturn;3\n";
    CHECK(saveError(head + "army;0;0\narmy;1;0\n").empty());
    CHECK(contains(saveError(""), "файл закончился"));
    CHECK(contains(saveError("version;1\n"), "не сохранение"));
    CHECK(contains(saveError("shieldwall-save;1\n"), "не поддерживается"));
    CHECK(contains(saveError("shieldwall-save;2\nphase;lunch\n"), "неизвестная фаза"));
    CHECK(contains(saveError("shieldwall-save;2\nphase;battle\nlimit;0\nseed;5\nturn;3\narmy;0;0\narmy;1;0\n"),
                   "лимит цены"));
    CHECK(contains(saveError("shieldwall-save;2\nphase;battle\nlimit;40\nseed;-5\n"), "32 бита"));
    CHECK(contains(saveError(head + "army;0;2\nlight;Тиль;20;7;0;0;1;10\n"), "файл закончился"));

    std::string err = saveError(head + "army;0;1\narcher;Вейн;16;4;11;0;0;18\narmy;1;0\n");
    CHECK(contains(err, "save.txt, строка 7") && contains(err, "лучника"));

    // Состав дороже лимита и ход в фазе закупки: такие сохранения не открываются
    const std::string heavy = "army;0;2\nheavy;А;45;9;0;0;5;30\nheavy;Б;45;9;0;0;5;30\narmy;1;0\n";
    CHECK(contains(saveError(head + heavy), "не помещается в лимит 40"));
    const std::string recruitment = "shieldwall-save;2\nphase;recruitment\nlimit;40\nseed;5\nturn;1\n";
    CHECK(contains(saveError(recruitment + "army;0;0\narmy;1;0\n"), "в фазе закупки"));
}

void testSavePhases() {
    // Сохранение в середине закупки: первая армия собрана, вторая пустая
    UnitCatalog catalog = loadDefault();
    std::mt19937 rng(8);
    ArmyBuilder first(catalog, 60);
    first.buy(UnitKind::Heavy, rng);
    first.buy(UnitKind::Archer, rng);

    SaveData data;
    data.phase = SavePhase::Recruitment;
    data.costLimit = 60;
    data.seed = 9;
    data.armyA = first.units();
    std::istringstream in(serialize(data));
    SaveData loaded = SaveService::read(in, "save.txt");
    CHECK(loaded.phase == SavePhase::Recruitment && loaded.turn == 0 && loaded.armyB.empty());

    // Закупка продолжается с того же места и по тем же правилам лимита
    ArmyBuilder restored = ArmyBuilder::restore(catalog, loaded.costLimit, loaded.armyA);
    CHECK(restored.spent() == 48 && restored.remaining() == 12);
    CHECK(restored.buy(UnitKind::Heavy, rng) == BuildResult::NotEnoughBudget);
    CHECK(restored.buy(UnitKind::Light, rng) == BuildResult::Ok);
    CHECK(contains(errorOf([&] { ArmyBuilder::restore(catalog, 40, loaded.armyA); }),
                   "не помещается в лимит 40"));

    // Боя в фазе закупки ещё нет
    CHECK(contains(errorOf([&] { BattleReplay::replay(loaded); }), "в фазе закупки"));

    // Результат: сохранение с последним ходом открывается в уже законченный бой
    SaveData finished = makeSave(42, 0);
    BattleSession live = BattleReplay::replay(finished);
    int turns = 0;
    while (!live.engine->finished()) {
        live.engine->nextTurn();
        ++turns;
    }
    finished.phase = SavePhase::Result;
    finished.turn = turns;
    std::istringstream resultIn(serialize(finished));
    BattleSession over = BattleReplay::replay(SaveService::read(resultIn, "save.txt"));
    CHECK(over.engine->finished() && over.turn == turns);
}

void testSaveSlots() {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "shieldwall_slots";
    fs::remove_all(dir);
    std::string d = dir.string();

    // Папки ещё нет: список пуст, а не ошибка
    CHECK(SaveSlots::list(d).empty());
    CHECK(SaveSlots::suggestName(d) == "save-1");

    SaveData battle = makeSave(1, 5);
    SaveData result = makeSave(2, 9);
    result.phase = SavePhase::Result;
    SaveService::saveToFile(battle, SaveSlots::pathFor(d, "бой"));
    SaveService::saveToFile(result, SaveSlots::pathFor(d, "save-1"));
    SaveService::saveToFile(makeSave(3, 0), SaveSlots::autosavePath(d));
    std::ofstream(dir / "broken.txt") << "мусор\n";
    std::ofstream(dir / "notes.md") << "не сохранение\n";

    // Порядок: автосохранение первым, дальше от новых к старым
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(dir / "save-1.txt", now - std::chrono::hours(1));
    fs::last_write_time(dir / "бой.txt", now - std::chrono::hours(2));
    fs::last_write_time(dir / "broken.txt", now - std::chrono::hours(3));
    auto list = SaveSlots::list(d);
    CHECK(list.size() == 4);   // notes.md не в счёт
    CHECK(list[0].name == "autosave" && list[1].name == "save-1");
    CHECK(list[2].name == "бой" && list[3].name == "broken");

    CHECK(list[1].valid && list[1].phase == SavePhase::Result && list[1].turn == 9);
    CHECK(list[1].costLimit == 120 && list[1].seed == 2);
    CHECK(list[2].valid && list[2].phase == SavePhase::Battle && list[2].turn == 5);
    CHECK(!list[3].valid && contains(list[3].error, "broken.txt"));   // битый не роняет список

    CHECK(SaveSlots::suggestName(d) == "save-2");
    fs::remove_all(dir);

    // Имена: безопасные проходят, пути и зарезервированные нет
    CHECK(SaveSlots::pathFor(d, "Слот 1").size() > 0);
    for (std::string bad : std::vector<std::string>{"", " x", "x ", ".x", "x.", "../x", "a/b", "a\\b", "a:b", "a*b", "con", "COM3",
                            "autosave", std::string(65, 'a')}) {
        CHECK(!errorOf([&] { SaveSlots::pathFor(d, bad); }).empty());
    }
}

void testReplayRestoresSamePicture() {
    SaveData data = makeSave(42, 0);
    BattleSession original = BattleReplay::replay(data);

    int turn = 0;
    while (!original.engine->finished() && turn < 200) {
        original.engine->nextTurn();
        ++turn;

        // Сохранение "на ходу turn" открывается в ту же картину, что и живой бой
        data.turn = turn;
        std::istringstream in(serialize(data));
        BattleSession restored = BattleReplay::replay(SaveService::read(in, "save.txt"));
        CHECK(restored.turn == turn);
        CHECK(describe(*restored.armyA) == describe(*original.armyA));
        CHECK(describe(*restored.armyB) == describe(*original.armyB));
    }
    CHECK(turn > 3);

    data.turn = 5000;   // дальше конца боя
    CHECK(contains(errorOf([&] { BattleReplay::replay(data); }), "сохранение повреждено"));
}

}  // namespace

int main() {
    testCatalogReadsFile();
    testBrokenCatalog();
    testFactory();
    testGeneratorStaysWithinLimit();
    testGeneratorIsDeterministic();
    testManualPurchaseUsesSameRule();
    testArmyBuilder();
    testSaveRoundTrip();
    testBrokenSave();
    testSavePhases();
    testSaveSlots();
    testReplayRestoresSamePicture();

    std::cout << (failures == 0 ? "все тесты пройдены\n" : "есть провалы\n");
    return failures == 0 ? 0 : 1;
}
