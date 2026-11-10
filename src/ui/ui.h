#pragma once

#include <Arduino.h>

#include <vector>

namespace ui {

// Builds theme, tile pages and the system page from app::config.
void build();
// Tears down and rebuilds the whole UI (after config, theme or background changes).
void rebuild(bool reloadBackground);
void dispatch(const String& topic, const String& payload);
std::vector<String> subscriptions();

}  // namespace ui
