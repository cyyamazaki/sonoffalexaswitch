/*
  StatusLed.ino - non-blocking status LED patterns

  All timing uses millis() deltas rather than delay(), so the LED
  can blink while everything else (WiFi, OTA, web server, Alexa,
  button) keeps running every loop() cycle.
*/

#include "Config.h"

static LedPattern currentPattern = LED_OFF;
static unsigned long lastToggleMs = 0;
static bool ledOn = false;

// One-shot "flash" overlay (e.g. acknowledge a command) that
// temporarily interrupts whatever pattern is showing.
static bool flashActive = false;
static unsigned long flashStartMs = 0;
static const unsigned long FLASH_DURATION_MS = 120;

static void LedWrite(bool on) {
  ledOn = on;
  bool level_low = LED_ACTIVE_LOW ? !on : on;
  digitalWrite(LED_PIN, level_low ? LOW : HIGH);
}

void LedBegin() {
  pinMode(LED_PIN, OUTPUT);
  LedWrite(false);
}

void LedSetPattern(LedPattern pattern) {
  currentPattern = pattern;
  lastToggleMs = millis();
}

void LedFlash() {
  flashActive = true;
  flashStartMs = millis();
  LedWrite(true);
}

void LedLoop() {
  unsigned long now = millis();

  if (flashActive) {
    if (now - flashStartMs >= FLASH_DURATION_MS) {
      flashActive = false;
      // fall through and let the regular pattern repaint below
    } else {
      return;  // flash still showing, don't let the pattern interfere
    }
  }

  switch (currentPattern) {
    case LED_OFF:
      LedWrite(false);
      break;

    case LED_SOLID_ON:
      LedWrite(true);
      break;

    case LED_BLINK_SLOW:
      if (now - lastToggleMs >= 600) {
        lastToggleMs = now;
        LedWrite(!ledOn);
      }
      break;

    case LED_BLINK_FAST:
      if (now - lastToggleMs >= 150) {
        lastToggleMs = now;
        LedWrite(!ledOn);
      }
      break;

    case LED_FLASH_ONCE:
      // handled via LedFlash(); nothing to sustain here
      break;
  }
}
