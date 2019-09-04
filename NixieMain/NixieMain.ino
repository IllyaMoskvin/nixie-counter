// ====================================================================================================================
// Libraries.
// ====================================================================================================================

// https://doayee.co.uk/nixie/library/
#include <NixieDriver_ESP.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

// https://arduinojson.org/v6/example/http-client/
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>


// ====================================================================================================================
// Error codes. Displayed left-aligned, filled with BLANK.
// ====================================================================================================================

const int ERROR_CONNECTION_FAILED = 1; // cannot connect to API host via port
const int ERROR_REQUEST_FAILED = 2; // cannot send request to API
const int ERROR_RESPONSE_UNEXPECTED = 3; // first line of API response was not HTTP/1.1 200 OK
const int ERROR_RESPONSE_INVALID = 4; // cannot find the end of headers in API response
const int ERROR_PARSE_JSON = 5; // cannot parse valid json from API response


// ====================================================================================================================
// Global variables.
// ====================================================================================================================

// Stack of digits in each tube, used to determine digit depth for animations
const int nixieDigitStack[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};

// Pinouts
const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;
const int btnPin = 13;

nixie_esp nixie(dataPin, clockPin);

// Serve the config webserver via normal HTTP
ESP8266WebServer server(80);

// Minify, then replace " with \", and % with %% in CSS, but keep %s in form
const char* indexHtml = "<!DOCTYPE html><html><head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <link rel=\"icon\" href=\"data:,\"> <title>Nixie</title> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css\"/> <link href=\"https://fonts.googleapis.com/css?family=Nixie+One&display=swap\" rel=\"stylesheet\"> <style>*{box-sizing: border-box;}html{background: #ddd; font-family: sans-serif; text-align: center;}div{padding-top: 1.5em; background: #eee; border-top: 3px solid #ccc; border-bottom: 3px solid #ccc;}h1{padding: 2rem; font-size: 2rem; font-weight: 600; font-family: 'Nixie One', sans-serif;}label{display: block; margin-bottom: 0.5rem; margin-left: 1rem; text-align: left;}input[type=text]{width: 100%%; margin-bottom: 1.5rem; padding: .75rem 1rem; border: none; color: #555; font-size: .9rem;}input[type=submit]{cursor: pointer; margin: 1.5em; padding: .5rem 2rem; background: #636363; border: none; color: #fff; font-size: 1rem;}</style></head><body> <h1>Nixie</h1> <form action=\"/update\" method=\"get\"> <div> <label for=\"host\">Host</label> <input type=\"text\" name=\"host\" value=\"%s\" maxlength=\"39\" required/> <label for=\"path\">Path</label> <input type=\"text\" name=\"path\" value=\"%s\" maxlength=\"254\" required/> </div><input type=\"submit\" value=\"Update\"/> </form></body></html>";
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

// All outbound requests to API use HTTPS
const int apiPort = 443;

int currentDots[] = {0, 0, 0, 0, 0, 0};
int currentDigits[] = {0, 0, 0, 0, 0, 0};
int nextDigits[] = {0, 0, 0, 0, 0, 0};

const unsigned long numberSetInterval = 5000; // how often to query the API
const unsigned long numberUpdateInterval = 75; // time between each frame of animation
const unsigned long displayRefreshInterval = 20; // minor throttle to prevent flicker

unsigned long numberSetAt;
unsigned long numberUpdatedAt;
unsigned long displayRefreshedAt;



// ====================================================================================================================
// Function declarations for compiler weirdness.
// ====================================================================================================================

void setup();
void loop();
void initWiFiManager();
void configModeCallback (WiFiManager *myWiFiManager);
void saveConfigCallback();
void handleRoot();
void handleUpdate();
void sendUpdate(const int code, const char *message);
void handleNotFound();
void loadParamsFromEEPROM();
void saveParamsFromEEPROM();
void throwError(const int errorCode);
void setNextNumber();
void setCurrentNumber();
bool isCurrentNumberIdenticalToNextNumber();
void getDigitDepths(int digits[], int digitDepths[]);
int getDigitDepth(int value);
void setDigitsFromNumber(long num, int seg[]);
void nixieDisplay(long num);
void nixieDisplay(int seg[]);


// ====================================================================================================================
// Main program.
// ====================================================================================================================

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

  pinMode(btnPin, INPUT_PULLUP);

  // Run WiFiManager in autoconnect mode
  initWiFiManager(true);

  // Access this IP via HTTP to see the control panel
  Serial.println(WiFi.localIP().toString());

  // Params persist between restarts
  loadParamsFromEEPROM();

  // Bind routes
  server.on(F("/"), handleRoot);
  server.on(F("/update"), handleUpdate);
  server.onNotFound(handleNotFound);

  server.begin();
}

void loop()
{
  server.handleClient();

  if (millis() - numberSetAt > numberSetInterval) {
    setNextNumber();
    numberSetAt = millis();

    // Uncomment to see remaining memory:
    Serial.println(ESP.getFreeHeap());
  }

  if (millis() - numberUpdatedAt > numberUpdateInterval) {
    setCurrentNumber();
    numberUpdatedAt = millis();
  }

  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    nixieDisplay(currentDigits);
    displayRefreshedAt = millis();
  }
}


// ====================================================================================================================
// Run WiFiManager to define which WiFi network to use.
// ====================================================================================================================

void initWiFiManager(const bool shouldAutoConnect)
{
  // Local intialization: once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Uncomment and run once, if you want to erase stored WiFi credentials
  // wifiManager.resetSettings();

  // Check "Network Connections > Wi-Fi > Properties > TCP/IPv4 > Properties"
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Fetches ssid and pass for WiFi from EEPROM
  // For auto connect, config portal is not persistent if WiFi credentials are valid
  // Goes into a blocking loop until its config form is submitted
  if (shouldAutoConnect) {
    wifiManager.autoConnect("Nixie");
  } else {
    wifiManager.startConfigPortal("Nixie");
  }

  // If you get here, you have connected to the WiFi
  Serial.println(F("Connected."));
}

void configModeCallback (WiFiManager *myWiFiManager)
{
  nixie.setDecimalPoint(2, true);
  nixie.setDecimalPoint(3, true);
  nixie.setDecimalPoint(4, true);
  nixie.displayDigits(BLANK, 1, 0, 0, 0, 1);
}

void saveConfigCallback()
{
  nixie.setDecimalPoint(2, false);
  nixie.setDecimalPoint(3, false);
  nixie.setDecimalPoint(4, false);
}


// ====================================================================================================================
// Handle ESP8266WebServer requests to configure API queries.
// ====================================================================================================================

void handleRoot()
{
  char temp[2000];
  snprintf(temp, sizeof(temp), indexHtml, apiHost, apiPath);
  server.send(200, F("text/html"), temp);
}

void handleUpdate()
{
  if (!server.hasArg(F("host")) || !server.hasArg(F("path"))) {
    return sendUpdate(400, "Missing param.");
  }

  const String & apiHostNew = server.arg(F("host"));
  const String & apiPathNew = server.arg(F("path"));

  if (apiHostNew == apiHost && apiPathNew == apiPath) {
    return sendUpdate(400, "Params not changed.");
  }

  if (apiHostNew.length() == 0 || apiPathNew.length() == 0) {
    return sendUpdate(400, "Param is empty.");
  }

  Serial.print(F("New host: "));
  Serial.println(apiHostNew);

  Serial.print(F("New path: "));
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
  server.send(code, F("text/html"), temp);
}

// https://github.com/esp8266/Arduino/blob/e9d052c/libraries/ESP8266WebServer/examples/HelloServer/HelloServer.ino#L24
void handleNotFound()
{
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, F("text/plain"), message);
}


// ====================================================================================================================
// Persist data between resets using EEPROM.
// ====================================================================================================================

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
    Serial.println(F("Failed to load params from EEPROM, using defaults..."));
    strcpy (apiHost, apiHostDefault);
    strcpy (apiPath, apiPathDefault);
    saveParamsToEEPROM();
  } else {
    Serial.println(F("Loaded params from EEPROM"));
  }

  Serial.print(F("Host: "));
  Serial.println(apiHost);

  Serial.print(F("Path: "));
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


// ====================================================================================================================
// Error handling.
// ====================================================================================================================
void throwError(const int errorCode)
{
  const int errorDigits[] = {errorCode, BLANK, BLANK, BLANK, BLANK, BLANK};
  memcpy(currentDigits, errorDigits, sizeof(errorDigits));
  memcpy(nextDigits, errorDigits, sizeof(errorDigits));
}

// ====================================================================================================================
// Query API for next number.
// ====================================================================================================================

void setNextNumber()
{
  WiFiClientSecure client;

  // Turn off certificate and/or fingerprint checking
  client.setInsecure();

  if (!client.connect(apiHost, apiPort)) {
    return throwError(ERROR_CONNECTION_FAILED);
  }

  // https://arduinojson.org/v6/example/http-client/
  client.println(String("GET ") + apiPath + " HTTP/1.1");
  client.println(String("Host: ") + apiHost);
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    client.stop();
    return throwError(ERROR_REQUEST_FAILED);
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    client.stop();
    return throwError(ERROR_RESPONSE_UNEXPECTED);
  }

  if (!client.find("\r\n\r\n")) {
    client.stop();
    return throwError(ERROR_RESPONSE_INVALID);
  }

  DynamicJsonDocument doc(250);

  // May require trailing \n in body to avoid delay:
  // https://forum.arduino.cc/index.php?topic=529440.30
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    client.stop();
    return throwError(ERROR_PARSE_JSON);
  }

  long total = doc[F("pagination")][F("total")];

  client.stop();

  setDigitsFromNumber(total, nextDigits);
}


// ====================================================================================================================
// Update current number for the next time the display refreshes. Animate transition.
// ====================================================================================================================

void setCurrentNumber()
{
  if (isCurrentNumberIdenticalToNextNumber()) {
    return;
  }

  int currentDigitDepths[6];
  int nextDigitDepths[6];

  getDigitDepths(currentDigits, currentDigitDepths);
  getDigitDepths(nextDigits, nextDigitDepths);

  int signOfChange;

  int resultDigits[6];

  int i; // Save this so we know when we called break

  for (i = 0; i < 6; i++) {
    if (currentDigits[i] == BLANK && nextDigits[i] != BLANK) {
      resultDigits[i] = nixieDigitStack[0];
      break;
    }

    if (currentDigits[i] != BLANK && nextDigits[i] == BLANK) {
      if (currentDigitDepths[i] == 0) {
        resultDigits[i] = BLANK;
        break;
      }

      resultDigits[i] = nixieDigitStack[currentDigitDepths[i] - 1];
      break;
    }

    if (currentDigits[i] != nextDigits[i]) {
      signOfChange = nextDigitDepths[i] < currentDigitDepths[i] ? -1 : 1;
      resultDigits[i] = nixieDigitStack[currentDigitDepths[i] + signOfChange];
      break;
    }

    resultDigits[i] = currentDigits[i];
  }

  // Fill out digits unfilled due to breaking
  for (i = i + 1; i < 6; i++) {
    resultDigits[i] = currentDigits[i];
  }

  memcpy(currentDigits, resultDigits, sizeof(resultDigits));
}

bool isCurrentNumberIdenticalToNextNumber()
{
  bool isIdentical = true;

  for (int i = 0; i < 6; i++) {
    if (currentDigits[i] != nextDigits[i]) {
      isIdentical = false;
      break;
    }
  }

  return isIdentical;
}

void getDigitDepths(int digits[], int digitDepths[])
{
  for (byte i = 0; i < 6; i++) {
    digitDepths[i] = getDigitDepth(digits[i]);
  }
}

int getDigitDepth(int value)
{
  for (int i = 0; i < 10; i++) {
    if (nixieDigitStack[i] == value) {
      return i;
    }
  }

  return BLANK;
}

void setDigitsFromNumber(long number, int digits[])
{
  digits[0] = number / 100000;
  digits[1] = (number / 10000) % 10;
  digits[2] = (number / 1000) % 10;
  digits[3] = (number / 100) % 10;
  digits[4] = (number / 10) % 10;
  digits[5] = (number % 10);

  if (number < 10) {
    digits[0] = BLANK;
    digits[1] = BLANK;
    digits[2] = BLANK;
    digits[3] = BLANK;
    digits[4] = BLANK;
  } else if (number < 100) {
    digits[0] = BLANK;
    digits[1] = BLANK;
    digits[2] = BLANK;
    digits[3] = BLANK;
  } else if (number < 1000) {
    digits[0] = BLANK;
    digits[1] = BLANK;
    digits[2] = BLANK;
  } else if (number < 10000) {
    digits[0] = BLANK;
    digits[1] = BLANK;
  } else if (number < 100000) {
    digits[0] = BLANK;
  }
}

void nixieDisplay(long number)
{
  int digits[6];
  setDigitsFromNumber(number, digits);
  nixieDisplay(digits);
}

void nixieDisplay(int digits[])
{
  nixie.displayDigits(digits[0], digits[1], digits[2], digits[3], digits[4], digits[5]);
}
