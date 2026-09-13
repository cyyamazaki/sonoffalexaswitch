/*
  WemoEmulation.ino - Alexa/Echo discovery and control, Tasmota-style

  This intentionally does NOT use a third-party library (like
  fauxmoESP). After the fauxmoESP-based version of this sketch turned
  out unreliable in practice, this was rewritten to mirror how
  Tasmota's own, long-proven "Wemo emulation" actually works
  (tasmota/tasmota_xdrv_driver/xdrv_21_wemo.ino +
  tasmota/tasmota_support/support_udp.ino), which is the most likely
  reason it holds up better over days/weeks of real use:

    1. It shares the SAME web server already running for the local
       status page (WebInterface.ino) instead of opening a second,
       separate HTTP server. One less server, one less thing that can
       silently stop working or fight over resources.

    2. The SSDP UDP multicast subscription is explicitly torn down
       and rebuilt every time WiFi goes down and comes back up
       (WemoNetworkDown() / WemoNetworkUp(), called from WifiSetup.ino).
       This is the actual fix for the most common real-world failure
       mode of this kind of emulation: after a WiFi drop/reconnect the
       device gets a new IP and the previous multicast group
       membership can go stale, so Alexa's "Alexa, descubra novos
       dispositivos" (or its periodic background re-discovery) quietly
       stops finding the device until it's power-cycled. Tasmota's own
       code logs this exact event as "Multicast (re)joined" - this
       sketch does the same thing, just under a different name.

  Honesty note on what could NOT be verified here: Tasmota also calls
  a low-level lwIP function (igmp_joingroup) as an extra belt-and-
  suspenders step before WiFiUDP::beginMulticast(). Its exact
  signature depends on the installed ESP8266 core version, and this
  sandbox has no network access to the real ESP8266 core headers to
  confirm it - so rather than guess and risk a wrong low-level call,
  this file sticks to the stable, publicly documented WiFiUDP API and
  compensates with a full stop()+rebuild on every reconnect plus a
  periodic self-check (WEMO below), which is the same net effect
  (drop and recreate the multicast membership) even if it goes through
  one fewer internal step. If you still see missed discoveries after
  this change, that igmp_joingroup call is the next thing to add -
  see README.md.
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "Config.h"

static WiFiUDP ssdpUdp;
static bool ssdpConnected = false;
static unsigned long lastRejoinAttemptMs = 0;
static unsigned long lastMSearchMs = 0;

static IPAddress remoteIp;
static uint16_t remotePort = 0;

static String wemoUuid;
static String wemoSerial;

static const IPAddress SSDP_MCAST_ADDR(SSDP_MULTICAST_IP_0, SSDP_MULTICAST_IP_1,
                                        SSDP_MULTICAST_IP_2, SSDP_MULTICAST_IP_3);

// ------------------------------------------------------------------
// Identity
// ------------------------------------------------------------------
static void WemoEnsureIdentity() {
  if (wemoUuid.length()) return;  // already computed
  char buf[24];
  snprintf(buf, sizeof(buf), "201612K%08X", ESP.getChipId());
  wemoSerial = String(buf);
  wemoUuid = "Socket-1_0-" + wemoSerial;
}

// ------------------------------------------------------------------
// SSDP multicast (re)join / leave
// ------------------------------------------------------------------
void WemoNetworkUp() {
  if (ssdpConnected) return;
  if (!WifiIsConnected()) return;

  ssdpUdp.stop();  // make sure we start from a clean socket, not a stale one

  if (ssdpUdp.beginMulticast(WiFi.localIP(), SSDP_MCAST_ADDR, SSDP_PORT)) {
    ssdpConnected = true;
    lastRejoinAttemptMs = millis();
    Serial.println(F("Wemo: SSDP multicast (re)joined"));
  } else {
    Serial.println(F("Wemo: SSDP multicast join failed, will retry"));
  }
}

void WemoNetworkDown() {
  if (!ssdpConnected) return;
  ssdpUdp.stop();
  ssdpConnected = false;
  Serial.println(F("Wemo: SSDP multicast left (network down)"));
}

// ------------------------------------------------------------------
// M-SEARCH handling
// ------------------------------------------------------------------
static void ToLowerInPlace(char *s) {
  for (; *s; s++) *s = tolower((unsigned char)*s);
}

static void RespondToMSearch(bool rootDeviceStyle) {
  IPAddress ip = WiFi.localIP();
  const char *searchTarget = rootDeviceStyle ? "upnp:rootdevice" : "urn:Belkin:device:**";

  char response[420];
  snprintf(response, sizeof(response),
    "HTTP/1.1 200 OK\r\n"
    "CACHE-CONTROL: max-age=86400\r\n"
    "EXT:\r\n"
    "LOCATION: http://%d.%d.%d.%d:80/setup.xml\r\n"
    "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
    "01-NLS: %s\r\n"
    "SERVER: Arduino/1.0 UPnP/1.0 %s/%s\r\n"
    "ST: %s\r\n"
    "USN: uuid:%s::%s\r\n"
    "X-User-Agent: redsonic\r\n"
    "\r\n",
    ip[0], ip[1], ip[2], ip[3],
    wemoUuid.c_str(),
    FW_NAME, FW_VERSION,
    searchTarget,
    wemoUuid.c_str(), searchTarget);

  if (ssdpUdp.beginPacket(remoteIp, remotePort)) {
    ssdpUdp.write((const uint8_t *)response, strlen(response));
    ssdpUdp.endPacket();
    Serial.printf("Wemo: M-SEARCH answered (%s) -> %s:%u\n",
                  searchTarget, remoteIp.toString().c_str(), remotePort);
  }
}

void WemoLoop() {
  unsigned long now = millis();

  // Self-healing: if we're supposed to be connected but somehow
  // aren't (a beginMulticast() attempt failed earlier, or the WiFi
  // event was missed), keep retrying in the background instead of
  // requiring a reboot.
  if (!ssdpConnected) {
    if (WifiIsConnected() && (now - lastRejoinAttemptMs >= SSDP_REJOIN_RETRY_MS)) {
      lastRejoinAttemptMs = now;
      WemoNetworkUp();
    }
    return;
  }

  int packetSize = ssdpUdp.parsePacket();
  if (packetSize <= 0) return;

  char buf[160];
  int len = ssdpUdp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  if (!strstr(buf, "M-SEARCH")) return;

  // Debounce: routers/multicast reflection can deliver the same
  // M-SEARCH burst more than once in quick succession.
  if (now - lastMSearchMs < SSDP_MSEARCH_DEBOUNCE_MS) return;
  lastMSearchMs = now;

  remoteIp = ssdpUdp.remoteIP();
  remotePort = ssdpUdp.remotePort();

  char lower[160];
  strncpy(lower, buf, sizeof(lower) - 1);
  lower[sizeof(lower) - 1] = '\0';
  ToLowerInPlace(lower);

  if (strstr(lower, "urn:belkin:device")) {
    RespondToMSearch(false);
  } else if (strstr(lower, "upnp:rootdevice") || strstr(lower, "ssdp:all")) {
    RespondToMSearch(true);
  }
}

// ------------------------------------------------------------------
// HTTP side: served from the same ESP8266WebServer as the local UI
// ------------------------------------------------------------------
static void HandleSetupXml() {
  WemoEnsureIdentity();
  IPAddress ip = WiFi.localIP();

  String xml =
    "<?xml version=\"1.0\"?>"
    "<root xmlns=\"urn:Belkin:device-1-0\">"
      "<device>"
        "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
        "<friendlyName>" + String(settings.friendly_name) + "</friendlyName>"
        "<manufacturer>Belkin International Inc.</manufacturer>"
        "<modelName>Socket</modelName>"
        "<modelNumber>3.1415</modelNumber>"
        "<UDN>uuid:" + wemoUuid + "</UDN>"
        "<serialNumber>" + wemoSerial + "</serialNumber>"
        "<presentationURL>http://" + ip.toString() + ":80/</presentationURL>"
        "<binaryState>" + String(RelayGet() ? 1 : 0) + "</binaryState>"
        "<serviceList>"
          "<service>"
            "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
            "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
            "<controlURL>/upnp/control/basicevent1</controlURL>"
            "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
            "<SCPDURL>/eventservice.xml</SCPDURL>"
          "</service>"
        "</serviceList>"
      "</device>"
    "</root>\r\n";

  server.send(200, "text/xml", xml);
}

static void HandleEventServiceXml() {
  const char xml[] =
    "<scpd xmlns=\"urn:Belkin:service-1-0\">"
      "<actionList>"
        "<action><name>SetBinaryState</name></action>"
        "<action><name>GetBinaryState</name></action>"
      "</actionList>"
      "<serviceStateTable>"
        "<stateVariable sendEvents=\"yes\"><name>BinaryState</name><dataType>bool</dataType><defaultValue>0</defaultValue></stateVariable>"
      "</serviceStateTable>"
    "</scpd>\r\n\r\n";
  server.send(200, "text/xml", xml);
}

static void HandleBasicEvent() {
  WemoEnsureIdentity();
  String body = server.hasArg("plain") ? server.arg("plain") : server.arg(0);

  char state = 'G';  // Get by default
  if (body.indexOf("SetBinaryState") >= 0) {
    state = 'S';
    if (body.indexOf("State>1</Binary") >= 0) {
      RelaySet(true, true);
      LedFlash();
    } else if (body.indexOf("State>0</Binary") >= 0) {
      RelaySet(false, true);
      LedFlash();
    }
  }

  char resp[340];
  snprintf(resp, sizeof(resp),
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
    "<s:Body><u:%cetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">"
    "<BinaryState>%d</BinaryState>"
    "</u:%cetBinaryStateResponse></s:Body></s:Envelope>\r\n",
    state, RelayGet() ? 1 : 0, state);

  server.send(200, "text/xml", resp);
}

void WemoRegisterHandlers() {
  WemoEnsureIdentity();
  server.on("/setup.xml", HTTP_GET, HandleSetupXml);
  server.on("/eventservice.xml", HTTP_GET, HandleEventServiceXml);
  server.on("/upnp/control/basicevent1", HTTP_POST, HandleBasicEvent);

  Serial.print(F("Wemo: registered as \""));
  Serial.print(settings.friendly_name);
  Serial.println(F("\" - say \"Alexa, descubra novos dispositivos\" once."));
}
