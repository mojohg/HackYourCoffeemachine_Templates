#include "wifi_handler.h"

namespace WifiHandler {

static const char* TAG = "WiFi";

static wifi_country_t make_eu_country() {
  wifi_country_t c{};
  c.cc[0] = 'E'; c.cc[1] = 'U'; c.cc[2] = '\0';
  c.schan  = 1;
  c.nchan  = 13;
  c.policy = WIFI_COUNTRY_POLICY_AUTO;
  return c;
}

static wifi_power_t powerFromDbm(int8_t dbm) {
  // Mapped auf die nächsten erlaubten Stufen
  // (nur die wichtigsten; feingranular wäre auch möglich)
  if (dbm >= 19) return WIFI_POWER_19_5dBm;
  if (dbm >= 15) return WIFI_POWER_15dBm;
  if (dbm >= 11) return WIFI_POWER_11dBm;
  if (dbm >= 8)  return WIFI_POWER_8_5dBm;
  return WIFI_POWER_8_5dBm; // defensiv
}

static void onEvent(WiFiEvent_t ev, WiFiEventInfo_t info) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ESP_LOGW(TAG, "DISCONNECTED, reason=%u (%s)",
               info.wifi_sta_disconnected.reason,
               reasonToString(info.wifi_sta_disconnected.reason));
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      ESP_LOGI(TAG, "CONNECTED to AP");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      ESP_LOGI(TAG, "GOT IP: %s",
               WiFi.localIP().toString().c_str());
      break;
    default:
      // optional: weitere Events loggen
      break;
  }
}

void begin() {
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Country EU
  auto eu = make_eu_country();
  esp_wifi_set_country(&eu);

  // b/g/n erlauben
  esp_wifi_set_protocol(WIFI_IF_STA,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  WiFi.onEvent(onEvent);
  ESP_LOGI(TAG, "WiFi stack initialized (EU, b/g/n, sleep off)");
}

int scanAndFindChannel(const char* ssid) {
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  ESP_LOGI(TAG, "Scan done: %d networks", n);
  for (int i = 0; i < n; i++) {
    const String s = WiFi.SSID(i);
    ESP_LOGD(TAG, "%s ch=%d rssi=%d enc=%d",
             s.c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i));
    if (s == ssid) {
      return WiFi.channel(i);
    }
  }
  return -1;
}

ConnectResult connect(const WifiConfig& cfg, uint32_t timeout_ms) {
  if (!cfg.ssid || !cfg.password) {
    ESP_LOGE(TAG, "SSID/PWD not set");
    return ConnectResult::FailedStart;
  }

  int channel = cfg.channel;

if (cfg.use_static_ip) {
    // Fallback-Logik: wenn kein DNS gesetzt ist → Gateway als DNS nehmen
    IPAddress dns1 = (cfg.dns1 != IPAddress(0,0,0,0)) ? cfg.dns1 : cfg.gateway;
    IPAddress dns2 = (cfg.dns2 != IPAddress(0,0,0,0)) ? cfg.dns2 : cfg.gateway;

    WiFi.config(cfg.local_ip, cfg.gateway, cfg.subnet, dns1, dns2);
    ESP_LOGI(TAG, "Using static IP %s, GW=%s, DNS=%s",
             cfg.local_ip.toString().c_str(),
             cfg.gateway.toString().c_str(),
             dns1.toString().c_str());
}
  ESP_LOGI(TAG, "Connecting to \"%s\" (ch=%d)...", cfg.ssid, channel);
  WiFi.begin(cfg.ssid, cfg.password, channel > 0 ? channel : 0);

  // WICHTIG: Tx-Power NACH begin()
  if (cfg.tx_power_dbm > 0) {
    WiFi.setTxPower(powerFromDbm(cfg.tx_power_dbm));
    ESP_LOGI(TAG, "Tx power set to ~%d dBm", (int)cfg.tx_power_dbm);
  }

  uint32_t t0 = millis();
  wl_status_t last = WL_IDLE_STATUS;
  while (millis() - t0 < timeout_ms) {
    wl_status_t s = WiFi.status();
    if (s != last) { last = s; ESP_LOGI(TAG, "status=%d", (int)s); }
    if (s == WL_CONNECTED) {
      ESP_LOGI(TAG, "Connected: IP=%s RSSI=%d",
               WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return ConnectResult::Connected;
    }
    delay(250);
  }

  ESP_LOGE(TAG, "Connect timeout after %lu ms", (unsigned long)timeout_ms);
  return ConnectResult::Timeout;
}

wl_status_t status() { return WiFi.status(); }
IPAddress localIP()   { return WiFi.localIP(); }
int rssi()            { return WiFi.RSSI(); }

const char* reasonToString(uint8_t r) {
  // Kurz-Mapping der häufigsten Gründe
  switch (r) {
    case 1:  return "UNSPECIFIED";
    case 2:  return "AUTH_EXPIRE";
    case 3:  return "AUTH_LEAVE";
    case 4:  return "ASSOC_EXPIRE";
    case 8:  return "ASSOC_LEAVE";
    case 15: return "4WAY_HANDSHAKE_TIMEOUT";
    case 17: return "AP_NOT_AUTHED";
    case 201:return "NO_AP_FOUND";
    default: return "?";
  }
}

} // namespace WifiHandler