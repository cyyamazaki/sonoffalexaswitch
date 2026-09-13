/*
  Storage.ino - persistent settings in emulated EEPROM

  ESP8266's EEPROM library emulates EEPROM using one flash sector;
  nothing is actually written until EEPROM.commit() is called, so we
  only commit when a value really changes (same philosophy as
  Tasmota's throttled "SaveData" - avoid wearing out the flash).
*/

#include <EEPROM.h>
#include "Config.h"

void StorageBegin() {
  EEPROM.begin(EEPROM_SIZE);
}

void StorageResetToDefaults() {
  memset(&settings, 0, sizeof(settings));
  settings.magic = EEPROM_MAGIC;
  settings.relay_state = 0;
  settings.power_on_state = DEFAULT_POWER_ON_STATE;
  strncpy(settings.friendly_name, "Tomada", sizeof(settings.friendly_name) - 1);
  strncpy(settings.ota_password, OTA_PASSWORD_DEFAULT, sizeof(settings.ota_password) - 1);
}

void StorageLoad() {
  EEPROM.get(0, settings);
  if (settings.magic != EEPROM_MAGIC) {
    Serial.println(F("Storage: no valid settings found, loading defaults."));
    StorageResetToDefaults();
    StorageSave();
  } else {
    // Defensive null-termination in case of corrupt/garbage flash content
    settings.friendly_name[sizeof(settings.friendly_name) - 1] = '\0';
    settings.ota_password[sizeof(settings.ota_password) - 1] = '\0';
  }
}

void StorageSave() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}
