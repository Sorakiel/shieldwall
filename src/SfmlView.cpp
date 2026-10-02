#include "SfmlView.h"
#include "TextInput.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <deque>
#include <stdexcept>

namespace {

constexpr float canvasWidth = 1280.f;
constexpr float canvasHeight = 820.f;
constexpr unsigned logFontSize = 15;
constexpr int visibleLogLines = 23;
constexpr std::size_t visibleUnits = 5;
constexpr int nextAction = 100;
constexpr int autoAction = 101;
constexpr int submitAction = 102;
constexpr int saveAction = 103;
constexpr int cancelAction = 104;

const sf::Color background(14, 20, 30);
const sf::Color panel(23, 32, 45);
const sf::Color raised(33, 45, 62);
const sf::Color foreground(232, 239, 247);
const sf::Color muted(148, 164, 184);
const sf::Color blue(94, 171, 255);
const sf::Color red(251, 117, 127);
const sf::Color accent(112, 218, 173);

sf::String utf8(const std::string& text) {
    return sf::String::fromUtf8(text.begin(), text.end());
}

sf::Color kindColor(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light: return sf::Color(77, 159, 142);
    case UnitKind::Heavy: return sf::Color(157, 127, 192);
    case UnitKind::Archer: return sf::Color(207, 162, 91);
    }
    return muted;
}

struct Button {
    sf::FloatRect bounds;
    int action;
    bool enabled;
};

}  // namespace

struct SfmlView::Impl {
    sf::Font font;
    sf::RenderWindow window;
    sf::View camera{sf::FloatRect({0.f, 0.f}, {canvasWidth, canvasHeight})};
    BattleSnapshot snapshot;
    RecruitmentSnapshot recruitment;
    bool recruiting = false;
    int handoffTeam = -1;
    ViewStage stage = ViewStage::Setup;
    std::deque<sf::String> log;
    std::vector<Button> buttons;
    std::vector<MenuChoice> choices;
    std::array<std::size_t, 2> unitOffsets{};
    int logOffset = 0;
    int turn = 0;
    int winner = -1;
    bool automatic = false;
    bool enteringNumber = false;
    bool enteringText = false;
    TextInput textInput;
    bool selectAll = true;
    bool waitingForTurn = false;
    std::string prompt;
    std::string input;
    std::string error;
    std::string result;
    sf::Clock autoClock;

    explicit Impl(const std::filesystem::path& fontPath) {
        if (!font.openFromFile(fontPath)) {
            throw std::runtime_error("Не удалось открыть шрифт: " + fontPath.u8string());
        }
        window.create(sf::VideoMode({1280, 820}), "Shieldwall");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        updateViewport(window.getSize());
    }

    void updateViewport(sf::Vector2u size) {
        if (size.x == 0 || size.y == 0) return;
        const float scale = std::min(static_cast<float>(size.x) / canvasWidth,
                                     static_cast<float>(size.y) / canvasHeight);
        const float width = canvasWidth * scale / static_cast<float>(size.x);
        const float height = canvasHeight * scale / static_cast<float>(size.y);
        camera.setViewport(sf::FloatRect({(1.f - width) / 2.f, (1.f - height) / 2.f},
                                         {width, height}));
        window.setView(camera);
    }

    void rectangle(float x, float y, float width, float height, sf::Color color,
                   sf::Color outline = sf::Color::Transparent) {
        sf::RectangleShape shape({width, height});
        shape.setPosition({x, y});
        shape.setFillColor(color);
        if (outline.a) {
            shape.setOutlineColor(outline);
            shape.setOutlineThickness(-1.f);
        }
        window.draw(shape);
    }

    void text(const sf::String& value, float x, float y, unsigned size = 16,
              sf::Color color = foreground) {
        sf::Text label(font, value, size);
        label.setPosition({x, y});
        label.setFillColor(color);
        window.draw(label);
    }

    float textWidth(const sf::String& value, unsigned size) const {
        return sf::Text(font, value, size).getLocalBounds().size.x;
    }

    sf::String elide(sf::String value, float width, unsigned size) const {
        if (textWidth(value, size) <= width) return value;
        while (!value.isEmpty() && textWidth(value + utf8("…"), size) > width) {
            value.erase(value.getSize() - 1);
        }
        return value + utf8("…");
    }

    void button(const std::string& label, float x, float y, float width, int action,
                bool enabled = true, bool primary = false, float height = 48.f) {
        const sf::FloatRect bounds({x, y}, {width, height});
        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), camera);
        const bool hovered = enabled && bounds.contains(mouse);
        const sf::Color fill = !enabled ? panel : primary ? accent : hovered ? raised : panel;
        rectangle(x, y, width, height, fill, hovered ? accent : sf::Color(53, 70, 91));
        const auto value = elide(utf8(label), width - 18.f, 16);
        text(value, x + (width - textWidth(value, 16)) / 2.f, y + (height - 24.f) / 2.f,
             16, !enabled ? sf::Color(81, 97, 116) : primary ? background : foreground);
        buttons.push_back({bounds, action, enabled});
    }

    bool hasChoice(std::uint32_t value) const {
        return std::any_of(choices.begin(), choices.end(),
            [value](const MenuChoice& choice) { return choice.enabled && choice.value == value; });
    }

    void appendLog(const std::string& value) {
        const sf::String full = utf8(value);
        sf::String line;
        auto flush = [&] {
            log.push_back(line);
            if (logOffset > 0) ++logOffset;
            if (log.size() > 5000) log.pop_front();
            line.clear();
        };
        for (const auto character : full) {
            if (character == U'\r') continue;
            if (character == U'\n') { flush(); continue; }
            line += character;
            if (textWidth(line, logFontSize) > 324.f) {
                std::size_t split = line.getSize() - 1;
                while (split > 0 && line[split] != U' ') --split;
                if (split == 0) split = line.getSize() - 1;
                const bool space = line[split] == U' ';
                const sf::String rest = line.substring(split + (space ? 1 : 0));
                line = line.substring(0, split);
                flush();
                line = rest;
            }
        }
        if (!line.isEmpty()) flush();
        clampLogOffset();
    }

    void clampLogOffset() {
        logOffset = std::clamp(logOffset, 0,
            std::max(0, static_cast<int>(log.size()) - visibleLogLines));
    }

    void shiftUnits(int team, int direction) {
        const std::size_t size = snapshot.armies[team].units.size();
        const std::size_t last = size == 0 ? 0 : ((size - 1) / visibleUnits) * visibleUnits;
        auto& offset = unitOffsets[team];
        if (direction < 0) offset = offset >= visibleUnits ? offset - visibleUnits : 0;
        else offset = std::min(last, offset + visibleUnits);
    }

    void drawRecruitment() {
        rectangle(24.f, 116.f, 832.f, 552.f, panel);
        text(utf8("Закупка: " + recruitment.armies[recruitment.activeTeam].name),
             48.f, 140.f, 23, accent);
        text(utf8("Бюджет " + std::to_string(recruitment.limit) +
                  "   Потрачено " + std::to_string(recruitment.spent) +
                  "   Осталось " + std::to_string(recruitment.remaining)),
             48.f, 178.f, 16, muted);
        drawArmy(recruitment.activeTeam, 222.f);
        float y = 462.f;
        for (const auto& offer : recruitment.offers) {
            rectangle(48.f, y + 8.f, 10.f, 10.f, kindColor(offer.kind));
            text(utf8(std::string(unitKindName(offer.kind)) + "  |  цена " +
                      std::to_string(offer.cost) + "  |  HP " + std::to_string(offer.hp) +
                      "  |  атака " + std::to_string(offer.melee) +
                      "  |  выстрел " + std::to_string(offer.ranged) +
                      "  |  дальн. " + std::to_string(offer.range) +
                      "  |  защита " + std::to_string(offer.defense)),
                 70.f, y, 14, offer.enabled ? foreground : muted);
            y += 43.f;
        }
        text(utf8("Номера в строю нужны для удаления и перестановки."), 48.f, 628.f, 14, muted);
    }

    void drawArmy(int team, float y) {
        const auto& army = snapshot.armies[team];
        const auto color = team == 0 ? blue : red;
        const std::string teamName = army.name.empty() ? (team == 0 ? "Синие" : "Красные") : army.name;
        rectangle(44.f, y + 5.f, 4.f, 24.f, color);
        text(utf8(teamName), 60.f, y, 23, color);
        text(utf8(std::to_string(army.units.size()) + " бойцов  /  цена " +
                  std::to_string(army.totalCost)), 214.f, y + 6.f, 15, muted);

        const std::size_t last = army.units.empty() ? 0 :
            ((army.units.size() - 1) / visibleUnits) * visibleUnits;
        unitOffsets[team] = std::min(unitOffsets[team], last);
        const auto offset = unitOffsets[team];
        button("<", 720.f, y, 42.f, 200 + team * 2, offset > 0, false, 34.f);
        button(">", 770.f, y, 42.f, 201 + team * 2, offset + visibleUnits < army.units.size(),
               false, 34.f);

        if (army.units.empty()) {
            text(utf8(stage == ViewStage::Setup ? "Армия появится после настройки" : "Строй пуст"),
                 60.f, y + 99.f, 18, muted);
            return;
        }
        for (std::size_t index = offset; index < std::min(army.units.size(), offset + visibleUnits); ++index) {
            const auto& unit = army.units[index];
            const float x = 52.f + static_cast<float>(index - offset) * 154.f;
            const float top = y + 60.f;
            text(utf8(index == 0 ? "1 · передний" : std::to_string(index + 1) + " · в строю"),
                 x, top - 23.f, 12, index == 0 ? color : muted);
            rectangle(x, top, 140.f, 120.f, raised, color);
            rectangle(x + 1.f, top + 1.f, 138.f, 27.f, kindColor(unit.kind));
            text(utf8(unitKindName(unit.kind)), x + 10.f, top + 3.f, 14, background);
            text(elide(utf8(unit.name), 118.f, 16), x + 10.f, top + 35.f, 16);
            text(utf8(std::to_string(unit.hp) + " / " + std::to_string(unit.maxHp) + " HP"),
                 x + 10.f, top + 65.f, 14, muted);
            const float health = unit.maxHp > 0 ?
                std::clamp(static_cast<float>(unit.hp) / static_cast<float>(unit.maxHp), 0.f, 1.f) : 0.f;
            rectangle(x + 10.f, top + 98.f, 120.f, 8.f, background);
            rectangle(x + 10.f, top + 98.f, 120.f * health, 8.f, health > .3f ? accent : red);
        }
        text(utf8("Позиции " + std::to_string(offset + 1) + "–" +
                  std::to_string(std::min(army.units.size(), offset + visibleUnits)) +
                  " из " + std::to_string(army.units.size())), 54.f, y + 191.f, 13, muted);
    }

    void drawLog() {
        rectangle(880.f, 116.f, 376.f, 674.f, panel);
        text(utf8("Журнал боя"), 904.f, 138.f, 22);
        text(utf8("События и результаты ходов"), 904.f, 173.f, 13, muted);
        rectangle(904.f, 204.f, 328.f, 1.f, raised);
        clampLogOffset();
        const int end = static_cast<int>(log.size()) - logOffset;
        const int first = std::max(0, end - visibleLogLines);
        float y = 220.f;
        if (log.empty()) text(utf8("Здесь появятся события боя"), 904.f, y, 14, muted);
        for (int index = first; index < end; ++index, y += 22.f) {
            const auto& line = log[static_cast<std::size_t>(index)];
            sf::Color color = foreground;
            if (line.find(utf8("[Синие]")) != sf::String::InvalidPos) color = blue;
            else if (line.find(utf8("[Красные]")) != sf::String::InvalidPos) color = red;
            text(line, 904.f, y, logFontSize, color);
        }
        const int count = static_cast<int>(log.size());
        if (count > visibleLogLines) {
            rectangle(1242.f, 220.f, 3.f, 506.f, raised);
            const float thumb = std::max(24.f, 506.f * visibleLogLines / static_cast<float>(count));
            const float position = (506.f - thumb) * static_cast<float>(first) /
                static_cast<float>(count - visibleLogLines);
            rectangle(1242.f, 220.f + position, 3.f, thumb, muted);
        }
        text(utf8(logOffset == 0 ? "Последние события · прокрутка колесом" :
                 "Просмотр истории · прокрутка колесом"), 904.f, 753.f, 12, muted);
    }

    bool saveList() const {
        return std::any_of(choices.begin(), choices.end(), [](const MenuChoice& choice) {
            return choice.label.find('\n') != std::string::npos;
        });
    }

    void drawSaveList() {
        buttons.clear();
        rectangle(24.f, 116.f, 832.f, 584.f, panel);
        text(utf8("Сохранения"), 48.f, 134.f, 26, accent);
        std::size_t row = 0, navigation = 0;
        for (const auto& choice : choices) {
            const auto split = choice.label.find('\n');
            if (split == std::string::npos) {
                button(choice.label, 24.f + static_cast<float>(navigation++) * 278.f,
                       708.f, 266.f, static_cast<int>(choice.value), choice.enabled);
                continue;
            }
            const float y = 182.f + static_cast<float>(row++) * 76.f;
            const sf::FloatRect bounds({44.f, y}, {790.f, 66.f});
            const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), camera);
            const bool hovered = choice.enabled && bounds.contains(mouse);
            rectangle(44.f, y, 790.f, 66.f, raised, hovered ? accent : sf::Color(53, 70, 91));
            text(elide(utf8(choice.label.substr(0, split)), 754.f, 19),
                 60.f, y + 6.f, 19, choice.enabled ? foreground : muted);
            text(elide(utf8(choice.label.substr(split + 1)), 754.f, 14),
                 60.f, y + 37.f, 14, choice.enabled ? muted : red);
            buttons.push_back({bounds, static_cast<int>(choice.value), choice.enabled});
        }
        text(utf8("Esc — отмена. Причины повреждения также показаны в журнале."),
             26.f, 770.f, 13, muted);
    }

    void draw() {
        if (!window.isOpen()) throw ViewClosed{};
        window.clear(background);
        window.setView(camera);
        buttons.clear();
        if (handoffTeam >= 0) {
            rectangle(250.f, 220.f, 780.f, 360.f, panel);
            text(utf8("Передайте управление игроку " + std::to_string(handoffTeam + 1)),
                 292.f, 270.f, 27, accent);
            text(utf8("Предыдущий игрок должен отвернуться от экрана."),
                 292.f, 330.f, 19, muted);
            text(utf8("Нажмите Enter, когда будете готовы."), 292.f, 374.f, 19);
            button("Продолжить", 292.f, 458.f, 240.f, submitAction, true, true);
            button("Выход", 552.f, 458.f, 180.f, 0);
            window.display();
            return;
        }
        text("SHIELDWALL", 28.f, 22.f, 32);
        text(utf8("Две армии. Один строй. Каждый ход на виду."), 30.f, 70.f, 15, muted);
        const std::string phase = stage == ViewStage::Setup ? "Настройка" :
            stage == ViewStage::Recruitment ? (recruiting ? "Закупка" : "Армии готовы") :
            stage == ViewStage::Battle ? "Ход " + std::to_string(turn) : "Бой завершён";
        rectangle(1066.f, 31.f, 190.f, 42.f, raised);
        text(utf8(phase), 1082.f, 41.f, 16, accent);

        if (recruiting) drawRecruitment();
        else {
            rectangle(24.f, 116.f, 832.f, 552.f, panel);
            drawArmy(0, 140.f);
            rectangle(48.f, 368.f, 780.f, 1.f, raised);
            drawArmy(1, 390.f);
            const UnitKind kinds[] = {UnitKind::Light, UnitKind::Heavy, UnitKind::Archer};
            for (int i = 0; i < 3; ++i) {
                const float x = 48.f + static_cast<float>(i) * 170.f;
                rectangle(x, 628.f, 12.f, 12.f, kindColor(kinds[i]));
                text(utf8(unitKindName(kinds[i])), x + 20.f, 622.f, 14, muted);
            }
            text(utf8("Рамка — команда"), 627.f, 622.f, 14, muted);
        }
        drawLog();

        if (saveList() && !enteringText && !enteringNumber) {
            drawSaveList();
        } else if (waitingForTurn && !enteringNumber && !enteringText) {
            button("Следующий ход", 24.f, 708.f, 196.f, nextAction, true, true);
            button(automatic ? "Автобой: вкл." : "Автобой: выкл.",
                   232.f, 708.f, 184.f, autoAction);
            button("Сохранить", 428.f, 708.f, 172.f, saveAction);
            button("В главное меню", 612.f, 708.f, 244.f, 0);
            text(utf8("Enter / пробел — ход · A — режим · S — сохранить"), 26.f, 770.f, 13, muted);
        } else {
            for (std::size_t index = 0; index < choices.size(); ++index) {
                const auto& choice = choices[index];
                button(choice.label, 24.f + static_cast<float>(index % 4) * 208.f,
                       706.f + static_cast<float>(index / 4) * 44.f, 196.f,
                       static_cast<int>(choice.value), choice.enabled, index == 0, 38.f);
            }
        }
        if (!saveList() && !result.empty()) {
            text(elide(utf8(result), 798.f, 19), 28.f, 674.f, 19,
                 winner == 0 ? blue : winner == 1 ? red : foreground);
        } else if (!saveList()) {
            text(utf8("Полоска показывает текущее HP / максимальное HP."), 28.f, 676.f, 13, muted);
        }

        if (enteringNumber || enteringText) {
            buttons.clear();
            rectangle(24.f, 116.f, 832.f, 680.f, sf::Color(14, 20, 30, 220));
            rectangle(116.f, 232.f, 648.f, 312.f, raised);
            text(elide(utf8(prompt), 590.f, 23), 144.f, 258.f, 23);
            rectangle(144.f, 317.f, 590.f, 62.f, background, accent);
            const unsigned size = enteringText ? 22u : 28u;
            sf::String value = utf8(enteringText ? textInput.value() : input);
            while (!value.isEmpty() && textWidth(value, size) > 545.f) value.erase(0);
            if (selectAll) rectangle(158.f, 329.f, std::max(15.f, textWidth(value, size) + 8.f),
                                     38.f, sf::Color(53, 80, 108));
            text(value + (selectAll ? sf::String{} : sf::String("|")), 162.f, 328.f, size);
            const auto hint = enteringText ? "Enter — сохранить · Esc — отмена · максимум 64 байта UTF-8" :
                                            "Введите число и нажмите Enter";
            text(utf8(error.empty() ? hint : error), 144.f, 397.f, 14,
                 error.empty() ? muted : red);
            button(enteringText ? "Сохранить" : "Продолжить",
                   144.f, 455.f, 210.f, submitAction, true, true);
            button(enteringText ? "Отмена" : "Выход",
                   374.f, 455.f, 150.f, enteringText ? cancelAction : 0);
        }
        window.display();
    }

    std::optional<int> pollAction() {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                throw ViewClosed{};
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                updateViewport(resized->size);
            }
            if (const auto* entered = event->getIf<sf::Event::TextEntered>()) {
                if (enteringText && entered->unicode >= 0x20) {
                    if (selectAll) textInput.clear();
                    selectAll = false;
                    if (!textInput.append(entered->unicode)) error = "Не удалось добавить символ: проверьте длину имени.";
                    else error.clear();
                }
                if (enteringNumber && entered->unicode >= U'0' && entered->unicode <= U'9') {
                    if (selectAll) input.clear();
                    selectAll = false;
                    if (input.size() < 20) input += static_cast<char>(entered->unicode);
                    error.clear();
                }
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    if (enteringText) return cancelAction;
                    if (!enteringNumber && std::any_of(choices.begin(), choices.end(),
                        [](const MenuChoice& choice) {
                            return choice.value == 0 && choice.enabled && choice.escapeCancel;
                        })) return 0;
                    window.close();
                    throw ViewClosed{};
                }
                if (handoffTeam >= 0 && key->code == sf::Keyboard::Key::Enter) {
                    return submitAction;
                }
                if (enteringNumber || enteringText) {
                    if (key->control && key->code == sf::Keyboard::Key::A) selectAll = true;
                    if (key->code == sf::Keyboard::Key::Backspace) {
                        if (enteringText) {
                            if (selectAll) textInput.clear();
                            else textInput.backspace();
                        } else {
                            if (selectAll) input.clear();
                            else if (!input.empty()) input.pop_back();
                        }
                        error.clear();
                        selectAll = false;
                    }
                    if (key->code == sf::Keyboard::Key::Enter) return submitAction;
                } else {
                    if (waitingForTurn && (key->code == sf::Keyboard::Key::Space ||
                                           key->code == sf::Keyboard::Key::Enter)) return nextAction;
                    if (hasChoice(1) && key->code == sf::Keyboard::Key::Enter) return 1;
                    if (waitingForTurn &&
                        key->code == sf::Keyboard::Key::A) return autoAction;
                }
            }
            // KeyReleased не оставляет TextEntered('s') в очереди нового поля.
            if (const auto* key = event->getIf<sf::Event::KeyReleased>()) {
                if (waitingForTurn && !enteringNumber && !enteringText &&
                    key->code == sf::Keyboard::Key::S) return saveAction;
            }
            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (enteringNumber || enteringText || handoffTeam >= 0) continue;
                const auto position = window.mapPixelToCoords(wheel->position, camera);
                if (position.x >= 880.f) {
                    logOffset += wheel->delta > 0 ? 3 : -3;
                    clampLogOffset();
                } else if (!saveList() && position.y >= 116.f && position.y < 668.f) {
                    shiftUnits(position.y < 368.f ? 0 : 1, wheel->delta > 0 ? -1 : 1);
                }
            }
            if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button != sf::Mouse::Button::Left) continue;
                const auto position = window.mapPixelToCoords(mouse->position, camera);
                for (const auto& item : buttons) {
                    if (item.enabled && item.bounds.contains(position)) return item.action;
                }
            }
        }
        return std::nullopt;
    }

    bool handleNavigation(int action) {
        if (action >= 200 && action <= 203) {
            shiftUnits((action - 200) / 2, action % 2 == 0 ? -1 : 1);
            return true;
        }
        return false;
    }

    TurnAction waitForTurn(BattleMode mode) {
        waitingForTurn = true;
        automatic = mode == BattleMode::Automatic;
        autoClock.restart();
        TurnAction outcome = TurnAction::Next;
        while (true) {
            draw();
            if (const auto action = pollAction()) {
                if (*action == nextAction) break;
                if (*action == 0) { outcome = TurnAction::Exit; break; }
                if (*action == saveAction) { outcome = TurnAction::Save; break; }
                if (*action == autoAction) {
                    outcome = automatic ? TurnAction::Manual : TurnAction::Automatic;
                    break;
                }
                handleNavigation(*action);
            }
            if (automatic && autoClock.getElapsedTime().asMilliseconds() >= 550) break;
        }
        waitingForTurn = false;
        return outcome;
    }
};

SfmlView::SfmlView(const std::filesystem::path& fontPath) : impl_(std::make_unique<Impl>(fontPath)) {}
SfmlView::~SfmlView() = default;

void SfmlView::setStage(ViewStage stage) {
    impl_->stage = stage;
    impl_->choices.clear();
    impl_->recruiting = false;
    if (stage == ViewStage::Setup) {
        impl_->snapshot = {};
        impl_->logOffset = 0;
        impl_->automatic = false;
        impl_->result.clear();
        impl_->turn = 0;
    }
    if (stage == ViewStage::Recruitment) {
        impl_->turn = 0;
        impl_->result.clear();
        impl_->unitOffsets = {};
    }
}

std::optional<std::uint32_t> SfmlView::readNumber(const std::string& prompt,
                                                std::uint32_t min, std::uint32_t max) {
    impl_->enteringNumber = true;
    impl_->prompt = prompt;
    impl_->input = std::to_string(std::clamp(min == 0 ? 42u : 120u, min, max));
    impl_->selectAll = true;
    impl_->error.clear();
    while (true) {
        impl_->draw();
        const auto action = impl_->pollAction();
        if (!action) continue;
        if (*action == 0) throw ViewClosed{};
        if (*action == submitAction) {
            if (const auto value = parseNumber(impl_->input, min, max)) {
                impl_->enteringNumber = false;
                return value;
            }
            impl_->error = "Введите целое число от " + std::to_string(min) + " до " +
                           std::to_string(max) + ".";
        }
    }
}

std::optional<std::string> SfmlView::readText(const std::string& prompt, std::size_t maxBytes) {
    impl_->enteringText = true;
    impl_->prompt = prompt;
    impl_->textInput.reset(maxBytes);
    impl_->selectAll = false;
    impl_->error.clear();
    while (true) {
        impl_->draw();
        const auto action = impl_->pollAction();
        if (!action) continue;
        if (*action == cancelAction || *action == submitAction) {
            impl_->enteringText = false;
            if (*action == cancelAction) return std::nullopt;
            return impl_->textInput.value();
        }
    }
}

std::optional<std::uint32_t> SfmlView::choose(const std::vector<MenuChoice>& choices) {
    impl_->choices = choices;
    if (choices.empty()) return std::nullopt;
    while (true) {
        impl_->draw();
        if (const auto action = impl_->pollAction()) {
            if (*action >= 0 && impl_->hasChoice(static_cast<std::uint32_t>(*action))) {
                impl_->choices.clear();
                return static_cast<std::uint32_t>(*action);
            }
            impl_->handleNavigation(*action);
        }
    }
}

void SfmlView::message(const std::string& text) { impl_->appendLog(text); }

void SfmlView::showArmies(const BattleSnapshot& snapshot) {
    impl_->snapshot = snapshot;
    impl_->recruiting = false;
    impl_->draw();
}

void SfmlView::showRecruitment(const RecruitmentSnapshot& snapshot) {
    impl_->recruitment = snapshot;
    impl_->snapshot.armies = snapshot.armies;
    impl_->recruiting = true;
}

void SfmlView::setBattleProgress(int turn, BattleMode mode) {
    impl_->turn = turn;
    impl_->automatic = mode == BattleMode::Automatic;
}

TurnAction SfmlView::waitForTurn(BattleMode mode) { return impl_->waitForTurn(mode); }

bool SfmlView::handoff(int team) {
    impl_->snapshot = {};
    impl_->recruitment = {};
    impl_->log.clear();
    impl_->logOffset = 0;
    impl_->choices.clear();
    impl_->handoffTeam = team;
    while (true) {
        impl_->draw();
        if (const auto action = impl_->pollAction()) {
            if (*action == 0) throw ViewClosed{};
            if (*action == submitAction) break;
        }
    }
    impl_->handoffTeam = -1;
    return true;
}

void SfmlView::showResult(int winner, const std::string& text) {
    impl_->winner = winner;
    impl_->result = text;
    impl_->appendLog(text);
}
