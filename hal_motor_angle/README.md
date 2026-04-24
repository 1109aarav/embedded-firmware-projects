Motor Angle Control — STM32 + AS5600 + VESC + CAN Bus

Closed-loop BLDC motor angle control on STM32F103. 
Reads rotor angle from an AS5600 magnetic encoder over I2C, 
computes shortest-path angular error, and drives a VESC motor controller over CAN bus using the VESC extended-ID packet protocol.

Hardware:
MCU: STM32F103 (Cortex-M3)
Encoder: AS5600 — 12-bit magnetic encoder, I2C @ 400 kHz
Motor Controller: VESC (Vedder ESC), CAN ID 76
Motor: BLDC
Bus: CAN @ 500 kbps (29-bit extended IDs)
DAC: MCP4725 — 12-bit I2C DAC on I2C2 @ 100 kHz

How It Works

AS5600 outputs a 12-bit raw angle (0–4095) via I2C register 0x0E
STM32 converts raw → degrees: angle_deg = raw × (360 / 4096)
Angular error computed with wraparound handling — always takes the shortest path around the circle
Bang-bang control with deadband:

err > +2° → forward RPM command to VESC
err < -2° → reverse RPM command to VESC
|err| ≤ 2° → stop motor + apply brake via MCP4725 DAC

VESC CAN command sent as 29-bit extended frame every 10 ms

