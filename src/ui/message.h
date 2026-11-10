#pragma once

#include <Arduino.h>

namespace ui {

struct MessageInfo {
    String id;
    String title;
    String text;
    String level;    // info | warning | error | alarm
    String buttons;  // none | ok | yes_no | ok_cancel
    String answer;   // empty while open
    String time;
    bool open = false;
};

// Shows a pop-up from a JSON object or plain text (plain text = info message that wakes the screen).
bool messageShow(const String& payload, String& error);
const MessageInfo& lastMessage();
// Increments whenever the last message or its answer changes.
uint32_t messageRevision();

}  // namespace ui
