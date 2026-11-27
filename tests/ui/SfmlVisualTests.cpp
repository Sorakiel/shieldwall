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
        auto render = [&](const std::string& name, const State& state, sf::Vector2f mouse = {-1.f, -1.f}) {
            target.clear();
            scene.draw(target, state, mouse);
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
        shared.choices[0].label = "Открытый набор";
        shared.choices[1].label = "Скрытый набор";
        render("23-shared-screen", shared);
        navigation();
        s.enteringNumber = true;
        s.prompt = "Seed для случайной генерации (например, 42): ";
        s.numberCancel = NumberCancel::Previous;
        s.number.reset("4294967295");
        s.number.selectAll();
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
                                {UnitKind::Mage, 25, 14, 2, 10, 3, 0, false},
                                {UnitKind::Healer, 22, 18, 2, 8, 3, 0, false},
                                {UnitKind::GulyayGorod, 42, 1000, 0, 0, 0, 0, false}};
        s.choices = {
            {1, "Случайный набор"}, {2, "Ручной набор"},     {3, "Набор по цене"},
            {20, "Удалить бойца"},   {21, "Переместить бойца"},       {uiAction::ClearArmy, "Очистить армию"},
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
            DragState drag{2, {348 + grab.x, 584 + grab.y}, {348 + grab.x, 584 + grab.y}, true, grab};
            if (dragPosition(drag) != sf::Vector2f(348, 584) || scene.dropTarget(drag) != 2)
                throw std::runtime_error("Card jumps when grabbed near its edge");
            drag.pointer.x = 499 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 2)
                throw std::runtime_error("Right drag advances before crossing the next center");
            drag.pointer.x = 573 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 3)
                throw std::runtime_error("Right drag does not insert after the crossed card");
            drag.from = 3;
            drag.pointer.x = 499 - RosterCardSize.x / 2 + grab.x;
            if (scene.dropTarget(drag) != 3)
                throw std::runtime_error("Left drag advances before crossing the previous center");
            drag.pointer.x = 416 - RosterCardSize.x / 2 + grab.x;
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
        State redEmpty = empty;
        redEmpty.recruitment.activeTeam = 1;
        redEmpty.battle.armies[1].units.clear();
        render("31-red-recruitment", redEmpty);
        render("24-empty-recruitment", empty);
        if (getButton(30).enabled || getButton(static_cast<int>(uiAction::ClearArmy)).enabled)
            throw std::runtime_error("Empty army can be confirmed or cleared");
        s.recruitment.screen = RecruitmentScreen::Academy;
        s.choices = {{1, "Купить: Лёгкий"},
                     {2, "Купить: Тяжёлый", false},
                     {3, "Купить: Лучник", false},
                     {4, "Купить: Маг", false},
                     {5, "Купить: Целитель", false},
                     {6, "Купить: Гуляй-город", false},
                     {20, "Удалить бойца"},
                     {21, "Переместить бойца"},
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
        State fortAcademy = s;
        fortAcademy.recruitment.spent = 42;
        fortAcademy.recruitment.remaining = 78;
        const UnitSnapshot fort{UnitKind::GulyayGorod, "Заслон", 850, 1000, 42, 0, 0, 0};
        fortAcademy.battle.armies[0].units = {fort};
        fortAcademy.battle.armies[0].totalCost = 42;
        for (auto& offer : fortAcademy.recruitment.offers) offer.enabled = true;
        for (auto& choice : fortAcademy.choices) if (choice.value >= 1 && choice.value <= 6)
            choice.enabled = true;
        render("32-fort-academy", fortAcademy);
        if (!getButton(6).enabled || getButton(6).bounds.position.x <= getButton(5).bounds.position.x)
            throw std::runtime_error("Fortification and healer purchase buttons are not independent");
        std::rotate(fortAcademy.recruitment.offers.begin(), fortAcademy.recruitment.offers.end() - 1,
                    fortAcademy.recruitment.offers.end());
        render("33-fort-first-academy", fortAcademy);
        if (getButton(6).bounds.position.x >= getButton(1).bounds.position.x)
            throw std::runtime_error("Fortification button depends on catalog order");
        State drag = s;
        drag.drag = DragState{0, {119.5f, 654}, {440, 654}, true};
        render("19-dragging", drag);
        State moved = s;
        auto unit = moved.battle.armies[0].units[0];
        moved.battle.armies[0].units.erase(moved.battle.armies[0].units.begin());
        moved.battle.armies[0].units.insert(moved.battle.armies[0].units.begin() + 2, unit);
        moved.rosterTransition =
            RosterTransition{ArmyEdit{ArmyEdit::Kind::Move, 0, 2}, unit, .5f, sf::Vector2f(348, 584)};
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
        budget.prompt = "Бюджет каждой армии: ";
        budget.number.reset("120");
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
        add("«Брон [конь] [щит] [шлем] [копьё]» наносит удар по «Мирн» — урон: 7, здоровье цели: 13.", 0);
        add("«Тиль» восстанавливает здоровье «Брон» — восстановлено здоровья: 8.", 1);
        render("08-battle", s);
        const auto battleState = s;
        State fortBattle = s;
        fortBattle.battle.armies[0].units = {fort, s.battle.armies[0].units[1]};
        fortBattle.battle.armies[1].units = {fort, s.battle.armies[1].units[0]};
        fortBattle.battle.armies[1].units[0].name = "Стена";
        fortBattle.battle.armies[1].units[0].hp = 214;
        fortBattle.battle.armies[0].totalCost = 52;
        fortBattle.battle.armies[1].totalCost = 72;
        render("34-fort-battle", fortBattle);
        const auto front = scene.slotCenter({0, 0}, fortBattle);
        render("35-fort-tooltip", fortBattle, {front.x, 360});
        State hit = fortBattle;
        hit.battle.armies[0].units = {s.battle.armies[0].units[1]};
        hit.battle.armies[1].units[0].hp = 850;
        presentation::Playback fortHit;
        const BattleEvent strike{EventType::Melee, 17, 0, names[1], "Стена", 6, 844};
        hit.effect = fortHit.begin(hit.battle, strike);
        hit.effect->advance(presentation::duration(strike.type) * .6f);
        fortHit.commit(hit.battle, *hit.effect);
        hit.playing = true;
        hit.waiting = false;
        render("36-fort-hit", hit);
        if (hit.battle.armies[1].units[0].hp != 844)
            throw std::runtime_error("Fortification playback does not use reported durability");
        State draw = fortBattle;
        for (auto& army : draw.battle.armies) {
            army.units.resize(1);
            army.totalCost = 42;
        }
        draw.stage = ViewStage::Result;
        draw.waiting = false;
        draw.winner = -2;
        draw.choices = {{1, "Новая партия"}, {2, "Загрузить"}, {0, "В главное меню"}};
        render("37-fort-draw-result", draw);

        presentation::Playback playback;
        const EventType types[] = {EventType::Melee, EventType::Shot, EventType::Heal,
                                   EventType::Clone, EventType::Buff, EventType::Death};
        for (int i = 0; i < 6; ++i) {
            State effect = s;
            BattleEvent e{types[i], 17, 0,    names[i == 1 ? 2 : i == 2 ? 4 : i == 3 ? 3 : 0], names[1], 7,
                          13,       8,  "щит"};
            effect.effect = playback.begin(effect.battle, e);
            if (effect.effect->advance(presentation::duration(e.type) * .57f))
                playback.commit(effect.battle, *effect.effect);
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
        auto logEvent = [&](State& image, const BattleEvent& event, const std::string& copyName = "") {
            image.log.clear();
            image.log.startTurn(event.turn,
                [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); }, LogWidth);
            std::mt19937 phrases(42);
            const auto message = battletext::icons(EventFormatter::format(event, phrases, false, copyName));
            image.log.append(sf::String::fromUtf8(message.begin(), message.end()).toUtf32(), event.team,
                [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); }, LogWidth);
        };
        State speeds = battleState;
        speeds.speed = 4;
        render("38-direct-speeds", speeds);
        for (const auto action : {SpeedHalf, Speed, Speed2, Speed4})
            if (!getButton(action).enabled) throw std::runtime_error("Direct speed selector is disabled");
        speeds.speed = .5f;
        render("38-half-speed", speeds);
        speeds.speed = 1.f;
        render("38-default-speed", speeds);
        State travel = battleState;
        travel.playing = true; travel.waiting = false;
        for (int i = 5; i < 20; ++i) {
            auto unit = travel.battle.armies[0].units[static_cast<std::size_t>(i % 5)];
            unit.name = "Боец " + std::to_string(i + 1);
            travel.battle.armies[0].units.push_back(unit);
        }
        travel.offsets[0] = 12;
        for (int i = 0; i < 3; ++i) {
            travel.scrolls[0] = presentation::Scroll(0, 12);
            travel.scrolls[0]->elapsed = travel.scrolls[0]->duration * i / 2;
            render("39-page-scroll-" + std::to_string(i), travel);
        }
        State editable = budget;
        editable.error.clear();
        editable.number.reset("120");
        editable.number.insert('0');
        render("46-budget-appended-zero", editable);
        if (editable.number.value() != "1200") throw std::runtime_error("Budget editing replaced the old value");
        scene.placeNumberCaret(editable, {150 + scene.textWidth("12", 32), 487});
        if (editable.number.cursor() != 2) throw std::runtime_error("Click did not place the numeric caret");
        editable.number.move(-1, true);
        render("46-budget-selection", editable);
        if (hoveredArmy(battleState, {470,400}) != 0 || hoveredArmy(battleState, {618,400}) != 1 ||
            hoveredArmy(battleState, {546,400}) || hoveredArmy(battleState, {1200,400}))
            throw std::runtime_error("Wheel routing does not follow the hovered army");
        State crossPage = methods;
        for (int i = 5; i < 24; ++i) {
            auto unit = crossPage.battle.armies[0].units[1];
            unit.name = "Боец " + std::to_string(i + 1);
            crossPage.battle.armies[0].units.push_back(unit);
        }
        crossPage.offsets[0] = 8;
        crossPage.drag = DragState{7, {1176.5f, 654}, {130.f, 654}, true};
        render("40-cross-page-drag", crossPage);
        if (scene.dropTarget(*crossPage.drag) != 8)
            throw std::runtime_error("Cannot swap units eight and nine across pages");

        State rosterScroll = crossPage;
        rosterScroll.drag.reset();
        for (const auto offset : {.25f, 3.5f, 8.75f}) {
            rosterScroll.offsets[0] = offset;
            render("47-roster-scroll-" + std::to_string(offset), rosterScroll);
            if (std::any_of(scene.buttons().begin(), scene.buttons().end(), [](const Button& b) {
                return b.action >= 200 && b.action <= 203;
            })) throw std::runtime_error("Roster still contains page arrows");
        }
        State battleScroll = travel;
        battleScroll.scrolls[0].reset(); battleScroll.offsets = {2.5f, .5f};
        for (int i = 0; i < 5; ++i) battleScroll.battle.armies[1].units.push_back(battleScroll.battle.armies[1].units[i]);
        render("48-battle-wheel-scroll", battleScroll);
        if (std::any_of(scene.buttons().begin(), scene.buttons().end(), [](const Button& b) {
            return b.action >= 200 && b.action <= 203;
        })) throw std::runtime_error("Battle still contains page arrows");
        State hoveredBattle = battleScroll;
        hoveredBattle.playing = false; hoveredBattle.waiting = true;
        render("50-scrolled-unit-tooltip", hoveredBattle, {422, 400});
        State formation = battleState;
        for (int team = 0; team < 2; ++team) {
            for (int i = 5; i < 9; ++i) formation.battle.armies[team].units.push_back(formation.battle.armies[team].units[i % 5]);
            formation.battle.armies[team].units[0].hp = 0;
            formation.battle.armies[team].units[2].hp = 0;
            formation.battle.armies[team].units[5].hp = 0;
        }
        formation.effect = presentation::Playback::cleanup(formation.battle, 17);
        formation.playing = true; formation.waiting = false;
        for (int i = 0; i < 5; ++i) {
            formation.effect->elapsed = presentation::duration(EventType::Promote) * i / 4;
            render("49-whole-formation-" + std::to_string(i), formation);
        }
        formation.effect->elapsed = 0;
        struct DetailedClone : BattleEvent {
            int sourceIndex = -1;
            std::string cloneName;
            int cloneHp = -1;
        };
        State cloneBase = battleState;
        cloneBase.battle.armies[0].units = {{UnitKind::Archer, "Дарр", 20, 20, 18, 4, 0, 11},
            {UnitKind::Archer, "Дарр", 1, 20, 18, 4, 0, 11}, {UnitKind::Mage, "Маг", 14, 14, 25}};
        cloneBase.battle.armies[0].totalCost = 61;
        cloneBase.log.clear();
        DetailedClone copy;
        copy.type = EventType::Clone; copy.turn = 17; copy.team = 0;
        copy.actor = "Маг"; copy.target = "Дарр";
        copy.sourceIndex = 1; copy.cloneName = "Дарр [Клон 1]"; copy.cloneHp = 1;
        const float clonePhases[] = {.16f, .34f, .52f, .85f};
        for (int i = 0; i < 4; ++i) {
            State image = cloneBase;
            presentation::Playback motion;
            image.effect = motion.begin(image.battle, copy);
            if (image.effect->advance(presentation::duration(copy.type) * clonePhases[i])) {
                motion.commit(image.battle, *image.effect);
                logEvent(image, copy, copy.cloneName);
            }
            image.playing = true; image.waiting = false;
            render("41-clone-sequence-" + std::to_string(i), image);
        }
        State ambiguous = cloneBase;
        presentation::Playback oldEvents;
        ambiguous.effect = oldEvents.begin(ambiguous.battle, static_cast<const BattleEvent&>(copy));
        ambiguous.effect->advance(presentation::duration(copy.type) * .62f);
        oldEvents.commit(ambiguous.battle, *ambiguous.effect);
        ambiguous.playing = true; ambiguous.waiting = false;
        render("42-clone-without-source-index", ambiguous);
        for (const float phase : {.32f, .65f}) {
            State healed = battleState;
            healed.battle.armies[0].units[1].hp = 5;
            const BattleEvent event{EventType::Heal, 17, 0, names[4], names[1], 0, 0, 8};
            presentation::Playback motion;
            healed.effect = motion.begin(healed.battle, event);
            if (healed.effect->advance(presentation::duration(event.type) * phase)) {
                motion.commit(healed.battle, *healed.effect);
                logEvent(healed, event);
            }
            healed.playing = true; healed.waiting = false;
            render(phase < .5f ? "43-heal-transfer" : "43-heal-impact", healed);
        }
        for (const auto& equipment : {"шлем", "щит", "копьё", "конь"}) {
            State granted = battleState;
            granted.battle.armies[0].units[0] = {UnitKind::Heavy, "Брон", 31, 45, 30, 9, 5};
            const BattleEvent event{EventType::Buff, 17, 0, names[1], "Брон", 0, 0, 0, equipment};
            presentation::Playback motion;
            granted.effect = motion.begin(granted.battle, event);
            if (granted.effect->advance(presentation::duration(event.type) * .65f)) {
                motion.commit(granted.battle, *granted.effect);
                logEvent(granted, event);
            }
            granted.playing = true; granted.waiting = false;
            render("44-equipment-" + std::string(equipment), granted);
        }
        State damage = battleState;
        damage.battle.armies[1].units[0] = {UnitKind::Heavy, "Щитник", 900, 1000, 30, 9, 5};
        const BattleEvent hitText{EventType::Melee, 17, 0, names[0], "Щитник", 257, 643};
        presentation::Playback damageMotion;
        damage.effect = damageMotion.begin(damage.battle, hitText);
        damage.effect->advance(presentation::duration(hitText.type) * .68f);
        damageMotion.commit(damage.battle, *damage.effect);
        logEvent(damage, hitText);
        damage.playing = true; damage.waiting = false;
        render("45-readable-damage", damage);
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
                                 "Сохранение " + std::to_string(i + 1) +
                                     "\nБой, ход 17 | бюджет 120 | seed 42 | 31.10.2026 18:00"});
        s.choices.insert(
            s.choices.end(),
            {{10, "Предыдущая страница", false}, {11, "Следующая страница"}, {12, "Удаление сохранений"}, {0, "Отмена"}});
        render("12-archive", s);
        s.choices[0].label = std::string(64, 'W') + "\nСохранение повреждено: " + std::string(1500, 'W');
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
        paged.choices = {{1, "Купить: Лёгкий"}, {20, "Удалить бойца"},
                         {21, "Переместить бойца"},   {uiAction::ClearArmy, "Очистить армию"},
                         {30, "Армия готова"},  {uiAction::Back, "Назад"},
                         {0, "В главное меню"}};
        render("17-large-budget", paged);
        paged.enteringNumber = true;
        paged.numberCancel = NumberCancel::Dialog;
        paged.prompt = "Кого убрать (номер): ";
        paged.number.reset("1");
        render("18-cancel-dialog", paged);

        s = {};
        s.handoff = 1;
        render("13-handoff", s);
        s = {};
        s.choices = {{1, "Заменить текущую партию"}, {0, "Отмена"}};
        render("14-confirm", s);

        if (argc > 3 && std::string(argv[3]) == "--animate") {
            fs::create_directories(out / "animation");
            fs::create_directories(out / "scroll");
            State scrollMovie = travel;
            scrollMovie.scrolls[0] = presentation::Scroll(0, 12);
            int scrollFrame = 0;
            while (scrollMovie.scrolls[0]) {
                std::ostringstream name;
                name << "scroll/frame-" << std::setw(4) << std::setfill('0') << scrollFrame++;
                render(name.str(), scrollMovie);
                if (scrollMovie.scrolls[0]->advance(1.f / 24.f))
                    scrollMovie.scrolls[0].reset();
            }
            render("scroll/last", scrollMovie);
            fs::create_directories(out / "clone");
            State cloneMovie = cloneBase;
            cloneMovie.playing = true; cloneMovie.waiting = false;
            presentation::Playback cloning;
            cloneMovie.effect = cloning.begin(cloneMovie.battle, copy);
            int cloneFrame = 0;
            while (!cloneMovie.effect->finished()) {
                if (cloneMovie.effect->advance(presentation::playbackSeconds(1.f / 24.f, 1))) {
                    cloning.commit(cloneMovie.battle, *cloneMovie.effect);
                    logEvent(cloneMovie, copy, copy.cloneName);
                }
                std::ostringstream name;
                name << "clone/frame-" << std::setw(4) << std::setfill('0') << cloneFrame++;
                render(name.str(), cloneMovie);
            }
            fs::create_directories(out / "formation");
            State formationMovie = formation;
            int formationFrame = 0;
            while (!formationMovie.effect->finished()) {
                formationMovie.effect->advance(presentation::playbackSeconds(1.f / 24.f, 1));
                std::ostringstream name;
                name << "formation/frame-" << std::setw(4) << std::setfill('0') << formationFrame++;
                render(name.str(), formationMovie);
            }
            fs::create_directories(out / "roster-scroll");
            State rosterMovie = rosterScroll;
            rosterMovie.offsets[0] = 2;
            rosterMovie.scrolls[0] = presentation::Scroll(0, 2, .18f);
            int rosterFrame = 0;
            while (rosterMovie.scrolls[0]) {
                std::ostringstream name;
                name << "roster-scroll/frame-" << std::setw(4) << std::setfill('0') << rosterFrame++;
                render(name.str(), rosterMovie);
                if (rosterMovie.scrolls[0]->advance(1.f / 60.f)) rosterMovie.scrolls[0].reset();
            }
            rosterMovie.drag = DragState{7, {1162.5f, 654}, {1317.f, 654}, true};
            for (int i = 0; i < 60; ++i) {
                rosterMovie.offsets[0] = presentation::edgeScrollOffset(rosterMovie.offsets[0], 1, 1.f/60.f,
                    rosterMovie.battle.armies[0].units.size(), RosterUnits);
                std::ostringstream name;
                name << "roster-scroll/drag-" << std::setw(4) << std::setfill('0') << i;
                render(name.str(), rosterMovie);
            }
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
                const auto message = battletext::icons(EventFormatter::format(event, phrases, false,
                    movie.effect->clone ? movie.effect->clone->name : ""));
                while (!movie.effect->finished()) {
                    if (movie.effect->advance(presentation::playbackSeconds(1.f / 24.f, movie.speed))) {
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
