# 📡 IoT Sensor Network — Embedded Motion Detection with RTOS and Custom Firmware
A low-power, real-time sensor network built from the ground up for reliable remote monitoring in constrained environments. This system uses ESP32 microcontrollers running FreeRTOS, with custom C drivers interfacing directly with IMU registers to detect and transmit movement events using Matter over Thread.

🔧 Designed to test and validate sensor fusion, embedded firmware, and register-level debugging under real-world conditions — with a sharp focus on observability and system stability.

## ⚙️ Tech Stack & Architecture
- ESP32 + FreeRTOS → Multitasking kernel for real-time responsiveness and low-power operation
- C/C++ Bare-Metal Firmware → Handwritten I2C drivers and register-level access to IMUs
- UART Serial Output → Raw sensor data and event flags for debugging and validation
- Linux Toolchain + GDB → Debugging and flashing via CLI and open-source toolchains
- Hardware Schematics → Custom PCB designs for sensor node power and connectivity
- Built entirely on the metal — no Arduino layers or vendor frameworks. Just clean, direct control over every register, pin, and interrupt.

## 🧠 Features and System Highlights
🔍 Real-Time IMU Monitoring
- Polls accelerometer and gyroscope registers to detect sudden movement and threshold-based events
- Configurable sensitivity, debounce, and output formatting for edge deployment
🔒 Custom I2C Drivers
- Firmware includes fully custom I2C implementation for sensor reads and bus recovery
- Enables tight control over timing, retries, and error handling in noisy environments
💬 Serial Telemetry for Debugging
- Outputs event triggers, timestamps, and diagnostic info over UART to a connected host
- Used for regression testing, live debugging, and logging
🪛 Hardware-Firmware Co-Design
- Hand-soldered prototype boards with modular breakout headers
- Designed for extensibility — additional sensors or radios can be added with minimal firmware changes

## 🧪 Why I Built It
This project was about proving reliability under constraint. I needed a motion detection system that was:
- Fast enough to detect micro-movements in real time
- Low-power enough to deploy on battery in the field
- And transparent enough to debug when things broke
So I built one — from firmware to hardware. I wrote all the drivers myself, tested everything over UART with gdb and a Linux toolchain, and got it running on FreeRTOS to support future task scaling (e.g., wireless transmission, storage buffering). This wasn’t just an experiment — it was an exercise in embedded rigor, real-time behavior, and debugging with minimal tools in constrained conditions.

👋 About Me
I’m an embedded systems engineer focused on sensor interfaces, real-time systems, and low-level performance. Whether I’m tuning telemetry pipelines for flight systems or hand-building sensor nodes like these, I bring a builder’s mindset and full-stack ownership from pinout to packet.
