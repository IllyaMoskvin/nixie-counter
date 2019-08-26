#include <NixieDriver_ESP.h>

//const int nixieDigits[] = {3, 8, 9, 4, 0, 5, 2, 6, 1};
const int nixieDigits[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

long prevNumber = 0;
long currentNumber = 0;
long nextNumber = 0;

int prevNumberSegments[] = {0, 0, 0, 0, 0, 0};
int currentNumberSegments[] = {0, 0, 0, 0, 0, 0};
int nextNumberSegments[] = {0, 0, 0, 0, 0, 0};

bool renderUsingSegments = true;

const unsigned long numberSetInterval = 5000;
const unsigned long numberUpdateInterval = 75;
const unsigned long displayRefreshInterval = 20;

unsigned long numberSetAt;
unsigned long numberUpdatedAt;
unsigned long displayRefreshedAt;

nixie_esp nixie(dataPin, clockPin);

void setup()
{
  Serial.setDebugOutput(true);
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

long getNextNumber()
{
  // return currentNumber + random(0, 1000);
  return random(0, 999999);
}

long setCurrentNumber()
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

long setCurrentNumberLinear()
{
  currentNumber = prevNumber + (nextNumber - prevNumber) * min(1.0, (millis() - numberSetAt) / ((double) numberSetInterval * 2 / 3));
  getSegmentedNumber(currentNumber, currentNumberSegments);
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
  } else if(num < 100) {
    seg[0] = BLANK;
    seg[1] = BLANK;
    seg[2] = BLANK;
    seg[3] = BLANK;
  } else if(num < 1000) {
    seg[0] = BLANK;
    seg[1] = BLANK;
    seg[2] = BLANK;
  } else if(num < 10000) {
    seg[0] = BLANK;
    seg[1] = BLANK;
  } else if(num < 100000) {
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

void printSegments(int seg[])
{
  for (int i = 0; i < 5; i++) {
    Serial.print(seg[i]);
    Serial.print(", ");
  }

  Serial.println(seg[5]);
}
