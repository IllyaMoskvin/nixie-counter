#include <ESP8266WiFi.h>
#include <NixieDriver_ESP.h>

const char* ssid     = "FoobarNetwork";
const char* password = "foobar";

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

const unsigned long ipCheckInterval = 5000;
unsigned long ipCheckedAt;

const unsigned long displayRefreshInterval = 15;
unsigned long displayRefreshedAt;

bool shouldScroll = false;

const unsigned long scrollRestInterval = 2500;
const unsigned long scrollTickInterval = 125;
unsigned long scrollUpdatedAt;

const int SCROLL_REST_LEFT = 0;
const int SCROLL_MOVE_LEFT = 1;
const int SCROLL_REST_RIGHT = 2;
const int SCROLL_MOVE_RIGHT = 3;

int scrollState = SCROLL_REST_LEFT;
bool isScrollResting = true;
int scrollOffset = 0;

int digits[6] = {0, 0, 0, 0, 0, 0};
int dots[6] = {0, 0, 0, 0, 0, 0};

String ip;
String oldIp;

int ipDigitCount;
int ipCharCount;

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
  if (millis() - ipCheckedAt > ipCheckInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      oldIp = ip;
      ip = WiFi.localIP().toString();

      // Uncomment for testing:
      // ip = String("192.168.123.245");
      // ip = String("10.0.0.2");

      if (ip != oldIp) {
        ipDigitCount = 0;
        ipCharCount = ip.length();

        for (int i = 0; i < ipCharCount; i++) {
          if (isDigit(ip[i])) {
            ipDigitCount++;
          }
        }

        shouldScroll = ipDigitCount > 6;

        // TODO: Is left-aligned the right choice for IPs?
        scrollState = SCROLL_REST_LEFT;
        isScrollResting = true;
        scrollOffset = 0;
        scrollUpdatedAt = millis();
      }

      Serial.println(ip);
    }

    ipCheckedAt = millis();
  }

  if (shouldScroll && millis() - scrollUpdatedAt > (isScrollResting ? scrollRestInterval : scrollTickInterval)) {
    switch (scrollState) {
      case SCROLL_REST_LEFT:
        scrollState = SCROLL_MOVE_LEFT;
        isScrollResting = false;  
        break;
      case SCROLL_MOVE_LEFT:
        if (scrollOffset + 6 == ipDigitCount) {
          scrollState = SCROLL_REST_RIGHT;
          isScrollResting = true;
        } else {
          scrollOffset += 1;
        }
        break;
      case SCROLL_REST_RIGHT:
        scrollState = SCROLL_MOVE_RIGHT;
        isScrollResting = false;
        break;
      case SCROLL_MOVE_RIGHT:
        if (scrollOffset == 0) {
          scrollState = SCROLL_REST_LEFT;
          isScrollResting = true;
        } else {
          scrollOffset -= 1;
        }
        break;
    }

    scrollUpdatedAt = millis();
  }
  
  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    displayIp(ip, scrollOffset);

    for(int i = 0; i < 6; i++) {
      nixie.setDecimalPoint(i, dots[i]);
    }

    nixie.displayDigits(digits[0], digits[1], digits[2], digits[3], digits[4], digits[5]);

    displayRefreshedAt = millis();
  }
}

void displayIp(String ip, int offset)
{
  int c = ip.length();

  if (c < 1) {
    return;
  }

  for(int i = 0; i < 6; i++) {
    digits[i] = BLANK;
    dots[i] = false;
  }

  int d = 0;

  for (int i = 0; i < c; i++) {
    if (isDigit(ip[i]) && d < 6) {
      if (offset == 0) {
        // Convert char to int by subtracting the ASCII value for zero
        digits[d] = ip[i] - '0';
        d++;
      } else {
        offset -= 1;
      }
      continue;
    }

    // For decimals, segment zero is 2nd tube from left
    if (ip[i] == '.' && d > 0 && d < 6) {
      dots[d - 1] = true;
    }
  }
}
