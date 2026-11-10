#include "detail.h"

#include "theme.h"

namespace ui {

namespace {

lv_obj_t* overlay = nullptr;
void* overlayOwner = nullptr;
DetailCloseCb overlayCloseCb = nullptr;

lv_obj_t* confirmBox = nullptr;
std::function<void()> pendingAction;

void closeClicked(lv_event_t*) {
    detailClose();
}

void confirmOk(lv_event_t*) {
    std::function<void()> action = std::move(pendingAction);
    confirmCancel();
    if (action) {
        action();
    }
}

void confirmNo(lv_event_t*) {
    confirmCancel();
}

}  // namespace

lv_obj_t* detailOpen(const char* title, void* owner, DetailCloseCb onClose) {
    detailClose();
    const Theme& th = theme();

    overlay = lv_obj_create(lv_layer_top());
    overlayOwner = owner;
    overlayCloseCb = onClose;
    lv_obj_set_size(overlay, 480, 480);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, th.dialog, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 16, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* titleLabel = lv_label_create(overlay);
    lv_label_set_text(titleLabel, title);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(titleLabel, 360);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(titleLabel, th.text, 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 8);

    lv_obj_t* close = lv_button_create(overlay);
    lv_obj_set_size(close, 64, 52);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(close, closeClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* closeLabel = lv_label_create(close);
    lv_label_set_text(closeLabel, LV_SYMBOL_CLOSE);
    lv_obj_center(closeLabel);

    lv_obj_t* content = lv_obj_create(overlay);
    lv_obj_set_size(content, 448, 448 - 68);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_row(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    return content;
}

void detailClose() {
    confirmCancel();
    if (!overlay) {
        return;
    }
    lv_obj_t* obj = overlay;
    DetailCloseCb cb = overlayCloseCb;
    void* owner = overlayOwner;
    overlay = nullptr;
    overlayOwner = nullptr;
    overlayCloseCb = nullptr;
    if (cb) {
        cb(owner);
    }
    lv_obj_delete_async(obj);
}

bool detailIsOpen() {
    return overlay != nullptr;
}

void runConfirmed(bool needConfirm, const char* question, std::function<void()> action) {
    if (!needConfirm) {
        action();
        return;
    }
    confirmCancel();
    pendingAction = std::move(action);

    confirmBox = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(confirmBox, "Are you sure?");
    lv_msgbox_add_text(confirmBox, question);
    lv_obj_t* no = lv_msgbox_add_footer_button(confirmBox, "Cancel");
    lv_obj_add_event_cb(no, confirmNo, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* yes = lv_msgbox_add_footer_button(confirmBox, "Yes");
    lv_obj_add_event_cb(yes, confirmOk, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_width(confirmBox, 400);
}

void confirmCancel() {
    pendingAction = nullptr;
    if (confirmBox) {
        lv_msgbox_close_async(confirmBox);
        confirmBox = nullptr;
    }
}

}  // namespace ui
