#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <hardware/watchdog.h>
#define PIN_ADC0        26
#define PIN_ADC1        27
#define PIN_ADC2        28
#define GPIO23          23
#define PIN_LEDB         15
#define RES           1024.0
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define ALPHA 0.75
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
QueueHandle_t dispQueue;
struct message {
 int line;
 float val;
 char unit;

} ;

void vBlinkTask( void * pvParameters) {

   for (;;) {

     digitalWrite(LED_BUILTIN,HIGH);

    Serial.println("High"); 
  vTaskDelay(pdMS_TO_TICKS(1000));
  // for(long i=1;i<100000000;i++);
      digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Low");
   vTaskDelay(pdMS_TO_TICKS(1000));
  // for(long i=1;i<100000000;i++);
   }

}

void vReadpot(void *pvParameters){
  for(;;){
  struct message msgT;
  int adcVal = analogRead(PIN_ADC0);
  float voltage = adcVal / RES * 3.3;
  analogWrite(PIN_LEDB, map(adcVal, 0, RES, 0, 255));
  msgT.line=0;
  msgT.val=voltage;
  msgT.unit='V';
  if (xQueueSend(dispQueue,( void * ) &msgT,10)!= pdPASS){
    Serial.println("send failed");
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void vReadPhoto(void *pvParameters){
  for (;;){
  struct message msgP;
  int padvVal=analogRead(PIN_ADC2);

   if (Serial.availableForWrite()>1)
       Serial.println("lum:1 "+String(padvVal));

  msgP.line=20;
  msgP.val=padvVal;
  msgP.unit=' '; 
   if (xQueueSend(dispQueue,( void * ) &msgP,10)!= pdPASS){
    Serial.println("send failed");
  }      
  vTaskDelay(pdMS_TO_TICKS(500));
}
}
void vWatchDog(void *pvParametes){
  rp2040.wdt_begin(100);
  
  for (;;){
    rp2040.wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(50));

  }
}


void vReadtemp(void *pvParameters){

  Serial.println("Temp");
  float tempPrev=0.0;
  
  
 
  for (;;) {
  struct message msgT;
  //vTaskDelay(pdMS_TO_TICKS(1000));
  int adcValueTemp = analogRead(PIN_ADC1); 
  Serial.println("Temp:1 "+String(adcValueTemp));

  float voltageT = (float)adcValueTemp / RES * 3.3;
  float  Rt = 10 * voltageT / (3.3 - voltageT);
  float tempK = 1 / (1 / (273.15 + 25) + log(Rt / 10) / 3950.0); //calculate temperature (Kelvin)
  float tempCa = tempK - 273.15;
  float tempC=ALPHA*tempCa+ (1-ALPHA)* tempPrev;
  tempPrev= tempCa;
  if (Serial.availableForWrite()>1){
    Serial.print("Temp:2 ");
    Serial.print(tempC);
    Serial.println();
  }
  
  msgT.line=10;
  msgT.val=tempC;
  msgT.unit='C';
  if (Serial.availableForWrite()>1){
  Serial.print("msgT->val");
  //Serial.print(msgT.val);
  Serial.println();
  }
  //strncpy(msgT.msg,String(tempC).c_str(),strnlen(msgT.msg,128));
  if (xQueueSend(dispQueue,( void * ) &msgT,10)!= pdPASS){
    Serial.println("send failed");
  }
 
  //Serial.println("VoltageT: " + String(voltageT) + "V"+"Temperature: " + String(tempC) + "C");
  
  vTaskDelay(pdMS_TO_TICKS(500));
}
}
void vRcvQueue(void *pvParameters){
  
   for (;;) {
    struct message msgR;
  if (xQueueReceive(dispQueue,&msgR,10) !=pdPASS){
  //Serial.println("receive failed");
  }
  else{
  float valore=msgR.val;
  Serial.print("val:"+String(valore));
  Serial.println();
  
  display.setTextSize(1);
  //display.setCursor(0, msgR.line);
  //display.setTextColor(WHITE,BLACK);
  //display.println("         ");
  //display.display();
    display.setCursor(0, msgR.line);
  display.setTextColor(WHITE,BLACK);
  display.println(String(msgR.val) + " "+String(msgR.unit)+" ");
  display.display();
  }
   }
}

void initdbg() {
 Serial.begin(115200);
 // vTaskDelay(1000/portTICK_RATE_MS);
 delay(1000);
 
 Serial.println("Raspberry Pi Pico initialization completed!");
}
void initGPIO(){
   pinMode(PIN_LEDB, OUTPUT);
   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(PIN_ADC0,INPUT);
   pinMode(PIN_ADC1,INPUT);
   pinMode(PIN_ADC2,INPUT);
   analogReadResolution(10);
}
void initDisplay(){
if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println("SSD1306 allocation failed");
    for(;;);
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


void setup ()
{
   initdbg();
   initGPIO();
   initDisplay();
   float d=1.0;
   
   dispQueue=xQueueCreate(10,sizeof( struct message ));


   Serial.println("Step1"+ String(d));  
 
xTaskCreate(vBlinkTask, "Blink Task", 128, NULL, 1, NULL);
xTaskCreate(vReadtemp, "Temp Task", 128, NULL, 1, NULL);
xTaskCreate(vRcvQueue, "Receive Task", 128, NULL, 1, NULL);
xTaskCreate(vReadpot, "Pot Task", 128, NULL, 1, NULL);
xTaskCreate(vReadPhoto, "Lum Task", 128, NULL, 1, NULL);
xTaskCreate(vWatchDog, "WatchDog Task", 128, NULL, 1, NULL);

//Serial.println("task Create");
//vTaskStartScheduler();

}

void loop() {

    


  // 

  //vBlinkTask(NULL);

  

   
 
 

}
