// ... deine Includes bleiben ...
#include <ArduinoJson.h>
#include <arduino_stopwatch.h>
#include "esp_log.h"

// Credentials + Handlers
#include "wifi/wifiCredentials.h"
#include "wifi/wifi_handler.h"
#include "mqtt/mqtt_handler.h"
#include "ntp/ntp_handler.h"

// Sensors
#include "sensors/ads1115_continuous.h"
#include "sensors/debounced_button.h"
#include "sensors/ldr_blink_sensor.h"
#include "sensors/threshold_sensor.h"
#include "sensors/slider_button.h"

using ArduinoStopwatch::Stopwatch32MS;
using namespace WifiHandler;

// ---------- Logging ----------
static const char* ESP_LOG_TAG = "ESP";

// ---------- MQTT (JETZT mit Host + Credentials) ----------
static MqttHandler mqtt("IOT-Coffeemaker");
static const char*   MQTT_BROKER_HOST   = "mqtt.i40-iaam.de";   // <- anpassen
static const uint16_t MQTT_BROKER_PORT  = 1883;
static const char*   MQTT_USER          = "i40";             // <- anpassen
static const char*   MQTT_PASSWORD      = "123lalelu";         // <- anpassen
static const char*   PUBLISH_TOPIC      = "State";
static const uint32_t PUBLISH_INTERVAL_MS = 500;
static Stopwatch32MS publishWatch;

// ---------- NTP ----------
static const IPAddress NTP_SERVER_IP(192,168,178,21);
static const uint16_t NTP_UPDATE_INTERVAL_MS = 60000;
static const uint32_t TIMEZONE_OFFSET = 7200; // CET+DST in Sek.
static NtpHandler ntp(NTP_SERVER_IP, NTP_UPDATE_INTERVAL_MS, TIMEZONE_OFFSET);

// ---------- I2C / ADC ----------
static const int I2C_SCL_PIN = 15;
static const int I2C_SDA_PIN = 16;
static const uint8_t ADC_FILTER_SIZE = 4;
static Ads1115Continuous adc;
static const uint8_t CH_LIGHT_LEFT  = 0;
static const uint8_t CH_LIGHT_RIGHT = 1;
static const uint8_t CH_WATER_LEVEL = 2;

// ---------- LDR ----------
static LdrBlinkSensor lightLeft;
static LdrBlinkSensor lightRight;

// ---------- Water Level ----------
static const uint16_t WATER_ON_THR  = 13000;
static const uint16_t WATER_OFF_THR = 7000;
static const uint8_t  WATER_FILT    = 3;
static ThresholdSensor waterSwitch(WATER_ON_THR, WATER_OFF_THR, WATER_FILT);

// ---------- Buttons ----------
static const int PIN_BTN_COFFEE_LEFT   = 33;
static const int PIN_BTN_COFFEE_RIGHT  = 17;
static const int PIN_BTN_SLIDER_LEFT   = 21;
static const int PIN_BTN_SLIDER_RIGHT  = 18;

static DebouncedButton btnCoffeeLeft (PIN_BTN_COFFEE_LEFT,  ButtonType::NORMALY_CLOSED, InputType::ENABLE_PULLUP);
static DebouncedButton btnCoffeeRight(PIN_BTN_COFFEE_RIGHT, ButtonType::NORMALY_CLOSED, InputType::ENABLE_PULLUP);
static DebouncedButton btnSliderLeft  (PIN_BTN_SLIDER_LEFT,  ButtonType::NORMALY_CLOSED, InputType::ENABLE_PULLUP);
static DebouncedButton btnSliderRight (PIN_BTN_SLIDER_RIGHT, ButtonType::NORMALY_CLOSED, InputType::ENABLE_PULLUP);
static SliderButton slider;

// ---------- JSON ----------
static DynamicJsonDocument jsonDoc(512);

// ---------- NEU: EINZELNER Funktionsaufruf für alle Sensorupdates + JSON ----------
static inline void updateAllSensorsAndJson() {
  // 1) Rohdaten/Sensorzustände aktualisieren
  adc.update();
  btnCoffeeLeft.update();
  btnCoffeeRight.update();
  btnSliderLeft.update();
  btnSliderRight.update();
  lightLeft.update(adc, CH_LIGHT_LEFT);
  lightRight.update(adc, CH_LIGHT_RIGHT);
  waterSwitch.update(adc, CH_WATER_LEVEL);
  slider.update(btnSliderLeft.isPressed(), btnSliderRight.isPressed());

  // 2) JSON befüllen (kein Publish hier!)
  jsonDoc["ButtonCoffeeLeft"]  = btnCoffeeLeft.isPressed();
  jsonDoc["ButtonCoffeeRight"] = btnCoffeeRight.isPressed();
  jsonDoc["SliderPosition"]    = slider.getStateAsString();
  jsonDoc["LightLeft"]         = lightLeft.getStateString();
  jsonDoc["LightRight"]        = lightRight.getStateString();
  jsonDoc["WaterSwitch"]       = waterSwitch.getState();
  jsonDoc["Timestamp"]         = ntp.getFormattedTime();
}

void setup() {
  Serial.begin(115200);
  ESP_LOGI(ESP_LOG_TAG, "[APP] setup start");

  // I2C + ADC
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!adc.begin(true, true, true, false, ADC_FILTER_SIZE)) {
    ESP_LOGE(ESP_LOG_TAG, "ADS1115 init failed");
    delay(3000);
  }

  // WiFi
  WifiHandler::begin();
  WifiConfig cfg{};
  cfg.ssid = WIFI_SSID;
  cfg.password = WIFI_PWD;
  cfg.scan_for_channel = true;
  cfg.tx_power_dbm = 8;
  auto cres = WifiHandler::connect(cfg, 15000);
  if (cres != ConnectResult::Connected) {
    ESP_LOGE(ESP_LOG_TAG, "WiFi connect failed");
    delay(2000);
  }

  // NTP
  //ntp.begin();

  // MQTT (Host + Credentials) – kein exit bei Fail, Auto-Reconnect übernimmt
  mqtt.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_USER, MQTT_PASSWORD, 5000);

  // Initiale JSON-Struktur (damit erste Payload komplett ist)
  jsonDoc["ButtonCoffeeLeft"]  = false;
  jsonDoc["ButtonCoffeeRight"] = false;
  jsonDoc["SliderPosition"]    = "None";
  jsonDoc["LightLeft"]         = "OFF";
  jsonDoc["LightRight"]        = "OFF";
  jsonDoc["WaterSwitch"]       = false;
  //jsonDoc["Timestamp"]         = ntp.getFormattedTime();

  publishWatch.restart();
  ESP_LOGI(ESP_LOG_TAG, "[APP] setup done");
}

void loop() {
  // Handler updaten
  mqtt.update();
  ntp.update();

  // *** EINZIGER SENSOR-CALL ***
  updateAllSensorsAndJson();

  // Periodisch publishen – exakt gesteuert über publishWatch
  if (publishWatch.getTimeSinceStart() >= PUBLISH_INTERVAL_MS) {
    publishWatch.restart();
    if (mqtt.connected()) {
      mqtt.publish(PUBLISH_TOPIC, jsonDoc);
    }
  }

  // kleine Entlastung
  delay(2);
}
