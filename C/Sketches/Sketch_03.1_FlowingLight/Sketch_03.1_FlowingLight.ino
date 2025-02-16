/**********************************************************************
  Filename    : FlowingLight
  Description : Using ledbar to demonstrate flowing lamp.
  Auther      : www.freenove.com
  Modification: 2021/10/13
**********************************************************************/
byte ledPins[] = {16, 17, 18, 19, 20, 21, 22, 26, 27, 28};
int ledCounts;
unsigned long ldelay;

void setup() {
  ldelay=50;
  ledCounts = sizeof(ledPins);
  for (int i = 0; i < ledCounts; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
}

void loop() {
  digitalWrite(LED_BUILTIN,HIGH);
  for (int i = 0; i < ledCounts; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(ldelay);
    digitalWrite(ledPins[i], LOW);
  }
  for (int i = ledCounts - 1; i > -1; i--) {
    digitalWrite(ledPins[i], HIGH);
    delay(ldelay);
    digitalWrite(ledPins[i], LOW);
  }
}
