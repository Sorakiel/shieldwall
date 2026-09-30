#include "SaveService.h"
#include <fstream>
#include <sstream>
#include "UnitFactory.h"

namespace {

const char* const Header = "shieldwall-save";

void checkName(const std::string& name) {
    if (name.empty() || name.find_first_of(";\r\n") != std::string::npos ||
        name != dataformat::trim(name)) {
        throw DataError("имя юнита «" + name + "» нельзя записать в сохранение");
    }
}

void writeRecords(std::ostream& out, int index, const std::vector<UnitRecord>& records) {
    out << "army;" << index << ";" << records.size() << "\n";
    for (const UnitRecord& r : records) {
        checkName(r.name);
        dataformat::validateSpec(r.spec);
        out << dataformat::kindToken(r.spec.kind) << ";" << r.name << ";" << r.spec.maxHp << ";"
            << r.spec.melee << ";" << r.spec.ranged << ";" << r.spec.range << ";"
            << r.spec.defense << ";" << r.spec.cost << "\n";
    }
}

std::vector<std::string> nextFields(dataformat::LineReader& reader, const std::string& expected) {
    std::string line;
    if (!reader.next(line)) {
        throw DataError("файл закончился, а ожидалось: " + expected);
    }
    return dataformat::split(line, ';');
}

std::uint32_t parseSeed(const std::string& field) {
    std::size_t used = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(field, &used);
    } catch (const std::exception&) {
        throw DataError("seed: «" + field + "» не целое число");
    }
    if (used != field.size() || field[0] == '-' || value > 0xFFFFFFFFull) {
        throw DataError("seed: «" + field + "» не помещается в 32 бита");
    }
    return static_cast<std::uint32_t>(value);
}

void expectFields(const std::vector<std::string>& fields, const std::string& key,
                  std::size_t count) {
    if (fields[0] != key || fields.size() != count) {
        throw DataError("ожидалась строка «" + key + "» из " + std::to_string(count) + " полей");
    }
}

std::vector<UnitRecord> readArmy(dataformat::LineReader& reader, int index) {
    std::string title = "army;" + std::to_string(index) + ";N";
    std::vector<std::string> head = nextFields(reader, title);
    expectFields(head, "army", 3);
    if (dataformat::parseInt(head[1], "номер армии") != index) {
        throw DataError("армии должны идти по порядку, ожидалась «" + title + "»");
    }
    int count = dataformat::parseInt(head[2], "размер армии");
    if (count < 0) {
        throw DataError("размер армии не может быть отрицательным");
    }

    std::vector<UnitRecord> records;
    for (int i = 0; i < count; ++i) {
        std::vector<std::string> fields = nextFields(reader, "юнит " + std::to_string(i + 1) +
                                                            " из " + std::to_string(count));
        if (fields.size() != 8) {
            throw DataError("у юнита должно быть 8 полей, найдено " +
                            std::to_string(fields.size()));
        }
        UnitRecord record;
        record.name = fields[1];
        if (record.name.empty()) {
            throw DataError("у юнита пустое имя");
        }
        record.spec = dataformat::parseSpec(dataformat::parseKind(fields[0]), fields, 2);
        records.push_back(record);
    }
    return records;
}

}  // namespace

std::vector<UnitRecord> SaveService::snapshot(Army& army) {
    std::vector<UnitRecord> records;
    for (std::size_t i = 0; i < army.size(); ++i) {
        const Unit* unit = army.at(i);
        UnitRecord record;
        record.name = unit->name();
        record.spec.kind = unit->kind();
        record.spec.maxHp = unit->maxHp();
        record.spec.melee = unit->meleeAttack();
        record.spec.defense = unit->defense();
        record.spec.cost = unit->cost();
        if (const Archer* archer = dynamic_cast<const Archer*>(unit)) {
            record.spec.ranged = archer->rangedAttack();
            record.spec.range = archer->range();
        }
        records.push_back(record);
    }
    return records;
}

void SaveService::write(const SaveData& data, std::ostream& out) {
    if (data.turn < 0) {
        throw DataError("номер хода не может быть отрицательным");
    }
    out << Header << ";" << SupportedVersion << "\n";
    out << "seed;" << data.seed << "\n";
    out << "turn;" << data.turn << "\n";
    writeRecords(out, 0, data.armyA);
    writeRecords(out, 1, data.armyB);
}

void SaveService::saveToFile(const SaveData& data, const std::string& path) {
    // Пишем сначала в строку: при битых данных не остаётся наполовину записанного файла
    std::ostringstream text;
    write(data, text);

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw DataError("не удалось открыть файл сохранения «" + path + "» для записи");
    }
    out << text.str();
    if (!out) {
        throw DataError("не удалось записать файл сохранения «" + path + "»");
    }
}

SaveData SaveService::read(std::istream& in, const std::string& source) {
    dataformat::LineReader reader(in);
    SaveData data;
    try {
        std::vector<std::string> head = nextFields(reader, std::string(Header) + ";N");
        if (head[0] != Header || head.size() != 2) {
            throw DataError(std::string("это не сохранение, первой должна идти строка «") +
                            Header + ";N»");
        }
        int version = dataformat::parseInt(head[1], "версия");
        if (version != SupportedVersion) {
            throw DataError("версия сохранения " + std::to_string(version) +
                            " не поддерживается, нужна " + std::to_string(SupportedVersion));
        }

        std::vector<std::string> seed = nextFields(reader, "seed;N");
        expectFields(seed, "seed", 2);
        data.seed = parseSeed(seed[1]);

        std::vector<std::string> turn = nextFields(reader, "turn;N");
        expectFields(turn, "turn", 2);
        data.turn = dataformat::parseInt(turn[1], "номер хода");
        if (data.turn < 0) {
            throw DataError("номер хода не может быть отрицательным");
        }

        data.armyA = readArmy(reader, 0);
        data.armyB = readArmy(reader, 1);
        std::string extra;
        if (reader.next(extra)) {
            throw DataError("после второй армии есть лишние строки");
        }
    } catch (const DataError& e) {
        throw DataError(source + ", строка " + std::to_string(reader.lineNo()) + ": " + e.what());
    }
    return data;
}

SaveData SaveService::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw DataError("не удалось открыть файл сохранения «" + path + "»");
    }
    return read(in, path);
}

BattleSession SaveService::replay(const SaveData& data) {
    auto build = [](const std::vector<UnitRecord>& records) {
        auto army = std::make_unique<Army>();
        for (const UnitRecord& r : records) {
            army->add(UnitFactory::create(r.spec, r.name));
        }
        return army;
    };

    BattleSession session;
    session.armyA = build(data.armyA);
    session.armyB = build(data.armyB);
    session.engine = std::make_unique<BattleEngine>(*session.armyA, *session.armyB, data.seed);

    for (int t = 0; t < data.turn; ++t) {
        if (session.engine->finished()) {
            throw DataError("сохранение повреждено: бой закончился на ходу " + std::to_string(t) +
                            ", а в файле записан ход " + std::to_string(data.turn));
        }
        session.engine->nextTurn();
    }
    session.turn = data.turn;
    return session;
}
