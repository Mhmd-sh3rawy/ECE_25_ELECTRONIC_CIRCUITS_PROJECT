#include <Arduino.h>

void setup() {
  // Initialize the LED pin as an output
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  
  Serial.println("Magdy");

  for(int i=0;i<=255;i++){
    analogWrite(LED_BUILTIN, i);
    delay(10);
  }

  for(int i=255;i>=0;i--){
    analogWrite(LED_BUILTIN, i);
    delay(10);
  }

}

