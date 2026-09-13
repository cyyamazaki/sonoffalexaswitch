/*
  WifiSetup.ino - WiFi connection with a captive-portal fallback

  Uses the WiFiManager library (tzapu/WiFiManager) so no WiFi
  credentials are ever hardcoded in the sketch:
    - On first boot (or after a factory reset) the device opens its
      own access point named "SonoffSetup-XXXXXX". Connect a phone
      or laptop to it, a configuration page opens automatically
      (captive portal) where you pick your WiFi network and set the
      device's Alexa name and an optional OTA password.
    - On every later boot it reconnects to the saved network by
      itself.
    - If the saved network is ever unreachable, it re-opens the
      config portal automatically instead of leaving the device
      bricked without a screen or serial cable.

  After the initial (blocking, one-time) setup in WifiSetupBegin(),
  WifiLoop() only does a cheap, non-blocking health check so the
  rest of the sketch is never held up by WiFi.
*/

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "Config.h"

static WiFiManager wm;
static String hostname;
static unsigned long lastWifiCheckMs = 0;
static bool everConnected = false;
static bool wasConnectedLastCheck = false;

static void ConfigPortalStartedCallback(WiFiManager *mgr) {
  Serial.println(F("WiFi: no known network reachable, opened setup AP: "));
  Serial.println(mgr->getConfigPortalSSID());
  LedSetPattern(LED_BLINK_SLOW);
}

void WifiSetupBegin() {
  hostname = "sonoff-" + String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname.c_str());

  LedSetPattern(LED_BLINK_FAST);

  wm.setAPCallback(ConfigPortalStartedCallback);
  wm.setConfigPortalTimeout(AP_CONFIG_TIMEOUT_S);   // give up and retry later instead of hanging forever
  wm.setConnectTimeout(20);                          // seconds per connection attempt

  WiFiManagerParameter customName(
      "name", "Nome do dispositivo (como a Alexa vai chamar)",
      settings.friendly_name, sizeof(settings.friendly_name) - 1);
  WiFiManagerParameter customOta(
      "ota", "Senha de atualizacao OTA (opcional)",
      settings.ota_password, sizeof(settings.ota_password) - 1);
  wm.addParameter(&customName);
  wm.addParameter(&customOta);

  String apName = String(AP_CONFIG_SSID_PREFIX) + String(ESP.getChipId(), HEX);
  bool connected = wm.autoConnect(apName.c_str());

  // Persist whatever the user entered in the portal, even if this
  // attempt ultimately failed to reach the internet.
  bool changed = false;
  if (strncmp(settings.friendly_name, customName.getValue(), sizeof(settings.friendly_name)) != 0) {
    strncpy(settings.friendly_name, customName.getValue(), sizeof(settings.friendly_name) - 1);
    changed = true;
  }
  if (strncmp(settings.ota_password, customOta.getValue(), sizeof(settings.ota_password)) != 0) {
    strncpy(settings.ota_password, customOta.getValue(), sizeof(settings.ota_password) - 1);
    changed = true;
  }
  if (changed) StorageSave();

  if (connected) {
    everConnected = true;
    Serial.print(F("WiFi connected, IP: "));
    Serial.println(WiFi.localIP());
    LedSetPattern(LED_OFF);
    WemoNetworkUp();
  } else {
    // autoConnect() timed out (AP_CONFIG_TIMEOUT_S) without anyone
    // configuring it. Keep running - WifiLoop() below will keep
    // retrying the last known network in the background instead of
    // rebooting into another blocking portal loop.
    Serial.println(F("WiFi: setup portal timed out, will keep retrying in background."));
    LedSetPattern(LED_BLINK_SLOW);
  }

  lastWifiCheckMs = millis();
}

void WifiLoop() {
  unsigned long now = millis();
  if (now - lastWifiCheckMs < WIFI_RECONNECT_CHECK_MS) return;
  lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    if (!everConnected) {
      everConnected = true;
      Serial.print(F("WiFi connected, IP: "));
      Serial.println(WiFi.localIP());
    }
    if (!wasConnectedLastCheck) {
      // Just (re)connected - the previous SSDP multicast membership
      // (if any) is tied to the old link state, so tear it down and
      // rebuild it fresh. This is the actual fix for Alexa silently
      // losing the device after a WiFi blip - see WemoEmulation.ino.
      WemoNetworkUp();
    }
    wasConnectedLastCheck = true;
    LedSetPattern(LED_OFF);
    return;
  }

  if (wasConnectedLastCheck) {
    WemoNetworkDown();
  }
  wasConnectedLastCheck = false;

  Serial.println(F("WiFi: link down, attempting reconnect..."));
  LedSetPattern(LED_BLINK_SLOW);
  WiFi.reconnect();
}

bool WifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String WifiGetHostname() {
  return hostname;
}

void WifiFactoryReset() {
  wm.resetSettings();
  delay(200);
  ESP.restart();
}
