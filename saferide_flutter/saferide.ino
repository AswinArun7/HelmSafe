
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// Pin Definitions
#define TRIG_PIN D1
#define ECHO_PIN D2
#define ALCOHOL_SENSOR_PIN A0 
#define RELAY_PIN D6
#define MPU_SDA D3
#define MPU_SCL D4
#define GPS_RX D5  // Use D5 for RX
#define GPS_TX D7  // Use D7 for TX

// WiFi credentials
const char* ssid = "THAKKALI";
const char* password = "Mace@2026";

// Thresholds
const float alcoholThreshold = 1000;
const float crashThreshold = 1.15;

// Global Variables
bool isHelmetOn = false;
bool isDrunk = false;
bool crashDetected = false;

// Objects
MPU6050 mpu;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);  // TX of GPS -> RX pin (D5), RX of GPS -> TX pin (D7)
ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ALCOHOL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  Wire.begin(MPU_SDA, MPU_SCL);
  mpu.initialize();

  connectToWiFi();

  server.on("/data", HTTP_GET, handleDataRequest);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.enableCORS(true);
  server.begin();

  Serial.println("Setup complete");
}

void loop() {
  server.handleClient();

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
      isDrunk = true;
      digitalWrite(RELAY_PIN, LOW);
    } else {
      isDrunk = false;
      digitalWrite(RELAY_PIN, HIGH);
      checkAccident();
    }
  } else {
    Serial.println(" Helmet Not Worn! Ignition OFF");
    digitalWrite(RELAY_PIN, LOW);
  }

  delay(1000);
}

void handleDataRequest() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  server.sendHeader("Access-Control-Max-Age", "86400");
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Connection", "keep-alive");

  StaticJsonDocument<512> doc;

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  doc["timestamp"] = millis();
  doc["status"] = "OK";

  JsonObject sensorData = doc.createNestedObject("sensorData");
  sensorData["accelerationX"] = ax / 16384.0;
  sensorData["accelerationY"] = ay / 16384.0;
  sensorData["accelerationZ"] = az / 16384.0;
  sensorData["alcoholLevel"] = analogRead(ALCOHOL_SENSOR_PIN);
  sensorData["isHelmetOn"] = isHelmetOn;
  sensorData["isDrunk"] = isDrunk;
  sensorData["crashDetected"] = crashDetected;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  server.sendHeader("Access-Control-Max-Age", "86400");
  server.send(204);
}

bool checkHelmet() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
   if (duration == 0) {
    Serial.println("No echo received (Helmet Sensor Issue)");
    return false;
  }

  float distance = duration * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.println(distance);

  return (distance < 10);
}

void checkAccident() {
  if (crashDetected) return;

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float acceleration = sqrt(ax * ax + ay * ay + az * az) / 16384.0;

  Serial.print("Acceleration: ");
  Serial.println(acceleration);
  if (acceleration > crashThreshold) {
    Serial.println("🚨 CRASH DETECTED! 🚨");
    crashDetected = true;
    sendCrashLocation();
  }
}

void sendCrashLocation() {
  Serial.println("Getting GPS data...");
  unsigned long start = millis();
  while (gpsSerial.available() > 0 && millis() - start < 2000) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid()) {
    Serial.print("Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
    Serial.println("Google Maps: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6));
  } else {
    Serial.println("Location not available");
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed. Restarting...");
    ESP.restart();
  }
}