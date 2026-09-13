/*
  ButtonControl.ino - debounced physical button

  Short press  -> toggle the relay locally (works even if WiFi/Alexa
                  is down - this is the "keeps working standalone"
                  guarantee, same principle as Tasmota's local
                  button handling).
  Long press (>= BUTTON_LONGPRESS_MS) -> wipe WiFi config and reboot
                  into the captive config portal (factory reset of
                  network settings only, relay settings are kept).
*/

#include "Config.h"

static bool lastRawState = false;      // debounced logical state (true = pressed)
static bool lastReading = false;       // raw last reading, for debounce edge detection
static unsigned long lastEdgeMs = 0;
static unsigned long pressStartMs = 0;
static bool longPressFired = false;

static bool ReadButtonRaw() {
  int level = digitalRead(BUTTON_PIN);
  bool pressed = BUTTON_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
  return pressed;
}

void ButtonBegin() {
  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  lastReading = ReadButtonRaw();
  lastRawState = lastReading;
}

void ButtonLoop() {
  unsigned long now = millis();
  bool reading = ReadButtonRaw();

  if (reading != lastReading) {
    lastEdgeMs = now;
    lastReading = reading;
  }

  if ((now - lastEdgeMs) > BUTTON_DEBOUNCE_MS && reading != lastRawState) {
    // Debounced state actually changed
    lastRawState = reading;

    if (lastRawState) {
      // Just pressed
      pressStartMs = now;
      longPressFired = false;
    } else {
      // Just released
      unsigned long heldMs = now - pressStartMs;
      if (!longPressFired && heldMs < BUTTON_LONGPRESS_MS) {
        RelayToggle();
        LedFlash();
      }
    }
  }

  // Fire the long-press action while still held, so the user gets
  // immediate feedback (fast LED blink) without waiting for release.
  if (lastRawState && !longPressFired && (now - pressStartMs) >= BUTTON_LONGPRESS_MS) {
    longPressFired = true;
    Serial.println(F("Button: long press detected, resetting WiFi settings..."));
    LedSetPattern(LED_BLINK_FAST);
    WifiFactoryReset();  // does not return - reboots the device
  }
}
