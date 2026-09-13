/*
  Config.h - Pin map, persistent settings and shared prototypes
  ---------------------------------------------------------------
  Project : SonoffAlexaSwitch
  Target  : Sonoff Basic (ESP8266, 1MB flash, no filesystem) - same
            hardware this project was originally built for.
            Any generic ESP8266 + relay board works, just change
            the pin numbers below.

  This file is included by every tab (.ino) in this sketch. Arduino
  IDE concatenates all .ino files in this folder into one big
  translation unit (main sketch file first, then alphabetically),
  so every function below is declared once here and defined in
  exactly one tab - this avoids the "implicit prototype" surprises
  that large multi-tab sketches (like Tasmota itself) are known to
  run into with the IDE's automatic prototype generator.
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <Arduino.h>
#include <ESP8266WebServer.h>

// ------------------------------------------------------------------
// Firmware identity
// ------------------------------------------------------------------
#define FW_NAME                "SonoffAlexaSwitch"
#define FW_VERSION              "1.0.0"

// ------------------------------------------------------------------
// Hardware pin map - Sonoff Basic default wiring
// ------------------------------------------------------------------
#define RELAY_PIN               12      // Relay control output
#define RELAY_ACTIVE_HIGH       true    // true: HIGH = relay ON (Sonoff Basic)

#define LED_PIN                 13      // Status LED
#define LED_ACTIVE_LOW          true    // true: LOW = LED ON (Sonoff Basic)

#define BUTTON_PIN              0       // Physical push button
#define BUTTON_ACTIVE_LOW       true    // true: pressed = LOW (uses INPUT_PULLUP)

// ------------------------------------------------------------------
// Button timing (milliseconds)
// ------------------------------------------------------------------
#define BUTTON_DEBOUNCE_MS       50
#define BUTTON_LONGPRESS_MS      5000    // Hold this long to reset WiFi config

// ------------------------------------------------------------------
// Power-on / relay persistence behaviour
// ------------------------------------------------------------------
enum PowerOnState : uint8_t {
  POWER_ON_ALWAYS_OFF   = 0,
  POWER_ON_ALWAYS_ON    = 1,
  POWER_ON_TOGGLE_LAST  = 2,  // opposite of last saved state
  POWER_ON_RESTORE_LAST = 3   // same as last saved state (default, like Tasmota)
};
#define DEFAULT_POWER_ON_STATE   POWER_ON_RESTORE_LAST

// ------------------------------------------------------------------
// Network defaults
// ------------------------------------------------------------------
#define AP_CONFIG_SSID_PREFIX    "SonoffSetup-"   // captive portal AP name prefix
#define AP_CONFIG_TIMEOUT_S      180              // give up captive portal after N s and retry
#define WIFI_RECONNECT_CHECK_MS  10000             // how often to check the WiFi link is alive
#define OTA_PASSWORD_DEFAULT     ""                // set one in the captive portal / EEPROM if you want OTA auth

// The Alexa/WeMo emulation below shares the SAME web server as the
// local status page (exactly like Tasmota does) instead of running a
// second HTTP server on its own port, so there's only one port to
// worry about.
#define WEBSERVER_PORT           80

// ------------------------------------------------------------------
// SSDP / WeMo (UPnP) emulation - lets an Amazon Echo discover and
// control this device with no cloud account, by pretending to be a
// Belkin WeMo switch. Same mechanism as Tasmota's built-in "Wemo
// emulation" (xdrv_21_wemo.ino) and the shared SSDP layer it uses
// (support_udp.ino) - reused here for the same reliability.
// ------------------------------------------------------------------
#define SSDP_PORT                 1900
#define SSDP_MULTICAST_IP_0       239
#define SSDP_MULTICAST_IP_1       255
#define SSDP_MULTICAST_IP_2       255
#define SSDP_MULTICAST_IP_3       250
#define SSDP_MSEARCH_DEBOUNCE_MS  300     // ignore duplicate M-SEARCH bursts within this window
#define SSDP_REJOIN_RETRY_MS      60000   // if joining the multicast group ever fails, retry this often

// ------------------------------------------------------------------
// Persistent settings stored in emulated EEPROM (ESP8266 flash sector)
// ------------------------------------------------------------------
#define EEPROM_SIZE              128
#define EEPROM_MAGIC              0xA5

struct PersistentSettings {
  uint8_t  magic;                 // validity marker
  uint8_t  relay_state;           // last known relay state (0/1)
  uint8_t  power_on_state;        // PowerOnState
  char     friendly_name[32];     // Alexa / mDNS / AP device name
  char     ota_password[32];      // optional OTA password
};

extern PersistentSettings settings;

// ------------------------------------------------------------------
// Shared runtime objects
// ------------------------------------------------------------------
extern bool relayState;
extern ESP8266WebServer server;   // defined in WebInterface.ino, shared
                                   // with WemoEmulation.ino so both the
                                   // local UI and the Alexa/UPnP
                                   // endpoints live on the same server

// ------------------------------------------------------------------
// Storage.ino
// ------------------------------------------------------------------
void StorageBegin();
void StorageLoad();
void StorageSave();
void StorageResetToDefaults();

// ------------------------------------------------------------------
// WifiSetup.ino
// ------------------------------------------------------------------
void WifiSetupBegin();
void WifiLoop();
void WifiFactoryReset();
bool WifiIsConnected();
String WifiGetHostname();

// ------------------------------------------------------------------
// RelayControl.ino
// ------------------------------------------------------------------
void RelayBegin();
void RelaySet(bool on, bool persist = true);
void RelayToggle();
bool RelayGet();

// ------------------------------------------------------------------
// ButtonControl.ino
// ------------------------------------------------------------------
void ButtonBegin();
void ButtonLoop();

// ------------------------------------------------------------------
// StatusLed.ino
// ------------------------------------------------------------------
enum LedPattern : uint8_t {
  LED_OFF,
  LED_SOLID_ON,
  LED_BLINK_SLOW,     // AP / config portal mode
  LED_BLINK_FAST,     // connecting to WiFi
  LED_FLASH_ONCE       // brief acknowledgement flash (button/voice/API command)
};
void LedBegin();
void LedSetPattern(LedPattern pattern);
void LedLoop();
void LedFlash();

// ------------------------------------------------------------------
// WebInterface.ino
// ------------------------------------------------------------------
void WebBegin();
void WebLoop();

// ------------------------------------------------------------------
// WemoEmulation.ino
// ------------------------------------------------------------------
void WemoRegisterHandlers();  // called once from WebBegin(), adds the
                               // UPnP routes to the shared `server`
void WemoNetworkUp();         // (re)join the SSDP multicast group -
                               // call whenever WiFi (re)connects
void WemoNetworkDown();       // leave the group - call on disconnect
void WemoLoop();              // poll for incoming M-SEARCH requests

// ------------------------------------------------------------------
// OtaUpdate.ino
// ------------------------------------------------------------------
void OtaBegin();
void OtaLoop();

#endif  // _CONFIG_H_
