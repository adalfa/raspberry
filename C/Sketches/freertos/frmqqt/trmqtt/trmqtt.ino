/*
   Chappie Weather upgrade/addition
   process wind speed direction and rain fall.
*/
#include "esp32/ulp.h"
//#include "ulptool.h"
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "certs.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "driver/pcnt.h"
#include <driver/adc.h>
#include <SimpleKalmanFilter.h>
#include <ESP32Time.h>
////
ESP32Time rtc;
WiFiClient wifiClient;
PubSubClient MQTTclient(mqtt_server, mqtt_port, wifiClient);
////
float CalculatedVoltage = 0.0f;
float kph = 0.0f;
float rain  = 0.0f;
/*
   PCNT PCNT_UNIT_0, PCNT_CHANNEL_0 GPIO_NUM_15 = pulse input pin
   PCNT PCNT_UNIT_1, PCNT_CHANNEL_0 GPIO_NUM_4 = pulse input pin
*/
pcnt_unit_t pcnt_unit00 = PCNT_UNIT_0; //pcnt unit 0 channel 0
pcnt_unit_t pcnt_unit10 = PCNT_UNIT_1; //pcnt unit 1 channel 0
//
//
hw_timer_t * timer = NULL;
//
#define evtAnemometer  ( 1 << 0 )
#define evtRainFall    ( 1 << 1 )
#define evtParseMQTT   ( 1 << 2 )
EventGroupHandle_t eg;
#define OneMinuteGroup ( evtAnemometer | evtRainFall )
////
QueueHandle_t xQ_Message; // payload and topic queue of MQTT payload and topic
const int payloadSize = 100;
struct stu_message
{
  char payload [payloadSize] = {'\0'};
  String topic ;
} x_message;
////
SemaphoreHandle_t sema_MQTT_KeepAlive;
SemaphoreHandle_t sema_mqttOK;
SemaphoreHandle_t sema_CalculatedVoltage;
////
int mqttOK = 0; // stores a count value that is used to cause an esp reset
volatile bool TimeSet = false;
////
/*
   A single subject has been subscribed to, the mqtt broker sends out "OK" messages if the client receives an OK message the mqttOK value is set back to zero.
*/
////
void IRAM_ATTR mqttCallback(char* topic, byte * payload, unsigned int length)
{
  memset( x_message.payload, '\0', payloadSize ); // clear payload char buffer
  x_message.topic = ""; //clear topic string buffer
  x_message.topic = topic; //store new topic
  int i = 0; // extract payload
  for ( i; i < length; i++)
  {
    x_message.payload[i] = ((char)payload[i]);
  }
  x_message.payload[i] = '\0';
  xQueueOverwrite( xQ_Message, (void *) &x_message );// send data to queue
} // void mqttCallback(char* topic, byte* payload, unsigned int length)
////
// interrupt service routine for WiFi events put into IRAM
void IRAM_ATTR WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      log_i("Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      log_i("WiFi client disconnected");
      break;
    default: break;
  }
} // void IRAM_ATTR WiFiEvent(WiFiEvent_t event)
////
void IRAM_ATTR onTimer()
{
  BaseType_t xHigherPriorityTaskWoken;
  xEventGroupSetBitsFromISR(eg, OneMinuteGroup, &xHigherPriorityTaskWoken);
} // void IRAM_ATTR onTimer()
////
void setup()
{
  eg = xEventGroupCreate(); // get an event group handle
  x_message.topic.reserve(100);
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);// using GPIO 34 wind direction
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);// using GPIO 39 current
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);// using GPIO 36 battery volts

  // hardware timer 4 set for one minute alarm
  timer = timerBegin( 3, 80, true );
  timerAttachInterrupt( timer, &onTimer, true );
  timerAlarmWrite(timer, 60000000, true);
  timerAlarmEnable(timer);
  /* Initialize PCNT's counter */
  int PCNT_H_LIM_VAL         = 3000;
  int PCNT_L_LIM_VAL         = -10;
  // 1st PCNT counter
  pcnt_config_t pcnt_config  = {};
  pcnt_config.pulse_gpio_num = GPIO_NUM_15;// Set PCNT input signal and control GPIOs
  pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
  pcnt_config.channel        = PCNT_CHANNEL_0;
  pcnt_config.unit           = PCNT_UNIT_0;
  // What to do on the positive / negative edge of pulse input?
  pcnt_config.pos_mode       = PCNT_COUNT_INC;   // Count up on the positive edge
  pcnt_config.neg_mode       = PCNT_COUNT_DIS;   // Count down disable
  // What to do when control input is low or high?
  pcnt_config.lctrl_mode     = PCNT_MODE_KEEP; // Keep the primary counter mode if low
  pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;    // Keep the primary counter mode if high
  // Set the maximum and minimum limit values to watch
  pcnt_config.counter_h_lim  = PCNT_H_LIM_VAL;
  pcnt_config.counter_l_lim  = PCNT_L_LIM_VAL;
  pcnt_unit_config(&pcnt_config); // Initialize PCNT unit
  pcnt_set_filter_value( PCNT_UNIT_0, 1); //Configure and enable the input filter
  pcnt_filter_enable( PCNT_UNIT_0 );
  pcnt_counter_pause( PCNT_UNIT_0 );
  pcnt_counter_clear( PCNT_UNIT_0 );
  pcnt_counter_resume( PCNT_UNIT_0); // start the show
  // setup 2nd PCNT
  pcnt_config = {};
  pcnt_config.pulse_gpio_num = GPIO_NUM_4;
  pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
  pcnt_config.channel        = PCNT_CHANNEL_0;
  pcnt_config.unit           = PCNT_UNIT_1;
  pcnt_config.pos_mode       = PCNT_COUNT_INC;
  pcnt_config.neg_mode       = PCNT_COUNT_DIS;
  pcnt_config.lctrl_mode     = PCNT_MODE_KEEP;
  pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;
  pcnt_config.counter_h_lim  = PCNT_H_LIM_VAL;
  pcnt_config.counter_l_lim  = PCNT_L_LIM_VAL;
  pcnt_unit_config(&pcnt_config);
  //pcnt_set_filter_value( PCNT_UNIT_1, 1 );
  //pcnt_filter_enable  ( PCNT_UNIT_1 );
  pcnt_counter_pause  ( PCNT_UNIT_1 );
  pcnt_counter_clear  ( PCNT_UNIT_1 );
  pcnt_counter_resume ( PCNT_UNIT_1 );
  //
  xQ_Message = xQueueCreate( 1, sizeof(stu_message) );
  //
  sema_CalculatedVoltage = xSemaphoreCreateBinary();
  xSemaphoreGive( sema_CalculatedVoltage );
  sema_mqttOK = xSemaphoreCreateBinary();
  xSemaphoreGive( sema_mqttOK );
  sema_MQTT_KeepAlive = xSemaphoreCreateBinary();
  ///
  xTaskCreatePinnedToCore( MQTTkeepalive, "MQTTkeepalive", 15000, NULL, 5, NULL, 1 );
  xTaskCreatePinnedToCore( fparseMQTT, "fparseMQTT", 10000, NULL, 5, NULL, 1 ); // assign all to core 1, WiFi in use.
  xTaskCreatePinnedToCore( fReadBattery, "fReadBattery", 4000, NULL, 3, NULL, 1 );
  xTaskCreatePinnedToCore( fReadCurrent, "fReadCurrent", 4000, NULL, 3, NULL, 1 );
  xTaskCreatePinnedToCore( fWindDirection, "fWindDirection", 10000, NULL, 4, NULL, 1 );
  xTaskCreatePinnedToCore( fAnemometer, "fAnemometer", 10000, NULL, 4, NULL, 1 );
  xTaskCreatePinnedToCore( fRainFall, "fRainFall", 10000, NULL, 4, NULL, 1 );
  xTaskCreatePinnedToCore( fmqttWatchDog, "fmqttWatchDog", 3000, NULL, 3, NULL, 1 ); // assign all to core 1
} //void setup()
static void init_ulp_program()
{

}
////
void fWindDirection( void *pvParameters )
// read the wind direction sensor, return heading in degrees
{
  float adcValue = 0.0f;
  uint64_t TimePastKalman  = esp_timer_get_time();
  SimpleKalmanFilter KF_ADC( 1.0f, 1.0f, .01f );
  float high = 0.0f;
  float low = 2000.0f;
  float ADscale = 3.3f / 4096.0f;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 100; //delay for mS
  int count = 0;
  String windDirection;
  windDirection.reserve(20);
  String MQTTinfo = "";
  MQTTinfo.reserve( 150 );
  while ( !MQTTclient.connected() )
  {
    vTaskDelay( 250 );
  }
  for (;;)
  {
    windDirection = "";
    adcValue = float( adc1_get_raw(ADC1_CHANNEL_6) ); //take a raw ADC reading
    KF_ADC.setProcessNoise( (esp_timer_get_time() - TimePastKalman) / 1000000.0f ); //get time, in microsecods, since last readings
    adcValue = KF_ADC.updateEstimate( adcValue ); // apply simple Kalman filter
    TimePastKalman = esp_timer_get_time(); // time of update complete
    adcValue = adcValue * ADscale;
    if ( (adcValue >= 0.0f) & (adcValue <= .25f )  )
    {
      // log_i( " n" );
      windDirection.concat( "N" );
    }
    if ( (adcValue > .25f) & (adcValue <= .6f ) )
    {
      //  log_i( " e" );
      windDirection.concat( "E" );
    }
    if ( (adcValue > 2.0f) & ( adcValue < 3.3f) )
    {
      //   log_i( " s" );
      windDirection.concat( "S");
    }
    if ( (adcValue >= 1.7f) & (adcValue < 2.0f ) )
    {
      // log_i( " w" );
      windDirection.concat( "W" );
    }
    if ( count >= 30 )
    {
      MQTTinfo.concat( String(kph, 2) );
      MQTTinfo.concat( ",");
      MQTTinfo.concat( windDirection );
      MQTTinfo.concat( ",");
      MQTTinfo.concat( String(rain, 2) );
      xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY );
      MQTTclient.publish( topicWSWDRF, MQTTinfo.c_str() );
      xSemaphoreGive( sema_MQTT_KeepAlive );
      count = 0;
    }
    count++;
    MQTTinfo = "";
    xLastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
  }
  vTaskDelete ( NULL );
}
// read rainfall
void fRainFall( void *pvParemeters )
{
  int count = 0;
  pcnt_counter_pause( PCNT_UNIT_1 );
  pcnt_counter_clear( PCNT_UNIT_1 );
  pcnt_counter_resume( PCNT_UNIT_1 );
  for  (;;)
  {
    xEventGroupWaitBits (eg, evtRainFall, pdTRUE, pdTRUE, portMAX_DELAY);
    pcnt_counter_pause( PCNT_UNIT_1 );
    pcnt_get_counter_value( PCNT_UNIT_1, &count );
    pcnt_counter_clear( PCNT_UNIT_1 );
    pcnt_counter_resume( PCNT_UNIT_1 );
    if ( count != 0 )
    {
      // 0.2794mm of rain per click clear clicks at mid night
      rain = 0.2794f * (float)count;
      //log_i( "count %d, rain rain = %f mm", count, rain );
    }
    if ( (rtc.getHour(true) == 0) && (rtc.getMinute() == 0) )
    {
      pcnt_counter_pause( PCNT_UNIT_1 );
      rain = 0.0;
      count = 0;
      pcnt_counter_clear( PCNT_UNIT_1 );
      pcnt_counter_resume( PCNT_UNIT_1 );
    }
  }
  vTaskDelete ( NULL );
}
////
void fAnemometer( void *pvParameters )
{
  //int count = 0;
  pcnt_counter_clear(PCNT_UNIT_0);
  pcnt_counter_resume(PCNT_UNIT_0);
  for (;;)
  {
    xEventGroupWaitBits (eg, evtAnemometer, pdTRUE, pdTRUE, portMAX_DELAY);
    pcnt_counter_pause( PCNT_UNIT_0 );
    pcnt_get_counter_value( PCNT_UNIT_0, &count); //int16_t *count
    // A wind speed of 2.4km/h causes the switch to close once per second
    kph = 2.4f * ((float)count / 60.0f);
    /*
      if ( count != 0 )
      {
        log_i( "count %d, wind KPH = %f", count, kph );
      }
    count = 0;
    */
    pcnt_counter_clear( PCNT_UNIT_0 );
    pcnt_counter_resume( PCNT_UNIT_0 );
  }
  vTaskDelete ( NULL );
}
//////
void fmqttWatchDog( void * paramater )
{
  int UpdateImeTrigger = 86400; //seconds in a day
  int UpdateTimeInterval = 86300; // 1st time update in 100 counts
  int maxNonMQTTresponse = 60;
  for (;;)
  {
    vTaskDelay( 1000 );
    if ( mqttOK >= maxNonMQTTresponse )
    {
      ESP.restart();
    }
    xSemaphoreTake( sema_mqttOK, portMAX_DELAY );
    mqttOK++;
    xSemaphoreGive( sema_mqttOK );
    UpdateTimeInterval++; // trigger new time get
    if ( UpdateTimeInterval >= UpdateImeTrigger )
    {
      TimeSet = false; // sets doneTime to false to get an updated time after a days count of seconds
      UpdateTimeInterval = 0;
    }
  }
  vTaskDelete( NULL );
}
//////
void fparseMQTT( void *pvParameters )
{
  struct stu_message px_message;
  for (;;)
  {
    if ( xQueueReceive(xQ_Message, &px_message, portMAX_DELAY) == pdTRUE )
    {
      // parse the time from the OK message and update MCU time
      if ( String(px_message.topic) == topicOK )
      {
        if ( !TimeSet)
        {
          String temp = "";
          temp =  px_message.payload[0];
          temp += px_message.payload[1];
          temp += px_message.payload[2];
          temp += px_message.payload[3];
          int year =  temp.toInt();
          temp = "";
          temp =  px_message.payload[5];
          temp += px_message.payload[6];
          int month =  temp.toInt();
          temp =  "";
          temp =  px_message.payload[8];
          temp += px_message.payload[9];
          int day =  temp.toInt();
          temp = "";
          temp = px_message.payload[11];
          temp += px_message.payload[12];
          int hour =  temp.toInt();
          temp = "";
          temp = px_message.payload[14];
          temp += px_message.payload[15];
          int min =  temp.toInt();
          rtc.setTime( 0, min, hour, day, month, year );
          log_i( "rtc  %s ", rtc.getTime() );
          TimeSet = true;
        }
      }
      //
    } //if ( xQueueReceive(xQ_Message, &px_message, portMAX_DELAY) == pdTRUE )
    xSemaphoreTake( sema_mqttOK, portMAX_DELAY );
    mqttOK = 0;
    xSemaphoreGive( sema_mqttOK );
  }
} // void fparseMQTT( void *pvParameters )#include <ESP32Time.h>
//////
void fReadCurrent( void * parameter )
{
  float ADbits = 4096.0f;
  float ref_voltage = 3.3f;
  uint64_t TimePastKalman  = esp_timer_get_time(); // used by the Kalman filter UpdateProcessNoise, time since last kalman calculation
  SimpleKalmanFilter KF_I( 1.0f, 1.0f, .01f );
  float mA = 0.0f;
  int   printCount = 0;
  /*
     185mv/A = 5 AMP MODULE
     100mv/A = 20 amp module
     66mv/A = 30 amp module
  */
  const float mVperAmp = 185.0f;
  float adcValue = 0;
  float Voltage = 0;
  float Power = 0.0;
  String powerInfo = "";
  powerInfo.reserve( 150 );
  while ( !MQTTclient.connected() )
  {
    vTaskDelay( 250 );
  }
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1000; //delay for mS
  for (;;)
  {
    adc1_get_raw(ADC1_CHANNEL_3); // read once discard reading
    adcValue = ( (float)adc1_get_raw(ADC1_CHANNEL_3) );
    //log_i( "adcValue I = %f", adcValue );
    Voltage = ( (adcValue * ref_voltage) / ADbits ) + offSET; // Gets you mV
    mA = Voltage / mVperAmp; // get amps
    KF_I.setProcessNoise( (esp_timer_get_time() - TimePastKalman) / 1000000.0f ); //get time, in microsecods, since last readings
    mA = KF_I.updateEstimate( mA ); // apply simple Kalman filter
    TimePastKalman = esp_timer_get_time(); // time of update complete
    printCount++;
    if ( printCount == 60 )
    {
      xSemaphoreTake( sema_CalculatedVoltage, portMAX_DELAY);
      Power = CalculatedVoltage * mA;
      log_i( "Voltage=%f mA=%f Power=%f", CalculatedVoltage, mA, Power );
      printCount = 0;
      powerInfo.concat( String(CalculatedVoltage, 2) );
      xSemaphoreGive( sema_CalculatedVoltage );
      powerInfo.concat( ",");
      powerInfo.concat( String(mA, 4) );
      powerInfo.concat( ",");
      powerInfo.concat( String(Power, 4) );
      xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY );
      MQTTclient.publish( topicPower, powerInfo.c_str() );
      xSemaphoreGive( sema_MQTT_KeepAlive );
      powerInfo = "";
    }
    xLastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
  }
  vTaskDelete( NULL );
} //void fReadCurrent( void * parameter )
////
void fReadBattery( void * parameter )
{
  float adcValue = 0.0f;
  const float r1 = 50500.0f; // R1 in ohm, 50K
  const float r2 = 10000.0f; // R2 in ohm, 10k potentiometer
  float Vbatt = 0.0f;
  int printCount = 0;
  float vRefScale = (3.3f / 4096.0f) * ((r1 + r2) / r2);
  uint64_t TimePastKalman  = esp_timer_get_time(); // used by the Kalman filter UpdateProcessNoise, time since last kalman calculation
  SimpleKalmanFilter KF_ADC_b( 1.0f, 1.0f, .01f );
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1000; //delay for mS
  for (;;)
  {
    adc1_get_raw(ADC1_CHANNEL_0); //read and discard
    adcValue = float( adc1_get_raw(ADC1_CHANNEL_0) ); //take a raw ADC reading
    KF_ADC_b.setProcessNoise( (esp_timer_get_time() - TimePastKalman) / 1000000.0f ); //get time, in microsecods, since last readings
    adcValue = KF_ADC_b.updateEstimate( adcValue ); // apply simple Kalman filter
    Vbatt = adcValue * vRefScale;
    xSemaphoreTake( sema_CalculatedVoltage, portMAX_DELAY );
    CalculatedVoltage = Vbatt;
    xSemaphoreGive( sema_CalculatedVoltage );
    /*
      printCount++;
      if ( printCount == 3 )
      {
      log_i( "Vbatt %f", Vbatt );
      printCount = 0;
      }
    */
    TimePastKalman = esp_timer_get_time(); // time of update complete
    xLastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
    //log_i( "fReadBattery %d",  uxTaskGetStackHighWaterMark( NULL ) );
  }
  vTaskDelete( NULL );
}
////
void MQTTkeepalive( void *pvParameters )
{
  sema_MQTT_KeepAlive   = xSemaphoreCreateBinary();
  xSemaphoreGive( sema_MQTT_KeepAlive ); // found keep alive can mess with a publish, stop keep alive during publish
  // setting must be set before a mqtt connection is made
  MQTTclient.setKeepAlive( 90 ); // setting keep alive to 90 seconds makes for a very reliable connection, must be set before the 1st connection is made.
  for (;;)
  {
    //check for a is-connected and if the WiFi 'thinks' its connected, found checking on both is more realible than just a single check
    if ( (wifiClient.connected()) && (WiFi.status() == WL_CONNECTED) )
    {
      xSemaphoreTake( sema_MQTT_KeepAlive, portMAX_DELAY ); // whiles MQTTlient.loop() is running no other mqtt operations should be in process
      MQTTclient.loop();
      xSemaphoreGive( sema_MQTT_KeepAlive );
    }
    else {
      log_i( "MQTT keep alive found MQTT status %s WiFi status %s", String(wifiClient.connected()), String(WiFi.status()) );
      if ( !(wifiClient.connected()) || !(WiFi.status() == WL_CONNECTED) )
      {
        connectToWiFi();
      }
      connectToMQTT();
    }
    vTaskDelay( 250 ); //task runs approx every 250 mS
  }
  vTaskDelete ( NULL );
}
////
void connectToWiFi()
{
  int TryCount = 0;
  while ( WiFi.status() != WL_CONNECTED )
  {
    TryCount++;
    WiFi.disconnect();
    WiFi.begin( SSID, PASSWORD );
    vTaskDelay( 4000 );
    if ( TryCount == 10 )
    {
      ESP.restart();
    }
  }
  WiFi.onEvent( WiFiEvent );
} // void connectToWiFi()
////
void connectToMQTT()
{
  MQTTclient.setKeepAlive( 90 ); // needs be made before connecting
  byte mac[5];
  WiFi.macAddress(mac);
  String clientID = String(mac[0]) + String(mac[4]) ; // use mac address to create clientID
  while ( !MQTTclient.connected() )
  {
    // boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
    MQTTclient.connect( clientID.c_str(), mqtt_username, mqtt_password, NULL , 1, true, NULL );
    vTaskDelay( 250 );
  }
  MQTTclient.setCallback( mqttCallback );
  MQTTclient.subscribe( topicOK );
} // void connectToMQTT()
////
void loop() {}


