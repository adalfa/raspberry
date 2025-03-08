/**********************************************************************
  Filename    : SoftLight
  Description : Controlling the brightness of LED by potentiometer.
  Auther      : www.freenove.com
  Modification: 2021/10/13
**********************************************************************/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define PIN_ADC0        26
#define PIN_LEDB         15
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void initdbg() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Raspberry Pi Pico initialization completed!");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
}
void setup() {
  initdbg();
  pinMode(PIN_LEDB, OUTPUT);

   if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  Serial.println("SSD1306 allocation OK");


  delay(2000);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Hello, world!!!");
  display.display();
  
}

void loop() {
  int adcVal = analogRead(PIN_ADC0); //read adc
  double voltage = adcVal / 1023.0 * 3.3;
  //Serial.println("ADC Value: " + String(adcVal) + " --- Voltage Value: " + String(voltage) + "V");
  analogWrite(PIN_LEDB, map(adcVal, 0, 1023, 0, 255));
  delay(10);
}
