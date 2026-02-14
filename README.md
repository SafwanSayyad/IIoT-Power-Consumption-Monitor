# IIoT Power Consumption Monitor
### **Industrial IoT Gateway for Real-time Energy Analytics**

## Overview
This project addresses critical industrial challenges like electromagnetic interference (EMI) and signal degradation by implementing an **RS-485 Modbus RTU** communication network. It monitors RMS voltage, current, and power factor, transmitting data directly to a **Telegram Bot** via secure HTTPS.

## Technical Architecture
- **Protocol:** Modbus RTU over RS-485 Physical Layer.
- **Communication:** Differential signaling for high noise immunity in factory environments.
- **Cloud Integration:** Direct HTTPS POST requests to Telegram Bot API (No intermediary broker needed).

## Tech Stack
- **Master Controller:** ESP32 DevKit V1.
- **Sensing Module:** PZEM-004T V3.0.
- **Physical Interface:** MAX485 Transceiver (TTL to RS-485 conversion).
- **Language:** Embedded C++ (Arduino Framework).

## Threshold & Alert Logic
The system includes an automated safety layer:
- **Voltage Limit:** 250.0V
- **Current Limit:** 20.0A
- **Action:** Triggers a local LED (GPIO 25) and sends an immediate "CRITICAL ALERT" message to Telegram.
