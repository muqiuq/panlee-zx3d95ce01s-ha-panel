#include "page_manager.h"

#include <vector>

#include "background.h"
#include "theme.h"

namespace ui {

namespace {

struct Page {
    lv_obj_t* screen;
    lv_obj_t* navLabel;
    String name;
};

std::vector<Page> pages;
size_t current = 0;
lv_obj_t* blankScreen = nullptr;

void enableGestureBubble(lv_obj_t* obj) {
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
        enableGestureBubble(child);
    }
}

void gestureCb(lv_event_t*) {
    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    switch (lv_indev_get_gesture_dir(indev)) {
        case LV_DIR_LEFT:
            lv_indev_wait_release(indev);
            nextPage();
            break;
        case LV_DIR_RIGHT:
            lv_indev_wait_release(indev);
            prevPage();
            break;
        default:
            break;
    }
}

void prevClicked(lv_event_t*) {
    prevPage();
}

void nextClicked(lv_event_t*) {
    nextPage();
}

lv_obj_t* navButton(lv_obj_t* parent, const char* symbol, lv_event_cb_t cb, lv_align_t align) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 72, kNavHeight - 8);
    lv_obj_align(btn, align, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, theme().text, 0);
    lv_obj_center(label);
    return btn;
}

void updateNavLabel() {
    if (current >= pages.size()) {
        return;
    }
    const Page& p = pages[current];
    lv_label_set_text_fmt(p.navLabel, "%s  %u/%u", p.name.c_str(), static_cast<unsigned>(current + 1),
                          static_cast<unsigned>(pages.size()));
}

}  // namespace

void pagesClear() {
    if (!blankScreen) {
        blankScreen = lv_obj_create(nullptr);
    }
    lv_obj_set_style_bg_color(blankScreen, theme().bg, 0);
    lv_screen_load(blankScreen);
    for (Page& p : pages) {
        lv_obj_delete(p.screen);
    }
    pages.clear();
}

lv_obj_t* pageCreate(const String& name) {
    const Theme& th = theme();
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, th.bg, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    if (const lv_image_dsc_t* bg = backgroundImage()) {
        lv_obj_t* img = lv_image_create(scr);
        lv_image_set_src(img, bg);
        lv_obj_set_pos(img, 0, 0);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_size(content, 480, 480 - kNavHeight);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* nav = lv_obj_create(scr);
    lv_obj_set_size(nav, 480, kNavHeight);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, th.tileBg, 0);
    lv_obj_set_style_bg_opa(nav, th.tileOpa, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    navButton(nav, LV_SYMBOL_LEFT, prevClicked, LV_ALIGN_LEFT_MID);
    navButton(nav, LV_SYMBOL_RIGHT, nextClicked, LV_ALIGN_RIGHT_MID);
    lv_obj_t* label = lv_label_create(nav);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, th.textSecondary, 0);
    lv_label_set_text(label, name.c_str());
    lv_obj_center(label);

    // Swiping is only recognized on the navigation bar.
    lv_obj_add_event_cb(nav, gestureCb, LV_EVENT_GESTURE, nullptr);
    enableGestureBubble(nav);
    pages.push_back({scr, label, name});
    return content;
}

void showPage(size_t index) {
    if (index >= pages.size()) {
        return;
    }
    current = index;
    updateNavLabel();
    lv_screen_load(pages[current].screen);
}

void nextPage() {
    if (!pages.empty()) {
        showPage((current + 1) % pages.size());
    }
}

void prevPage() {
    if (!pages.empty()) {
        showPage((current + pages.size() - 1) % pages.size());
    }
}

size_t currentPage() {
    return current;
}

size_t pageCount() {
    return pages.size();
}

}  // namespace ui
