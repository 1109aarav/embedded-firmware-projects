3D Scanner — ESP32 + VL53L5CX + BNO08x + WiFi UDP

A wireless 3D spatial scanner built on ESP32. 
Captures a full 8x8 depth grid from a VL53L5CX Time-of-Flight sensor, 
fuses with orientation quaternion from a BNO08x IMU and streams json frames over UDP for 3D point cloud reconstruction.

Hardware :
MCU: ESP32
Depth Sensor: VL53L5CX — 8×8 multizone ToF
IMU: BNO08x — Game Rotation Vector (quaternion) at 100 Hz
WirelessWiFi: (802.11), UDP to PC
Bus: I2C @ 400 kHz (SDA: GPIO21, SCL: GPIO22)


ESP32 connects to WiFi and initializes I2C (with bus scan for debug)

VL53L5CX is configured for 8×8 resolution at 15 Hz ranging — produces 64 distance values + 64 target status bytes per frame

BNO08x streams Game Rotation Vector at 100 Hz — quaternion (W, X, Y, Z) captured on every IMU event

When a ToF frame is ready, the latest quaternion is fused into the same JSON packet so the PC knows the sensor orientation at capture time

JSON frame sent via UDP to a fixed PC IP:port — low-latency, no TCP handshake overhead

Each UDP packet is a JSON frame:
{
  "distances": [342, 345, 389, ...],   // 64 values, one per zone (mm)
  "status":    [5, 5, 0, ...],         // 64 target status codes (5 = valid)
  "quat":      [0.999, 0.012, -0.003, 0.001],  // [W, X, Y, Z] orientation
  "v":         "0.1.0"
}

distances[i] — range in mm for zone i of the 8×8 grid (row-major)
status[i] — VL53L5CX target status (5 = valid measurement, 0 = no target)
quat — unit quaternion from BNO08x Game Rotation Vector report

used the python script from this repo for reconstruction:
https://github.com/ferrolho/VL53L5CX-BNO08X-viewer/tree/main/viewer



