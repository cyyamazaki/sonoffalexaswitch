/*
  WebInterface.ino - local status page + a small HTTP API

  Gives you control even without Alexa: open http://<hostname>.local/
  (or the device's IP) from any browser on the same network. The
  same /api endpoints are handy for Home Assistant, Node-RED or a
  shortcut on your phone, similar in spirit to Tasmota's HTTP API.

  This same server instance also serves the WeMo/UPnP emulation
  handlers registered by WemoEmulation.ino (WemoRegisterHandlers()),
  exactly like Tasmota runs its emulation on its one and only web
  server instead of a second, separate one.
*/

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Config.h"

ESP8266WebServer server(WEBSERVER_PORT);

static void HandleRoot() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>");
  html += settings.friendly_name;
  html += F("</title><style>"
    "body{font-family:sans-serif;text-align:center;margin-top:10vh;background:#111;color:#eee}"
    "h1{font-weight:600}"
    "button{font-size:1.4em;padding:0.6em 1.4em;border-radius:10px;border:none;cursor:pointer}"
    ".on{background:#2ecc71;color:#063}"
    ".off{background:#444;color:#ccc}"
    "small{opacity:0.6}"
    "</style></head><body>");
  html += "<h1>" + String(settings.friendly_name) + "</h1>";
  html += "<p>Estado atual: <b>" + String(RelayGet() ? "LIGADO" : "DESLIGADO") + "</b></p>";
  html += "<form action='/toggle' method='POST'>";
  html += String("<button class='") + (RelayGet() ? "on" : "off") + "'>";
  html += RelayGet() ? "Desligar" : "Ligar";
  html += "</button></form>";
  html += "<p><small>" + String(FW_NAME) + " v" + FW_VERSION + " &middot; " + WifiGetHostname() + ".local</small></p>";
  html += F("</body></html>");
  server.send(200, "text/html", html);
}

static void HandleToggle() {
  RelayToggle();
  LedFlash();
  server.sendHeader("Location", "/");
  server.send(303);
}

static void HandleApiState() {
  String json = String("{\"name\":\"") + settings.friendly_name +
                "\",\"state\":" + (RelayGet() ? "true" : "false") +
                ",\"hostname\":\"" + WifiGetHostname() +
                "\",\"firmware\":\"" + FW_VERSION + "\"}";
  server.send(200, "application/json", json);
}

static void HandleApiOn() {
  RelaySet(true, true);
  LedFlash();
  server.send(200, "application/json", "{\"state\":true}");
}

static void HandleApiOff() {
  RelaySet(false, true);
  LedFlash();
  server.send(200, "application/json", "{\"state\":false}");
}

static void HandleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void WebBegin() {
  server.on("/", HTTP_GET, HandleRoot);
  server.on("/toggle", HTTP_POST, HandleToggle);
  server.on("/api/state", HTTP_GET, HandleApiState);
  server.on("/api/on", HTTP_POST, HandleApiOn);
  server.on("/api/off", HTTP_POST, HandleApiOff);
  server.onNotFound(HandleNotFound);

  WemoRegisterHandlers();  // adds /setup.xml, /eventservice.xml, etc. to this same server

  server.begin();

  MDNS.addService("http", "tcp", WEBSERVER_PORT);

  Serial.print(F("Web UI ready: http://"));
  Serial.print(WifiGetHostname());
  Serial.println(F(".local/"));
}

void WebLoop() {
  server.handleClient();
}
