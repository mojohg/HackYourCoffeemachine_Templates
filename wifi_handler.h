#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_log.h"

namespace WifiHandler {

struct WifiConfig {
  const char* ssid;
  const char* password;

  // Optional: feste IP (sonst DHCP)
  bool use_static_ip = false;
  IPAddress local_ip{0,0,0,0};
  IPAddress gateway{0,0,0,0};
  IPAddress subnet{255,255,255,0};
  IPAddress dns1{0,0,0,0};
  IPAddress dns2{0,0,0,0};

  // Sendeleistung in dBm (z.B. 8, 11, 15, 19). 0 => nicht setzen
  int8_t tx_power_dbm = 8;

  // Zielkanal (0 = automatisch, sonst 1..13)
  int channel = 0;

  // Scan vor Connect und Kanal des Ziel-SSIDs übernehmen (falls gefunden)
  bool scan_for_channel = true;
};

enum class ConnectResult {
  Connected,
  Timeout,
  FailedStart
};

// Initialisiert den WiFi-Stack (Country EU, b/g/n, Sleep off, Events)
void begin();

// Sucht SSIDs und liefert den Kanal der ersten Übereinstimmung oder -1
int scanAndFindChannel(const char* ssid);

// Baut die Verbindung auf (optional Timeout & Kanalwahl über cfg)
ConnectResult connect(const WifiConfig& cfg, uint32_t timeout_ms = 15000);

// Helfer
wl_status_t status();
IPAddress localIP();
int rssi();
const char* reasonToString(uint8_t reason);

} // namespace WifiHandler
