// Тесты зоны "Армии и данные". Запуск: sh tests/run_data_tests.sh из корня репозитория.
#include <iostream>
#include <sstream>
#include <string>
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

}  // namespace

int main() {
    testCatalogFile();
    testCatalogTolerance();
    testBrokenCatalog();
    testMissingKind();
    testFactory();

    std::cout << "проверок: " << checks << ", провалено: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
