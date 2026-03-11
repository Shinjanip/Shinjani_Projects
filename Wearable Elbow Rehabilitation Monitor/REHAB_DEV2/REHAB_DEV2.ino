/*
 * ESP32 Elbow Rehabilitation Monitor with IoT
 * Hardware: 2x MPU6050 and 1x EMG sensor
 * 
 * Hardware Connections:
 * - MPU6050 #1 (Elbow): SDA=21, SCL=22, AD0=GND (0x68)
 * - MPU6050 #2 (Wrist): SDA=21, SCL=22, AD0=3.3V (0x69)
 * - EMG Sensor: OUT=34 (ADC pin)
 */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials - CHANGE THESE
const char* ssid = "Shini";
const char* password = "200320052007";

WebServer server(80);

const int MPU_ELBOW_ADDRESS = 0x68; 
const int MPU_WRIST_ADDRESS = 0x69; 

// Create MPU6050 objects for each sensor
Adafruit_MPU6050 mpuElbow;
Adafruit_MPU6050 mpuWrist;

// Pins
#define EMG_OUTPUT 34  // ESP32 ADC pin

// Thresholds
int emgRestStop = 35;           // EMG too high at rest
int emgRestWarning = 45;        // EMG warning at rest
int emgMovementStop = 200;      // EMG too low during movement
int emgMovementWarning = 300;   // EMG warning during movement
float stopAngle = 15.0;         // Angle too low
float warningAngle = 130.0;      // Angle warning

// Readings
float angle = 0;
int emgValue = 0;
String status = "OK";
bool moving = false;
float lastAngle = 0;
unsigned long lastMoveTime = 0;

// Data logging
const int MAX_LOG = 1000;
struct DataPoint {
  unsigned long time;
  float angle;
  int emg;
  String status;
  bool moving;
};
DataPoint dataLog[MAX_LOG];
int logIndex = 0;
bool logging = false;

void setup() {
  Serial.begin(115200);
  
  Serial.println("========================================");
  Serial.println("   Hardware Test - Rehab Monitor");
  Serial.println("========================================");
  Serial.println();
  
  // Initialize I2C
  Wire.begin(21, 22);  // ESP32 I2C pins
  
  delay(1000);
  
  // Test MPU6050 #1
  Serial.println("Initializing Elbow IMU...");
  if (!mpuElbow.begin(MPU_ELBOW_ADDRESS)) {
    Serial.println("Failed to find Elbow MPU6050 chip at address 0x68");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Elbow MPU6050 Found!");

  // Initialize the MPU6050 sensor for the wrist/forearm
  Serial.println("Initializing Wrist IMU...");
  if (!mpuWrist.begin(MPU_WRIST_ADDRESS)) {
    Serial.println("Failed to find Wrist MPU6050 chip at address 0x69");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Wrist MPU6050 Found!");
  
  // Test EMG Sensor
  Serial.println("Testing EMG Sensor...");
  int testRead = analogRead(EMG_OUTPUT);
  Serial.print("  EMG Raw Value: ");
  Serial.println(testRead);
  if (testRead >= 0 && testRead <= 4095) {
    Serial.println("✓ EMG SENSOR CONNECTED");
  } else {
    Serial.println("? EMG sensor reading unusual");
    Serial.println("  Check: VCC, GND, OUTPUT(34)");
  }
  Serial.println();
  
  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Setup server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/download", handleDownload);
  
  server.begin();
  Serial.println("Server started");
  
  Serial.println("========================================");
  Serial.println("Starting continuous readings...");
  Serial.println("Thresholds loaded:");
  Serial.println("  STOP: Angle<20°, EMG Rest>35, EMG Moving<200");
  Serial.println("  WARNING: Angle<60°, EMG Rest>45, EMG Moving<300");
  Serial.println("========================================");
  Serial.println();
  delay(2000);
}

void loop() {
  server.handleClient();
  
  // Read angle from MPU6050s
  readAngle();
  
  // Read EMG
  readEMG();
  
  // Detect movement
  detectMovement();
  
  // Check thresholds
  checkThresholds();
  
  // Log data
  if (logging && logIndex < MAX_LOG) {
    dataLog[logIndex].time = millis();
    dataLog[logIndex].angle = angle;
    dataLog[logIndex].emg = emgValue;
    dataLog[logIndex].status = status;
    dataLog[logIndex].moving = moving;
    logIndex++;
  }
  
  // Print readings to Serial
  Serial.println("========================================");
  
  // Print status with visual indicator
  Serial.print("STATUS: ");
  if (status == "OK") {
    Serial.println("✓ OK (Green)");
  } else if (status == "WARNING") {
    Serial.println("⚠ WARNING (Orange)");
  } else if (status == "STOP") {
    Serial.println("✗ STOP (Red) - STOP EXERCISE!");
  }
  Serial.println();
  
  // Print angle
  Serial.print("Angle: ");
  Serial.print(angle, 1);
  Serial.print(" degrees");
  if (angle < stopAngle) {
    Serial.println(" [TOO LOW - STOP!]");
  } else if (angle > warningAngle) {
    Serial.println(" [WARNING]");
  } else {
    Serial.println(" [OK]");
  }
  
  // Print EMG
  Serial.print("EMG Signal: ");
  Serial.print(emgValue);
  
  // Check EMG thresholds based on movement
  if (moving) {
    if (emgValue < emgMovementStop) {
      Serial.println(" [TOO LOW DURING MOVEMENT - STOP!]");
    } else if (emgValue < emgMovementWarning) {
      Serial.println(" [LOW DURING MOVEMENT - WARNING]");
    } else {
      Serial.println(" [OK - MOVING]");
    }
  } else {
    if (emgValue > emgRestStop) {
      Serial.println(" [TOO HIGH AT REST - STOP!]");
    } else if (emgValue > emgRestWarning) {
      Serial.println(" [HIGH AT REST - WARNING]");
    } else {
      Serial.println(" [OK - AT REST]");
    }
  }
  
  // Print movement state
  Serial.print("Movement: ");
  Serial.println(moving ? "MOVING" : "REST");
  
  Serial.println();
  
  delay(100); // Update every 100ms
}

void readAngle() {
  sensors_event_t elbowAccel, elbowGyro, elbowTemp;
  sensors_event_t wristAccel, wristGyro, wristTemp;
  
  // Get new sensor events from both IMUs
  mpuElbow.getEvent(&elbowAccel, &elbowGyro, &elbowTemp);
  mpuWrist.getEvent(&wristAccel, &wristGyro, &wristTemp);
  
  // Get the Z-axis accelerometer data from each sensor
  float elbowAngleZ = elbowAccel.acceleration.z;
  float elbowAngleY = elbowAccel.acceleration.y;
  
  float wristAngleZ = wristAccel.acceleration.z;
  float wristAngleY = wristAccel.acceleration.y;
  
  // Calculate the tilt angle relative to gravity for each sensor
  float elbowTilt = atan2(elbowAngleY, elbowAngleZ) * 180 / PI;
  float wristTilt = atan2(wristAngleY, wristAngleZ) * 180 / PI;
  
  // Calculate the relative angle between the two sensors
  angle = abs(elbowTilt - wristTilt);
}

void readEMG() {
  // Read raw value from ESP32 ADC (0-4095 for 12-bit)
  // Scale to match Arduino Uno range (0-1023)
  int rawValue = analogRead(EMG_OUTPUT);
  emgValue = map(rawValue, 0, 4095, 0, 1023);
}

void detectMovement() {
  if (abs(angle - lastAngle) > 6.0) {
    moving = true;
    lastMoveTime = millis();
  } else if (millis() - lastMoveTime > 2000) {
    moving = false;
  }
  lastAngle = angle;
}

void checkThresholds() {
  status = "OK";
  
  // Check angle first
  if (angle < stopAngle) {
    status = "STOP";
    return;
  } else if (angle > warningAngle) {
    status = "WARNING";
  }
  
  // Check EMG based on movement state
  if (moving) {
    // During movement: EMG should be high enough
    if (emgValue < emgMovementStop) {
      status = "STOP";
    } else if (emgValue < emgMovementWarning) {
      if (status != "STOP") status = "WARNING";
    }
  } else {
    // At rest: EMG should be low enough
    if (emgValue > emgRestStop) {
      status = "STOP";
    } else if (emgValue > emgRestWarning) {
      if (status != "STOP") status = "WARNING";
    }
  }
}

// ============ WEB SERVER HANDLERS ============

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Rehab Monitor</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:0;padding:20px;background:#f5f5f5}";
  html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{text-align:center;color:#333}";
  html += ".status{font-size:48px;font-weight:bold;text-align:center;padding:30px;margin:20px 0;border-radius:10px}";
  html += ".ok{background:#4CAF50;color:white}";
  html += ".warning{background:#FF9800;color:white}";
  html += ".stop{background:#F44336;color:white}";
  html += ".readings{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin:20px 0}";
  html += ".box{background:#f9f9f9;padding:20px;border-radius:8px;text-align:center}";
  html += ".value{font-size:36px;font-weight:bold;color:#2196F3}";
  html += ".label{font-size:14px;color:#666;margin-top:5px}";
  html += "button{padding:15px 30px;font-size:16px;margin:10px;border:none;border-radius:5px;cursor:pointer;font-weight:bold}";
  html += ".btn-start{background:#4CAF50;color:white}";
  html += ".btn-stop{background:#F44336;color:white}";
  html += ".btn-download{background:#2196F3;color:white}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Elbow Rehab Monitor</h1>";
  
  html += "<div id='status' class='status ok'>READY</div>";
  
  html += "<div class='readings'>";
  html += "<div class='box'><div class='value' id='angle'>0</div><div class='label'>Angle (deg)</div></div>";
  html += "<div class='box'><div class='value' id='emg'>0</div><div class='label'>EMG Signal</div></div>";
  html += "<div class='box'><div class='value' id='move'>REST</div><div class='label'>State</div></div>";
  html += "</div>";
  
  html += "<div style='text-align:center;margin:20px 0'>";
  html += "<button class='btn-start' onclick='start()'>Start</button>";
  html += "<button class='btn-stop' onclick='stop()'>Stop</button>";
  html += "<button class='btn-download' onclick='download()'>Download CSV</button>";
  html += "</div>";
  
  html += "<div style='text-align:center;color:#666'>Logged: <span id='count'>0</span> readings</div>";
  
  html += "</div>";
  
  html += "<script>";
  html += "function update(){";
  html += "fetch('/data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('angle').textContent=d.angle.toFixed(1);";
  html += "document.getElementById('emg').textContent=d.emg;";
  html += "document.getElementById('move').textContent=d.moving?'MOVING':'REST';";
  html += "document.getElementById('count').textContent=d.count;";
  html += "let s=document.getElementById('status');";
  html += "s.textContent=d.status;";
  html += "s.className='status '+d.status.toLowerCase();";
  html += "});}";
  html += "function start(){fetch('/start');}";
  html += "function stop(){fetch('/stop');}";
  html += "function download(){window.location.href='/download';}";
  html += "setInterval(update,200);";
  html += "update();";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"angle\":" + String(angle) + ",";
  json += "\"emg\":" + String(emgValue) + ",";
  json += "\"status\":\"" + status + "\",";
  json += "\"moving\":" + String(moving ? "true" : "false") + ",";
  json += "\"count\":" + String(logIndex);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleStart() {
  logging = true;
  logIndex = 0;
  Serial.println("Logging started");
  server.send(200, "text/plain", "Started");
}

void handleStop() {
  logging = false;
  Serial.println("Logging stopped. Records: " + String(logIndex));
  server.send(200, "text/plain", "Stopped");
}

void handleDownload() {
  String csv = "Timestamp_ms,Angle_deg,EMG_Signal,Status,Moving\n";
  
  for (int i = 0; i < logIndex; i++) {
    csv += String(dataLog[i].time) + ",";
    csv += String(dataLog[i].angle) + ",";
    csv += String(dataLog[i].emg) + ",";
    csv += dataLog[i].status + ",";
    csv += String(dataLog[i].moving ? "1" : "0") + "\n";
  }
  
  server.sendHeader("Content-Disposition", "attachment; filename=rehab_data.csv");
  server.send(200, "text/csv", csv);
  Serial.println("CSV downloaded");
}