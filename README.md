Embedded Firmware Projects — Aarav Nadkarni
Firmware and embedded systems projects built during my internship at Combat Robotics India. Primarily STM32-based with real hardware integration — sensors, motor controllers, USB, and CAN bus.

## Projects

air_mouse
MCU: STM32F103 | Key Tech: BNO085, SHTP, USB HID

Gesture mouse — implements raw SHTP protocol to communicate with BNO085 IMU, enumerates as a native USB HID device with no drivers needed.

---

hal_motor_angle
MCU: STM32F103 | **Key Tech:** AS5600, VESC, CAN Bus

Closed-loop BLDC motor angle control. Reads rotor position from AS5600 magnetic encoder over I2C, drives VESC motor controller over CAN bus using 29-bit extended ID protocol.

---

3d_scanner_wireless
MCU: ESP32 | **Key Tech:** VL53L5CX, BNO08x, WiFi UDP

Wireless 3D spatial scanner — captures 8×8 depth grid (64 zones) from VL53L5CX ToF sensor, fuses with BNO08x orientation quaternion, streams JSON frames over UDP at 15 Hz for point cloud reconstruction.

---

wifi_interference
 Python / Linux | **Key Tech:** iw, RF math

5 GHz WiFi channel interference analyzer. Parses iw scan output, accumulates signal power using correct dBm→mW→dBm math, outputs normalized interference scores across all UNII band channels.


Skills
Firmware: STM32 HAL Embedded C CAN Bus USB HID I2C SHTP Protocol
Hardware: BNO085 AS5600 VESC BLDC MCP4725 STM32F103
Software: Python Signal Processing RF Analysis
Tools: STM32CubeIDE ST-Link Logic Analyzer VESC Tool

About
Aarav Nadkarni
BTech ECE — MIT Manipal
Firmware Engineer
