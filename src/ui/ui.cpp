#include "ui.h"

#include <lvgl.h>

#include <algorithm>
#include <memory>

#include "app/config.h"
#include "background.h"
#include "detail.h"
#include "page_manager.h"
#include "settings_page.h"
#include "system_page.h"
#include "theme.h"
#include "tile.h"

namespace ui {

namespace {

constexpr int kGap = 12;
constexpr int kTileW = (480 - 3 * kGap) / 2;
constexpr int kTileH = (480 - kNavHeight - 3 * kGap) / 2;

std::vector<std::unique_ptr<Tile>> tiles;
lv_timer_t* tickTimer = nullptr;

void tickCb(lv_timer_t*) {
    for (auto& t : tiles) {
        t->tick();
    }
    settingsPageTick();
    systemPageTick();
}

}  // namespace

void build() {
    themeApply();

    for (const app::PageConfig& page : app::config.pages) {
        lv_obj_t* content = pageCreate(page.name);
        for (size_t i = 0; i < app::kTilesPerPage; ++i) {
            const int x = kGap + static_cast<int>(i % 2) * (kTileW + kGap);
            const int y = kGap + static_cast<int>(i / 2) * (kTileH + kGap);
            tiles.emplace_back(new Tile(page.tiles[i], content, x, y, kTileW, kTileH));
        }
    }
    settingsPageBuild(pageCreate("Settings"));
    systemPageBuild(pageCreate("System"));

    if (!tickTimer) {
        tickTimer = lv_timer_create(tickCb, 1000, nullptr);
    }
}

void rebuild(bool reloadBackground) {
    const size_t page = currentPage();
    detailClose();
    settingsPageReset();
    systemPageReset();
    pagesClear();
    tiles.clear();
    if (reloadBackground) {
        backgroundLoad();
    }
    build();
    showPage(page < pageCount() ? page : 0);
}

void dispatch(const String& topic, const String& payload) {
    for (auto& t : tiles) {
        t->onMessage(topic, payload);
    }
}

std::vector<String> subscriptions() {
    std::vector<String> topics;
    for (auto& t : tiles) {
        t->collectTopics(topics);
    }
    std::sort(topics.begin(), topics.end());
    topics.erase(std::unique(topics.begin(), topics.end()), topics.end());
    return topics;
}

}  // namespace ui
