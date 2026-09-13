/*
  RelayControl.ino - relay output + persisted power-on behaviour
*/

#include "Config.h"

void RelayBegin() {
  pinMode(RELAY_PIN, OUTPUT);

  bool initial;
  switch ((PowerOnState)settings.power_on_state) {
    case POWER_ON_ALWAYS_OFF:
      initial = false;
      break;
    case POWER_ON_ALWAYS_ON:
      initial = true;
      break;
    case POWER_ON_TOGGLE_LAST:
      initial = !settings.relay_state;
      break;
    case POWER_ON_RESTORE_LAST:
    default:
      initial = settings.relay_state;
      break;
  }

  RelaySet(initial, true);
}

static void RelayWritePin(bool on) {
  bool level_high = RELAY_ACTIVE_HIGH ? on : !on;
  digitalWrite(RELAY_PIN, level_high ? HIGH : LOW);
}

void RelaySet(bool on, bool persist) {
  relayState = on;
  RelayWritePin(on);

  if (persist && settings.relay_state != (uint8_t)on) {
    settings.relay_state = (uint8_t)on;
    StorageSave();
  }

  Serial.printf("Relay: %s\n", on ? "ON" : "OFF");
}

void RelayToggle() {
  RelaySet(!relayState, true);
}

bool RelayGet() {
  return relayState;
}
