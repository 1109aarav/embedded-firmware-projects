#include <Wire.h>
#include <vl53l5cx_class.h>
#include <SparkFun_BNO08x_Arduino_Library.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define VERSION  "0.1.0"
#define SDA_PIN  21
#define SCL_PIN  22

const char* WIFI_SSID = ""; //wifi_id
const char* WIFI_PASS = ""; //wifi_password
const char* PC_IP = "10.80.131.31";
const int UDP_PORT = 5000;

WiFiUDP udp;
VL53L5CX sensor(&Wire, -1, -1);
BNO08x   imu;

bool  imuAvailable = false;
float quatW = 1.0, quatX = 0.0, quatY = 0.0, quatZ = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("{\"status\":\"initializing\"}");

  //wifi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("connected to esp");
  Serial.println(WiFi.localIP());
  udp.begin(UDP_PORT);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Serial.println("{\"status\":\"i2c_ready\"}");

  // Scan bus
  Serial.print("{\"i2c_scan\":[");
  bool first = true;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      if (!first) Serial.print(",");
      Serial.print("\"0x");
      Serial.print(a, HEX);
      Serial.print("\"");
      first = false;
    }
  }
  Serial.println("]}");

  // Init sensor
  sensor.begin();
  Serial.println("{\"status\":\"sensor_begin\"}");

  Serial.println("{\"status\":\"loading_firmware\"}");
  uint8_t status = sensor.init_sensor(0x52);
  if (status != 0) {
    Serial.print("{\"error\":\"tof_failed\",\"code\":");
    Serial.print(status);
    Serial.println("}");
  } else {
    sensor.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_8X8);
    sensor.vl53l5cx_set_ranging_frequency_hz(15);
    sensor.vl53l5cx_start_ranging();
    Serial.println("{\"status\":\"tof_ready\"}");
  }

  // BNO08x
  if (imu.begin(0x4A, Wire)) {
    imuAvailable = true;
  } else if (imu.begin(0x4B, Wire)) {
    imuAvailable = true;
  }
  if (imuAvailable) {
    imu.enableGameRotationVector(10);
    Serial.println("{\"status\":\"imu_ready\"}");
  } else {
    Serial.println("{\"status\":\"imu_not_found\"}");
  }

  Serial.println("{\"status\":\"ready\",\"v\":\"" VERSION "\"}");
}

void loop() {
  if (imuAvailable && imu.wasReset()) imu.enableGameRotationVector(10);

  if (imuAvailable && imu.getSensorEvent()) {
    if (imu.getSensorEventID() == SENSOR_REPORTID_GAME_ROTATION_VECTOR) {
      quatW = imu.getQuatReal();
      quatX = imu.getQuatI();
      quatY = imu.getQuatJ();
      quatZ = imu.getQuatK();
    }
  }

  uint8_t dataReady = 0;
  sensor.vl53l5cx_check_data_ready(&dataReady);

  if (dataReady) {
    VL53L5CX_ResultsData results;
    sensor.vl53l5cx_get_ranging_data(&results);

    //json
    String json = "{\"distances\":[";
    for (int i = 0; i < 64; i++){
      json += results.distance_mm[i];
      if (i < 63) json += ",";
    }
    json += "],\"status\":[";
    for (int i = 0; i < 64; i++){
      json += results.target_status[i];
      if (i < 63) json += ",";
    }
    json += "],\"quat\":[";
    json += String(quatW, 6); json += ",";
    json += String(quatX, 6); json += ",";
    json += String(quatY, 6); json += ",";
    json += String(quatZ, 6);
    json += "],\"v\":\"";
    json += VERSION;
    json += "\"}";

    udp.beginPacket(PC_IP, UDP_PORT);
    udp.println(json);
    udp.endPacket();
  }


  delay(1);
}