#include <FreeRTOS.h>


#include <task.h>
//#define xPortSysTickHandler SysTick_Handler

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


void initdbg() {
 Serial.begin(115200);
 // vTaskDelay(1000/portTICK_RATE_MS);
 delay(1000);
 Serial.println("Raspberry Pi Pico initialization completed!");
}
void setup ()
{
   initdbg();
   Serial.println("Step1");  
pinMode(LED_BUILTIN, OUTPUT);
xTaskCreate(vBlinkTask, "Blink Task", 128, NULL, 1, NULL);
Serial.println("task Create");
//vTaskStartScheduler();

}

void loop() {

    


  // 

  //vBlinkTask(NULL);

  

   
 
 

}

