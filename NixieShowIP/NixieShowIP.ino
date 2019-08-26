#include <ESP8266WiFi.h>
#include <NixieDriver_ESP.h>

const char* ssid     = "FoobarNetwork";
const char* password = "foobar";

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

const unsigned long ipRefreshInterval = 5000;
unsigned long ipRefreshedAt;

const unsigned long displayRefreshInterval = 500;
unsigned long displayRefreshedAt;

String ip;

nixie_esp nixie(dataPin, clockPin);

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

  WiFi.begin(ssid, password);
}

void loop()
{
  if (millis() - ipRefreshInterval > ipRefreshedAt) {
    if (WiFi.status() == WL_CONNECTED) {
      ip = WiFi.localIP().toString();
      Serial.println(ip);
    }
    
    ipRefreshedAt = millis();
  }
  
  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    displayIp();
    
    displayRefreshedAt = millis();
  }
}

void displayIp()
{
  int c = ip.length();

  if (c < 1) {
    return;
  }

  for(int i = 0; i < 6; i++) {
    nixie.setDecimalPoint(i, false);
  }

  int seg[6];
  int d = 0;

  for (int i = 0; i < c; i++) {
    // Convert char to int by subtracting the ASCII value for zero
    if (isDigit(ip[i]) && d < 6) {
      seg[d] = ip[i] - '0';
      d++;
    }

    // For decimals, segment zero is 2nd tube from left
    if (ip[i] == '.' && d > 0 && d < 6) {
      nixie.setDecimalPoint(d - 1, true);
    }
  }

  nixie.displayDigits(seg[0], seg[1], seg[2], seg[3], seg[4], seg[5]);
}
