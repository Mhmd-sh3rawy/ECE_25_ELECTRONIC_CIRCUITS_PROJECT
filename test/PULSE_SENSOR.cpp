#include <Arduino.h>

// Pin Definitions
const int PULSE_PIN = 33;        // Use ADC1 pin (GPIO 34) for pulse sensor
const int BLINK_PIN = 2;         // Built-in LED on most ESP32 boards

// Volatile Variables for interrupt service routine
volatile int BPM;                
volatile int Signal;             
volatile int IBI = 600;          
volatile boolean Pulse = false;  
volatile boolean QS = false;     

static boolean serialVisual = true;

volatile int rate[10];           
volatile unsigned long sampleCounter = 0;
volatile unsigned long lastBeatTime = 0;
volatile int P = 2048;           // Adjusted for ESP32 12-bit ADC (0-4095)
volatile int T = 2048;           
volatile int thresh = 2100;      // Adjusted threshold
volatile int amp = 100;          
volatile boolean firstBeat = true;
volatile boolean secondBeat = false;

// Timer variables
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void arduinoSerialMonitorVisual(char symbol, int data);
void sendDataToSerial(char symbol, int data);

// Timer interrupt function
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  Signal = analogRead(PULSE_PIN);
  sampleCounter += 2;
  int N = sampleCounter - lastBeatTime;

  // Find trough
  if(Signal < thresh && N > (IBI/5)*3) {
    if (Signal < T) {
      T = Signal;
    }
  }

  // Find peak
  if(Signal > thresh && Signal > P) {
    P = Signal;
  }

  // Look for heartbeat
  if (N > 250) {
    if ((Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3)) {
      Pulse = true;
      digitalWrite(BLINK_PIN, HIGH);
      IBI = sampleCounter - lastBeatTime;
      lastBeatTime = sampleCounter;

      if(secondBeat) {
        secondBeat = false;
        for(int i = 0; i <= 9; i++) {
          rate[i] = IBI;
        }
      }

      if(firstBeat) {
        firstBeat = false;
        secondBeat = true;
        portEXIT_CRITICAL_ISR(&timerMux);
        return;
      }

      word runningTotal = 0;
      for(int i = 0; i <= 8; i++) {
        rate[i] = rate[i+1];
        runningTotal += rate[i];
      }

      rate[9] = IBI;
      runningTotal += rate[9];
      runningTotal /= 10;
      BPM = 60000/runningTotal;
      QS = true;
    }
  }

  if (Signal < thresh && Pulse == true) {
    digitalWrite(BLINK_PIN, LOW);
    Pulse = false;
    amp = P - T;
    thresh = amp/2 + T;
    P = thresh;
    T = thresh;
  }

  if (N > 2500) {
    thresh = 2048;
    P = 2048;
    T = 2048;
    lastBeatTime = sampleCounter;
    firstBeat = true;
    secondBeat = false;
  }

  portEXIT_CRITICAL_ISR(&timerMux);
}

void interruptSetup() {
  // Timer setup for ESP32: 80MHz clock, prescaler 80 = 1MHz
  // Timer counts every 1μs, interrupt every 2000μs (2ms) = 500Hz
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000, true);  // 2000μs = 2ms
  timerAlarmEnable(timer);
}

void serialOutput() {
  if (serialVisual == true) {
    arduinoSerialMonitorVisual('-', Signal);
  } else {
    sendDataToSerial('S', Signal);
  }
}

void serialOutputWhenBeatHappens() {
  if (serialVisual == true) {
    Serial.print(" Heart-Beat Found ");
    Serial.print("BPM: ");
    Serial.println(BPM);
  } else {
    sendDataToSerial('B', BPM);
    sendDataToSerial('Q', IBI);
  }
}

void arduinoSerialMonitorVisual(char symbol, int data) {
  const int sensorMin = 0;
  const int sensorMax = 4095;  // ESP32 has 12-bit ADC
  int sensorReading = data;
  int range = map(sensorReading, sensorMin, sensorMax, 0, 11);
  
  // Display visual bar
  for (int i = 0; i < range; i++) {
    Serial.print(symbol);
  }
  Serial.println();
}

void sendDataToSerial(char symbol, int data) {
  Serial.print(symbol);
  Serial.println(data);
}

void setup() {
  pinMode(BLINK_PIN, OUTPUT);
  Serial.begin(115200);
  
  // Configure ADC
  analogReadResolution(12);  // ESP32 12-bit resolution
  analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
  
  interruptSetup();
  
  Serial.println("Pulse Sensor Initialized");
}

void loop() {
  serialOutput();
  
  if (QS == true) {
    serialOutputWhenBeatHappens();
    QS = false;
  }
  
  delay(20);
}