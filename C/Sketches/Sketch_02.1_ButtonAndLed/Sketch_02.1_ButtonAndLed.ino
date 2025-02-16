/**********************************************************************
* Filename    : ButtonAndLed
* Description : Use a button to control LED light
* Auther      : www.freenove.com
* Modification: 2021/10/13
**********************************************************************/
#define LED    15
#define PIN_BUTTON 13

void setup() {
  // initialize digital pin PIN_LED as an output.
  pinMode(LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {
    digitalWrite(LED_BUILTIN,HIGH);

  if (digitalRead(PIN_BUTTON) == LOW) {
    digitalWrite(LED,HIGH);
  }else{
    digitalWrite(LED,LOW);
  }
}
