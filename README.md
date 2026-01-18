# ESP32 FreeRTOS Smart Watch

## Overview
This project implements a multi-sensor smart watch firmware based on **ESP32** and **FreeRTOS**.  
The system is designed as a real-time embedded application with multiple concurrent tasks handling sensing, processing, networking, and user interface rendering.

The firmware demonstrates professional embedded system design principles such as task separation, inter-task communication, interrupt-driven inputs, and deterministic scheduling.

---

## Key Features
- Real-Time Operating System (FreeRTOS) based architecture
- Multi-tasking with priority-based scheduling
- RTC with manual date and time editing
- Temperature and humidity monitoring using DHT11
- Motion tracking and step counting using MPU6050
- Heart rate monitoring using an analog pulse sensor
- Weather information via OpenWeather API
- OLED display (SH1106) with button-based UI navigation
- Queue- and semaphore-based inter-task communication

---

## System Architecture
The firmware follows a **producer-consumer** architecture:

- Sensor tasks produce data
- Processing tasks analyze sensor data
- Display task consumes processed data and renders UI
- Interrupts signal user input via semaphores

All tasks are managed by the FreeRTOS kernel and pinned to a single ESP32 core for simplified synchronization.

---

## Tasks Overview

| Task Name            | Priority | Responsibility |
|----------------------|----------|----------------|
| readRTC              | 1        | Timekeeping and manual edits |
| readDHT              | 1        | Temperature and humidity sampling |
| readMPU              | 2        | Accelerometer sampling (100 Hz) |
| stepDetection        | 2        | Step counting algorithm |
| readPulseSensor      | 1        | Heart rate measurement |
| openWeatherGet       | 1        | Weather API communication |
| screenDisplay        | 1        | OLED UI rendering |

---

## Hardware Components
- ESP32 Development Board
- SH1106 OLED Display (I2C)
- DHT11 Temperature and Humidity Sensor
- MPU6050 Accelerometer
- Analog Pulse Sensor
- Push Buttons (UI control)

---

## Pin Configuration

| GPIO | Function |
|-----:|---------|
| 13   | DHT11 Data |
| 18   | Screen Change Button |
| 21   | I2C SDA (OLED + MPU6050) |
| 22   | I2C SCL (OLED + MPU6050) |
| 25   | Increment Button |
| 26   | Decrement Button |
| 32   | Edit Enable Button |
| 33   | Pulse Sensor (ADC) |

---

## Inter-Task Communication
- **Queues** are used to safely transfer sensor and processed data between tasks
- **Binary Semaphores** are used for signaling events from ISRs to tasks
- ISRs are minimal and defer logic to tasks, following FreeRTOS best practices

---

## Setup and Build
1. Install Arduino IDE or PlatformIO
2. Install ESP32 board support
3. Install required libraries:
   - FreeRTOS (ESP32 core)
   - Adafruit Sensor
   - DHT Sensor Library
   - U8g2
   - MPU6050 library
4. Configure Wi-Fi credentials and OpenWeather API key
5. Build and flash the firmware to ESP32

---

## Design Patterns Used
- Producer–Consumer Pattern
- State Machine for UI navigation
- Command Pattern using semaphores
- Adaptive thresholding for step detection

---

## Known Limitations and Improvements
- Time editing overflow handling requires refinement
- Wi-Fi connection lacks timeout handling
- No retry mechanism for failed HTTP requests

Planned enhancements include low-power modes, BLE integration, SD card logging, and advanced motion classification.

---

## Project Structure
- ISRs: Handle button interrupts
- Tasks: Independent functional modules
- Queues: Data exchange
- Semaphores: Event signaling
- Display: Centralized UI rendering task

---

## References
- ESP32 Technical Reference Manual  
  https://www.espressif.com/en/products/socs/esp32/resources

- FreeRTOS Official Documentation  
  https://www.freertos.org/Documentation/RTOS_book.html

- ESP32 FreeRTOS Integration  
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html

- OpenWeather API Documentation  
  https://openweathermap.org/api

- MPU6050 Datasheet  
  https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/

---

## License
This project is intended for educational and research purposes.  
You may modify and reuse it with proper attribution.
