#pragma once

#include <functional>

#include <lvgl.h>

namespace ui {

using DetailCloseCb = void (*)(void* owner);

// Opens a full-screen detail page on the top layer and returns its content area (flex column).
// `onClose` is invoked synchronously when the page closes, before its widgets are deleted.
lv_obj_t* detailOpen(const char* title, void* owner, DetailCloseCb onClose);
void detailClose();
bool detailIsOpen();

// Runs `action` directly, or after an "Are you sure?" dialog when `needConfirm` is set.
void runConfirmed(bool needConfirm, const char* question, std::function<void()> action);
void confirmCancel();

}  // namespace ui
