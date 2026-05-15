#pragma once
#include "../include/shared_state.h"
#include <SFML/Graphics.hpp>
#include <vector>

// HUD draw helpers.
struct Button {
    sf::FloatRect rect;
    ActionType action;
    std::string label;
};

struct Snapshot {
    SharedState state;
};

void draw_hud(
    sf::RenderWindow& window,
    sf::Font& font,
    const Snapshot& snap,
    const std::vector<Button>& buttons,
    int hover_idx,
    ActionType selected_action,
    int selected_target,
    int active_tab);
