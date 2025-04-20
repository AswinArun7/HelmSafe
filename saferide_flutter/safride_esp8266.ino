#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// Function declarations
void alertError(int beeps);
void alertSuccess();
void connectToWiFi();
void setupWebServer();
void updateSensors();
bool checkHelmet();
void checkAccident();
void sendCrashLocation();
void handleDataRequest();
void handleOptions();
void handleReset();

// Sensor and Module Pins
#define TRIG_PIN D1
#define ECHO_PIN D2
#define ALCOHOL_SENSOR_PIN A0 
#define RELAY_PIN D6
#define MPU_SDA D3
#define MPU_SCL D4
#define BUZZER_PIN D5

// Thresholds
const float alcoholThreshold = 1000;
const float crashThreshold = 1.15;  // Crash detection threshold (g-force)
const unsigned long GPS_TIMEOUT = 5000;  // 5 seconds timeout for GPS
const unsigned long SENSOR_UPDATE_INTERVAL = 1000;  // 1 second between sensor readings

// Global variables
bool isHelmetOn = false;
bool isDrunk = false;
bool crashDetected = false;
unsigned long lastSensorUpdate = 0;
Ticker watchdog;
bool watchdogActive = false;

// Initialize objects
MPU6050 mpu;
TinyGPSPlus gps;
ESP8266WebServer server(80);
WiFiManager wifiManager;

// Watchdog timer function
void ICACHE_RAM_ATTR resetModule() {
  ESP.restart();
}

void setup() {
  // Initialize Serial communication
  Serial.begin(9600);  // For GPS
  Serial1.begin(115200);  // For debugging
  
  Serial1.println("\nStarting SafeRide System...");

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ALCOHOL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize watchdog timer
  watchdog.attach(60, resetModule);  // Reset if no activity for 60 seconds
  watchdogActive = true;

  // Initialize MPU6050
  Wire.begin(MPU_SDA, MPU_SCL);
  Serial1.println("Initializing MPU6050...");
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial1.println("MPU6050 connected successfully!");
    // Configure MPU6050
    mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);  // ±2g range
    mpu.setDLPFMode(MPU6050_DLPF_BW_5);  // 5Hz bandwidth
  } else {
    Serial1.println("ERROR: MPU6050 not detected!");
    alertError(3);  // 3 beeps for MPU error
  }

  // Initialize GPS
  Serial1.println("GPS initialized");

  // Connect to WiFi
  connectToWiFi();
  
  // Setup web server routes
  setupWebServer();
  
  Serial1.println("System initialization complete!");
  alertSuccess();  // One long beep for success
}

void loop() {
  server.handleClient();
  
  // Reset watchdog by detaching and reattaching
  if (watchdogActive) {
    watchdog.detach();
    watchdog.attach(60, resetModule);
  }
  
  unsigned long currentMillis = millis();
  
  // Update sensors at regular intervals
  if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    updateSensors();
    lastSensorUpdate = currentMillis;
  }

  // Process GPS data
  while (Serial.available() > 0) {
    gps.encode(Serial.read());
  }
}

void updateSensors() {
  // Check helmet status
  isHelmetOn = checkHelmet();
  int alcoholLevel = analogRead(ALCOHOL_SENSOR_PIN);

  // Print sensor data to Serial for debugging
  Serial.print("Helmet: ");
  Serial.print(isHelmetOn ? "Worn " : "Not Worn ");
  Serial.print("| Alcohol Level: ");
  Serial.println(alcoholLevel);

  if (isHelmetOn) {
    if (alcoholLevel > alcoholThreshold) {
      isDrunk = true;
      Serial.println("Drunk Driving Detected! Ignition OFF");
      digitalWrite(RELAY_PIN, LOW);
      alertError(4);  // 4 beeps for alcohol detection
    } else {
      isDrunk = false;
      Serial.println("Safe to drive. Ignition ON");
      digitalWrite(RELAY_PIN, HIGH);
      checkAccident();
    }
  } else {
    Serial.println("Helmet Not Worn! Ignition OFF");
    digitalWrite(RELAY_PIN, LOW);
    alertError(1);  // 1 beep for helmet warning
  }
}

void setupWebServer() {
  server.on("/data", HTTP_GET, handleDataRequest);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.on("/reset", HTTP_POST, handleReset);
  server.enableCORS(true);
  server.begin();
  Serial.println("HTTP server started");
}

void handleDataRequest() {
  // Add CORS headers
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  server.sendHeader("Access-Control-Max-Age", "86400");
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Connection", "keep-alive");
  
  StaticJsonDocument<512> doc;
  
  // Get current sensor data
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  
  // Create JSON response
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

  // Add GPS data if available
  JsonObject gpsData = doc.createNestedObject("gpsData");
  if (gps.location.isValid()) {
    gpsData["latitude"] = gps.location.lat();
    gpsData["longitude"] = gps.location.lng();
    gpsData["speed"] = gps.speed.kmph();
    gpsData["altitude"] = gps.altitude.meters();
  } else {
    gpsData["latitude"] = 0;
    gpsData["longitude"] = 0;
    gpsData["speed"] = 0;
    gpsData["altitude"] = 0;
  }

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

void handleReset() {
  server.send(200, "text/plain", "System resetting...");
  delay(1000);
  ESP.restart();
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

  return (distance < 10);  // Helmet detected if object is within 10 cm
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
    crashDetected = true;
    alertError(5);  // 5 beeps for crash detection
    sendCrashLocation();
  }
}

void sendCrashLocation() {
  Serial.println("Fetching location...");
  unsigned long startTime = millis();

  while (Serial.available() > 0 && (millis() - startTime) < GPS_TIMEOUT) {
    gps.encode(Serial.read());
  }

  if (gps.location.isValid()) {
    Serial.print("Real Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
    Serial.println("GPS Link: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6));
  } else {
    Serial.println("Location: 10.0603, 77.6352 (Default)");
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Uncomment to reset WiFi settings
  // wifiManager.resetSettings();
  
  Serial.print("Connecting to WiFi...");
  
  // Set custom parameters
  WiFiManagerParameter custom_text("<p>SafeRide System Configuration</p>");
  wifiManager.addParameter(&custom_text);
  
  if (!wifiManager.autoConnect("SafeRide_AP")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Signal Strength (RSSI): ");
  Serial.println(WiFi.RSSI());
}

void alertError(int beeps) {
  for (int i = 0; i < beeps; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void alertSuccess() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
} 