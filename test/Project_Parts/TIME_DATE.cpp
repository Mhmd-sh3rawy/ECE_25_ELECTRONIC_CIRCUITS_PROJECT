#include <WiFi.h>
#include <ESP32Time.h>

// Wifi Connection Params
#define ssid "Ahmed"
#define password "*asdf1234#"

//ESP32Time object created;
ESP32Time rtc(0); 

// define params for configuring time
#define gmOffset 7200
#define dayLightSavingOffset 0
#define ntpServer1 "pool.ntp.org"
//#define ntpServer2 "time.nist.gov"

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);

  while(WiFi.status()!=WL_CONNECTED){
    Serial.print(".");
  }

  Serial.printf("\nConnected Successfully to %s", ssid);
  Serial.print("\nLocal IP: ");
  Serial.println(WiFi.localIP());

  configTime(gmOffset, dayLightSavingOffset, ntpServer1);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    rtc.setTimeStruct(timeinfo);
  }
}

void loop() {

  struct tm timeinfo = rtc.getTimeStruct();
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");  
  delay(1000);
}
