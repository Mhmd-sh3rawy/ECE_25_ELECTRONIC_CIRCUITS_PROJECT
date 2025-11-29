#include <Arduino.h>

#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <DHT.h>

#define DHTPIN 13
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);

TaskHandle_t readDHT_handle;

void readDHT(void* parameters){
  dht.begin();
  sensors_event_t event;
  sensor_t DHTsensor;

  dht.temperature().getSensor(&DHTsensor);
  dht.humidity().getSensor(&DHTsensor);
  dht.temperature().getEvent(&event);
  dht.humidity().getEvent(&event);

  for(;;){
    
    if(isnan(event.temperature)){
      Serial.println("DHT11 FAILED TO READ TEMP");
    }else{
      Serial.printf("DHT11 Temp = %f \n", event.temperature);
    }

    if(isnan(event.relative_humidity)){
      Serial.println("DHT11 FAILED TO READ RELATIVE HUMIDITY"); 
    }else{
      Serial.printf("DHT11 REL_HUMIDITY = %f \n", event.relative_humidity); 
    }
    Serial.print("Free Stack DHT: ");
    Serial.println(uxTaskGetStackHighWaterMark(readDHT_handle));
    vTaskDelay((DHTsensor.min_delay/1000) / portTICK_RATE_MS);
  }
}

void setup(){
  Serial.begin(115200);

  xTaskCreatePinnedToCore(
    readDHT,
    "DHT SENSOR READING TASK",
    4000,
    NULL,
    1,
    &readDHT_handle,
    1    
  );
}

void loop(){

}