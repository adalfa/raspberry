#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <hardware/watchdog.h>
#include <DHT.h>

#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define SERIAL_DEBUG 1
#define PIN_ADC0 26
#define PIN_ADC1 27
#define PIN_ADC2 28
#define GPIO23 23
#define GPIO22 22
#define DHTTYPE DHT11
#define PIN_LEDB 15
#define RES 1024.0
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels
#define ALPHA 0.75

DHT dht(GPIO22, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
QueueHandle_t dispQueue;
struct message {
  int line;
  float val;
  char unit;
  int curs = 0;
};
// DHT dht(GPIO22, DHTTYPE);

void vBlinkTask(void *pvParameters) {

  for (;;) {

    digitalWrite(LED_BUILTIN, HIGH);

    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("High");

    vTaskDelay(pdMS_TO_TICKS(500));
    // for(long i=1;i<100000000;i++);
    digitalWrite(LED_BUILTIN, LOW);

    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("Low");

    vTaskDelay(pdMS_TO_TICKS(500));

    // for(long i=1;i<100000000;i++);
  }
}

void vReadpot(void *pvParameters) {
  for (;;) {
    struct message msgT;
    int adcVal = analogRead(PIN_ADC0);
    adcVal &= ~(1 );
    
    float voltage = adcVal / RES * 3.3;
    analogWrite(PIN_LEDB, map(adcVal, 0, RES, 0, 255));
    msgT.line = 20;
    msgT.val = voltage;
    msgT.unit = 'V';
    msgT.curs = 60;
    if (xQueueSend(dispQueue, (void *)&msgT, 10) != pdPASS) {
      if (SERIAL_DEBUG)
        Serial.println("send failed");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void vReadDHT(void *pvParameters) {
  for (;;) {

    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();

    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float f = dht.readTemperature(true);


    struct message msgD;
    struct message msgH;
    /*
   if (Serial.availableForWrite()>1)
   {
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  }
  */
    msgD.line = 0;
    msgD.unit = 'C';
    msgH.line = 0;
    msgH.curs = 60;
    msgH.unit = '%';
    msgH.val = h;
    msgD.val = t;

    if (xQueueSend(dispQueue, (void *)&msgD, 10) != pdPASS) {
      if (SERIAL_DEBUG)
        Serial.println("send failed");
    }
    if (xQueueSend(dispQueue, (void *)&msgH, 10) != pdPASS) {
      if (SERIAL_DEBUG)
        Serial.println("send failed");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vReadPhoto(void *pvParameters) {
  for (;;) {
    struct message msgP;
    int padvVal = analogRead(PIN_ADC2);

    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("lum:1 " + String(padvVal));

    msgP.line = 20;
    msgP.val = padvVal;
    msgP.unit = ' ';
    if (xQueueSend(dispQueue, (void *)&msgP, 10) != pdPASS) {
      if (SERIAL_DEBUG)
        Serial.println("send failed");
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
void vWatchDog(void *pvParametes) {
  rp2040.wdt_begin(100);

  for (;;) {
    rp2040.wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


void vReadtemp(void *pvParameters) {


  if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
    Serial.println("Temp");
  float tempPrev = 0.0;



  for (;;) {
    struct message msgT;
    //vTaskDelay(pdMS_TO_TICKS(1000));
    int adcValueTemp = analogRead(PIN_ADC1);

    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("Temp:1 " + String(adcValueTemp));


    float voltageT = (float)adcValueTemp / RES * 3.3;
    float Rt = 10 * voltageT / (3.3 - voltageT);
    float tempK = 1 / (1 / (273.15 + 25) + log(Rt / 10) / 3950.0);  //calculate temperature (Kelvin)
    float tempCa = tempK - 273.15;
    float tempC = ALPHA * tempCa + (1 - ALPHA) * tempPrev;
    tempPrev = tempCa;


    if (SERIAL_DEBUG && Serial.availableForWrite() > 1) {
      Serial.print("Temp:2 ");
      Serial.print(tempC);
      Serial.println();
    }

    msgT.line = 10;
    msgT.val = tempC;
    msgT.unit = 'C';

    if (SERIAL_DEBUG && Serial.availableForWrite() > 1) {
      Serial.print("msgT->val");
      //Serial.print(msgT.val);
      Serial.println();
    }

    //strncpy(msgT.msg,String(tempC).c_str(),strnlen(msgT.msg,128));
    if (xQueueSend(dispQueue, (void *)&msgT, 10) != pdPASS) {
      if (SERIAL_DEBUG)
        Serial.println("send failed");
    }

    //Serial.println("VoltageT: " + String(voltageT) + "V"+"Temperature: " + String(tempC) + "C");

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
void vRcvQueue(void *pvParameters) {

  for (;;) {
    struct message msgR;
    if (xQueueReceive(dispQueue, &msgR, 10) != pdPASS) {

      //Serial.println("receive failed");

    } else {
      float valore = msgR.val;

      if (SERIAL_DEBUG && Serial.availableForWrite() > 1) {
        Serial.print("val:" + String(valore));
        Serial.println();
      }

      display.setTextSize(1);
      //display.setCursor(0, msgR.line);
      //display.setTextColor(WHITE,BLACK);
      //display.println("         ");
      //display.display();
      display.setCursor(msgR.curs, msgR.line);
      display.setTextColor(WHITE, BLACK);
      display.println(String(msgR.val) + " " + String(msgR.unit) + " ");
      display.display();
    }
  }
}



void initdbg() {
  if (SERIAL_DEBUG)
    Serial.begin(115200);

  // vTaskDelay(1000/portTICK_RATE_MS);
  delay(1000);
  if (SERIAL_DEBUG)
    Serial.println("Raspberry Pi Pico initialization completed!");
}
void initGPIO() {
  pinMode(PIN_LEDB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_ADC0, INPUT);
  pinMode(PIN_ADC1, INPUT);
  pinMode(PIN_ADC2, INPUT);

  analogReadResolution(10);
}
void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    if (SERIAL_DEBUG)
      Serial.println("SSD1306 allocation failed");

    for (;;)
      ;
  }
  display.invertDisplay(false);
  display.setRotation(10);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("Init");
  display.display();
}
void initDHT() {


  dht.begin();
}

void setup1() {
  initDHT();
}

void setup() {
  initdbg();
  initGPIO();
  initDisplay();

  float d = 1.0;

  dispQueue = xQueueCreate(10, sizeof(struct message));

  if (SERIAL_DEBUG)
    Serial.println("Step1" + String(d));

  xTaskCreate(vBlinkTask, "Blink Task", 128, NULL, 1, NULL);
  xTaskCreate(vReadtemp, "Temp Task", 128, NULL, 1, NULL);
  xTaskCreate(vRcvQueue, "Receive Task", 128, NULL, 1, NULL);
  xTaskCreate(vReadpot, "Pot Task", 128, NULL, 1, NULL);
  xTaskCreate(vReadPhoto, "Lum Task", 128, NULL, 1, NULL);
  xTaskCreate(vWatchDog, "WatchDog Task", 128, NULL, 1, NULL);
  //xTaskCreate(vReadDHT, "Pot Task", 128, NULL, 1, NULL);

  char pcWriteBuffer[2048];
  vTaskList(pcWriteBuffer);

  if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
    Serial.printf("%s\n", pcWriteBuffer);

  //xTaskCreate(vTaskListF, "Top Task", 128, NULL, 1, NULL);
  //Serial.println("task Create");
  //vTaskStartScheduler();
}
void loop() {
}
void loop1() {

  //delay(2000);
  float h = 10;
  float t = 20;
  do {
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("read");

    h = dht.readHumidity();
    // Read temperature as Celsius (the default)




    t = dht.readTemperature();
  } while (h == NAN || t == NAN);
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float f = dht.readTemperature(true);
  if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
    Serial.println("read OK");

  struct message msgD;
  struct message msgH;

  if (SERIAL_DEBUG && Serial.availableForWrite() > 1) {
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
  }

  msgD.line = 0;
  msgD.unit = 'C';
  msgH.line = 0;
  msgH.curs = 60;
  msgH.unit = '%';
  msgH.val = h;
  msgD.val = t;

  if (xQueueSend(dispQueue, (void *)&msgD, 10) != pdPASS) {
    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("send failed");
  }
  if (xQueueSend(dispQueue, (void *)&msgH, 10) != pdPASS) {
    if (SERIAL_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("send failed");
  }


  //delay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)


  //Serial.print(F("°C "));

  //

  //vBlinkTask(NULL);
}
