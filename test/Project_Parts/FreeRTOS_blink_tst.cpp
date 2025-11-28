#include <Arduino.h>

TaskHandle_t task1_handle = NULL;

void task1(void *param){
  for(;;){
    digitalWrite(BUILTIN_LED, HIGH);
    vTaskDelay(1000/ portTICK_RATE_MS);
    digitalWrite(BUILTIN_LED, LOW);
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
}

void task2(void *param){
  for(;;){
    digitalWrite(BUILTIN_LED, HIGH);
    vTaskDelay(200/ portTICK_RATE_MS);
    digitalWrite(BUILTIN_LED, LOW);
    vTaskDelay(200 / portTICK_RATE_MS);
  }
}


void setup(){

  pinMode(BUILTIN_LED, OUTPUT);

  xTaskCreatePinnedToCore(
    task1,
    "Toggle LED1",
    1024,
    NULL,
    1,
    &task1_handle,
    1
  );
}

void loop(){

}