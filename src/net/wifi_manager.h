#pragma once

#include <Arduino.h>

namespace net {

enum class WifiState { Idle, Connecting, Connected, AccessPoint };

void wifiBegin();
void wifiLoop();

WifiState wifiState();
const char* wifiStateName();
// Setup access point credentials (valid while in AccessPoint state).
const String& apSsid();
const String& apPassword();

}  // namespace net
