#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class MqttHandler {
public:
    static const uint32_t DEFAULT_RETRY_WAIT_MILLIS = 2000;

    explicit MqttHandler(const char* clientName = "MyEspClient");

    // Connect by IP (no username/password)
    bool connect(const IPAddress& mqttBroker, uint16_t port = 1883, uint32_t timeoutMillis = 2000);

    // Connect by hostname/URL (optional username/password)
    // If username==nullptr or password==nullptr -> connect without auth.
    bool connect(const char* mqttBrokerHost, uint16_t port = 1883,
                 const char* username = nullptr, const char* password = nullptr,
                 uint32_t timeoutMillis = 2000);

    // Call regularly from loop(); maintains connection & auto-reconnects (non-blocking)
    void update();

    // Publish ArduinoJson to topic (returns false if not connected or send failed)
    bool publish(const char* topic, const JsonDocument& jsonMsg, bool retained = false);

    // Convenience overload for raw payloads
    bool publish(const char* topic, const char* payload, bool retained = false);

    // Is MQTT connected right now?
    bool connected() { return mqttClient.connected(); }

    // Optional: change retry interval (ms)
    void setRetryWait(uint32_t ms) { retryWaitMs = ms; }

private:
    // internal single attempt using cached config
    bool tryConnectOnce();

    enum class Mode { NONE, BY_IP, BY_HOST };

    // cache
    Mode mode = Mode::NONE;
    IPAddress brokerIp;
    String    brokerHost;
    uint16_t  brokerPort = 1883;
    String    user;
    String    pass;
    bool      useAuth = false;

    // timing
    uint32_t  retryWaitMs = DEFAULT_RETRY_WAIT_MILLIS;
    unsigned long lastReconnectMs = 0;

    // infra
    const char* clientName;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
};
