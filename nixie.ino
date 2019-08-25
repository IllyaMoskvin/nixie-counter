#include <NixieDriver_ESP.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

/**
   Error code reference:
   500001 = cannot connect to host via port
   500002 = did not receive a json response
   500003 = could not parse the json
*/

const int nixieDigits[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};

// All of our requests use HTTPS
const int api_port = 443;

// Config values stored in EEPROM
char api_host[40];
char api_path[255];

// Helpers for storing values in EEPROM
char mem_end_def[2 + 1] = "OK";
char mem_end_cur[sizeof(mem_end_def)];

// Pinouts
const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;
const int btnPin = 13;

long prevNumber = 0;
long currentNumber = 0;
long nextNumber = 0;

int prevNumberSegments[] = {0, 0, 0, 0, 0, 0};
int currentNumberSegments[] = {0, 0, 0, 0, 0, 0};
int nextNumberSegments[] = {0, 0, 0, 0, 0, 0};

const bool renderUsingSegments = true;

bool wifiManagerStarted = false;

const unsigned long numberSetInterval = 5000;
const unsigned long numberUpdateInterval = 75;
const unsigned long displayRefreshInterval = 20;
const unsigned long buttonCheckInterval = 2000;

unsigned long numberSetAt;
unsigned long numberUpdatedAt;
unsigned long displayRefreshedAt;
unsigned long buttonCheckedAt;

nixie_esp nixie(dataPin, clockPin);

// Function declarations
void runWiFiManager();
void configModeCallback (WiFiManager *myWiFiManager);
void saveConfigCallback();
void loadParams();
void saveParams();
long getNextNumber();
void setCurrentNumber();
void setCurrentNumberShift(bool individually);
bool isCurrentSegmentsIdenticalToNextSegments();
void getDigitIndexes(int numberSegments[], int digitIndexes[]);
int getDigitIndex(int value);
void getSegmentedNumber(long num, int seg[]);
void nixieDisplay(long num);
void nixieDisplay(int seg[]);

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

  // Uncomment and reset a couple times to forget saved WiFi credentials:
  // https://arduino.stackexchange.com/questions/52059/does-the-esp8266-somehow-remember-wifi-access-data
  // https://github.com/esp8266/Arduino/issues/1094
  // ESP.eraseConfig();

  loadParams(); // For api_host and api_path
}

void loop()
{
  // WiFiManger is blocking, but we use a toggle button
  if (millis() - buttonCheckedAt > buttonCheckInterval) {
    if (digitalRead(btnPin) == LOW && !wifiManagerStarted) {
      wifiManagerStarted = true;
      runWiFiManager();
    }
    if (digitalRead(btnPin) == HIGH && wifiManagerStarted) {
      wifiManagerStarted = false;
    }
  }

  if (millis() - numberSetAt > numberSetInterval) {
    prevNumber = currentNumber;
    nextNumber = getNextNumber();

    memcpy(prevNumberSegments, currentNumberSegments, sizeof(currentNumberSegments));
    getSegmentedNumber(nextNumber, nextNumberSegments);

    Serial.println(nextNumber);

    numberSetAt = millis();
  }

  if (millis() - numberUpdatedAt > numberUpdateInterval) {
    setCurrentNumber();
    numberUpdatedAt = millis();
  }

  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    if (renderUsingSegments) {
      nixieDisplay(currentNumberSegments);
    } else {
      nixieDisplay(currentNumber);
    }

    displayRefreshedAt = millis();
  }
}


void runWiFiManager()
{
  WiFiManager wifiManager;

  WiFiManagerParameter param_api_host("api_host", "API Host", api_host, sizeof(api_host));
  WiFiManagerParameter param_api_path("api_path", "API Path", api_path, sizeof(api_path));

  wifiManager.addParameter(&param_api_host);
  wifiManager.addParameter(&param_api_path);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Check "Network Connections > Wi-Fi > Properties > TCP/IPv4 > Properties"
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));

  wifiManager.startConfigPortal("Nixie");

  strcpy(api_host, param_api_host.getValue());
  strcpy(api_path, param_api_path.getValue());

  saveParams();
}

void configModeCallback (WiFiManager *myWiFiManager)
{
  // TODO: Is this an off-by-one error?
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


// Functions to persist data between resets
// http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/libraries.html#eeprom
// https://github.com/esp8266/Arduino/blob/4897e00/libraries/DNSServer/examples/CaptivePortalAdvanced/credentials.ino
// http://forum.arduino.cc/index.php?topic=189144.0
// TODO: Cycle EEPROM start point?
// TODO: Use arrays of pointers?

void loadParams()
{
  EEPROM.begin(sizeof(api_host) + sizeof(api_path) + sizeof(mem_end_cur));
  EEPROM.get(0, api_host);
  EEPROM.get(0 + sizeof(api_host), api_path);
  EEPROM.get(0 + sizeof(api_host) + sizeof(api_path), mem_end_cur);
  EEPROM.end();

  // strcpy(mem_end_cur, "NO"); // Uncomment for testing?

  if (String(mem_end_cur) != String(mem_end_def)) {
    Serial.println("Cannot load from EEPROM, using defaults...");
    sprintf(api_path, "%s", "/api/v1/artworks/search?limit=0&cache=false");
    sprintf(api_host, "%s", "nocache.aggregator-data.artic.edu");
    saveParams();
  } else {
    Serial.println("Loaded params from EEPROM");
  }

  Serial.print("Host: ");
  Serial.println(api_host);

  Serial.print("Path: ");
  Serial.println(api_path);
}

void saveParams()
{
  EEPROM.begin(sizeof(api_host) + sizeof(api_path) + sizeof(mem_end_def));
  EEPROM.put(0, api_host);
  EEPROM.put(0 + sizeof(api_host), api_path);
  EEPROM.put(0 + sizeof(api_host) + sizeof(api_path), mem_end_def);
  EEPROM.end();
}


long getNextNumber()
{
  WiFiClientSecure client;

  // Turn off certificate and/or fingerprint checking
  client.setInsecure();

  if (!client.connect(api_host, api_port)) {
    return 500001;
  }

  // https://arduinojson.org/v6/example/http-client/
  client.println(String("GET ") + api_path + " HTTP/1.1");
  client.println(String("Host: ") + api_host);
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return 0;
  }
  
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return 1;
  }
  
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return 1;
  }
  
  DynamicJsonDocument doc(250);

  // May require trailing \n in body to avoid delay:
  // https://forum.arduino.cc/index.php?topic=529440.30
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    return 500003;
  }

  long total = doc["pagination"]["total"];

  client.stop();

  return total;
}

void setCurrentNumber()
{
  setCurrentNumberShift(true);
}

void setCurrentNumberShift(bool individually)
{
  if (isCurrentSegmentsIdenticalToNextSegments()) {
    return;
  }

  int currentDigitIndexes[6];
  int nextDigitIndexes[6];

  getDigitIndexes(currentNumberSegments, currentDigitIndexes);
  getDigitIndexes(nextNumberSegments, nextDigitIndexes);

  int signOfChange;

  int resultNumberSegments[6];

  int i; // Save this so we know when we called break for individually

  for (i = 0; i < 6; i++) {
    if (currentNumberSegments[i] == BLANK && nextNumberSegments[i] != BLANK) {
      resultNumberSegments[i] = nixieDigits[0];
      if (individually) break;
      continue;
    }

    if (currentNumberSegments[i] != BLANK && nextNumberSegments[i] == BLANK) {
      if (currentDigitIndexes[i] == 0) {
        resultNumberSegments[i] = BLANK;
        if (individually) break;
        continue;
      }

      resultNumberSegments[i] = nixieDigits[currentDigitIndexes[i] - 1];
      if (individually) break;
      continue;
    }

    if (currentNumberSegments[i] != nextNumberSegments[i]) {
      signOfChange = nextDigitIndexes[i] < currentDigitIndexes[i] ? -1 : 1;
      resultNumberSegments[i] = nixieDigits[currentDigitIndexes[i] + signOfChange];
      if (individually) break;
      continue;
    }

    resultNumberSegments[i] = currentNumberSegments[i];
  }

  // Fill out digits unfilled due to breaking for individually
  for (i = i + 1; i < 6; i++) {
    resultNumberSegments[i] = currentNumberSegments[i];
  }

  memcpy(currentNumberSegments, resultNumberSegments, sizeof(resultNumberSegments));

  // If the number starts with a zero, we cannot save it to long
  bool hasLeadingZero = true;

  for (int i = 0; i < 6; i++) {
    if (resultNumberSegments[i] == BLANK) {
      continue;
    }
    hasLeadingZero = (resultNumberSegments[i] == 0);
    break;
  }

  if (!hasLeadingZero) {
    int resultNumber = 0;
    for (int i = 0; i < 6; i++) {
      if (resultNumberSegments[i] == BLANK) {
        continue;
      }
      resultNumber += resultNumberSegments[i] * pow(10, 5 - i);
    }
    currentNumber = resultNumber;
  }
}

bool isCurrentSegmentsIdenticalToNextSegments()
{
  bool isIdentical = true;

  for (int i = 0; i < 6; i++) {
    if (currentNumberSegments[i] != nextNumberSegments[i]) {
      isIdentical = false;
      break;
    }
  }

  return isIdentical;
}

void getDigitIndexes(int numberSegments[], int digitIndexes[])
{
  for (byte i = 0; i < 6; i++) {
    digitIndexes[i] = getDigitIndex(numberSegments[i]);
  }
}

int getDigitIndex(int value)
{
  for (int i = 0; i < 10; i++) {
    if (nixieDigits[i] == value) {
      return i;
    }
  }

  return BLANK;
}

void getSegmentedNumber(long num, int seg[])
{
  seg[0] = num / 100000;
  seg[1] = (num / 10000) % 10;
  seg[2] = (num / 1000) % 10;
  seg[3] = (num / 100) % 10;
  seg[4] = (num / 10) % 10;
  seg[5] = (num % 10);

  if (num < 10) {
    seg[0] = BLANK;
    seg[1] = BLANK;
    seg[2] = BLANK;
    seg[3] = BLANK;
    seg[4] = BLANK;
  } else if (num < 100) {
    seg[0] = BLANK;
    seg[1] = BLANK;
    seg[2] = BLANK;
    seg[3] = BLANK;
  } else if (num < 1000) {
    seg[0] = BLANK;
    seg[1] = BLANK;
    seg[2] = BLANK;
  } else if (num < 10000) {
    seg[0] = BLANK;
    seg[1] = BLANK;
  } else if (num < 100000) {
    seg[0] = BLANK;
  }
}

// Override nixie_esp::disp to blank out digits instead of using zeroes
void nixieDisplay(long num)
{
  int seg[6];
  getSegmentedNumber(num, seg);
  nixieDisplay(seg);
}

void nixieDisplay(int seg[])
{
  nixie.displayDigits(seg[0], seg[1], seg[2], seg[3], seg[4], seg[5]);
}
