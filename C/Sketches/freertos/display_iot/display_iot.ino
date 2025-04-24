#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <hardware/watchdog.h>
#include <DHT.h>

// C99 libraries
#include <cstdlib>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// Libraries for MQTT client, WiFi connection and SAS-token generation.

#include <WiFi.h>

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include <bearssl/bearssl.h>
#include <bearssl/bearssl_hmac.h>
#include <libb64/cdecode.h>

// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional sample headers
#include "iot_configs.h"

#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1
#define SERIAL_DEBUG 1
#define SERIAL_SENSOR_DEBUG 0
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

#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;rpipico)"

// Utility macros and defines
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define ONE_HOUR_IN_SECS 3600
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_PACKET_SIZE 1024

// Translate iot_configs.h defines into variables used by the sample
static const char* ssid = IOT_CONFIG_WIFI_SSID;
static const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const char* device_key = IOT_CONFIG_DEVICE_KEY;
static const int port = 8883;

// Memory allocated for the sample's variables and structures.
static WiFiClientSecure wifi_client;
static X509List cert((const char*)ca_pem);
static PubSubClient mqtt_client(wifi_client);
static az_iot_hub_client client;
static char sas_token[200];
static uint8_t signature[512];
static unsigned char encrypted_signature[32];
static char base64_decoded_device_key[32];
static unsigned long next_telemetry_send_time_ms = 0;
static char telemetry_topic[128];
static uint8_t telemetry_payload[100];
static uint32_t telemetry_send_count = 0;

#pragma region sensori
//DHT dht(GPIO22, DHTTYPE);
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
QueueHandle_t dispQueue;
struct message {
  int line;
  float val;
  char unit;
  int curs = 0;
};
// DHT dht(GPIO22, DHTTYPE);
#
void vBlinkTask(void *pvParameters) {

  for (;;) {

    digitalWrite(LED_BUILTIN, HIGH);

    if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("High");

    vTaskDelay(pdMS_TO_TICKS(500));
    // for(long i=1;i<100000000;i++);
    digitalWrite(LED_BUILTIN, LOW);

    if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("Low");

    vTaskDelay(pdMS_TO_TICKS(500));

    // for(long i=1;i<100000000;i++);
  }
}

void vReadpot(void *pvParameters) {
  for (;;) {
    struct message msgT;
    int adcVal = analogRead(PIN_ADC0);
    float voltage = adcVal / RES * 3.3;
    analogWrite(PIN_LEDB, map(adcVal, 0, RES, 0, 255));
    msgT.line = 20;
    msgT.val = voltage;
    msgT.unit = 'V';
    msgT.curs = 60;
    if (xQueueSend(dispQueue, (void *)&msgT, 10) != pdPASS) {
      if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
        Serial.println("send failed");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void vReadPhoto(void *pvParameters) {
  for (;;) {
    struct message msgP;
    int padvVal = analogRead(PIN_ADC2);

    if (SERIAL_DEBUG  && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("lum:1 " + String(padvVal));

    msgP.line = 20;
    msgP.val = padvVal;
    msgP.unit = ' ';
    if (xQueueSend(dispQueue, (void *)&msgP, 10) != pdPASS) {
      if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
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

    if (SERIAL_DEBUG  && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1)
      Serial.println("Temp:1 " + String(adcValueTemp));


    float voltageT = (float)adcValueTemp / RES * 3.3;
    float Rt = 10 * voltageT / (3.3 - voltageT);
    float tempK = 1 / (1 / (273.15 + 25) + log(Rt / 10) / 3950.0);  //calculate temperature (Kelvin)
    float tempCa = tempK - 273.15;
    float tempC = ALPHA * tempCa + (1 - ALPHA) * tempPrev;
    tempPrev = tempCa;


    if (SERIAL_DEBUG  && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1) {
      Serial.print("Temp:2 ");
      Serial.print(tempC);
      Serial.println();
    }

    msgT.line = 10;
    msgT.val = tempC;
    msgT.unit = 'C';

    if (SERIAL_DEBUG  && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1) {
      Serial.print("msgT->val");
      //Serial.print(msgT.val);
      Serial.println();
    }

    //strncpy(msgT.msg,String(tempC).c_str(),strnlen(msgT.msg,128));
    if (xQueueSend(dispQueue, (void *)&msgT, 10) != pdPASS) {
      if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
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

      if (SERIAL_DEBUG  && SERIAL_SENSOR_DEBUG && Serial.availableForWrite() > 1) {
        Serial.print("val:" + String(valore));
        Serial.println();
      }
/*
      display.setTextSize(1);
      display.setCursor(msgR.curs, msgR.line);
      display.setTextColor(WHITE, BLACK);
      display.println(String(msgR.val) + " " + String(msgR.unit) + " ");
      display.display();
      */
    }
  }
}



void initdbg() {
  if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
    Serial.begin(115200);

  // vTaskDelay(1000/portTICK_RATE_MS);
  delay(1000);
  if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
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
  /*
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    if (SERIAL_DEBUG && SERIAL_SENSOR_DEBUG )
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
*/
}
void initDHT() {


 // dht.begin();
}
#pragma endregion sensori

#pragma region iothub
static void connectToWiFi()
{
  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting to WIFI SSID ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

static void initializeTime()
{
  Serial.print("Setting time using SNTP");

  configTime(-5 * 3600, 0, "pool.ntp.org","time.nist.gov"); 

  time_t now = time(NULL);
  while (now < 1510592825)
  {
    delay(500);
    Serial.print(".");
    now = time(NULL);
  }
  Serial.println("done!");
}

static char* getCurrentLocalTimeString()
{
  time_t now = time(NULL);
  return ctime(&now);
}

static void printCurrentTime()
{
  Serial.print("Current time: ");
  Serial.print(getCurrentLocalTimeString());
}

void receivedCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
}

static void initializeClients()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  wifi_client.setTrustAnchors(&cert);
  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t*)host, strlen(host)),
          az_span_create((uint8_t*)device_id, strlen(device_id)),
          &options)))
  {
    Serial.println("Failed initializing Azure IoT Hub client");
    return;
  }

  mqtt_client.setServer(host, port);
  mqtt_client.setCallback(receivedCallback);
}

/*
 * @brief           Gets the number of seconds since UNIX epoch until now.
 * @return uint32_t Number of seconds.
 */
static uint32_t getSecondsSinceEpoch() { return (uint32_t)time(NULL); }

static int generateSasToken(char* sas_token, size_t size)
{
  az_span signature_span = az_span_create((uint8_t*)signature, sizeofarray(signature));
  az_span out_signature_span;
  az_span encrypted_signature_span
      = az_span_create((uint8_t*)encrypted_signature, sizeofarray(encrypted_signature));

  uint32_t expiration = getSecondsSinceEpoch() + ONE_HOUR_IN_SECS;

  // Get signature
  if (az_result_failed(az_iot_hub_client_sas_get_signature(
          &client, expiration, signature_span, &out_signature_span)))
  {
    Serial.println("Failed getting SAS signature");
    return 1;
  }

  // Base64-decode device key
  int base64_decoded_device_key_length
      = base64_decode_chars(device_key, strlen(device_key), base64_decoded_device_key);

  if (base64_decoded_device_key_length == 0)
  {
    Serial.println("Failed base64 decoding device key");
    return 1;
  }

  // SHA-256 encrypt
  br_hmac_key_context kc;
  br_hmac_key_init(
      &kc, &br_sha256_vtable, base64_decoded_device_key, base64_decoded_device_key_length);

  br_hmac_context hmac_ctx;
  br_hmac_init(&hmac_ctx, &kc, 32);
  br_hmac_update(&hmac_ctx, az_span_ptr(out_signature_span), az_span_size(out_signature_span));
  br_hmac_out(&hmac_ctx, encrypted_signature);

  // Base64 encode encrypted signature
  String b64enc_hmacsha256_signature = base64::encode(encrypted_signature, br_hmac_size(&hmac_ctx));

  az_span b64enc_hmacsha256_signature_span = az_span_create(
      (uint8_t*)b64enc_hmacsha256_signature.c_str(), b64enc_hmacsha256_signature.length());

  // URl-encode base64 encoded encrypted signature
  if (az_result_failed(az_iot_hub_client_sas_get_password(
          &client,
          expiration,
          b64enc_hmacsha256_signature_span,
          AZ_SPAN_EMPTY,
          sas_token,
          size,
          NULL)))
  {
    Serial.println("Failed getting SAS token");
    return 1;
  }

  return 0;
}

static int connectToAzureIoTHub()
{
  size_t client_id_length;
  char mqtt_client_id[128];
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length)))
  {
    Serial.println("Failed getting client id");
    return 1;
  }

  mqtt_client_id[client_id_length] = '\0';

  char mqtt_username[128];
  // Get the MQTT user name used to connect to IoT Hub
  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL)))
  {
    printf("Failed to get MQTT clientId, return code\n");
    return 1;
  }

  Serial.print("Client ID: ");
  Serial.println(mqtt_client_id);

  Serial.print("Username: ");
  Serial.println(mqtt_username);

  mqtt_client.setBufferSize(MQTT_PACKET_SIZE);

  while (!mqtt_client.connected())
  {
    time_t now = time(NULL);

    Serial.print("MQTT connecting ... ");

    if (mqtt_client.connect(mqtt_client_id, mqtt_username, sas_token))
    {
      Serial.println("connected.");
    }
    else
    {
      Serial.print("failed, status code =");
      Serial.print(mqtt_client.state());
      Serial.println(". Trying again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  mqtt_client.subscribe(AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC);

  return 0;
}

static void establishConnection()
{
  connectToWiFi();
  initializeTime();
  printCurrentTime();
  initializeClients();

  // The SAS token is valid for 1 hour by default in this sample.
  // After one hour the sample must be restarted, or the client won't be able
  // to connect/stay connected to the Azure IoT Hub.
  if (generateSasToken(sas_token, sizeofarray(sas_token)) != 0)
  {
    Serial.println("Failed generating MQTT password");
  }
  else
  {
    connectToAzureIoTHub();
  }

  digitalWrite(LED_BUILTIN, LOW);
}

#pragma endregion iothub
void setup1() {
 // initDHT();
 establishConnection();
}

void setup() {
  initdbg();
  initGPIO();
  initDisplay();

  float d = 1.0;

  //dispQueue = xQueueCreate(10, sizeof(struct message));

  if (SERIAL_DEBUG)
    Serial.println("Step1" + String(d));

 // xTaskCreate(vBlinkTask, "Blink Task", 128, NULL, 1, NULL);
  //xTaskCreate(vReadtemp, "Temp Task", 128, NULL, 1, NULL);
  //xTaskCreate(vRcvQueue, "Receive Task", 128, NULL, 1, NULL);
  //xTaskCreate(vReadpot, "Pot Task", 128, NULL, 1, NULL);
  //xTaskCreate(vReadPhoto, "Lum Task", 128, NULL, 1, NULL);
  //xTaskCreate(vWatchDog, "WatchDog Task", 128, NULL, 1, NULL);
  //xTaskCreate(vReadDHT, "Pot Task", 128, NULL, 1, NULL);

  char pcWriteBuffer[2048];
  //vTaskList(pcWriteBuffer);

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
 /*
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
*/

  //delay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)


  //Serial.print(F("°C "));

  //

  //vBlinkTask(NULL);
}
