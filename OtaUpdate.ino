/*
  OtaUpdate.ino - wireless firmware updates (ArduinoOTA)

  Lets you push new firmware from the Arduino IDE (Sketch > Upload
  Using Programmer is not needed; the board shows up under
  Tools > Port as a network port) once the device is on your WiFi,
  no USB cable required after the very first flash.

  ArduinoOTA.begin() also starts the mDNS responder for the chosen
  hostname; ArduinoOTA.handle() services both OTA and mDNS every
  loop, so nothing else needs to call MDNS.update() separately.
*/

#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "Config.h"

void OtaBegin() {
  ArduinoOTA.setHostname(WifiGetHostname().c_str());

  if (strlen(settings.ota_password) > 0) {
    ArduinoOTA.setPassword(settings.ota_password);
  }

  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA: update starting..."));
    LedSetPattern(LED_BLINK_FAST);
    // Make sure the relay is left in a safe, known state while the
    // MCU is busy flashing (avoid it dangling mid-toggle).
    RelaySet(RelayGet(), false);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("OTA: update complete, rebooting."));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error [%u]\n", error);
    LedSetPattern(LED_OFF);
  });

  ArduinoOTA.begin();

  Serial.print(F("OTA ready, hostname: "));
  Serial.println(WifiGetHostname());
}

void OtaLoop() {
  ArduinoOTA.handle();
}
