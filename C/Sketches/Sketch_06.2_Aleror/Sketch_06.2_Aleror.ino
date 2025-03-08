/**********************************************************************
  Filename    : Alertor
  Description : Control passive buzzer by button.
  Auther      : www.freenove.com
  Modification: 2021/10/13
**********************************************************************/
#define PIN_BUZZER 15
#define PIN_BUTTON 16
void initdbg(){
  Serial.begin(115200);
  delay(1000);
  Serial.println("Raspberry Pi Pico initialization completed!");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
}

void setup() {
  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
   
     initdbg();
}

void loop() {
  if (digitalRead(PIN_BUTTON) == LOW) {
    Serial.println("LOW");
    alert();
  }else {
    freq(PIN_BUZZER, 0, 10);
    Serial.println("HIGH");
  }
}

void alert() {
  float sinVal;         // Define a variable to save sine value
  int toneVal;          // Define a variable to save sound frequency
  for (int x = 0; x < 360; x += 10) {     // X from 0 degree->360 degree
    sinVal = sin(x * (PI / 180));       // Calculate the sine of x
    toneVal = 2000 + sinVal * 500;      // Calculate sound frequency according to the sine of x
    freq(PIN_BUZZER, toneVal, 5);
  }
}

void freq(int PIN, int freqs, int times) {
  if (freqs == 0) {
    digitalWrite(PIN, LOW);
  }
  else {
    for (int i = 0; i < times * freqs / 1000; i ++) {
      digitalWrite(PIN, HIGH);
      delayMicroseconds(1000000 / freqs / 2);
      digitalWrite(PIN, LOW);
      delayMicroseconds(1000000 / freqs / 2);
    }
  }
}
