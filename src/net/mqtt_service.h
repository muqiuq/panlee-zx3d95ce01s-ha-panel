#pragma once

#include <Arduino.h>

#include <vector>

namespace net {

struct MqttSettings {
    String host;
    uint16_t port = 1883;
    String user;
    String password;
    String clientId;
    String baseTopic;
    std::vector<String> topics;
};

struct MqttMessage {
    String topic;
    String payload;
};

// Runs the MQTT client in its own task. All functions are safe to call from the main loop.
void mqttBegin();
// Applies new settings and subscriptions; forces a reconnect.
void mqttConfigure(const MqttSettings& settings);
void mqttPublish(const String& topic, const String& payload, bool retain);
bool mqttPoll(MqttMessage& out);
bool mqttConnected();
const char* mqttStateName();
// Increments on every successful (re)connect.
uint32_t mqttConnectionCount();

}  // namespace net
