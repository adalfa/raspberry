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
#define PIN_ADC1        27
#define GPIO23          23
#define PIN_LEDB         15
#define RES           1024.0
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
 Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void initdbg() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Raspberry Pi Pico initialization completed!");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(GPIO23, LOW);
  digitalWrite(LED_BUILTIN,HIGH);
}
void setup() {
  //TwoWire CustomI2C1(6, 7);
 
  initdbg();
  pinMode(PIN_LEDB, OUTPUT);

   if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  Serial.println("SSD1306 allocation OK");


  delay(2000);
  display.invertDisplay(false);
  display.setRotation(10);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("Hello, world!!!");
  display.display();
  
}

void loop() {
  analogReadResolution(10);
  int adcVal = analogRead(PIN_ADC0);
  int adcValueTemp = analogRead(PIN_ADC1); 
  double voltageT = (float)adcValueTemp / RES * 3.3;
  double Rt = 10 * voltageT / (3.3 - voltageT);
  double tempK = 1 / (1 / (273.15 + 25) + log(Rt / 10) / 3950.0); //calculate temperature (Kelvin)
  double tempC = tempK - 273.15;  
  double voltage = adcVal / RES * 3.3;
 Serial.println("ADC Value: " + String(adcVal) + " --- Voltage Value: " + String(voltage) + "V\t"+"VoltageT: " + String(voltageT) + "V"+"Temperature: " + String(tempC) + "C");
  analogWrite(PIN_LEDB, map(adcVal, 0, RES, 0, 255));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(String(voltage) + " V");
  display.setCursor(0, 10);
  display.println(String(tempC) + " C");
  display.display();
  delay(100);
}
