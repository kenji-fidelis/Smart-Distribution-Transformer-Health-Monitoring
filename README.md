# Transformer Health Monitoring System
## Introduction
An IoT-based transformer health monitoring prototype built using the ESP32. The system monitors transformer temperature (DS18B20), ambient conditions (DHT22), simulated load factor, and ADC voltage, then calculates a real-time health score using a predictive stress model. Sensor data is transmitted to Firebase for live visualization, while historical CSV data can be analyzed using Python to generate maintenance reports and health trend graphs.

## Components
- ESP32 Development Board
- DHT22 Temperature & Humidity Sensor (to measure ambient temp)
- DS18B20 Waterproof Digital Temperature Sensor (to measure oil temp inside the transformer)
- 10kΩ Potentiometer (to simulate transformer load)
- Breadboard
- Jumper Wires 
- Micro-USB Cable (for programming and power)
