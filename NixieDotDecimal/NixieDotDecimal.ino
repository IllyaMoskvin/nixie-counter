#include <NixieDriver_ESP.h>

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

const unsigned long displayRefreshInterval = 20;
unsigned long displayRefreshedAt;

nixie_esp nixie(dataPin, clockPin);

void setup()
{
  Serial.begin(115200);

  // Set initial pin states
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);

  pinMode(oePin, OUTPUT);
  digitalWrite(oePin, HIGH);
}

void loop()
{
  if (millis() - displayRefreshedAt > displayRefreshInterval) {
    // The decimal point on the first segment lacks PCB connections
    // Segments are numbered left to right
    // Points are zero-indexed *and* shifted right by one
    nixie.setDecimalPoint(0, true); // lights up decimal in 2nd tube from left
    nixie.setDecimalPoint(1, true);
    nixie.setDecimalPoint(2, true);
    nixie.setDecimalPoint(3, true);
    nixie.setDecimalPoint(4, true);
    nixie.setDecimalPoint(5, true); // does nothing, but is allowed?

    nixie.displayDigits(1, 2, 3, 4, 5, 6);

    displayRefreshedAt = millis();
  }
}
