#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TinyGPS++.h>
#include "ThingSpeak.h"

// WiFi
const char* ssid = "moto_edge_50_pro_JD";  // Enter your Wi-Fi name
const char* password = "Canada031106";     // Enter Wi-Fi password

/*
// MQTT information
const char* server = "98.70.48.142";
int port = 1883;
const char* mqttUser = "ElectronicSensor";
const char* mqttPassword = "@Salmon 5G";
*/

WiFiClient esp32Client;
// PubSubClient mqttClient(esp32Client);

/*
char ax[40];
char ay[40];
char az[40];
char gx[40];
char gy[40];
char gz[40];
char LAT[40];
char LON[40];
char SPEED[40];
char ALT[40];
char SAT[40];
*/

// Define the RX and TX pins for GPS
#define RXD2 20
#define TXD2 21

#define SDA_PIN 8
#define SCL_PIN 9

#define GPS_BAUD 9600

// The TinyGPS++ object
TinyGPSPlus gps;

// Create an instance of the HardwareSerial class for Serial 2
HardwareSerial gpsSerial(2);

// 0: 250 dps (131 LSB/dps); 1: 500 dps (65.5 LSB/dps) ; 2: 1000 dps ( 32.8 LSB/dps) ; 3: 2000 dps (16.4 LSB/dps)
#define GYRO_CONFIG_MODE 0

// 0: 2g (16384 LSB/g); 1: 4g (8192 LSB/g) ; 2: 8g (4096 LSB/g) ; 3: 16g (2048 LSB/g)
#define ACC_CONFIG_MODE 1

#define MPU_POWER_REGISTER 0x6B
#define MPU_MODE 0x6C
#define GYRO_MODE 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_INIT 0x1C
#define ACCEL_CONFIG 0x1D
#define accX_H 0x3B
#define accY_H 0x3D
#define accZ_H 0x3F
#define gyroX_H 0x43
#define gyroY_H 0x45
#define gyroZ_H 0x47

// I2C Address of MPU6050
const uint8_t MPU = 0x68;

// Calibration Time Assigned
unsigned long delay_cal = 3000;

float rawaccX = 0, rawaccY = 0, rawaccZ = 0, rawgyroX = 0, rawgyroY = 0, rawgyroZ = 0;
float tempC, gyroX, gyroY, gyroZ, accX, accY, accZ, calgyroX, calgyroY, calgyroZ, calaccX, calaccY, calaccZ;
float lat, lon, speed, alt, sat;

int Led_WIFI = 0;
int Led_DATA = 1;

void wifiInit() {
  Serial.print("Conectándose a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("Conectado a WiFi");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Connected");
      mqttClient.subscribe("Lecturas/01");
      mqttClient.subscribe("Lecturas/02");
    } else {
      Serial.print("Failed with state , rc=");
      Serial.print(mqttClient.state());
      Serial.println("Retry in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }
}

void getInfo_GPS() {
  lat = gps.location.lat();
  lon = gps.location.lng();
  speed = gps.speed.kmph();
  alt = gps.altitude.meters();
  sat = gps.satellites.value();
}

void gpsInfo() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  if (gps.location.isUpdated()) {
    getInfo_GPS();
    displayInfo();
  } else {
    Serial.println("Inválido");
    getInfo_GPS();
    displayInfo();
  }
}

void displayInfo() {
  Serial.print("LAT: ");
  Serial.println(gps.location.lat(), 6);
  Serial.print("LONG: ");
  Serial.println(gps.location.lng(), 6);
  Serial.print("SPEED (km/h) = ");
  Serial.println(gps.speed.kmph());
  Serial.print("ALT (m)= ");
  Serial.println(gps.altitude.meters());
  Serial.print("Satellites = ");
  Serial.println(gps.satellites.value());
  Serial.println("");
}

void i2cWrite(uint8_t address, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// Read 2 Bytes from a given register address i.e H and L
int16_t i2cRead(uint8_t address, int8_t reg) {
  int16_t temp = 0;
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(address, 2, true);
  temp = (Wire.read() << 8 | Wire.read());
  return temp;
}

// status implies the calibration mode
void printAcc(bool status) {
  if (status) {
    Serial.printf("Calibrated AccX = ");
    Serial.println(calaccX);
    Serial.printf("Calibrated AccY = ");
    Serial.println(calaccY);
    Serial.printf("Calibrated AccZ = ");
    Serial.println(calaccZ);
  } else {
    Serial.printf("AccX = ");
    Serial.println(accX);
    Serial.printf("AccY = ");
    Serial.println(accY);
    Serial.printf("AccZ = ");
    Serial.println(accZ);
  }
}

void printGyro(bool status) {
  if (status) {
    Serial.printf("Calibrated GyroX = ");
    Serial.println(calgyroX);
    Serial.printf("Calibrated GyroY = ");
    Serial.println(calgyroY);
    Serial.printf("Calibrated GyroZ = ");
    Serial.println(calgyroZ);
  } else {
    Serial.printf("GyroX = ");
    Serial.println(gyroX);
    Serial.printf("GyroY = ");
    Serial.println(gyroY);
    Serial.printf("GyroZ = ");
    Serial.println(gyroZ);
  }
}

void powMan() {
  // Access the Power Management Register 1 (107)
  // Configured at no sleep mode
  i2cWrite(MPU, MPU_POWER_REGISTER, 0);

  // Access the Power Management Register 2 (108)
  // Configured at all readings
  i2cWrite(MPU, MPU_MODE, 0);
}

// 0: 250 dps (131 LSB/dps); 1: 500 dps (65.5 LSB/dps) ; 2: 1000 dps ( 32.8 LSB/dps) ; 3: 2000 dps (16.4 LSB/dps)
void gyroConfig(uint8_t choice) {
  uint8_t val;
  switch (choice) {
    case 0:
      val = 0b00000001;
      break;
    case 1:
      val = 0b00001001;
      break;
    case 2:
      val = 0b00010001;
      break;
    case 3:
      val = 0b00011001;
      break;
    default:
      Serial.printf("Invalid input! By default Case 0 selected\n");
      val = 0b00000001;
      break;
  }

  // Accessing Register 26
  // Selecting Mode 0
  i2cWrite(MPU, GYRO_MODE, 0x00);


  // // Accessing Register 27 (Gryo Config Register)
  // Setting the full scale range to 250dps
  i2cWrite(MPU, GYRO_CONFIG, val);
}

// 0: 2g (16384 LSB/g); 1: 4g (8192 LSB/g) ; 2: 8g (4096 LSB/g) ; 3: 16g (2048 LSB/g)
void accConfig(uint8_t choice) {
  uint8_t val;
  switch (choice) {
    case 0:
      val = 0b00000000;  // 0x00
      break;
    case 1:
      val = 0b00001000;  // 0x80
      break;
    case 2:
      val = 0b00010000;  // 0x10
      break;
    case 3:
      val = 0b00011000;  // 0x11
      break;
    default:
      Serial.printf("Invalid input! By default Case 0 selected\n");
      val = 0b00000000;  // 0x00
      break;
  }

  // Accessing Register 28 (Accel Config Initial)
  i2cWrite(MPU, ACCEL_INIT, val);

  // Accessing Register 29 (Accel Config Filter)
  // Selecting the mode ACCEL_CHOICE = 1 with BW = 1.13kHz
  i2cWrite(MPU, ACCEL_CONFIG, 0b00001000);
}

// Status will detect whether in caliberation mode or not -> true if in setup
void accData(bool status, int choice) {
  float rawVal;
  switch (choice) {
    case 0:
      rawVal = 16384.0;
      break;
    case 1:
      rawVal = 8192.0;
      break;
    case 2:
      rawVal = 4096.0;
      break;
    case 3:
      rawVal = 2048.0;
      break;
  }

  if (status) {
    // Registers 59 - 64: AccX_H, AccX_L, AccY_H, AccY_L, AccZ_H, AccZ_L {They provide raw value}
    unsigned int count = 0;
    unsigned long initTime = millis();
    Serial.printf("Do not move! Starting calibration for %d seconds\n", (delay_cal / 1000));
    // In the loop for 3 seconds
    while (millis() - initTime <= delay_cal) {
      rawaccX += (i2cRead(MPU, accX_H)) / rawVal;
      rawaccY += (i2cRead(MPU, accY_H)) / rawVal;
      rawaccZ += ((i2cRead(MPU, accZ_H)) / rawVal) - 1;
      count++;
    }
    Serial.printf("Count = %d\n", count);
    Serial.printf("Calibration done!\n");
    calaccX = (float)rawaccX / count;
    calaccY = (float)rawaccY / count;
    calaccZ = (float)rawaccZ / count;
    printAcc(true);
  } else {
    // Registers 59 - 64: AccX_H, AccX_L, AccY_H, AccY_L, AccZ_H, AccZ_L {They provide raw value}
    accX = (i2cRead(MPU, accX_H) / rawVal) - calaccX;
    accY = (i2cRead(MPU, accY_H) / rawVal) - calaccY;
    accZ = (i2cRead(MPU, accZ_H) / rawVal) - calaccZ;
    printAcc(false);
  }
}

// 0: 250 dps (131 LSB/dps); 1: 500 dps (65.5 LSB/dps) ; 2: 1000 dps ( 32.8 LSB/dps) ; 3: 2000 dps (16.4 LSB/dps)
void gyroData(bool status, int choice) {
  float rawVal;
  switch (choice) {
    case 0:
      rawVal = 131.0;
      break;
    case 1:
      rawVal = 65.5;
      break;
    case 2:
      rawVal = 32.8;
      break;
    case 3:
      rawVal = 16.4;
      break;
  }
  // Registers 67-72: GyroX_H, GyroX_L, GyroY_H, GyroY_L, GyroZ_H, GyroZ_L {They provide raw value}
  if (status) {
    unsigned int count = 0;
    unsigned long initTime = millis();
    Serial.printf("Do not move! Starting calibration for %d seconds\n", (delay_cal / 1000));
    // In the loop for 3 seconds
    while (millis() - initTime <= delay_cal) {
      rawgyroX += (i2cRead(MPU, gyroX_H)) / rawVal;
      rawgyroY += (i2cRead(MPU, gyroY_H)) / rawVal;
      rawgyroZ += (i2cRead(MPU, gyroZ_H)) / rawVal;
      count++;
    }
    Serial.printf("Count = %d\n", count);
    Serial.printf("Calibration done!\n");
    calgyroX = (float)rawgyroX / count;
    calgyroY = (float)rawgyroY / count;
    calgyroZ = (float)rawgyroZ / count;
    printGyro(true);
  } else {
    gyroX = (i2cRead(MPU, gyroX_H)) / rawVal - calgyroX;
    gyroY = (i2cRead(MPU, gyroY_H)) / rawVal - calgyroY;
    gyroZ = (i2cRead(MPU, gyroZ_H)) / rawVal - calgyroZ;
    printGyro(false);
  }
}

void sendData() {
  sprintf(ax, "Acc X: %f", accX);
  sprintf(ay, "Acc Y: %f", accY);
  sprintf(az, "Acc Z: %f", accZ);
  sprintf(gx, "Gyro X: %f", gyroX);
  sprintf(gy, "Gyro y: %f", gyroY);
  sprintf(gz, "Gyro Z: %f", gyroZ);
  sprintf(LAT, "Latitude: %f", lat);
  sprintf(LON, "Longitude: %f", lon);
  sprintf(SPEED, "Speed(km/h): %f", speed);
  sprintf(ALT, "Altitude(m): %f", alt);
  sprintf(SAT, "Satellites: %f", sat);

  if (!mqttClient.connected()) {
    reconnect();
  }

  mqttClient.loop();
  mqttClient.publish("Lecturas/01", ax);
  mqttClient.publish("Lecturas/01", ay);
  mqttClient.publish("Lecturas/01", az);
  mqttClient.publish("Lecturas/01", gx);
  mqttClient.publish("Lecturas/01", gy);
  mqttClient.publish("Lecturas/01", gz);
  mqttClient.publish("Lecturas/02", LAT);
  mqttClient.publish("Lecturas/02", LON);
  mqttClient.publish("Lecturas/02", SPEED);
  mqttClient.publish("Lecturas/02", ALT);
  mqttClient.publish("Lecturas/02", SAT);
}

void setup() {
  Serial.begin(115200);
  pinMode(Led_WIFI, OUTPUT);
  pinMode(Led_DATA, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);

  wifiInit();
  mqttClient.setServer(server, port);
  
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);

  // Using the I2C communication at fast mode (400kHz)
  Wire.setClock(400000);
  // Calling the power management function
  powMan();
  // Selection Mode 0: 250dps
  gyroConfig(GYRO_CONFIG_MODE);
  // Selection Mode 2: +-8g
  accConfig(ACC_CONFIG_MODE);
  // Calibration for Acceleration
  accData(true, ACC_CONFIG_MODE);
  // Calibration for Gyroscope
  gyroData(true, GYRO_CONFIG_MODE);
  delay(3000);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(Led_WIFI, HIGH);
  } else if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(Led_WIFI, LOW);
  }
  gpsInfo();
  accData(false, ACC_CONFIG_MODE);
  gyroData(false, GYRO_CONFIG_MODE);
  sendData();
  delay(500);
}
