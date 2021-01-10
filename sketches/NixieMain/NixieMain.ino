// ====================================================================================================================
// Libraries.
// ====================================================================================================================

// https://github.com/PatrickGlatz/OneButton
#include <OneButton.h>

// https://doayee.co.uk/nixie/library/
#include <NixieDriver_ESP.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <WiFiClient.h>

// https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>


// ====================================================================================================================
// Error codes. Displayed left-aligned, filled with BLANK.
// ====================================================================================================================

const byte ERROR_CONNECTION_FAILED = 1; // cannot connect to API host via port
const byte ERROR_REQUEST_FAILED = 2; // cannot send request to API
const byte ERROR_RESPONSE_UNEXPECTED = 3; // first line of API response was not HTTP/1.1 200 OK
const byte ERROR_RESPONSE_INVALID = 4; // cannot find the end of headers in API response


// ====================================================================================================================
// Global state machine.
// ====================================================================================================================

const byte STATE_DEFAULT = 0;
const byte STATE_CYCLE = 1;
const byte STATE_WEBSERVER = 2;

byte currentState = STATE_DEFAULT;


// ====================================================================================================================
// Global variables.
// ====================================================================================================================

// Stack of digits in each tube, used to determine digit depth for animations
const byte nixieDigitStack[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};
const byte nixieDigitStackBottom = 1;
const byte nixieDigitStackTop = 3;

// Pinouts
const byte dataPin = 12;
const byte clockPin = 14;
const byte oePin = 16;
const byte btnPin = 13;

// https://github.com/PatrickGlatz/OneButton/blob/a7e4bdc/src/OneButton.cpp#L33
OneButton button(btnPin, true, true);

nixie_esp nixie(dataPin, clockPin);

// WiFiManager starts network with this name
const char* WIFI_AP_SSID PROGMEM = "Nixie";

// Serve the config webserver via normal HTTP
ESP8266WebServer server(80);

// Minify, then replace " with \", and % with %% in CSS, but keep %s in form
// https://forum.arduino.cc/index.php?topic=293408.0
// https://www.arduino.cc/reference/en/language/variables/utilities/progmem/
const char* indexHtml PROGMEM = "<!doctype html><html lang=en><meta name=viewport content='width=device-width,initial-scale=1'><link rel=icon href=data:,><title>Nixie</title><link rel=stylesheet href=https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css><link rel=stylesheet href='https://fonts.googleapis.com/css?family=Nixie+One&display=swap'><style>*{box-sizing:border-box}html{background:#ddd;font-family:sans-serif;text-align:center}div{padding-top:1.5em;background:#eee;border-top:3px solid #ccc;border-bottom:3px solid #ccc}h1{padding:2rem;font-size:2rem;font-weight:600;font-family:'Nixie One',sans-serif}div{text-align:left}label{display:block;margin:0 0 .5rem 1rem}input[type=number],input[type=text]{width:100%%;margin-bottom:1.5rem;padding:.75rem 1rem;border:none;color:#555;font-size:.9rem}input[type=submit]{cursor:pointer;margin:1.5em;padding:.5rem 2rem;background:#636363;border:none;color:#fff;font-size:1rem}</style><h1>Nixie</h1><form action=/update method=get><div><label for=host>Host</label> <input type=text name=host id=host value=%s maxlength=39 required> <label for=port>Port</label> <input type=number name=port id=port value=%s min=1 max=65535 required> <label for=path>Path</label> <input type=text name=path id=path value=%s maxlength=254 required></div><input type=submit value=Update></form>";
const char* updateHtml PROGMEM = "<!doctype html><html lang=en><meta http-equiv=refresh content='5; URL=/'><meta name=viewport content='width=device-width,initial-scale=1'><link rel=icon href=data:,><title>Nixie</title><link rel=stylesheet href=https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css><link rel=stylesheet href='https://fonts.googleapis.com/css?family=Nixie+One&display=swap'><style>*{box-sizing:border-box}html{background:#ddd;font-family:sans-serif;text-align:center}h1{padding:2rem;font-size:2rem;font-weight:600;font-family:'Nixie One',sans-serif}</style><h1>%S</h1><h2>Returning to form...</h2>";

// Keep this global to avoid risking giant memory holes
// TODO; https://github.com/me-no-dev/ESPAsyncWebServer
char bufferHtml[2000];

// Generally, host could be up to 254 = 253 max for domain + 1 for null terminator
// https://webmasters.stackexchange.com/questions/16996/maximum-domain-name-length
const char apiHostDefault[40] PROGMEM = "10.0.0.42";
const char apiPathDefault[255] PROGMEM = "/";
const unsigned int apiPortDefault PROGMEM = 4224;

// Config values stored in EEPROM, with defaults above
char apiHost[40];
char apiPath[255];
unsigned int apiPort;

// Helpers for storing values in EEPROM
const char memEndDefined[2 + 1] = "OK";
char memEndCurrent[sizeof(memEndDefined)];

byte currentDots[] = {0, 0, 0, 0, 0, 0};
byte currentDigits[] = {BLANK, 8, 3, 7, 7, 0};
byte nextDigits[] = {BLANK, 8, 3, 7, 7, 0};
byte savedDigits[] = {0, 0, 0, 0, 0, 0};

const unsigned long numberSetInterval = 5000; // how often to query the API
const unsigned long numberUpdateInterval = 75; // time between each frame of animation
const unsigned long displayRefreshInterval = 20; // minor throttle to prevent flicker

unsigned long numberSetAt;
unsigned long numberUpdatedAt;
unsigned long displayRefreshedAt;


// ====================================================================================================================
// Global variables for digit cycle state to prevent cathode poisoning.
// ====================================================================================================================

const byte CYCLE_BEGIN = 0; // current number to blank
const byte CYCLE_ACTIVE = 1; // moving wave across
const byte CYCLE_END = 2; // exit cycle state

byte cycleState = CYCLE_BEGIN;
byte cycleOffset = 0;

// TODO: Make this definable via webserver and store in EEPROM
unsigned long cycleBeginInterval = 900000; // how often to enter cycle mode (15 min)
unsigned long cycleBeganAt;


// ====================================================================================================================
// Global variables for IP scroll state.
// ====================================================================================================================

// IPv4: 4 bytes with 3 chars max + 3 dots + nullterm, or '(IP unset)'
char currentIP[16];
char oldIP[16];

const byte IP_SCROLL_REST_LEFT = 0;
const byte IP_SCROLL_MOVE_LEFT = 1;
const byte IP_SCROLL_REST_RIGHT = 2;
const byte IP_SCROLL_MOVE_RIGHT = 3;

bool ipShouldScroll = false;
byte ipScrollState = IP_SCROLL_REST_LEFT;
bool ipIsScrollResting = true;
byte ipScrollOffset = 0;

byte ipDigitCount = 0;
byte ipCharCount = 0;

const unsigned long ipCheckInterval = 10000;
const unsigned long ipScrollRestInterval = 2500; // how long to rest at each end of the ip
const unsigned long ipScrollTickInterval = 125; // how long to rest at each offset

unsigned long ipCheckedAt;
unsigned long ipScrollUpdatedAt;


// ====================================================================================================================
// Function declarations for compiler weirdness.
// ====================================================================================================================

void setup();
void loop();
void loopStateDefault();
void loopStateCycle();
void loopStateWebserver();
void handleClick();
void handleDoubleClick();
void handleTripleClick();
void initWiFiManager();
void configModeCallback (WiFiManager *myWiFiManager);
void saveConfigCallback();
void beginCycle();
void setCurrentNumberForCycle();
void checkIP();
void scrollIP();
void displayIP();
void handleRoot();
void handleUpdate();
void sendUpdate(const int code, const char *message);
void handleNotFound();
void loadParamsFromEEPROM();
void saveParamsFromEEPROM();
void throwError(const byte errorCode);
void setNextNumber();
void setCurrentNumber();
bool isCurrentNumberIdenticalToNextNumber();
void getDigitDepths(byte digits[], byte digitDepths[]);
byte getDigitDepth(byte value);
void setDigitsFromNumber(long number, byte digits[]);
void refreshDisplay();
void refreshDisplayDuringButtonPress();


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

  // For safety, enable internal pull-up manually
  // https://github.com/mathertel/OneButton/issues/51
  pinMode(btnPin, INPUT_PULLUP);

  // When webserver is active, try accessing http://nixie.local
  WiFi.hostname(F("Nixie"));

  // Say "hello" in l33t
  refreshDisplay();

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

  // Allow input last
  button.attachClick(handleClick);
  button.attachDoubleClick(handleDoubleClick);
  button.attachTripleClick(handleTripleClick);
}

void loop()
{
  button.tick();

  switch (currentState) {
    case STATE_DEFAULT:
      loopStateDefault();
      break;
    case STATE_CYCLE:
      loopStateCycle();
      break;
    case STATE_WEBSERVER:
      loopStateWebserver();
      break;
  }

  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    refreshDisplay();
    displayRefreshedAt = millis();
  }
}

void loopStateDefault()
{
  if (millis() - cycleBeganAt > cycleBeginInterval) {
    beginCycle();
    cycleBeganAt = millis();
    return;
  }

  if (millis() - numberSetAt > numberSetInterval) {
    setNextNumber();
    numberSetAt = millis();

    // Uncomment to see remaining memory:
    // Serial.print(ESP.getFreeHeap());
    // Serial.print(F(" "));
    // Serial.print(ESP.getHeapFragmentation());
    // Serial.println();
  }

  if (millis() - numberUpdatedAt > numberUpdateInterval) {
    setCurrentNumber();
    numberUpdatedAt = millis();
  }
}

void loopStateCycle()
{
  switch (cycleState) {
    case CYCLE_BEGIN:
      // Waiting for current number to blank out
      if (millis() - numberUpdatedAt > numberUpdateInterval) {
        setCurrentNumber();
        numberUpdatedAt = millis();
      }
      if (isCurrentNumberIdenticalToNextNumber()) {
        cycleState = CYCLE_ACTIVE;
      }
      break;
    case CYCLE_ACTIVE:
      if (millis() - numberUpdatedAt > numberUpdateInterval) {
        setCurrentNumberForCycle();
        numberUpdatedAt = millis();
      }
      break;
    case CYCLE_END:
      currentState = STATE_DEFAULT;
      memcpy(nextDigits, savedDigits, sizeof(savedDigits));
      numberSetAt = millis();
      break;
  }
}

void loopStateWebserver()
{
  server.handleClient();

  if (millis() - ipCheckedAt > ipCheckInterval) {
    checkIP();
    ipCheckedAt = millis();
  }

  if (millis() - ipScrollUpdatedAt > (ipIsScrollResting ? ipScrollRestInterval : ipScrollTickInterval)) {
    scrollIP();
    ipScrollUpdatedAt = millis();
  }
}


// ====================================================================================================================
// Button click functions for state machine.
// ====================================================================================================================

void handleClick()
{
  if (currentState == STATE_DEFAULT) {
    beginCycle();
    cycleBeganAt = millis();
  }
}

void handleDoubleClick()
{
  for (byte i = 0; i < 6; i++) {
    currentDots[i] = false;
  }

  switch (currentState) {
    case STATE_DEFAULT:
      currentState = STATE_WEBSERVER;
      memcpy(savedDigits, currentDigits, sizeof(currentDigits));
      server.begin();
      currentIP[0] = '\0';
      checkIP();
      break;
    case STATE_WEBSERVER:
      currentState = STATE_DEFAULT;
      memcpy(currentDigits, savedDigits, sizeof(savedDigits));
      numberSetAt = millis();
      server.stop();
      break;
  }

  refreshDisplay();
}

void handleTripleClick()
{
  if (currentState == STATE_DEFAULT) {
    initWiFiManager(false);
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
  if (shouldAutoConnect) {
    // For auto connect, portal is not persistent if WiFi credentials are valid
    wifiManager.autoConnect((PGM_P) WIFI_AP_SSID);
  } else {
    // Goes into a blocking loop until its webform is submitted
    wifiManager.startConfigPortal((PGM_P) WIFI_AP_SSID);

    // Restart even if nothing changed
    restartDevice();
  }

  // If you get here, you have connected to the WiFi
  Serial.println(F("Connected."));
}

void configModeCallback (WiFiManager *myWiFiManager)
{
  // Aligned right for dot bug and to differentiate from webserver state
  // See refreshDisplayDuringButtonPress() regarding dot bug
  nixie.setDecimalPoint(2, true);
  nixie.setDecimalPoint(3, true);
  nixie.setDecimalPoint(4, true);
  nixie.displayDigits(BLANK, 1, 0, 0, 0, 1);
}

void saveConfigCallback()
{
  // Restart if any settings changed to prevent weirdness
  restartDevice();
}

// After flashing, manually reboot the device once via power or RST pin
// Otherwise, you may get an error: rst cause:4, boot mode:(1,7) wdt reset
// https://www.pieterverhees.nl/sparklesagarbage/esp8266/130-difference-between-esp-reset-and-esp-restart
// https://github.com/esp8266/Arduino/issues/1017
void restartDevice()
{
  ESP.wdtDisable();
  ESP.restart();
}


// ====================================================================================================================
// Cycle digits in each tube to prevent cathode poisoning. Wave animation.
// ====================================================================================================================

void beginCycle()
{
  currentState = STATE_CYCLE;

  memcpy(savedDigits, nextDigits, sizeof(nextDigits));

  for (byte i = 0; i < 6; i++) {
    nextDigits[i] = BLANK;
  }

  cycleState = CYCLE_BEGIN;
  cycleOffset = 0;
}

void setCurrentNumberForCycle()
{
  if (cycleOffset < 6) {
    if (currentDigits[cycleOffset] == nixieDigitStackTop) {
      cycleOffset++;
    }
  } else {
    if (currentDigits[cycleOffset - 2] == BLANK) {
      cycleOffset++;
    }
  }

  if (cycleOffset > 7) {
    cycleState = CYCLE_END;
    return;
  }

  if (cycleOffset < 6) {
    switch (currentDigits[cycleOffset]) {
      case nixieDigitStackTop:
        break;
      case BLANK:
        currentDigits[cycleOffset] = nixieDigitStackBottom;
        break;
      default:
        currentDigits[cycleOffset] = nixieDigitStack[getDigitDepth(currentDigits[cycleOffset]) + 1];
        break;
    }
  }

  if (cycleOffset > 1) {
    switch (currentDigits[cycleOffset - 2]) {
      case BLANK:
        break;
      case nixieDigitStackBottom:
        currentDigits[cycleOffset - 2] = BLANK;
        break;
      default:
        currentDigits[cycleOffset - 2] = nixieDigitStack[getDigitDepth(currentDigits[cycleOffset - 2]) - 1];
        break;
    }
  }
}


// ====================================================================================================================
// Handle IP checking and display during web server state.
// ====================================================================================================================

void checkIP()
{
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  memcpy(oldIP, currentIP, sizeof(currentIP));
  WiFi.localIP().toString().toCharArray(currentIP, sizeof(currentIP));

  // Uncomment for testing:
  // memcpy(currentIP, "192.168.123.245", sizeof(currentIP));
  // memcpy(currentIP, "10.0.0.2", sizeof(currentIP));

  if (strcmp(currentIP, oldIP) == 0) {
    return;
  }

  ipDigitCount = 0;
  ipCharCount = strlen(currentIP);

  for (byte i = 0; i < ipCharCount; i++) {
    if (isDigit(currentIP[i])) {
      ipDigitCount++;
    }
  }

  ipShouldScroll = ipDigitCount > 6;
  ipScrollState = IP_SCROLL_REST_LEFT;
  ipIsScrollResting = true;
  ipScrollOffset = 0;
  ipScrollUpdatedAt = millis();

  displayIP();
}

void scrollIP()
{
  switch (ipScrollState) {
    case IP_SCROLL_REST_LEFT:
      ipScrollState = IP_SCROLL_MOVE_LEFT;
      ipIsScrollResting = false;
      break;
    case IP_SCROLL_MOVE_LEFT:
      if (ipScrollOffset + 6 == ipDigitCount) {
        ipScrollState = IP_SCROLL_REST_RIGHT;
        ipIsScrollResting = true;
      } else {
        ipScrollOffset += 1;
      }
      break;
    case IP_SCROLL_REST_RIGHT:
      ipScrollState = IP_SCROLL_MOVE_RIGHT;
      ipIsScrollResting = false;
      break;
    case IP_SCROLL_MOVE_RIGHT:
      if (ipScrollOffset == 0) {
        ipScrollState = IP_SCROLL_REST_LEFT;
        ipIsScrollResting = true;
      } else {
        ipScrollOffset -= 1;
      }
      break;
  }

  displayIP();
}

void displayIP()
{
  if (ipCharCount < 1) {
    return;
  }

  for (byte i = 0; i < 6; i++) {
    currentDigits[i] = BLANK;
    currentDots[i] = false;
  }

  byte currentDigit = 0;
  byte digitsToSkip = ipScrollOffset;

  for (byte i = 0; i < ipCharCount; i++) {
    if (isDigit(currentIP[i]) && currentDigit < 6) {
      if (digitsToSkip == 0) {
        currentDigits[currentDigit] = currentIP[i] - '0'; // Subtract the ASCII value for zero
        currentDigit++;
      } else {
        digitsToSkip -= 1;
      }
      continue;
    }

    // For decimals, segment zero is 2nd tube from left
    if (currentIP[i] == '.' && currentDigit > 0 && currentDigit < 6) {
      currentDots[currentDigit - 1] = true;
    }
  }
}


// ====================================================================================================================
// Handle ESP8266WebServer requests to configure API queries.
// ====================================================================================================================

void handleRoot()
{
  char apiPortDisplay[5];
  itoa(apiPort, apiPortDisplay, 10);

  // Form field order: host, port, path
  snprintf_P(bufferHtml, sizeof(bufferHtml), indexHtml, apiHost, apiPortDisplay, apiPath);
  server.send(200, F("text/html"), bufferHtml);
}

void handleUpdate()
{
  if (!server.hasArg(F("host")) || !server.hasArg(F("path")) || !server.hasArg(F("port"))) {
    return sendUpdate(400, F("Missing param."));
  }

  const String & apiHostNew = server.arg(F("host"));
  const String & apiPathNew = server.arg(F("path"));
  const String & apiPortNew = server.arg(F("port"));

  if (apiHostNew.length() == 0 || apiPathNew.length() == 0 || apiPortNew.length() == 0) {
    return sendUpdate(400, F("Param is empty."));
  }

  unsigned int apiPortParsed = apiPortNew.toInt();

  if (apiHostNew == apiHost && apiPathNew == apiPath && apiPortParsed == apiPort) {
    return sendUpdate(400, F("Params not changed."));
  }

  Serial.print(F("New host: "));
  Serial.println(apiHostNew);

  Serial.print(F("New path: "));
  Serial.println(apiPathNew);

  Serial.print(F("New port: "));
  Serial.println(apiPortParsed);

  apiHostNew.toCharArray(apiHost, sizeof(apiHost));
  apiPathNew.toCharArray(apiPath, sizeof(apiPath));

  apiPort = apiPortParsed;

  saveParamsToEEPROM();

  return sendUpdate(200, F("Success."));
}

// https://forum.arduino.cc/index.php?topic=293408.msg2050273
void sendUpdate(const int code, const __FlashStringHelper* message)
{
  snprintf_P(bufferHtml, sizeof(bufferHtml), updateHtml, message);
  server.send(code, F("text/html"), bufferHtml);
}

// https://github.com/esp8266/Arduino/blob/e9d052c/libraries/ESP8266WebServer/examples/HelloServer/HelloServer.ino#L24
// TODO: https://majenko.co.uk/blog/evils-arduino-strings
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
    message += (PGM_P) F(" ") + server.argName(i) + (PGM_P) F(": ") + server.arg(i) + (PGM_P) F("\n");
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
  EEPROM.begin(sizeof(apiHost) + sizeof(apiPath) + sizeof(apiPort) + sizeof(memEndCurrent));
  EEPROM.get(0, apiHost);
  EEPROM.get(0 + sizeof(apiHost), apiPath);
  EEPROM.get(0 + sizeof(apiHost) + sizeof(apiPath), apiPort);
  EEPROM.get(0 + sizeof(apiHost) + sizeof(apiPath) + sizeof(apiPort), memEndCurrent);
  EEPROM.end();

  // Uncomment to simulate EEPROM read fail:
  // strcpy(memEndCurrent, "NO");

  if (strcmp(memEndCurrent, memEndDefined) == 0) {
    Serial.println(F("Loaded params from EEPROM"));
  } else {
    Serial.println(F("Failed to load params from EEPROM, using defaults..."));
    strcpy_P(apiHost, apiHostDefault);
    strcpy_P(apiPath, apiPathDefault);
    apiPort = apiPortDefault;
    saveParamsToEEPROM();
  }

  Serial.print(F("Host: "));
  Serial.println(apiHost);

  Serial.print(F("Path: "));
  Serial.println(apiPath);

  Serial.print(F("Port: "));
  Serial.println(apiPort);
}

void saveParamsToEEPROM()
{
  EEPROM.begin(sizeof(apiHost) + sizeof(apiPath) + sizeof(apiPort) + sizeof(memEndDefined));
  EEPROM.put(0, apiHost);
  EEPROM.put(0 + sizeof(apiHost), apiPath);
  EEPROM.put(0 + sizeof(apiHost) + sizeof(apiPath), apiPort);
  EEPROM.put(0 + sizeof(apiHost) + sizeof(apiPath) + sizeof(apiPort), memEndDefined);
  EEPROM.end();
}


// ====================================================================================================================
// Error handling.
// ====================================================================================================================

void throwError(const byte errorCode)
{
  const byte errorDigits[] = {errorCode, BLANK, BLANK, BLANK, BLANK, BLANK};
  memcpy(currentDigits, errorDigits, sizeof(errorDigits));
  memcpy(nextDigits, errorDigits, sizeof(errorDigits));
}


// ====================================================================================================================
// Query API for next number.
// ====================================================================================================================

// Unfortunately, while the API is being queried, the button becomes unresponsive.
// TODO: Use AsyncClient from me-no-dev/ESPAsyncTCP? KISS for now.
// https://github.com/me-no-dev/ESPAsyncTCP/issues/18#issuecomment-240777516
// TODO: Alternatively, look into interrupts for the button.

void setNextNumber()
{
  WiFiClient client;

  if (!client.connect(apiHost, apiPort)) {
    return throwError(ERROR_CONNECTION_FAILED);
  }

  // https://arduinojson.org/v6/example/http-client/
  client.print(F("GET "));
  client.print(apiPath);
  client.println(F(" HTTP/1.1"));
  // TODO: Add ability to set Host "header" separately from connection host
  client.print(F("Host: "));
  client.println(apiHost);
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    client.stop();
    return throwError(ERROR_REQUEST_FAILED);
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  // https://forum.arduino.cc/index.php?topic=362476.0
  if (strcmp_P(status, (PGM_P) F("HTTP/1.1 200 OK")) != 0) {
    client.stop();
    return throwError(ERROR_RESPONSE_UNEXPECTED);
  }

  if (!client.find((PGM_P) F("\r\n\r\n"))) {
    client.stop();
    return throwError(ERROR_RESPONSE_INVALID);
  }

  // End body with trailing \n to avoid 5 second delay:
  // https://forum.arduino.cc/index.php?topic=529440.30
  long result = client.parseInt();

  client.stop();

  setDigitsFromNumber(result, nextDigits);
}


// ====================================================================================================================
// Update current number for the next time the display refreshes. Animate transition.
// ====================================================================================================================

void setCurrentNumber()
{
  if (isCurrentNumberIdenticalToNextNumber()) {
    return;
  }

  byte currentDigitDepths[6];
  byte nextDigitDepths[6];

  getDigitDepths(currentDigits, currentDigitDepths);
  getDigitDepths(nextDigits, nextDigitDepths);

  signed char signOfChange;

  byte resultDigits[6];

  byte i; // Save this so we know when we called break

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
  for (byte i = 0; i < 6; i++) {
    if (currentDigits[i] != nextDigits[i]) {
      return false;
    }
  }

  return true;
}

void getDigitDepths(byte digits[], byte digitDepths[])
{
  for (byte i = 0; i < 6; i++) {
    digitDepths[i] = getDigitDepth(digits[i]);
  }
}

byte getDigitDepth(byte value)
{
  for (byte i = 0; i < 10; i++) {
    if (nixieDigitStack[i] == value) {
      return i;
    }
  }

  return BLANK;
}

void setDigitsFromNumber(long number, byte digits[])
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

void refreshDisplay()
{
  // Sometimes, dots don't light up if the tube is cold-started.
  // We really only need the button work-around during IP display.
  if (digitalRead(btnPin) == LOW && currentState == STATE_WEBSERVER) {
    return refreshDisplayDuringButtonPress();
  }

  for (int i = 0; i < 6; i++) {
    nixie.setDecimalPoint(i, currentDots[i]);
  }

  nixie.displayDigits(currentDigits[0], currentDigits[1], currentDigits[2], currentDigits[3], currentDigits[4], currentDigits[5]);
}

// Work-around for weird bug, probably due to using the 5v Doayee board with the 3.3v ESP8266.
// When the button is held down, if there is a dot in the second-to-last segment (currentDots[3]),
// then the dot in the last segment lights up as well. We hide it here by blanking the digits
// and lighting up all the dots during a state transition.
void refreshDisplayDuringButtonPress()
{
  for (int i = 0; i < 6; i++) {
    nixie.setDecimalPoint(i, true);
  }

  nixie.displayDigits(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
}
