#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h> ///add////
#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>

// تضمين ملف بيانات الاعتماد
// #include <credentials.h>

// تعريف بيانات WiFi (أو استخدم ملف credentials.h)
#ifndef WIFI_SSID
#define WIFI_SSID "Lamis's Galaxy A24"
#define WIFI_PASSWORD "lamis27.2005@"
#endif

// تعريف الأجهزة
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 22, 21);
MPU6050 mpu; ///
WebServer server(80);

// إعدادات عداد الخطوات
#define THRESHOLD 1.0 ////
#define BUFFER_LENGTH 15 ///
#define DEBOUNCE_DELAY 300 /// 

// هياكل البيانات
typedef struct {
  int stepCount;
  float avgMagnitude;
  bool stepDetected;
} StepData;

typedef struct {
  String ipAddress;
  bool wifiConnected;
} NetworkStatus;

// معالجات المهام
TaskHandle_t readMPU_handle;        ////////
TaskHandle_t stepDetection_handle;  ////////
TaskHandle_t displayUpdate_handle;   ///////
TaskHandle_t webServer_handle;

// معالجات الطوابير
QueueHandle_t mpuDataQueue_handle; //// 
QueueHandle_t stepDataQueue_handle; //// 
QueueHandle_t displayQueue_handle;   ////

// معالج Semaphore لإعادة تعيين العداد
SemaphoreHandle_t resetSemaphore_handle; ///

// متغيرات مشتركة
volatile int globalStepCount = 0; ///

// ============================================
// معالجات Web Server
// ============================================

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Step Counter</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<style>";
  html += "body{font-family:Arial;background:#f9f9f9;margin:0;padding:0;}";
  html += "#container{width:300px;margin:50px auto;padding:20px;background:#fff;border-radius:10px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}";
  html += "h1{text-align:center;color:#333;}";
  html += ".info{text-align:center;font-size:32px;color:#007bff;margin:20px 0;font-weight:bold;}";
  html += ".btn{display:block;width:100%;padding:15px;color:#fff;background:#007bff;border:none;border-radius:5px;font-size:18px;cursor:pointer;margin-top:20px;}";
  html += ".btn:hover{background:#0056b3;}";
  html += ".ip-box{text-align:center;color:#666;margin-top:20px;padding:10px;background:#f0f0f0;border-radius:5px;font-size:14px;}";
  html += "</style></head><body>";
  html += "<div id='container'>";
  html += "<h1>🚶 Step Counter</h1>";
  html += "<p class='info'>" + String(globalStepCount) + " خطوة</p>";
  html += "<button class='btn' onclick='location.href=\"/reset\"'>إعادة تعيين العداد</button>";
  html += "<div class='ip-box'>IP: " + WiFi.localIP().toString() + "</div>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleReset() {
  xSemaphoreGive(resetSemaphore_handle);
  server.send(200, "application/json", "{\"stepCount\":0}");
}

// ============================================
// مهمة قراءة MPU6050
// ============================================

void readMPU(void* parameters) {
  float accelerationData[3];
  
  for(;;) {
    // قراءة البيانات من MPU-6050
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    
    // تحويل للـ g (±16g range)
    accelerationData[0] = ax / 2048.0;
    accelerationData[1] = ay / 2048.0;
    accelerationData[2] = az / 2048.0;
    
    // إرسال البيانات إلى الطابور
    xQueueSend(mpuDataQueue_handle, &accelerationData, portMAX_DELAY);
    
    Serial.print("Free MPU Stack: ");
    Serial.println(uxTaskGetStackHighWaterMark(readMPU_handle));
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ============================================
// مهمة كشف الخطوات
// ============================================

void stepDetection(void* parameters) {
  float buffer[BUFFER_LENGTH] = {0};
  int bufferIndex = 0;
  float accelerationData[3];
  unsigned long lastStepTime = 0;
  StepData stepData = {0, 0, false};
  
  for(;;) {
    // استقبال بيانات التسارع
    if(xQueueReceive(mpuDataQueue_handle, &accelerationData, pdMS_TO_TICKS(50))) {
      
      // حساب المقدار الكلي
      float accelerationMagnitude = sqrt(
        accelerationData[0] * accelerationData[0] +
        accelerationData[1] * accelerationData[1] +
        accelerationData[2] * accelerationData[2]
      );
      
      // إضافة للبفر
      buffer[bufferIndex] = accelerationMagnitude;
      bufferIndex = (bufferIndex + 1) % BUFFER_LENGTH;
      
      // حساب المتوسط
      float avgMagnitude = 0;
      for (int i = 0; i < BUFFER_LENGTH; i++) {
        avgMagnitude += buffer[i];
      }
      avgMagnitude /= BUFFER_LENGTH;
      
      stepData.avgMagnitude = avgMagnitude;
      
      unsigned long currentMillis = millis();
      
      // كشف الخطوة
      if (accelerationMagnitude > (avgMagnitude + THRESHOLD)) {
        if (!stepData.stepDetected && (currentMillis - lastStepTime) > DEBOUNCE_DELAY) {
          globalStepCount++;
          stepData.stepCount = globalStepCount;
          stepData.stepDetected = true;
          lastStepTime = currentMillis;
          
          Serial.print("👟 Step detected! Total: ");
          Serial.println(globalStepCount);
          
          // إرسال تحديث للشاشة
          xQueueOverwrite(stepDataQueue_handle, &stepData);
        }
      } else {
        stepData.stepDetected = false;
      }
      
      // التحقق من إعادة تعيين العداد
      if(xSemaphoreTake(resetSemaphore_handle, 0)) {
        globalStepCount = 0;
        stepData.stepCount = 0;
        Serial.println("🔄 Step counter reset!");
        xQueueOverwrite(stepDataQueue_handle, &stepData);
      }
    }
    
    Serial.print("Free StepDetection Stack: ");
    Serial.println(uxTaskGetStackHighWaterMark(stepDetection_handle));
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ============================================
// مهمة تحديث الشاشة
// ============================================

void displayUpdate(void* parameters) {
  StepData stepData = {0, 0, false};
  
  for(;;) {
    // استقبال بيانات الخطوات
    xQueueReceive(stepDataQueue_handle, &stepData, pdMS_TO_TICKS(100));
    
    // تحديث الشاشة
    display.clearBuffer();
    
    // رسم إطار
    display.drawFrame(0, 0, 128, 64);
    
    // العنوان
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(25, 12, "Step Counter");
    
    // خط فاصل
    display.drawLine(5, 15, 123, 15);
    
    // عدد الخطوات (كبير)
    display.setFont(u8g2_font_logisoso20_tn);
    String steps = String(stepData.stepCount);
    int textWidth = steps.length() * 12;
    display.setCursor((128 - textWidth) / 2, 40);
    display.print(stepData.stepCount);
    
    // كلمة "خطوة"
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(45, 52, "Steps");
    
    // IP Address
    display.setFont(u8g2_font_5x7_tr);
    String ip = WiFi.localIP().toString();
    display.setCursor(2, 62);
    display.print("IP: " + ip);
    
    display.sendBuffer();
    
    Serial.print("Free Display Stack: ");
    Serial.println(uxTaskGetStackHighWaterMark(displayUpdate_handle));
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ============================================
// مهمة Web Server
// ============================================

void webServerTask(void* parameters) {
  for(;;) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ============================================
// دالة Setup
// ============================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=============================");
  Serial.println("MPU-6050 Step Counter - RTOS");
  Serial.println("=============================\n");
  
  // تهيئة I2C
  Wire.begin(21, 22);
  Wire.setClock(400000);
  
  // تهيئة الشاشة
  Serial.println("Initializing OLED...");
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(10, 30, "Step Counter");
  display.drawStr(20, 45, "Starting...");
  display.sendBuffer();
  delay(1000);
  
  // تهيئة MPU-6050
  Serial.println("Initializing MPU-6050...");
  mpu.initialize();
  
  if (!mpu.testConnection()) {
    Serial.println("❌ MPU-6050 connection failed!");
    display.clearBuffer();
    display.drawStr(5, 20, "MPU-6050 Error!");
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(5, 35, "Check wiring:");
    display.drawStr(5, 50, "VCC->3.3V GND->GND");
    display.drawStr(5, 58, "SCL->22 SDA->21");
    display.sendBuffer();
    while (1) delay(1000);
  }
  
  Serial.println("✅ MPU-6050 connected!");
  
  // ضبط إعدادات MPU-6050
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  
  // الاتصال بالواي فاي
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(10, 30, "Connecting WiFi");
  display.sendBuffer();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi Connected!");
    Serial.print("📱 IP Address: ");
    Serial.println(WiFi.localIP());
    
    display.clearBuffer();
    display.drawStr(5, 20, "WiFi Connected!");
    display.drawStr(5, 40, WiFi.localIP().toString().c_str());
    display.sendBuffer();
    delay(2000);
  } else {
    Serial.println("❌ WiFi connection failed!");
  }
  
  // إنشاء Semaphores
  resetSemaphore_handle = xSemaphoreCreateBinary();
  
  // إنشاء Queues
  mpuDataQueue_handle = xQueueCreate(10, sizeof(float) * 3);
  stepDataQueue_handle = xQueueCreate(1, sizeof(StepData));
  displayQueue_handle = xQueueCreate(1, sizeof(StepData));
  
  // بدء Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/reset", HTTP_GET, handleReset);
  server.begin();
  Serial.println("✅ HTTP server started");
  
  // إنشاء المهام
  xTaskCreatePinnedToCore(
    readMPU,
    "MPU6050 Reading Task",
    3000,
    NULL,
    2,
    &readMPU_handle,
    1
  );
  
  xTaskCreatePinnedToCore(
    stepDetection,
    "Step Detection Task",
    4000,
    NULL,
    2,
    &stepDetection_handle,
    1
  );
  
  xTaskCreatePinnedToCore(
    displayUpdate,
    "Display Update Task",
    4000,
    NULL,
    1,
    &displayUpdate_handle,
    1
  );
  
  xTaskCreatePinnedToCore(
    webServerTask,
    "Web Server Task",
    4000,
    NULL,
    1,
    &webServer_handle,
    1
  );
  
  Serial.println("\n=============================");
  Serial.println("Ready! Start walking...");
  Serial.println("=============================\n");
}

void loop() {
  // Loop فارغة - كل العمل يتم في المهام
}