#include "mqtt_handler.h"

MqttHandler::MqttHandler(const char* clientName)
: clientName(clientName), mqttClient(wifiClient) {}

bool MqttHandler::connect(const IPAddress& mqttBroker, uint16_t port, uint32_t timeoutMillis) {
    mode = Mode::BY_IP;
    brokerIp = mqttBroker;
    brokerPort = port;
    useAuth = false; user = ""; pass = "";

    mqttClient.setServer(brokerIp, brokerPort);

    unsigned long start = millis();
    while (millis() - start < timeoutMillis) {
        if (mqttClient.connect(clientName)) {
            mqttClient.publish("esp_state", "i am alive");
            return true;
        }
        delay(100);
    }
    // no exit; update() will retry
    return false;
}

bool MqttHandler::connect(const char* mqttBrokerHost, uint16_t port,
                          const char* username, const char* password,
                          uint32_t timeoutMillis) {
    mode = Mode::BY_HOST;
    brokerHost = mqttBrokerHost ? mqttBrokerHost : "";
    brokerPort = port;
    useAuth = (username && password);
    user = useAuth ? String(username) : "";
    pass = useAuth ? String(password) : "";

    mqttClient.setServer(brokerHost.c_str(), brokerPort);

    unsigned long start = millis();
    while (millis() - start < timeoutMillis) {
        if (useAuth) {
            if (mqttClient.connect(clientName, user.c_str(), pass.c_str())) {
                mqttClient.publish("esp_state", "i am alive");
                return true;
            }
        } else {
            if (mqttClient.connect(clientName)) {
                mqttClient.publish("esp_state", "i am alive");
                return true;
            }
        }
        delay(100);
    }
    return false;
}

void MqttHandler::update() {
    // keep-alive processing regardless of state
    mqttClient.loop();

    if (mqttClient.connected()) return;

    const unsigned long now = millis();
    if (now - lastReconnectMs < retryWaitMs) return; // throttle retries
    lastReconnectMs = now;

    // Try one non-blocking reconnect using cached parameters
    tryConnectOnce();
}

bool MqttHandler::tryConnectOnce() {
    if (mode == Mode::NONE) return false;

    if (mode == Mode::BY_IP) {
        mqttClient.setServer(brokerIp, brokerPort);
        if (mqttClient.connect(clientName)) {
            mqttClient.publish("esp_state", "i am alive");
            return true;
        }
        return false;
    }

    // BY_HOST
    mqttClient.setServer(brokerHost.c_str(), brokerPort);
    if (useAuth) {
        if (mqttClient.connect(clientName, user.c_str(), pass.c_str())) {
            mqttClient.publish("esp_state", "i am alive");
            return true;
        }
    } else {
        if (mqttClient.connect(clientName)) {
            mqttClient.publish("esp_state", "i am alive");
            return true;
        }
    }
    return false;
}

bool MqttHandler::publish(const char* topic, const JsonDocument& jsonMsg, bool retained) {
    if (!mqttClient.connected()) return false;
    String payload;
    serializeJson(jsonMsg, payload);
    return mqttClient.publish(topic, payload.c_str(), retained);
}

bool MqttHandler::publish(const char* topic, const char* payload, bool retained) {
    if (!mqttClient.connected()) return false;
    return mqttClient.publish(topic, payload, retained);
}
