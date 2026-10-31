#include "SfmlView.h"
#include "SfmlScene.h"
#include "TextInput.h"
#include "ShortcutGuard.h"
#include <algorithm>

struct SfmlView::Impl {
    sf::RenderWindow window;
    sf::View camera{sf::FloatRect({0, 0}, {sfmlui::Width, sfmlui::Height})};
    sfmlui::Scene scene;
    sfmlui::State state;
    TextInput textInput;
    ShortcutGuard shortcuts;
    presentation::Playback playback;
    PlaybackHandler playbackHandler;
    std::optional<ArmyEdit> pendingEdit;
    std::optional<sfmlui::RosterTransition> nextTransition;
    std::optional<std::uint32_t> numberDefault;
    sf::Clock uiFrame;
    sf::Clock ambience;
    explicit Impl(const std::filesystem::path& assetDirectory) : scene(assetDirectory) {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        const auto desktop = sf::VideoMode::getDesktopMode().size;
        const float scale = std::min(
            {1.f, std::max(640.f, desktop.x - 80.f) / 1280.f, std::max(400.f, desktop.y - 100.f) / 800.f});
        window.create(
            sf::VideoMode({static_cast<unsigned>(1280 * scale), static_cast<unsigned>(800 * scale)}),
            "Shieldwall", sf::Style::Default, sf::State::Windowed, settings);
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        viewport(window.getSize());
    }
    void viewport(sf::Vector2u size) {
        if (size.x == 0 || size.y == 0)
            return;
        const float scale = std::min(size.x / sfmlui::Width, size.y / sfmlui::Height);
        const float w = sfmlui::Width * scale / size.x, h = sfmlui::Height * scale / size.y;
        camera.setViewport(sf::FloatRect({(1 - w) / 2, (1 - h) / 2}, {w, h}));
        window.setView(camera);
    }
    void draw() {
        if (!window.isOpen())
            throw ViewClosed{};
        state.ambience = ambience.getElapsedTime().asSeconds();
        const auto dt = uiFrame.restart().asSeconds();
        if (state.rosterTransition) {
            state.rosterTransition->progress = std::min(1.f, state.rosterTransition->progress + dt / .26f);
            if (state.rosterTransition->progress >= 1.f)
                state.rosterTransition.reset();
        }
        if (state.enteringText)
            state.input = textInput.value();
        window.clear(sf::Color(10, 16, 20));
        window.setView(camera);
        scene.draw(window, state, window.mapPixelToCoords(sf::Mouse::getPosition(window), camera));
        window.display();
    }
    bool hasChoice(std::uint32_t value) const {
        return std::any_of(state.choices.begin(), state.choices.end(),
                           [&](const MenuChoice& c) { return c.value == value && c.enabled; });
    }
    void clampLog() {
        state.logOffset = std::clamp(
            state.logOffset, 0, std::max(0, static_cast<int>(state.log.lines.size()) - sfmlui::LogLines));
    }
    void append(const std::string& text, int team = -1) {
        const auto icons = battletext::icons(text);
        const auto added = state.log.append(
            sf::String::fromUtf8(icons.begin(), icons.end()).toUtf32(), team,
            [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); }, sfmlui::LogWidth);
        if (state.logOffset > 0)
            state.logOffset += static_cast<int>(added);
        clampLog();
    }
    void eventText(int turn, int team, const std::string& text) {
        const auto added = state.log.startTurn(
            turn, [&](const std::u32string& row) { return scene.textWidth(sf::String(row)); },
            sfmlui::LogWidth);
        if (state.logOffset > 0)
            state.logOffset += static_cast<int>(added);
        append(text, team);
    }
    void notice(const std::string& text) {
        if (text.rfind("Набор армии:", 0) == 0 || text == "Выберите управление ходами." ||
            text == "Как игроки набирают армии?")
            return;
        state.notices.push_back(text);
        if (state.notices.size() > 4)
            state.notices.erase(state.notices.begin());
        if (state.stage == ViewStage::Battle || state.stage == ViewStage::Result ||
            (state.stage == ViewStage::Recruitment && !state.recruiting))
            append(text);
    }
    void clampUnits() {
        const std::size_t page = state.recruiting ? 8 : sfmlui::PageUnits;
        for (int team = 0; team < 2; ++team) {
            const auto n = state.battle.armies[team].units.size();
            state.offsets[team] = std::min(state.offsets[team], n == 0 ? 0 : ((n - 1) / page) * page);
        }
    }
    bool common(int action) {
        if (action == sfmlui::Speed) {
            state.speed = state.speed == .5f ? 1.f : state.speed == 1.f ? 2.f : .5f;
            return true;
        }
        if (action == sfmlui::Pause) {
            state.paused = !state.paused;
            return true;
        }
        if (action >= 200 && action <= 203) {
            const int team = (action - 200) / 2;
            const auto n = state.battle.armies[team].units.size();
            const std::size_t page = state.recruiting ? 8 : sfmlui::PageUnits;
            const auto last = n == 0 ? 0 : ((n - 1) / page) * page;
            auto& offset = state.offsets[team];
            offset = action % 2 == 0 ? (offset >= page ? offset - page : 0) : std::min(last, offset + page);
            return true;
        }
        return false;
    }
    static bool modifier(sf::Keyboard::Key key) {
        using K = sf::Keyboard::Key;
        return key == K::LAlt || key == K::RAlt || key == K::LControl || key == K::RControl ||
               key == K::LShift || key == K::RShift || key == K::LSystem || key == K::RSystem;
    }
    bool modified() const {
        using K = sf::Keyboard::Key;
        for (const auto key :
             {K::LAlt, K::RAlt, K::LControl, K::RControl, K::LShift, K::RShift, K::LSystem, K::RSystem})
            if (sf::Keyboard::isKeyPressed(key))
                return true;
        return false;
    }
    std::optional<int> poll() {
        using K = sf::Keyboard::Key;
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                throw ViewClosed{};
            }
            if (const auto* resize = event->getIf<sf::Event::Resized>())
                viewport(resize->size);
            if (event->is<sf::Event::FocusLost>() || event->is<sf::Event::FocusGained>())
                shortcuts.cancel();
            if (!window.hasFocus()) {
                state.drag.reset();
                shortcuts.cancel();
                continue;
            }
            if (const auto* entered = event->getIf<sf::Event::TextEntered>()) {
                if (state.enteringText && entered->unicode >= 0x20) {
                    if (state.selectAll)
                        textInput.clear();
                    state.selectAll = false;
                    state.error = textInput.append(entered->unicode) ? "" : "Имя слишком длинное.";
                }
                if (state.enteringNumber && entered->unicode >= U'0' && entered->unicode <= U'9') {
                    if (state.selectAll)
                        state.input.clear();
                    state.selectAll = false;
                    if (state.input.size() < 20)
                        state.input += static_cast<char>(entered->unicode);
                    state.error.clear();
                }
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                const bool mods = key->alt || key->control || key->shift || key->system || modified();
                if (mods || modifier(key->code))
                    shortcuts.cancel();
                if ((state.waiting || state.playing) && state.choices.empty() && !state.enteringNumber &&
                    !state.enteringText && key->code == K::S)
                    shortcuts.pressSave(true, mods);
                if ((state.enteringNumber || state.enteringText) && key->control && !key->alt &&
                    !key->system && key->code == K::A) {
                    state.selectAll = true;
                    continue;
                }
                if (!ShortcutGuard::plain(true, mods))
                    continue;
                if (key->code == K::Escape) {
                    if (state.drag) {
                        const auto drag = *state.drag;
                        state.drag.reset();
                        const auto& units = state.battle.armies[state.recruitment.activeTeam].units;
                        if (drag.active && drag.from < units.size()) {
                            state.rosterTransition =
                                sfmlui::RosterTransition{ArmyEdit{ArmyEdit::Kind::Move, drag.from, drag.from},
                                                         units[drag.from], 0, sfmlui::dragPosition(drag)};
                            uiFrame.restart();
                        }
                        continue;
                    }
                    if (state.handoff >= 0)
                        return static_cast<int>(uiAction::Back);
                    if (state.enteringText)
                        return sfmlui::Cancel;
                    if (state.enteringNumber)
                        return state.numberCancel == NumberCancel::Previous ? static_cast<int>(uiAction::Back)
                                                                            : sfmlui::Cancel;
                    if (hasChoice(uiAction::Back))
                        return static_cast<int>(uiAction::Back);
                    if (std::any_of(state.choices.begin(), state.choices.end(), [](const MenuChoice& c) {
                            return c.value == 0 && c.enabled && c.escapeCancel;
                        }))
                        return 0;
                    if (state.playing) {
                        state.paused = !state.paused;
                        continue;
                    }
                    window.close();
                    throw ViewClosed{};
                }
                if (state.handoff >= 0 && key->code == K::Enter)
                    return sfmlui::Submit;
                if (state.enteringNumber || state.enteringText) {
                    if (key->code == K::Backspace) {
                        if (state.enteringText) {
                            if (state.selectAll)
                                textInput.clear();
                            else
                                textInput.backspace();
                        } else if (state.selectAll)
                            state.input.clear();
                        else if (!state.input.empty())
                            state.input.pop_back();
                        state.selectAll = false;
                        state.error.clear();
                    }
                    if (key->code == K::Enter)
                        return sfmlui::Submit;
                } else if (!state.choices.empty()) {
                    if (key->code == K::Enter && hasChoice(1))
                        return 1;
                } else if (state.playing) {
                    if (key->code == K::A)
                        return sfmlui::Auto;
                    if (key->code == K::Space)
                        return sfmlui::Pause;
                } else {
                    if (state.waiting && (key->code == K::Space || key->code == K::Enter))
                        return sfmlui::Next;
                    if (!scene.saves(state) && hasChoice(1) && key->code == K::Enter)
                        return 1;
                    if (state.waiting && key->code == K::A)
                        return sfmlui::Auto;
                }
            }
            if (const auto* key = event->getIf<sf::Event::KeyReleased>()) {
                const bool mods = key->alt || key->control || key->shift || key->system || modified();
                if (mods || modifier(key->code))
                    shortcuts.cancel();
                if (key->code == K::S) {
                    const bool save = shortcuts.releaseSave(window.hasFocus(), mods);
                    if (save && state.waiting && !state.playing && !state.enteringNumber &&
                        !state.enteringText)
                        return sfmlui::Save;
                }
            }
            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (state.enteringNumber || state.enteringText || state.handoff >= 0)
                    continue;
                const auto pos = window.mapPixelToCoords(wheel->position, camera);
                if (pos.x >= 1072 && state.stage != ViewStage::MainMenu && !state.recruiting) {
                    state.logOffset += wheel->delta > 0 ? 3 : -3;
                    clampLog();
                } else if (!state.playing && !scene.saves(state) && pos.y >= 140 && pos.y < 736) {
                    const int team = state.recruiting ? state.recruitment.activeTeam : pos.x < 546 ? 0 : 1;
                    common(200 + team * 2 + (wheel->delta > 0 ? 0 : 1));
                    if (state.drag)
                        return std::nullopt; // Refresh page hit regions before a queued drop.
                }
            }
            const bool editable =
                state.recruiting && !state.rosterTransition && !state.enteringNumber && !state.enteringText;
            if (const auto* down = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (editable && down->button == sf::Mouse::Button::Left) {
                    const auto pos = window.mapPixelToCoords(down->position, camera);
                    const bool overButton = std::any_of(
                        scene.buttons().begin(), scene.buttons().end(),
                        [&](const sfmlui::Button& b) { return b.enabled && b.bounds.contains(pos); });
                    if (!overButton)
                        for (const auto& card : scene.rosterCards())
                            if (card.bounds.contains(pos)) {
                                state.drag = sfmlui::DragState{card.index, pos, pos, false,
                                                               pos - card.bounds.position};
                                break;
                            }
                }
            }
            if (const auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (state.drag) {
                    auto& drag = *state.drag;
                    drag.pointer = window.mapPixelToCoords(move->position, camera);
                    const auto d = drag.pointer - drag.origin;
                    if (d.x * d.x + d.y * d.y > 36)
                        drag.active = true;
                }
            }
            if (const auto* click = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (click->button != sf::Mouse::Button::Left)
                    continue;
                const auto pos = window.mapPixelToCoords(click->position, camera);
                if (state.drag) {
                    auto drag = *state.drag;
                    drag.pointer = pos;
                    state.drag.reset();
                    if (drag.active) {
                        if (const auto target = scene.dropTarget(drag); target && *target != drag.from) {
                            pendingEdit = ArmyEdit{ArmyEdit::Kind::Move, drag.from, *target};
                            const auto& old = state.battle.armies[state.recruitment.activeTeam].units;
                            nextTransition = sfmlui::RosterTransition{*pendingEdit, old[drag.from], 0,
                                                                      sfmlui::dragPosition(drag)};
                            return static_cast<int>(uiAction::ArmyEdit);
                        }
                        const auto& units = state.battle.armies[state.recruitment.activeTeam].units;
                        if (drag.from < units.size()) {
                            state.rosterTransition =
                                sfmlui::RosterTransition{ArmyEdit{ArmyEdit::Kind::Move, drag.from, drag.from},
                                                         units[drag.from], 0, sfmlui::dragPosition(drag)};
                            uiFrame.restart();
                        }
                        continue;
                    }
                }
                for (const auto& b : scene.buttons())
                    if (b.enabled && b.bounds.contains(pos))
                        return b.action;
            }
        }
        return std::nullopt;
    }
    void follow(const presentation::Step& step) {
        for (int team = 0; team < 2; ++team) {
            int low = -1, high = -1;
            for (const auto slot : {step.actor, step.target})
                if (slot.valid() && slot.team == team) {
                    low = low < 0 ? slot.index : std::min(low, slot.index);
                    high = std::max(high, slot.index);
                }
            if (low >= 0) {
                const auto current = static_cast<int>(state.offsets[team]);
                if (low >= current && high < current + static_cast<int>(sfmlui::PageUnits))
                    continue;
                state.offsets[team] =
                    static_cast<std::size_t>(high - low < static_cast<int>(sfmlui::PageUnits) ? low : high);
            }
        }
    }
    void play(const BattleEvent& event, const std::string& text) {
        state.playing = true;
        state.paused = false;
        state.turn = event.turn;
        shortcuts.cancel();
        state.effect = playback.begin(state.battle, event);
        follow(*state.effect);
        sf::Clock frame;
        while (!state.effect->finished()) {
            draw();
            if (const auto action = poll()) {
                if (*action == sfmlui::Auto || *action == sfmlui::Save || *action == 0) {
                    const auto request = *action == 0              ? TurnAction::Exit
                                         : *action == sfmlui::Save ? TurnAction::Save
                                         : state.automatic         ? TurnAction::Manual
                                                                   : TurnAction::Automatic;
                    if (playbackHandler && !playbackHandler(request)) {
                        state.effect.reset();
                        state.playing = false;
                        throw PlaybackStopped{};
                    }
                    frame.restart();
                } else
                    common(*action);
            }
            const float dt = std::min(frame.restart().asSeconds(), .075f);
            if (!state.paused && window.hasFocus() && state.effect->advance(dt * state.speed)) {
                playback.commit(state.battle, *state.effect);
                eventText(event.turn, event.team, text);
            }
        }
        state.effect.reset();
        state.playing = false;
        draw();
    }
};
SfmlView::SfmlView(const std::filesystem::path& assetDirectory)
    : impl_(std::make_unique<Impl>(assetDirectory)) {}
SfmlView::~SfmlView() = default;
void SfmlView::setPlaybackHandler(PlaybackHandler handler) {
    impl_->playbackHandler = std::move(handler);
}
std::optional<ArmyEdit> SfmlView::takeArmyEdit() {
    auto edit = impl_->pendingEdit;
    impl_->pendingEdit.reset();
    return edit;
}
std::optional<std::uint32_t> SfmlView::readSetupNumber(const std::string& p, std::uint32_t min,
                                                       std::uint32_t max, NumberCancel cancel,
                                                       std::uint32_t initial) {
    impl_->numberDefault = initial;
    return readNumberCancelable(p, min, max, cancel);
}

void SfmlView::clearDisplay() {
    const auto speed = impl_->state.speed;
    impl_->state = sfmlui::State{};
    impl_->state.speed = speed;
    impl_->playback.reset();
    impl_->shortcuts.cancel();
}
void SfmlView::setStage(ViewStage stage) {
    const auto previous = impl_->state.stage;
    if (stage == ViewStage::MainMenu || stage == ViewStage::Setup)
        clearDisplay();
    impl_->state.stage = stage;
    impl_->state.choices.clear();
    impl_->state.recruiting = false;
    impl_->shortcuts.cancel();
    if (stage != ViewStage::Result) {
        impl_->state.result.clear();
        impl_->state.winner = -1;
    }
    if (stage == ViewStage::Recruitment && previous != stage) {
        impl_->state.turn = 0;
        impl_->state.offsets = {};
    }
    if (stage == ViewStage::Battle && previous != ViewStage::Battle) {
        impl_->state.log.clear();
        impl_->playback.reset();
    }
}
std::optional<std::uint32_t> SfmlView::readNumberCancelable(const std::string& p, std::uint32_t min,
                                                            std::uint32_t max, NumberCancel cancel) {
    impl_->state.numberCancel = cancel;
    return readNumber(p, min, max);
}
std::optional<std::uint32_t> SfmlView::readNumber(const std::string& prompt, std::uint32_t min,
                                                  std::uint32_t max) {
    auto& s = impl_->state;
    s.enteringNumber = true;
    s.prompt = prompt;
    s.selectAll = true;
    s.error.clear();
    s.input = std::to_string(std::clamp(impl_->numberDefault.value_or(min == 0 ? 42u : 120u), min, max));
    impl_->numberDefault.reset();
    impl_->shortcuts.cancel();
    impl_->window.setKeyRepeatEnabled(true);
    while (true) {
        impl_->draw();
        const auto action = impl_->poll();
        if (!action)
            continue;
        if (*action == static_cast<int>(uiAction::Back)) {
            s.enteringNumber = false;
            impl_->window.setKeyRepeatEnabled(false);
            throw ViewBack{};
        }
        if (*action == sfmlui::Cancel || *action == 0) {
            s.enteringNumber = false;
            impl_->window.setKeyRepeatEnabled(false);
            return std::nullopt;
        }
        if (*action == sfmlui::Submit) {
            if (const auto value = parseNumber(s.input, min, max)) {
                s.enteringNumber = false;
                impl_->window.setKeyRepeatEnabled(false);
                return value;
            }
            s.error = "Введите число от " + std::to_string(min) + " до " + std::to_string(max) + ".";
        }
    }
}
std::optional<std::string> SfmlView::readText(const std::string& prompt, std::size_t maxBytes) {
    auto& s = impl_->state;
    s.enteringText = true;
    s.prompt = prompt;
    s.selectAll = false;
    s.error.clear();
    impl_->textInput.reset(maxBytes);
    impl_->shortcuts.cancel();
    impl_->window.setKeyRepeatEnabled(true);
    while (true) {
        impl_->draw();
        const auto action = impl_->poll();
        if (!action)
            continue;
        if (*action == sfmlui::Cancel || *action == sfmlui::Submit) {
            s.enteringText = false;
            impl_->window.setKeyRepeatEnabled(false);
            if (*action == sfmlui::Cancel)
                return std::nullopt;
            return impl_->textInput.value();
        }
    }
}
std::optional<std::uint32_t> SfmlView::choose(const std::vector<MenuChoice>& choices) {
    auto& s = impl_->state;
    s.choices = choices;
    if (choices.empty())
        return std::nullopt;
    while (true) {
        impl_->draw();
        if (const auto action = impl_->poll()) {
            if (s.recruiting) {
                const auto& units = s.battle.armies[s.recruitment.activeTeam].units;
                if (*action >= sfmlui::RemoveBase &&
                    static_cast<std::size_t>(*action - sfmlui::RemoveBase) < units.size()) {
                    const auto from = static_cast<std::size_t>(*action - sfmlui::RemoveBase);
                    impl_->pendingEdit = ArmyEdit{ArmyEdit::Kind::Remove, from, from};
                    impl_->nextTransition = sfmlui::RosterTransition{*impl_->pendingEdit, units[from], 0, {}};
                    s.choices.clear();
                    return uiAction::ArmyEdit;
                }
                if (*action == static_cast<int>(uiAction::ArmyEdit)) {
                    s.choices.clear();
                    return uiAction::ArmyEdit;
                }
            }
            if (*action >= 0 && impl_->hasChoice(static_cast<std::uint32_t>(*action))) {
                if (s.recruiting && (*action == static_cast<int>(uiAction::ClearArmy) ||
                                     (s.recruitment.screen == RecruitmentScreen::Methods &&
                                      (*action == 1 || *action == 3)))) {
                    s.rosterTransition.reset();
                    s.offsets[s.recruitment.activeTeam] = 0;
                }
                s.choices.clear();
                return static_cast<std::uint32_t>(*action);
            }
            impl_->common(*action);
        }
    }
}
void SfmlView::message(const std::string& text) {
    impl_->notice(text);
}
void SfmlView::battleMessage(int turn, int team, const std::string& text) {
    impl_->eventText(turn, team, text);
}
void SfmlView::battleEvent(const BattleEvent& event, const std::string& text) {
    if (impl_->state.stage == ViewStage::Battle)
        impl_->play(event, text);
    else
        impl_->eventText(event.turn, event.team, text);
}
void SfmlView::showArmies(const BattleSnapshot& frame) {
    impl_->state.battle = frame;
    impl_->state.recruiting = false;
    impl_->clampUnits();
    impl_->draw();
}
void SfmlView::showRecruitment(const RecruitmentSnapshot& frame) {
    if (frame.screen != impl_->state.recruitment.screen ||
        frame.activeTeam != impl_->state.recruitment.activeTeam) {
        impl_->state.drag.reset();
        impl_->state.rosterTransition.reset();
    }
    if (impl_->nextTransition) {
        impl_->state.rosterTransition = impl_->nextTransition;
        impl_->nextTransition.reset();
        impl_->uiFrame.restart();
    }
    impl_->state.recruitment = frame;
    impl_->state.battle.armies = frame.armies;
    impl_->state.recruiting = true;
    impl_->clampUnits();
}
void SfmlView::setBattleProgress(int turn, BattleMode mode) {
    impl_->state.turn = turn;
    impl_->state.automatic = mode == BattleMode::Automatic;
}
TurnAction SfmlView::waitForTurn(BattleMode mode) {
    auto& s = impl_->state;
    s.waiting = true;
    s.automatic = mode == BattleMode::Automatic;
    sf::Clock clock;
    TurnAction result = TurnAction::Next;
    while (true) {
        impl_->draw();
        if (const auto action = impl_->poll()) {
            if (*action == sfmlui::Next)
                break;
            if (*action == 0) {
                result = TurnAction::Exit;
                break;
            }
            if (*action == sfmlui::Save) {
                result = TurnAction::Save;
                break;
            }
            if (*action == sfmlui::Auto) {
                result = s.automatic ? TurnAction::Manual : TurnAction::Automatic;
                break;
            }
            impl_->common(*action);
        }
        if (s.automatic && impl_->window.hasFocus() && clock.getElapsedTime().asMilliseconds() >= 650)
            break;
    }
    s.waiting = false;
    if (result != TurnAction::Next)
        impl_->shortcuts.cancel();
    return result;
}
bool SfmlView::handoff(int team) {
    impl_->state.battle = {};
    impl_->state.recruitment = {};
    impl_->state.log.clear();
    impl_->state.notices.clear();
    impl_->state.logOffset = 0;
    impl_->state.choices.clear();
    impl_->state.handoff = team;
    while (true) {
        impl_->draw();
        if (const auto action = impl_->poll()) {
            if (*action == static_cast<int>(uiAction::Back)) {
                impl_->state.handoff = -1;
                throw ViewBack{};
            }
            if (*action == sfmlui::Submit)
                break;
        }
    }
    impl_->state.handoff = -1;
    return true;
}
void SfmlView::showResult(int winner, const std::string& text) {
    impl_->state.winner = winner;
    impl_->state.offsets = {};
    impl_->state.result = text;
    impl_->append(text);
}
