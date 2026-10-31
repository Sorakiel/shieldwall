#include "SfmlScene.h"
#include "EventFormatter.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
namespace fs = std::filesystem;
using namespace sfmlui;
int main(int argc, char** argv) {
    try {
        const fs::path assets = argc > 1 ? argv[1] : "assets", out = argc > 2 ? argv[2] : "build/previews";
        fs::create_directories(out);
        Scene scene(assets);
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        sf::RenderTexture target({1440, 900}, settings);
        int frames = 0;
        auto render = [&](const std::string& name, const State& state) {
            target.clear();
            scene.draw(target, state);
            target.display();
            for (const auto& bounds : scene.textBounds()) {
                if (bounds.position.x < -1 || bounds.position.y < -1 ||
                    bounds.position.x + bounds.size.x > Width + 1 ||
                    bounds.position.y + bounds.size.y > Height + 1)
                    throw std::runtime_error(name + ": text outside canvas");
            }
            const auto& buttons = scene.buttons();
            for (std::size_t i = 0; i < buttons.size(); ++i) {
                const auto& a = buttons[i].bounds;
                if (a.position.x < 0 || a.position.y < 0 || a.position.x + a.size.x > Width ||
                    a.position.y + a.size.y > Height)
                    throw std::runtime_error(name + ": button outside canvas");
                if (a.position.y >= 741 && a.size.y == FooterHeight && a.size.x != FooterButtonWidth)
                    throw std::runtime_error(name + ": inconsistent footer button width");
                for (std::size_t j = i + 1; j < buttons.size(); ++j)
                    if (a.findIntersection(buttons[j].bounds))
                        throw std::runtime_error(name + ": overlapping buttons");
            }
            if (!target.getTexture().copyToImage().saveToFile(out / (name + ".png")))
                throw std::runtime_error("PNG export failed");
            ++frames;
        };
        auto getButton = [&](int action) {
            const auto found = std::find_if(scene.buttons().begin(), scene.buttons().end(),
                                            [&](const Button& b) { return b.action == action; });
            if (found == scene.buttons().end())
                throw std::runtime_error("Missing button " + std::to_string(action));
            return *found;
        };
        auto navigation = [&] {
            const auto back = getButton(static_cast<int>(uiAction::Back));
            const auto menu = getButton(0);
            if (back.bounds.position.x + FooterButtonWidth + FooterGap != menu.bounds.position.x)
                throw std::runtime_error("Back must be immediately left of Main Menu");
            if (back.bounds.size.x != FooterButtonWidth || menu.bounds.size.x != FooterButtonWidth)
                throw std::runtime_error("Navigation button widths differ");
        };
        State s;
        s.choices = {{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}};
        render("01-main-menu", s);
        s.notices = {"Сохранений нет."};
        render("02-empty-saves-notice", s);
        s = {};
        s.stage = ViewStage::Setup;
        s.choices = {{1, "Автобой"}, {2, "По одному ходу"}, {uiAction::Back, "Назад"}, {0, "В главное меню"}};
        render("03-settings", s);
        navigation();
        State shared = s;
        shared.choices[0].label = "Общий экран";
        shared.choices[1].label = "Раздельный ввод";
        render("23-shared-screen", shared);
        navigation();
        s.enteringNumber = true;
        s.prompt = "Seed (например, 42): ";
        s.numberCancel = NumberCancel::Previous;
        s.input = "4294967295";
        s.selectAll = true;
        render("04-seed-page", s);
        navigation();
        if (getButton(Submit).bounds.position.x >= Width / 2 ||
            getButton(static_cast<int>(uiAction::Back)).bounds.position.y <= 678)
            throw std::runtime_error("Seed field/footer layout is wrong");
        s = {};
        s.stage = ViewStage::Recruitment;
        s.recruiting = true;
        s.recruitment.activeTeam = 0;
        s.recruitment.limit = 120;
        s.recruitment.spent = 105;
        s.recruitment.remaining = 15;
        const UnitKind kinds[] = {UnitKind::Heavy, UnitKind::Light, UnitKind::Archer, UnitKind::Mage,
                                  UnitKind::Healer};
        const char* names[] = {"Брон [конь] [щит] [шлем] [копьё]", "Мирн", "Дарр", "Зэн", "Тиль"};
        const int health[] = {45, 20, 16, 14, 18}, costs[] = {30, 10, 18, 25, 22};
        const int attack[] = {15, 7, 4, 2, 2}, defense[] = {11, 1, 0, 0, 0};
        for (int team = 0; team < 2; ++team) {
            s.battle.armies[team].name = team == 0 ? "Синие" : "Красные";
            s.battle.armies[team].totalCost = 105;
            for (int i = 0; i < 5; ++i)
                s.battle.armies[team].units.push_back({kinds[i], names[i], i == 0 ? 31 : health[i], health[i],
                                                       costs[i], attack[i], defense[i], i == 2 ? 11 : 0});
        }
        s.recruitment.armies = s.battle.armies;
        s.recruitment.offers = {{UnitKind::Light, 10, 20, 7, 0, 0, 1, true},
                                {UnitKind::Heavy, 30, 45, 9, 0, 0, 5, false},
                                {UnitKind::Archer, 18, 16, 4, 11, 3, 0, false},
                                {UnitKind::Mage, 25, 14, 2, 60, 3, 0, false},
                                {UnitKind::Healer, 22, 18, 2, 8, 3, 0, false}};
        s.choices = {
            {1, "Случайный набор"}, {2, "Ручная закупка"},     {3, "Сжатый набор"},
            {20, "Убрать бойца"},   {21, "Переставить"},       {uiAction::ClearArmy, "Очистить армию"},
            {30, "Армия готова"},   {uiAction::Back, "Назад"}, {0, "В главное меню"}};
        render("05-army-methods", s);
        navigation();
        if (scene.rosterCards().size() != 5 || !getButton(RemoveBase).enabled ||
            !getButton(static_cast<int>(uiAction::ClearArmy)).enabled ||
            getButton(30).bounds.position.x + FooterButtonWidth + FooterGap !=
                getButton(static_cast<int>(uiAction::Back)).bounds.position.x)
            throw std::runtime_error("Main recruitment screen lacks direct army controls");
        const State methods = s;
        for (const auto grab : {sf::Vector2f(5, 12), sf::Vector2f(130, 125)}) {
            DragState drag{2, {352 + grab.x, 584 + grab.y}, {352 + grab.x, 584 + grab.y}, true, grab};
            if (dragPosition(drag) != sf::Vector2f(352, 584) || scene.dropTarget(drag) != 2)
                throw std::runtime_error("Card jumps when grabbed near its edge");
            drag.pointer.x = 500 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 2)
                throw std::runtime_error("Right drag advances before crossing the next center");
            drag.pointer.x = 573 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 3)
                throw std::runtime_error("Right drag does not insert after the crossed card");
            drag.from = 3;
            drag.pointer.x = 500 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 3)
                throw std::runtime_error("Left drag advances before crossing the previous center");
            drag.pointer.x = 420 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 2)
                throw std::runtime_error("Left drag does not insert before the crossed card");
            drag.pointer.y = 150;
            if (scene.dropTarget(drag))
                throw std::runtime_error("Drop outside roster should cancel");
        }
        State empty = methods;
        empty.battle.armies[0].units.clear();
        empty.recruitment.spent = 0;
        empty.recruitment.remaining = 120;
        for (auto& choice : empty.choices)
            if (choice.value == 30 || choice.value == uiAction::ClearArmy || choice.value == 20 ||
                choice.value == 21)
                choice.enabled = false;
        render("24-empty-recruitment", empty);
        if (getButton(30).enabled || getButton(static_cast<int>(uiAction::ClearArmy)).enabled)
            throw std::runtime_error("Empty army can be confirmed or cleared");
        s.recruitment.screen = RecruitmentScreen::Academy;
        s.choices = {{1, "Купить: Лёгкий"},
                     {2, "Купить: Тяжёлый", false},
                     {3, "Купить: Лучник", false},
                     {4, "Купить: Маг", false},
                     {5, "Купить: Целитель", false},
                     {20, "Убрать бойца"},
                     {21, "Переставить"},
                     {uiAction::ClearArmy, "Очистить армию"},
                     {30, "Армия готова"},
                     {uiAction::Back, "Назад"},
                     {0, "В главное меню"}};
        render("06-academy", s);
        if (std::none_of(scene.buttons().begin(), scene.buttons().end(),
                         [](const Button& b) { return b.action == static_cast<int>(uiAction::Back); }))
            throw std::runtime_error("Academy back button missing");
        if (scene.rosterCards().size() != 5)
            throw std::runtime_error("Academy card targets missing");
        for (const auto action : {20, 21})
            for (const auto& button : scene.buttons())
                if (button.action == action)
                    throw std::runtime_error("Legacy army dialog button is visible");
        State drag = s;
        drag.drag = DragState{0, {119.5f, 654}, {440, 654}, true};
        render("19-dragging", drag);
        State moved = s;
        auto unit = moved.battle.armies[0].units[0];
        moved.battle.armies[0].units.erase(moved.battle.armies[0].units.begin());
        moved.battle.armies[0].units.insert(moved.battle.armies[0].units.begin() + 2, unit);
        moved.rosterTransition =
            RosterTransition{ArmyEdit{ArmyEdit::Kind::Move, 0, 2}, unit, .5f, sf::Vector2f(352, 584)};
        render("20-reordering", moved);
        State generated = methods;
        render("21-generated-on-methods", generated);
        State methodDrag = methods;
        methodDrag.drag = drag.drag;
        render("25-drag-on-methods", methodDrag);
        State methodTransition = moved;
        methodTransition.recruitment.screen = RecruitmentScreen::Methods;
        methodTransition.choices = methods.choices;
        render("26-reorder-on-methods", methodTransition);
        State budget = s;
        budget.stage = ViewStage::Setup;
        budget.enteringNumber = true;
        budget.numberCancel = NumberCancel::Previous;
        budget.prompt = "Лимит цены каждой армии: ";
        budget.input = "120";
        budget.choices.clear();
        render("22-budget-page", budget);
        navigation();
        if (getButton(Submit).bounds.position.x >= Width / 2)
            throw std::runtime_error("Budget field should be on the left");
        budget.error = "Введите число от 10 до 2147483647.";
        render("27-budget-error", budget);

        s.recruiting = false;
        s.choices = {{1, "К бою"}, {uiAction::Back, "Назад"}, {0, "В главное меню"}};
        render("07-ready", s);
        s.stage = ViewStage::Battle;
        s.turn = 17;
        s.waiting = true;
        s.choices.clear();
        auto add = [&](const std::string& str, int team) {
            const auto icon = battletext::icons(str);
            const auto t = sf::String::fromUtf8(icon.begin(), icon.end());
            s.log.append(
                t.toUtf32(), team,
                [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); }, LogWidth);
        };
        s.log.startTurn(
            17, [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); }, LogWidth);
        add("«Брон [конь] [щит] [шлем] [копьё]» наносит удар по «Мирн» — 7 урона, осталось 13 hp.", 0);
        add("«Тиль» восстанавливает здоровье «Брон» — восстановлено 8 hp.", 1);
        render("08-battle", s);
        const auto battleState = s;
        presentation::Playback playback;
        const EventType types[] = {EventType::Melee, EventType::Shot, EventType::Heal,
                                   EventType::Clone, EventType::Buff, EventType::Death};
        for (int i = 0; i < 6; ++i) {
            State effect = s;
            BattleEvent e{types[i], 17, 0,    names[i == 1 ? 2 : i == 2 ? 4 : i == 3 ? 3 : 0], names[1], 7,
                          13,       8,  "щит"};
            effect.effect = playback.begin(effect.battle, e);
            effect.effect->elapsed = presentation::duration(e.type) * .57f;
            effect.playing = true;
            effect.waiting = false;
            render("09-effect-" + std::to_string(i), effect);
            for (const auto action : {Auto, Save, 0}) {
                const auto button = std::find_if(scene.buttons().begin(), scene.buttons().end(),
                                                 [&](const Button& b) { return b.action == action; });
                if (button == scene.buttons().end() || !button->enabled)
                    throw std::runtime_error("Playback controls are disabled");
            }
        }
        State equipped = battleState;
        equipped.battle.armies[0].units[0].name = "Брон";
        equipped.battle.armies[0].units[0].attack = 9;
        equipped.battle.armies[0].units[0].defense = 5;
        presentation::Playback equipment;
        for (const auto kind : {BuffKind::Helmet, BuffKind::Shield, BuffKind::Spear, BuffKind::Horse}) {
            BattleEvent event{EventType::Buff, 17, 0, "Мирн", equipped.battle.armies[0].units[0].name};
            event.buff = buffName(kind);
            equipped.effect = equipment.begin(equipped.battle, event);
            equipped.playing = true;
            equipped.effect->advance(presentation::duration(event.type) * .49f);
            render("28-before-" + event.buff, equipped);
            if (equipped.effect->advance(presentation::duration(event.type) * .02f))
                equipment.commit(equipped.battle, *equipped.effect);
            render("29-after-" + event.buff, equipped);
        }
        s.stage = ViewStage::Result;
        s.waiting = false;
        s.winner = 0;
        s.choices = {{1, "Новая партия"}, {2, "Загрузить"}, {0, "В главное меню"}};
        render("10-result", s);
        s.enteringText = true;
        s.prompt = "Имя сохранения [Партия 1]: ";
        s.input = "Последняя битва";
        render("11-save-dialog", s);
        s = {};
        s.choices.clear();
        for (int i = 0; i < 6; ++i)
            s.choices.push_back({static_cast<std::uint32_t>(i + 1),
                                 "Поход " + std::to_string(i + 1) +
                                     "\nБой, ход 17 | лимит 120 | seed 42 | 31.10.2026 18:00"});
        s.choices.insert(
            s.choices.end(),
            {{10, "Назад по страницам", false}, {11, "Далее"}, {12, "Удалить сохранения"}, {0, "Отмена"}});
        render("12-archive", s);
        s.choices[0].label = std::string(64, 'W') + "\nПовреждён: " + std::string(1500, 'W');
        s.choices[0].enabled = false;
        render("15-archive-long-name", s);
        State paged = battleState;
        for (int i = 0; i < 60; ++i)
            paged.battle.armies[0].units.push_back(paged.battle.armies[0].units[0]);
        paged.offsets[0] = 60;
        render("16-battle-last-page", paged);
        paged.stage = ViewStage::Recruitment;
        paged.recruiting = true;
        paged.recruitment.screen = RecruitmentScreen::Academy;
        paged.recruitment.activeTeam = 0;
        paged.recruitment.limit = 2147483647;
        paged.recruitment.spent = 2147483637;
        paged.recruitment.remaining = 10;
        paged.recruitment.offers = {{UnitKind::Light, 10, 20, 7, 0, 0, 1, true}};
        paged.choices = {{1, "Купить: Лёгкий"}, {20, "Убрать бойца"},
                         {21, "Переставить"},   {uiAction::ClearArmy, "Очистить армию"},
                         {30, "Армия готова"},  {uiAction::Back, "Назад"},
                         {0, "В главное меню"}};
        render("17-large-budget", paged);
        paged.enteringNumber = true;
        paged.numberCancel = NumberCancel::Dialog;
        paged.prompt = "Кого убрать (номер): ";
        paged.input = "1";
        render("18-cancel-dialog", paged);

        s = {};
        s.handoff = 1;
        render("13-handoff", s);
        s = {};
        s.choices = {{1, "Заменить текущую партию"}, {0, "Отмена"}};
        render("14-confirm", s);

        if (argc > 3 && std::string(argv[3]) == "--animate") {
            fs::create_directories(out / "animation");
            State movie = battleState;
            movie.log.clear();
            movie.battle.armies[0].units[0].name = "Брон";
            movie.battle.armies[1].units[0].name = "Хорд";
            for (auto& army : movie.battle.armies) {
                army.units[0].attack = 9;
                army.units[0].defense = 5;
            }
            movie.playing = true;
            movie.waiting = false;
            presentation::Playback sequence;
            std::mt19937 phrases(42);
            int frameNumber = 0;
            std::vector<BattleEvent> events = {{EventType::Melee, 17, 0, "Брон", "Хорд", 6, 25},
                                               {EventType::Shot, 17, 1, "Дарр", "Мирн", 6, 14},
                                               {EventType::Heal, 17, 1, "Тиль", "Хорд", 0, 0, 8},
                                               {EventType::Clone, 17, 0, "Зэн", "Дарр"},
                                               {EventType::Buff, 17, 0, "Мирн", "Брон", 0, 0, 0, "щит"}};
            for (const auto& event : events) {
                movie.effect = sequence.begin(movie.battle, event);
                const auto message = battletext::icons(EventFormatter::format(event, phrases, false));
                while (!movie.effect->finished()) {
                    if (movie.effect->advance(1.f / 24.f)) {
                        sequence.commit(movie.battle, *movie.effect);
                        movie.log.startTurn(
                            17, [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); },
                            LogWidth);
                        movie.log.append(
                            sf::String::fromUtf8(message.begin(), message.end()).toUtf32(), event.team,
                            [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); },
                            LogWidth);
                    }
                    std::ostringstream name;
                    name << "animation/frame-" << std::setw(4) << std::setfill('0') << frameNumber++;
                    render(name.str(), movie);
                }
            }
        }

        std::cout << "Rendered and checked " << frames << " scenes without opening a window\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
