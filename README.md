# Aquality

![Aquality](https://res.cloudinary.com/de86eimvq/image/upload/v1729611982/portfolio/Projects/aquality/vtzrbqiherwqyrdtucas.png)

## 📋 Overview

Aquality is an IoT-based water quality monitoring and analysis system developed as a thesis project to address the challenges faced by the Rosario Water System, an LGU-operated utility serving over 11,000 households. The project tackled the specific problem of monitoring isolated cases of muddy water in their groundwater-sourced system through a comprehensive solution that combined IoT hardware prototypes installed in individual households, a web-based monitoring application, and a robust cloud database infrastructure. The system features real-time water quality data collection, SMS notifications for water quality alerts, and a reliable database mirroring technique between local SD card storage and cloud databases for data backup. Upon rigorous testing, the prototype demonstrated 90% accuracy compared to laboratory results, establishing itself as a cost-effective, reliable, and scalable solution for municipal water quality monitoring needs.

## 🎯 Objectives

- Design a monitoring system that assesses water quality parameters (pH level, turbidity, total dissolved solids, pressure, and temperature).
- Assemble a hardware system comprising an array of water sensors, a robust microprocessor, and a wireless communication module.
- Include an intuitive interface for displaying the recorded sensor readings.
- Implement database mirroring technique between local SD card and Firebase RTDB.
- Perform calibration techniques to match the accuracy of laboratory results.

## 🛠️ Tech Stack

- **Microcontroller:** ESP32
- **Firmware:** Arduino
- **Sensors:** DS18B20, DFRobot pH Sensor, DFRobot EC Sensor, Analog Turbidity Sensor, Pressure Sensor
- **Cloud Database:** Firebase RTDB
- **Libraries:** Firebase ESP Client, OneWire, DallasTemperature, LiquidCrystal_I2C, DFRobot_ESP_PH, DFRobot_ESP_EC, ArduinoJson, SPI, SD Card
- **Tools:** Arduino IDE

## ✨ Features

- Real-time monitoring
- Calibrated sensor readings
- Cloud database integration

## 📄 License

This project is licensed under the [MIT License](LICENSE).

## 📞 Contact

Antonio Malabago
- Website: [antoniomalabago.com](https://antoniomalabago.com)
- LinkedIn: [linkedin.com/in/antonio-emmanuel-malabago](https://www.linkedin.com/in/antonio-emmanuel-malabago/)

## 🙏 Acknowledgments

- [Makerlab Electronics Philippines](https://www.makerlab-electronics.com/)
- Rosario Water System
- Pamantasan ng Lungsod ng Maynila
