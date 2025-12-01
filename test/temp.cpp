#include <Arduino.h>

#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <DHT.h>

#include <WiFi.h>
#include <ESP32Time.h>

#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_BUTTON 

#define DHTPIN 13
#define DHTTYPE DHT11

#define PULSE_PIN 33

DHT_Unified dht(DHTPIN, DHTTYPE);

#define SSID "Ahmed"
#define PASSWORD "*asdf1234#"

ESP32Time rtc(0);

#define gmOffset 7200     // (GMT+2) in seconds
#define dayLightSaving 0 
#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "time.nist.gov" 

typedef struct{
  String date;
  String time;
  String AmPm;
}timeStrings;

U8G2_SH1106_128X64_NONAME_F_HW_I2C screen(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);

TaskHandle_t readDHT_handle;
TaskHandle_t readPulseSensor_handle;
TaskHandle_t readRTC_handle;
TaskHandle_t screenDisplay_handle;

void readDHT(void* parameters){
  for(;;){
    vTaskDelay(1000 / portTICK_RATE_MS);

    sensors_event_t event;
    dht.temperature().getEvent(&event);

    if(isnan(event.temperature)){
      Serial.println("DHT11 FAILED TO READ TEMP");
    }else{
      Serial.print("DHT11 Temp = ");
      Serial.print(event.temperature);
      Serial.println("°C");
    }

    dht.humidity().getEvent(&event);

    if(isnan(event.relative_humidity)){
      Serial.println("DHT11 FAILED TO READ RELATIVE HUMIDITY"); 
    }else{
      Serial.print("DHT11 REL_HUMIDITY = ");
      Serial.print(event.relative_humidity);
      Serial.println("%");  
    }

    Serial.print("Free Stack DHT: ");
    Serial.println(uxTaskGetStackHighWaterMark(readDHT_handle));
  }
}

void readPulseSensor(void *parameters){
  uint32_t lastPeakTime = 0;
  uint16_t threshold = 2650;
  uint32_t timeDelay = 400;
  bool pulseOcurred = false;

  uint8_t READING_NUM = 10;
  uint8_t CURRENT_INDEX = 0;
  uint16_t lastPulses[READING_NUM] = {0,0,0,0,0,0,0,0,0,0};

  uint16_t signal;
  uint16_t BPM = 0;
  uint16_t tempBPM;

  for(;;){
    signal = analogRead(PULSE_PIN);

    if(signal > threshold && !pulseOcurred){
      
      uint32_t currentTime = millis();

      if(currentTime - lastPeakTime > timeDelay){

        tempBPM = 60000 / (currentTime - lastPeakTime);
        
        if(tempBPM>40 && tempBPM<120){
          lastPulses[CURRENT_INDEX] = tempBPM;
          CURRENT_INDEX = (CURRENT_INDEX+1)%10;
        }

        lastPeakTime = currentTime;
        pulseOcurred = !pulseOcurred;

      }

      uint16_t sumBPM = 0;
      uint8_t validReading = 0;

      for(int i=0;i<=READING_NUM;i++){
        if(lastPulses[i]>0){
            sumBPM += lastPulses[i];
            validReading++;
        }
      }
      
      if(validReading>0){
        BPM = sumBPM/validReading;
      }

      Serial.print("Current BPM = ");
      Serial.println(BPM);

    }


    if(signal < (threshold-100) && pulseOcurred){
      pulseOcurred = !pulseOcurred;
    }

    vTaskDelay(10/portTICK_PERIOD_MS);

  }
}

void readRTC(void *parameters){
  tm timeInfo;
  timeStrings strTime;

  for(;;){
    if(getLocalTime(&timeInfo)){
      strTime.date = rtc.getDate(true);
      strTime.time = rtc.getTime();
      strTime.AmPm = rtc.getAmPm(true);

      Serial.print(strTime.date);
      Serial.printf("  %s  %s \n", strTime.time, strTime.AmPm);
    }else{
      Serial.println("FAILED TO READTIME");
    }

    vTaskDelay(1000/portTICK_PERIOD_MS);

  }
}

void setup(){
  Serial.begin(115200);

  delay(1000);
  
  dht.begin();
  
  WiFi.mode(WIFI_STA); // Set to station mode
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("Connecting to %s", SSID);

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(250);
  }

  configTime(gmOffset, dayLightSaving, ntpServer1, ntpServer2);

  xTaskCreatePinnedToCore(
    readDHT,
    "DHT SENSOR READING TASK",
    2048,
    NULL,
    1,
    &readDHT_handle,
    1    
  );

  xTaskCreatePinnedToCore(
    readPulseSensor,
    "READ PULSE SENSOR TASK",
    3000,
    NULL,
    1,
    &readPulseSensor_handle,
    1
  );

  xTaskCreatePinnedToCore(
    readRTC,
    "READ INTERNAL RTC TASK",
    3000,
    NULL,
    1,
    &readRTC_handle,
    1
  );
  
}

void loop(){

}