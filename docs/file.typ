#set document(
  title: "ESP32 FreeRTOS Watch Monitor Documentation",
  author: "Mohammed"
)
#set page(paper: "a4", margin: (x: 2cm, y: 2cm))
#set text(font: "New Computer Modern", size: 10pt)
#set par(justify: true, leading: 0.55em)
#set heading(numbering: "1.1")

#show raw.where(block: true): it => block(
  fill: luma(245),
  inset: 8pt,
  radius: 3pt,
  width: 100%,
  it
)

#align(center)[
  #text(size: 22pt, weight: "bold")[ESP32 FreeRTOS Smart Watch]
  #v(200pt)
  #text(size: 14pt)[
Alaa Maher Eldeeb \ Ezzat Saeed Fotouh \ Lamis Mohammed Elkhateeb \ Mahmoud Alaa Mahmoud \ Mohammed Abdelbaset Elshaarawy \ Waad Gamal Emam \ Yara Adel Abdelfatah]
]

#v(1em)
#pagebreak()
#outline(indent: auto, depth: 2)
#pagebreak()

= Introduction

This ESP32 firmware implements a multi-sensor smart watch using FreeRTOS for concurrent task execution. Features: RTC with manual adjustment, environmental monitoring (DHT11), motion tracking with step counting (MPU6050), heart rate monitoring (analog pulse sensor), weather API integration, and OLED display (SH1106) with button-based UI.

*Key principle:* This is a concurrent, real-time embedded system using proper task separation, inter-process communication, and interrupt handling.

= FreeRTOS Fundamentals

== Core Concepts

*FreeRTOS* is a preemptive, priority-based scheduler enabling multiple concurrent tasks. Unlike Arduino's sequential `loop()`, FreeRTOS provides: multiple independent tasks, priority-based scheduling, safe inter-task communication via queues/semaphores, and deterministic timing.

*Tasks:* Independent execution threads with their own stack, priority, and entry function running in infinite loops.

*Queues:* Thread-safe FIFO data transfer. Producers write sensor data, consumers read it. No race conditions.

*Semaphores:* Signaling mechanisms for synchronization. Binary semaphores signal events from ISRs to tasks.

*ISRs (Interrupt Service Routines):* Handle hardware events. Must be fast, marked `IRAM_ATTR`, and defer work to tasks via semaphores.

== Scheduling and Priorities

FreeRTOS uses preemptive priority scheduling: higher priority tasks always run first. If a high-priority task becomes ready, it immediately preempts lower-priority tasks. Equal priority tasks share CPU (round-robin).

*This firmware:* Motion processing (MPU, step detection) at priority 2; all other tasks at priority 1.
#pagebreak()
= System Architecture

== High-Level Design

=== ASCII diagram of code architecture
#block(
  fill: luma(245),
  inset: 3pt,
  radius: 6pt,
)[
```text
                 ┌──────────────────────┐
                 │        ESP32         │
                 │   FreeRTOS Kernel    │
                 └───────────┬──────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼───────┐   ┌────────▼────────┐   ┌────────▼────────┐
│   ISRs        │   │  Sensor Tasks   │   │  Network Tasks  │
│ (GPIO Buttons)│   │                 │   │                 │
└───────┬───────┘   │ readDHT         │   │ OpenWeather API │
        │           │ readPulse       │   │ NTP / RTC Sync  │
        │           │ readMPU         │   └────────┬────────┘
        │           └────────┬────────┘            │
        │                    │                     │
        │        ┌───────────▼───────────┐         │
        │        │   Processing Task     │         │
        │        │  Step Detection       │         │
        │        └───────────┬───────────┘         │
        │                    │                     │
        └──────────────┬─────▼─────────────────────▼────────┐
                       │        FreeRTOS IPC                │
                       │  Queues + Binary Semaphores        │
                       └──────────────┬─────────────────────┘
                                      │
                           ┌──────────▼──────────┐
                           │  screenDisplay Task │
                           │  (UI Renderer)      │
                           └──────────┬──────────┘
                                      │
                           ┌──────────▼──────────┐
                           │   SH1106 OLED       │
                           │   (U8g2 / I2C)      │
                           └─────────────────────┘

```
]
Data-flow architecture: Sensors (tasks) → Queues → Display (task). Interrupts signal events via semaphores.

*Seven concurrent tasks:*

#table(
  columns: (auto, auto, 1fr),
  [*Task*], [*Pri*], [*Responsibility*],
  [`readRTC`], [1], [Timekeeping, time edits],
  [`readDHT`], [1], [Temperature/humidity sampling],
  [`readMPU`], [2], [Accelerometer sampling (100Hz)],
  [`stepDetection`], [2], [Step counting algorithm],
  [`readPulseSensor`], [1], [Heart rate measurement],
  [`openWeatherGet`], [1], [HTTP weather API],
  [`screenDisplay`], [1], [UI rendering],
)

All tasks pinned to Core 1 for simplified shared state reasoning.

== Communication Channels

*Queues:* `screenRTCQueue` (time/date), `screenDHTQueue` (temp/humidity), `screenPulseQueue` (BPM), `screenOpenWeather` (weather), `mpuDataQueue` (acceleration), `stepDataQueue` (steps).

*Semaphores:* `timeIncerementSemaphore`, `timeDecrementSemaphore`, `resetSemaphore` for button signaling.

= Hardware Configuration

*Buttons:* GPIO 18 (screen change), 32 (edit enable), 25 (increment), 26 (decrement). All use `INPUT_PULLUP` and falling-edge interrupts.

*Sensors:* DHT11 on GPIO 13, Pulse sensor on GPIO 33 (ADC), MPU6050 and SH1106 OLED on I²C (SDA=21, SCL=22).

*Constants:* `DEBOUNCE_TIME=250ms`, `THRESHOLD=1.0g` (step detection), `BUFFER_LENGTH=15` (acceleration smoothing), `DEBOUNCE_DELAY=300ms` (step timing).

*MPU6050 config:* ±16g accelerometer range, raw-to-g conversion: `acceleration_g = raw_value / 2048.0`

= Data Structures

```c
// Time display
typedef struct {
  String date, time, AmPm;
} timeStrings;

// Environmental data
typedef struct {
  float temp, rh;
} DHT_sensor_data;

// Step counting
typedef struct {
  int stepCount;
  float avgMagnitude;
  bool stepDetected;
} StepData;

// Weather API
typedef struct {
  String description;
  float tempFeelLike, humidity, windSpeed;
} openWeatherJSONParsed;

// UI state (volatile - shared with ISRs)
typedef struct {
  volatile uint8_t screenCurrentIndex;        // 0-4
  volatile uint8_t currentBlinkingTimeField;  // 0-5
  volatile uint8_t timeChange;
} ScreenStatus;
```

*Global:* `volatile int globalStepCount = 0;`

= Interrupt Handlers

All ISRs use software debouncing (check `millis()` timestamp) and are marked `IRAM_ATTR`.

*Screen change ISR:* Cycles `screenCurrentIndex` (0→4→0).

*Edit enable ISR:* Cycles `currentBlinkingTimeField` (0→5→0) to select time/date field.

*Increment/Decrement ISRs:* Signal semaphores to RTC task:
```c
void IRAM_ATTR screenTimeIncrementButtonISR(){
  if(millis() - lastPress > DEBOUNCE_TIME){
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(timeIncerementSemaphore_handle, &woken);
    portYIELD_FROM_ISR(woken);
  }
}
```

*Reset ISR:* Signals `resetSemaphore` to clear step counter.

*Design rationale:* ISRs are fast (just state change or semaphore signal). Complex logic deferred to tasks.

= Task Implementations

== RTC Task

*Rate:* 1Hz | *Stack:* 3000 bytes

Reads time with `getLocalTime()`, checks increment/decrement semaphores with non-blocking `xSemaphoreTake(..., 0)`, modifies `tm` structure based on `currentBlinkingTimeField`, formats strings, publishes via `xQueueOverwrite()`.

```c
if(xSemaphoreTake(timeIncerementSemaphore_handle, 0)){
  switch(currentBlinkingTimeField){
    case 1: timeInfo.tm_min += 1; break;
    case 2: timeInfo.tm_hour += 1; break;
    case 3: timeInfo.tm_mday += 1; break;
    // ...
  }
  rtc.setTimeStruct(timeInfo);
}
```

== DHT Sensor Task

*Rate:* 1Hz | *Stack:* 2000 bytes

Reads temperature and humidity using Adafruit unified sensor API, validates with `isnan()`, publishes to `screenDHTQueue`.

== MPU6050 Task

*Rate:* 100Hz | *Stack:* 3000 bytes | *Priority:* 2

Samples accelerometer at high rate:
```c
int16_t ax, ay, az;
mpu.getAcceleration(&ax, &ay, &az);
accelerationData[0] = ax / 2048.0;  // Convert to g
xQueueSend(mpuDataQueue_handle, &accelerationData, portMAX_DELAY);
```

== Step Detection Task

*Rate:* 100Hz | *Stack:* 4000 bytes | *Priority:* 2

*Algorithm:* Magnitude-based peak detection with adaptive thresholding.

1. Receive acceleration vector from queue
2. Calculate magnitude: $sqrt(a_x^2 + a_y^2 + a_z^2)$
3. Update circular buffer (15 samples = 150ms window)
4. Calculate moving average (baseline)
5. Detect peak: `magnitude > baseline + THRESHOLD`
6. Debounce: 300ms minimum between steps
7. Increment `globalStepCount` and publish

```c
if(accelerationMagnitude > (avgMagnitude + THRESHOLD)){
  if(!stepDetected && (millis() - lastStepTime) > DEBOUNCE_DELAY){
    globalStepCount++;
    stepData.stepCount = globalStepCount;
    xQueueOverwrite(stepDataQueue_handle, &stepData);
  }
}
```

Check reset semaphore non-blocking to clear counter.

== Pulse Sensor Task

*Rate:* 100Hz | *Stack:* 3000 bytes

*Screen-conditional:* Only runs when `screenCurrentIndex == 2` (power optimization).

Peak detection algorithm:
- Read ADC: `signal = analogRead(PULSE_PIN)`
- Detect peaks above threshold (2450)
- Calculate BPM: `60000 / interval_ms`
- Validate range (40-120 BPM)
- Smooth with 10-sample moving average
- Publish to `screenPulseQueue`

Hysteresis on falling edge prevents rapid toggling.

== Weather API Task

*Rate:* 0.2Hz (5s delay) | *Stack:* 4000 bytes

HTTP GET to OpenWeather API, parse JSON response:
```c
httpClient.begin(openWeatherUrl);
if(httpClient.GET() > 0){
  String json = httpClient.getString();
  JSONVar parsed = JSON.parse(json);
  weatherInfo.description = parsed["weather"][0]["description"];
  weatherInfo.tempFeelLike = atof(parsed["main"]["feels_like"]);
  // ...
  xQueueSend(screenOpenWeather_handle, &weatherInfo, portMAX_DELAY);
}
```

== Display Task

*Rate:* 2.5Hz (400ms) | *Stack:* 5000 bytes

Central consumer task implementing UI state machine based on `screenCurrentIndex`:

*Screen 0 (Time/Date):* Complex blinking logic for editing. Helper function `createTimeDateFrames()` generates 7 frame variations with dashes replacing selected field. Toggle `currentBlinkingState` every 400ms for blink effect.

*Screen 1 (Temp/Humidity):* Simple two-line display from `screenDHTQueue`.

*Screen 2 (BPM):* Display heart rate from `screenPulseQueue`.

*Screen 3 (Weather):* Display description, temperature (convert from Kelvin: `temp - 273.25`), humidity, wind speed from `screenOpenWeather`.

*Screen 4 (Steps):* Frame border, title, large centered step count, IP address at bottom.

All `xQueueReceive()` use short timeouts (10-100ms) to remain responsive to screen changes.

= Setup and Initialization

```c
void setup(){
  Serial.begin(115200);
  dht.begin();
  
  // Button interrupts
  pinMode(SCREEN_CHANGE_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SCREEN_CHANGE_BUTTON), 
                  screenChangeButtonISR, FALLING);
  // ... (repeat for all 4 buttons)
  
  // Create RTOS primitives
  screenRTCQueue_handle = xQueueCreate(1, sizeof(timeStrings));
  timeIncerementSemaphore_handle = xSemaphoreCreateBinary();
  // ... (all queues and semaphores)
  
  // Initialize I²C devices
  Wire.begin();
  screen.begin();
  mpu.initialize();
  if(!mpu.testConnection()){
    // Display error and halt
    while(1) delay(1000);
  }
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
  
  // Wi-Fi connection with animated display
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while(WiFi.status() != WL_CONNECTED){
    // Show "Connecting..." animation
  }
  
  // NTP sync
  configTime(gmOffset, dayLightSaving, ntpServer1, ntpServer2);
  
  // Create all 7 tasks pinned to Core 1
  xTaskCreatePinnedToCore(readDHT, "DHT", 2000, NULL, 1, &readDHT_handle, 1);
  // ... (repeat for all tasks with appropriate stack/priority)
}

void loop(){} // Empty - FreeRTOS handles everything
```

= Design Patterns

*Producer-Consumer:* Queues decouple data production rates from consumption. DHT at 1Hz, MPU at 100Hz, display at 2.5Hz - all independent.

*Command Pattern:* ISRs signal intent via semaphores, tasks execute commands.

*State Machine:* `screenCurrentIndex` and `currentBlinkingTimeField` drive UI rendering.

*Adaptive Thresholding:* Moving average baseline for step detection works regardless of device orientation.

*Power Optimization:* Pulse sensor only runs when BPM screen active.

= Performance Analysis

*Stack usage:* ~24KB total (7% of 350KB available RAM). Display task uses most (5000 bytes due to multiple buffers).

*CPU utilization:* ~17% on Core 1. Motion tasks at 100Hz consume 5% each. Display at 2.5Hz uses 2.5%. Weather HTTP intermittent at 2%.

*Queue memory:* ~420 bytes total - negligible.

= Critical Issues & Improvements

*Bugs to fix:*
1. Time edit overflow: `tm_min += 1` at 59 → 60 (invalid). Use modulo or RTC wrapping functions.
2. Wi-Fi timeout: Infinite loop if network unavailable. Add timeout and offline mode.
3. HTTP retry: No error recovery for failed weather API calls.

*Enhancements:*
- Sleep mode when display off (80%+ power savings)
- SD card logging for step/heart history
- BLE for smartphone sync
- Battery monitoring with adaptive scheduling
- Machine learning for walk/run classification
- Gesture recognition (wrist raise, shake)

= Troubleshooting

*MPU not detected:* Check I²C wiring (SDA→21, SCL→22), verify power supply, try I²C scanner, add 4.7kΩ pull-ups.

*DHT returns NaN:* Increase task delay to 2s, verify pin 13, add 10kΩ pull-up.

*Steps not counting:* Lower THRESHOLD to 0.7, check MPU initialization, monitor serial for acceleration values.

*Pulse shows zero:* Navigate to screen 2, ensure firm finger contact, shield from light, adjust threshold (try 2000-3000).

*Weather not updating:* Check Wi-Fi connection, verify API key, monitor serial for HTTP codes.

*Stack overflow:* Increase task stack size, monitor with `uxTaskGetStackHighWaterMark()`.

*Debug technique - I²C scanner:*
```c
for(uint8_t addr = 1; addr < 127; addr++){
  Wire.beginTransmission(addr);
  if(Wire.endTransmission() == 0)
    Serial.printf("Device at 0x%02X\n", addr);
}
```

= Conclusion

This firmware demonstrates production-grade embedded systems architecture: modular task design, safe inter-task communication, interrupt-driven responsiveness, and deterministic real-time behavior. The FreeRTOS foundation scales from smartwatches to autonomous vehicles.

*Key takeaway:* Professional embedded systems succeed through clear architecture, explicit contracts (data structures), safe communication (queues/semaphores), and defensive coding - not clever tricks.

#pagebreak()

= Appendix: Quick Reference

== Pin Assignments
#table(
  columns: (auto, auto, 1fr),
  [*GPIO*], [*Type*], [*Function*],
  [13], [Digital], [DHT11 Data],
  [18], [Input], [Screen Change Button],
  [21], [I²C SDA], [OLED + MPU6050],
  [22], [I²C SCL], [OLED + MPU6050],
  [25], [Input], [Increment Button],
  [26], [Input], [Decrement Button],
  [32], [Input], [Edit Enable Button],
  [33], [ADC], [Pulse Sensor],
)

== Queue Reference
#table(
  columns: (auto, auto, auto),
  [*Queue*], [*Size*], [*Item Type*],
  [`screenRTCQueue`], [1], [`timeStrings`],
  [`screenDHTQueue`], [5], [`DHT_sensor_data`],
  [`screenPulseQueue`], [10], [`uint16_t`],
  [`screenOpenWeather`], [1], [`openWeatherJSONParsed`],
  [`mpuDataQueue`], [10], [`float[3]`],
  [`stepDataQueue`], [1], [`StepData`],
)

== Screen States
- 0: Time and Date (editable)
- 1: Temperature and Humidity
- 2: Heart Rate (BPM)
- 3: Weather Information
- 4: Step Counter

== Editing Fields (Screen 0)
- 0: No editing
- 1: Minutes
- 2: Hours
- 3: Day
- 4: Month
- 5: Year
