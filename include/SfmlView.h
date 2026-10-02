#pragma once
#include <filesystem>
#include <memory>
#include "View.h"

class SfmlView final : public View {
public:
    explicit SfmlView(const std::filesystem::path& fontPath);
    ~SfmlView() override;
    void setStage(ViewStage stage) override;
    std::optional<std::uint32_t> readNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max) override;
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override;
    void message(const std::string& text) override;
    void showArmies(const BattleSnapshot& snapshot) override;
    void showResult(int winner, const std::string& text) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
