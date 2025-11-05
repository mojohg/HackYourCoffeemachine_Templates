#include "mqtt_handler.h"

MqttHandler::MqttHandler(const char* clientName) : 
    clientName(clientName),
    mqttClient(wifiClient)
{
}
bool MqttHandler::connect(IPAddress mqttBroker, u16_t port, MQTT_CALLBACK_SIGNATURE, u32_t timeoutMillis) {
    mqttClient.setServer(mqttBroker, port);
    mqttClient.setCallback(callback);

    String ip = mqttBroker.toString();            // <-- safe
    ESP_LOGI(MQTT_LOG_TAG, "Connecting to %s:%u as %s", ip.c_str(), (unsigned)port, clientName);

    // sinnvolle Defaults
    mqttClient.setKeepAlive(30);                  // 30s keepalive
    mqttClient.setSocketTimeout(3);               // 3s socket timeout
    // mqttClient.setBufferSize(1024);            // bei Bedarf größer

    Stopwatch32MS watch; watch.restart();
    for(;;) {
        if (mqttClient.connect(clientName)) {
            break;
        }
        if (watch.getTimeSinceStart() >= timeoutMillis) {
            ESP_LOGW(MQTT_LOG_TAG, "Connecting to %s timed out after %u ms", ip.c_str(), (unsigned)timeoutMillis);
            return false;
        }
        delay(50);
    }

    ESP_LOGI(MQTT_LOG_TAG, "Connected to %s:%u", ip.c_str(), (unsigned)port);

    // für Auto-Reconnect den "Host" als String cachen (auch bei IP)
    last_host_ = ip;
    last_port_ = port;
    last_use_auth_ = false;
    last_user_.clear();
    last_pass_.clear();

    mqttClient.publish("esp_state", "i am alive");
    return true;
}
bool MqttHandler::connect(const char* host, u16_t port,
                          const char* username, const char* password,
                          u32_t timeoutMillis) {
    mqttClient.setServer(host, port);
    last_host_ = host; last_port_ = port;
    last_use_auth_ = (username && password);
    last_user_ = last_use_auth_ ? String(username) : String();
    last_pass_ = last_use_auth_ ? String(password) : String();

    Stopwatch32MS watch;
    ESP_LOGI(MQTT_LOG_TAG, "Connecting to %s:%u as %s (%s)",
             host, port, clientName, last_use_auth_ ? "auth" : "no-auth");
    watch.restart();
    for(;;){
        bool ok = last_use_auth_
            ? mqttClient.connect(clientName, last_user_.c_str(), last_pass_.c_str())
            : mqttClient.connect(clientName);
        if (ok) break;
        if (watch.getTimeSinceStart() >= timeoutMillis) {
            ESP_LOGW(MQTT_LOG_TAG, "Connecting failed (timeout)");
            return false;
        }
        delay(50);
    }
    ESP_LOGI(MQTT_LOG_TAG, "Connected to %s:%u", host, port);
    mqttClient.publish("esp_state", "i am alive");
    return true;
}

bool MqttHandler::connect(const char* host, u16_t port, MQTT_CALLBACK_SIGNATURE,
                          const char* username, const char* password,
                          u32_t timeoutMillis) {
    mqttClient.setCallback(callback);
    return connect(host, port, username, password, timeoutMillis);
}

// update(): Reconnect mit Cache (User/Pass wiederverwenden)
void MqttHandler::update(){
    if(!mqttClient.connected() && lastConnectionWatch.getTimeSinceStart() >= DEFAULT_RETRY_WAIT_MILLIS){
        ESP_LOGW(MQTT_LOG_TAG, "lost connection to mqtt broker!");
        bool ok = false;
        if (!last_host_.isEmpty() && last_port_ > 0) {
            mqttClient.setServer(last_host_.c_str(), last_port_);
            ok = last_use_auth_
                ? mqttClient.connect(clientName, last_user_.c_str(), last_pass_.c_str())
                : mqttClient.connect(clientName);
        }
        if (!ok) {
            lastConnectionWatch.restart();
            return;
        }
        ESP_LOGI(MQTT_LOG_TAG, "reconnected to mqtt broker!");
        mqttClient.publish("esp_state", "i am alive");
        if(sub_topic != "") mqttClient.subscribe(sub_topic.c_str());
    }
    mqttClient.loop();
}
bool MqttHandler::publish(const char* topic, JsonDocument jsonMsg){
    String jsonString;
    serializeJson(jsonMsg, jsonString);
    return mqttClient.publish(topic, jsonString.c_str());
}
bool MqttHandler::subscribe(const char* topic) {
    sub_topic = String(topic);
    return mqttClient.subscribe(topic);
}

