/*
  SonoffAlexaSwitch.ino
  ---------------------------------------------------------------
  A compact, robust ESP8266 smart-switch sketch that compiles
  cleanly as a plain Arduino IDE sketch (no PlatformIO needed).

  Inspired by / built to replace a Tasmota-based setup: same end
  goal (control the relay by voice through an Amazon Echo / Alexa)
  and the same robustness principles Tasmota follows, condensed
  into a single-purpose sketch:
    - persists relay state and survives power loss / reboots
    - self-heals the WiFi connection instead of needing a restart
    - configurable without reflashing (captive-portal WiFi setup)
    - controllable locally even if the voice assistant / cloud is
      unreachable (physical button + local web page + HTTP API)
    - over-the-air firmware updates
    - fully non-blocking main loop (no long delay() calls), so it
      stays responsive to button presses, WiFi, OTA and Alexa at
      the same time

  See Config.h for all pin numbers and tunables, and README.md for
  wiring, required libraries and setup instructions.
*/

#include "Config.h"

// ------------------------------------------------------------------
// Global, shared state (declared extern in Config.h)
// ------------------------------------------------------------------
PersistentSettings settings;
bool relayState = false;

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.printf("%s v%s starting...\n", FW_NAME, FW_VERSION);

  StorageBegin();
  StorageLoad();

  LedBegin();
  RelayBegin();
  ButtonBegin();

  // Blocks (with its own internal captive portal loop) only until
  // WiFi is configured; once connected, everything else is
  // non-blocking.
  WifiSetupBegin();

  OtaBegin();
  WebBegin();  // also registers the Wemo/UPnP HTTP handlers, see WemoEmulation.ino

  Serial.println(F("Setup complete."));
}

void loop() {
  // Each *Loop() function below is expected to return quickly
  // (no delay()/blocking waits) so every subsystem gets serviced
  // every cycle - this is what keeps button presses, OTA, the web
  // UI and Alexa discovery all responsive at the same time.
  WifiLoop();
  OtaLoop();
  WebLoop();
  WemoLoop();
  ButtonLoop();
  LedLoop();

  yield();  // let the ESP8266 WiFi/TCP stack and OTA handlers breathe
}
