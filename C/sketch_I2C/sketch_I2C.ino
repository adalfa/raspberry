/**************************************************************************
 * 1306_simple_pico.ino
 * 
 * SSD1306 checkout
 * 
 */
 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3C 

//Perhaps the following 2 lines are required for the Pico?  Or use 4,5 instead of (6u), (7u)?
//#define PIN_WIRE_SDA   (6u)
//#define PIN_WIRE_SCL   (7u)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
 Serial.begin(115200);
  delay(1000);
  Serial.println("Raspberry Pi Pico initialization completed!");
  Serial.println("SDA");
  Serial.println(PIN_WIRE1_SDA);
  Serial.println("SDC");
  Serial.println(PIN_WIRE1_SCL);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("SSD1306 allocation OK");
  // Clear the buffer
 
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             
  display.println("HOWDY !");
  display.display();
}

void loop() {
}
