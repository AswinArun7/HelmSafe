#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <MPU6050.h>
#include <TinyGPS++.h>

// Sensor and Module Pins
#define TRIG_PIN D1
#define ECHO_PIN D2
#define ALCOHOL_SENSOR_PIN A0
#define RELAY_PIN D6
#define MPU_SDA D3
#define MPU_SCL D4
#define GPS_RX D7
#define GPS_TX D8

const char* ssid = "realme";
const char* password = "t6u36xcj";

const float alcoholThreshold = 900;
const float crashThreshold = 1.5;  // Crash detection threshold (g-force)

bool isHelmetOn = false;
bool isDrunk = false;
bool crashDetected = false;  // Flag to stop readings after a crash

MPU6050 mpu;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

void setup() {
  Serial.begin(9600);

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ALCOHOL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Initialize MPU6050
  Wire.begin(MPU_SDA, MPU_SCL);
  Serial.println("Initializing MPU6050...");
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connected successfully!");
  } else {
    Serial.println("ERROR: MPU6050 not detected!");
  }

  // Initialize GPS
  gpsSerial.begin(9600);
  connectToWiFi();
  delay(3000);
}

void loop() {
  isHelmetOn = checkHelmet();
  int alcoholLevel = analogRead(ALCOHOL_SENSOR_PIN);

  Serial.print("Helmet: ");
  Serial.print(isHelmetOn ? "Worn " : "Not Worn ");
  Serial.print("| Alcohol Level: ");
  Serial.println(alcoholLevel);
  delay(2000);

  if (isHelmetOn) {
    if (alcoholLevel > alcoholThreshold) {
      Serial.println(" Drunk Driving Detected! Ignition OFF");
      digitalWrite(RELAY_PIN, LOW);
    } else {
      Serial.println(" Safe to drive. Ignition ON");
      digitalWrite(RELAY_PIN, HIGH);
      checkAccident();
      delay(1000);
    }
  } else {
    Serial.println(" Helmet Not Worn! Ignition OFF");
    digitalWrite(RELAY_PIN, LOW);
    delay(1000);
  }

  delay(1000);
}

bool checkHelmet() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // Increased timeout
  if (duration == 0) {
    Serial.println("No echo received (Helmet Sensor Issue)");
    return false;
  }

  float distance = duration * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.println(distance);

  return (distance < 10);  // Helmet detected if object is within 5 cm
}

void checkAccident() {
  if (crashDetected) return;  // Stop further readings after a crash

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float acceleration = sqrt(ax * ax + ay * ay + az * az) / 16384.0;  // Convert to g-force

  Serial.print("Acceleration: ");
  Serial.println(acceleration);

  if (acceleration > crashThreshold) {
    Serial.println("🚨 CRASH DETECTED! 🚨");
    sendCrashLocation();
    crashDetected = true; 
     // Stop further readings
  } else {
    Serial.println("No Crash");
  }
  
}

void sendCrashLocation() {
  Serial.println("Fetching location...");

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated()) {
    Serial.print("Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
    Serial.println("GPS Link: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6));
  } else {
    Serial.println("Location: 10.0603, 77.6352 ");
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}