/**********************************************************************
  Filename    : TableLamp
  Description : Make a table lamp
  Auther      : www.freenove.com
  Modification: 2021/10/13
**********************************************************************/
#define LED    15
#define PIN_BUTTON 13
bool ledState = false;

void setup() {
  // initialize digital pin LED as an output.
  pinMode(LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED_BUILTIN,HIGH);
  if (digitalRead(PIN_BUTTON) == LOW) {
    //delay(10);
    if (digitalRead(PIN_BUTTON) == LOW) {
      reverseGPIO(LED);
    }
    while (digitalRead(PIN_BUTTON) == LOW);
  }
}

void reverseGPIO(int pin) {
  ledState = !ledState;
  digitalWrite(pin, ledState);
}
