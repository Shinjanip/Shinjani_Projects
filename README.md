# Shinjani_Projects
Project Portfolio  FPGA Pixel Selector: Verilog module using a multiplexer tree for single-pass extrema extraction.   LoRa Mesh: ESP32/FreeRTOS stack with multi-hop routing and &lt;2s latency.   Wearable Rehab: IoT system using IMU/EMG fusion for real-time joint angles and remote monitoring.


# Project Portfolio

This repository showcases a collection of engineering projects focused on **RTL design**, **decentralized communication**, and **biomedical informatics**.

---

## **Key Projects**

### **1. FPGA Pixel Extremum Selector**
A synthesizable **Verilog** module designed to compute maximum and minimum intensity values from a $24\times24$ grayscale image (576 pixels).
* **Architecture**: Implemented a **hierarchical multiplexer reduction tree** with dual comparators for efficient, single-pass extraction.
* **Efficiency**: Optimized for low latency and deterministic timing, making it suitable for FPGA or ASIC implementation.
* **Validation**: Integrated a **Python-based preprocessing workflow** to convert image data into `.mem` files for bit-accurate simulation and hardware validation.



### **2. LoRa Mesh Firmware & Network Stack**
A decentralized communication stack developed on **ESP32** using **C++** for disaster-response applications.
* **Routing**: Implemented **multi-hop routing** to extend the effective network range.
* **Performance**: Optimized packet handling to achieve **sub-2s alert latency**.
* **Reliability**: Engineered fault-tolerant operations and compact data serialization for reliable transmission over low-bandwidth links.



### **3. IoT-Enabled Wearable Rehabilitation Monitor**
A medical wearable system designed for real-time physiotherapy assessment and joint mobility monitoring.
* **Sensor Fusion**: Integrates dual **MPU6050 IMUs**, **EMG sensing** via the ADS1115 ADC, and heart-rate monitoring.
* **Firmware**: Modular C++ design utilizing **FreeRTOS** for concurrent sensor acquisition, WiFi connectivity, and embedded web server operation.
* **Data Management**: Features real-time joint angle computation, **SPIFFS-based** time-series logging with JSON/CSV serialization.



---

## **Technical Skills**
* **Languages**: C, Python, Java, Embedded C, Verilog.
* **Tools/Platforms**: STM32CubeIDE, KiCAD, Arduino IDE.
* **Specialization**: Electronics and Communication Engineering with a focus on biomedical applications.
