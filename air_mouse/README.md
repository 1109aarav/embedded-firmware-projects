Air Mouse — STM32 + BNO085 + USB HID

A gesture-controlled air mouse built on STM32F103.
Reads gyroscope data from a BNO085 IMU over I2C using the raw SHTP protocol,
applies filtering,
and sends cursor movement as a native USB HID mouse report —
no drivers required on any OS.

Hardware :
MCU: STM32F103
IMU: BNO085 ( onboard sensor fusion, SHTP/SH2 protocol)
Interface to PC: USB Full Speed (USB HID Mouse)
MCU ↔ IMU: I2C1 @ 100 kHz, address 0x4A

Working:

BNO085 init — drains boot packets over I2C, 
then enables gyroscope report (0x02) at 100 Hz via SHTP Feature Set command on Control channel

SHTP packet reception — reads 4-byte header to get packet length, 
reads full packet, parses channel 3 (Input) for report ID 0x02, 
skipping base timestamp frames (0xFB)

Gyro data extraction — raw 16-bit signed X/Y values scaled by 1/512.0 to get rad/s

Signal processing:
Deadband filter (±0.05 rad/s) eliminates sensor noise at rest
Exponential moving average (α = 0.3) smooths cursor motion
Sensitivity scaling (5.0×) and int8_t clamping for HID report

USB HID report — packed struct {buttons, x, y, wheel} 
sent via USBD_HID_SendReport() at ~100 Hz

SHTP Protocol
The BNO085 uses SHTP (Sensor Hub Transport Protocol) — a framed I2C packet format:
Byte 0-1 : Packet length (little-endian, bit15 = continuation flag)
Byte 2   : Channel number (2 = Control, 3 = Input)
Byte 3   : Sequence number (per-channel)
Byte 4+  : Payload

Enabling gyro report at 100Hz:
cmd[0] = 0xFD;           // Set Feature command
cmd[1] = 0x02;           // Gyroscope report ID
cmd[5..8] = 10000;       // 10 ms interval in microseconds
shtp_write(CH_CONTROL, cmd, 17);
