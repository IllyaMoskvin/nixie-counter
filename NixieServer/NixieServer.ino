// https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
// https://randomnerdtutorials.com/esp8266-web-server/
// https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
#include <NixieDriver_ESP.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

// https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

const unsigned long displayRefreshInterval = 20;
unsigned long displayRefreshedAt;

nixie_esp nixie(dataPin, clockPin);

// Set web server port number to 80
ESP8266WebServer server(80);

// Variable to store the HTTP request
String header;

// Minify, then replace " with \", and % with %% in CSS, but keep %s in form
const char* indexHtml = "<!DOCTYPE html><html><head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <link rel=\"icon\" href=\"data:,\"> <title>Nixie</title> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css\"/> <link href=\"https://fonts.googleapis.com/css?family=Nixie+One&display=swap\" rel=\"stylesheet\"> <style>*{box-sizing: border-box;}html{background: #ddd; font-family: sans-serif; text-align: center;}div{padding-top: 1.5em; background: #eee; border-top: 3px solid #ccc; border-bottom: 3px solid #ccc;}h1{padding: 2rem; font-size: 2rem; font-weight: 600; font-family: 'Nixie One', sans-serif;}label{display: block; margin-bottom: 0.5rem; margin-left: 1rem; text-align: left;}input[type=text]{width: 100%%; margin-bottom: 1.5rem; padding: .75rem 1rem; border: none; color: #555; font-size: .9rem;}input[type=submit]{cursor: pointer; margin: 1.5em; padding: .5rem 2rem; background: #636363; border: none; color: #fff; font-size: 1rem;}</style></head><body> <h1>Nixie</h1> <form action=\"/update\" method=\"get\"> <div> <label for=\"host\">Host</label> <input type=\"text\" name=\"host\" value=\"%s\" maxlength=\"39\"/> <label for=\"path\">Path</label> <input type=\"text\" name=\"path\" value=\"%s\" maxlength=\"254\"/> </div><input type=\"submit\" value=\"Update\"/> </form></body></html>";
const char* updateHtml = "<!DOCTYPE html><html><head> <meta http-equiv=\"refresh\" content=\"5; URL=/\"/> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <link rel=\"icon\" href=\"data:,\"> <title>Nixie</title> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css\"/> <link href=\"https://fonts.googleapis.com/css?family=Nixie+One&display=swap\" rel=\"stylesheet\"> <style>*{box-sizing: border-box;}html{background: #ddd; font-family: sans-serif; text-align: center;}h1{padding: 2rem; font-size: 2rem; font-weight: 600; font-family: 'Nixie One', sans-serif;}</style></head><body> <h1>%s</h1> <h2>Returning to form...</h2></body></html>";

// Generally, host could be up to 254 = 253 max for domain + 1 for null terminator
// https://webmasters.stackexchange.com/questions/16996/maximum-domain-name-length
const char apiHostDefault[40] = "nocache.aggregator-data.artic.edu";
const char apiPathDefault[255] = "/api/v1/artworks/search?cache=false&query[range][timestamp][gte]=now-1d";

// Config values stored in EEPROM, with defaults above
char apiHost[40];
char apiPath[255];

// Helpers for storing values in EEPROM
const char memEndDefined[2 + 1] = "OK";
char memEndCurrent[sizeof(memEndDefined)];

void setup()
{
  // Uncomment to see WiFiManager data:
  // Serial.setDebugOutput(true);
  Serial.begin(115200);

  // Set initial pin states
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);

  pinMode(oePin, OUTPUT);
  digitalWrite(oePin, HIGH);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();
  
  // Check "Network Connections > Wi-Fi > Properties > TCP/IPv4 > Properties"
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Nixie");
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  // Access this IP via HTTP to see the control panel
  Serial.println(WiFi.localIP());

  // Params persist between restarts
  loadParamsFromEEPROM();

  // Bind routes
  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.onNotFound(handleNotFound);
  
  server.begin();
}

void loop()
{
  server.handleClient();
}


void handleRoot()
{
  char temp[2000];
  snprintf(temp, sizeof(temp), indexHtml, apiHost, apiPath);
  server.send(200, "text/html", temp);
}

void handleUpdate()
{
  if (!server.hasArg("host") || !server.hasArg("path")) {
    return sendUpdate(400, "Missing param.");
  }

  const String & apiHostNew = server.arg("host");
  const String & apiPathNew = server.arg("path");

  if (apiHostNew == apiHost && apiPathNew == apiPath) {
    return sendUpdate(400, "Params not changed.");
  }

  if (apiHostNew.length() == 0 || apiPathNew.length() == 0) {
    return sendUpdate(400, "Param is empty.");
  }

  Serial.print("New host: ");
  Serial.println(apiHostNew);

  Serial.print("New path: ");
  Serial.println(apiPathNew);

  apiHostNew.toCharArray(apiHost, sizeof(apiHost));
  apiPathNew.toCharArray(apiPath, sizeof(apiPath));

  saveParamsToEEPROM();

  return sendUpdate(200, "Success.");
}

// https://majenko.co.uk/blog/evils-arduino-strings
void sendUpdate(const int code, const char *message)
{
  char temp[1000];
  snprintf(temp, sizeof(temp), updateHtml, message);
  server.send(code, "text/html", temp);
}

// https://github.com/esp8266/Arduino/blob/e9d052c/libraries/ESP8266WebServer/examples/HelloServer/HelloServer.ino#L24
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


// Functions to persist data between resets using EEPROM.
// https://arduino-esp8266.readthedocs.io/en/2.5.2/libraries.html#eeprom
// https://github.com/esp8266/Arduino/blob/4897e00/libraries/DNSServer/examples/CaptivePortalAdvanced/credentials.ino
// TODO: Cycle EEPROM start point?
// https://blog.hackster.io/eeprom-rotation-for-the-esp8266-and-esp32-e42403b11df0
// TODO: Use Filesystem instead?
// https://arduino-esp8266.readthedocs.io/en/2.5.2/filesystem.html

void loadParamsFromEEPROM()
{
  EEPROM.begin(sizeof(apiHost) + sizeof(apiPath) + sizeof(memEndCurrent));
  EEPROM.get(0, apiHost);
  EEPROM.get(0 + sizeof(apiHost), apiPath);
  EEPROM.get(0 + sizeof(apiHost) + sizeof(apiPath), memEndCurrent);
  EEPROM.end();

  // Uncomment to simulate EEPROM read fail:
  // strcpy(memEndCurrent, "NO");

  if (String(memEndCurrent) != String(memEndDefined)) {
    Serial.println("Failed to load params from EEPROM, using defaults...");
    strcpy (apiHost, apiHostDefault);
    strcpy (apiPath, apiPathDefault);
    saveParamsToEEPROM();
  } else {
    Serial.println("Loaded params from EEPROM");
  }

  Serial.print("Host: ");
  Serial.println(apiHost);

  Serial.print("Path: ");
  Serial.println(apiPath);
}

void saveParamsToEEPROM()
{
  EEPROM.begin(sizeof(apiHost) + sizeof(apiPath) + sizeof(memEndDefined));
  EEPROM.put(0, apiHost);
  EEPROM.put(0 + sizeof(apiHost), apiPath);
  EEPROM.put(0 + sizeof(apiHost) + sizeof(apiPath), memEndDefined);
  EEPROM.end();
}
