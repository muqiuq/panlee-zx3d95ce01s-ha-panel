#include "mqtt_service.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include <atomic>
#include <deque>
#include <mutex>

namespace net {

namespace {

constexpr size_t kMaxQueue = 64;
constexpr uint32_t kRetryMs = 5000;

std::mutex mutex;
MqttSettings pendingSettings;
bool settingsChanged = false;
std::deque<MqttMessage> inbound;
std::deque<MqttMessage> outbound;
std::deque<bool> outboundRetain;

std::atomic<bool> connected{false};
std::atomic<uint32_t> connectionCount{0};
std::atomic<int> lastState{MQTT_DISCONNECTED};
std::atomic<bool> configured{false};

WiFiClient wifiClient;
PubSubClient client(wifiClient);
MqttSettings active;

void onMessage(char* topic, byte* payload, unsigned int length) {
    MqttMessage msg;
    msg.topic = topic;
    msg.payload.concat(reinterpret_cast<const char*>(payload), length);
    std::lock_guard<std::mutex> lock(mutex);
    if (inbound.size() >= kMaxQueue) {
        inbound.pop_front();
    }
    inbound.push_back(std::move(msg));
}

void connect() {
    const String willTopic = active.baseTopic + "/status";
    const char* user = active.user.isEmpty() ? nullptr : active.user.c_str();
    const char* pass = active.password.isEmpty() ? nullptr : active.password.c_str();
    client.setServer(active.host.c_str(), active.port);
    if (!client.connect(active.clientId.c_str(), user, pass, willTopic.c_str(), 1, true, "offline")) {
        lastState = client.state();
        return;
    }
    client.publish(willTopic.c_str(), "online", true);
    client.subscribe((active.baseTopic + "/cmd/#").c_str());
    for (const String& t : active.topics) {
        client.subscribe(t.c_str());
    }
    lastState = MQTT_CONNECTED;
    connected = true;
    ++connectionCount;
}

void flushOutbound() {
    for (int i = 0; i < 16; ++i) {
        MqttMessage msg;
        bool retain;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (outbound.empty()) {
                return;
            }
            msg = std::move(outbound.front());
            retain = outboundRetain.front();
            outbound.pop_front();
            outboundRetain.pop_front();
        }
        client.publish(msg.topic.c_str(), reinterpret_cast<const uint8_t*>(msg.payload.c_str()), msg.payload.length(),
                       retain);
    }
}

void task(void*) {
    uint32_t lastAttemptMs = 0;
    bool firstAttempt = true;
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (settingsChanged) {
                active = pendingSettings;
                settingsChanged = false;
                if (client.connected()) {
                    client.disconnect();
                }
                connected = false;
                firstAttempt = true;
            }
        }

        if (active.host.isEmpty() || WiFi.status() != WL_CONNECTED) {
            connected = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (!client.connected()) {
            connected = false;
            if (firstAttempt || millis() - lastAttemptMs > kRetryMs) {
                firstAttempt = false;
                lastAttemptMs = millis();
                connect();
            }
        } else {
            client.loop();
            flushOutbound();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

}  // namespace

void mqttBegin() {
    client.setBufferSize(2048);
    client.setKeepAlive(30);
    client.setSocketTimeout(5);
    client.setCallback(onMessage);
    xTaskCreatePinnedToCore(task, "mqtt", 8192, nullptr, 1, nullptr, 0);
}

void mqttConfigure(const MqttSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex);
    pendingSettings = settings;
    settingsChanged = true;
    configured = !settings.host.isEmpty();
}

void mqttPublish(const String& topic, const String& payload, bool retain) {
    std::lock_guard<std::mutex> lock(mutex);
    if (outbound.size() >= kMaxQueue) {
        outbound.pop_front();
        outboundRetain.pop_front();
    }
    outbound.push_back({topic, payload});
    outboundRetain.push_back(retain);
}

bool mqttPoll(MqttMessage& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (inbound.empty()) {
        return false;
    }
    out = std::move(inbound.front());
    inbound.pop_front();
    return true;
}

bool mqttConnected() {
    return connected;
}

const char* mqttStateName() {
    if (!configured) {
        return "not configured";
    }
    if (connected) {
        return "connected";
    }
    switch (lastState.load()) {
        case MQTT_CONNECTION_TIMEOUT: return "timeout";
        case MQTT_CONNECTION_LOST: return "connection lost";
        case MQTT_CONNECT_FAILED: return "connect failed";
        case MQTT_CONNECT_BAD_CREDENTIALS: return "bad credentials";
        case MQTT_CONNECT_UNAUTHORIZED: return "unauthorized";
        default: return "disconnected";
    }
}

uint32_t mqttConnectionCount() {
    return connectionCount;
}

}  // namespace net
