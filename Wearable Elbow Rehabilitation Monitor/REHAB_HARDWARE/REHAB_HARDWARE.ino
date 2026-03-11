/*
 * Hardware Test Code for Arduino Uno
 * Tests: 2x MPU6050 and 1x AD8232 EMG sensor
 * 
 * Hardware Connections:
 * - MPU6050 #1 (Upper Arm): SDA=A4, SCL=A5, AD0=GND (0x68)
 * - MPU6050 #2 (Forearm): SDA=A4, SCL=A5, AD0=5V (0x69)
 * - AD8232 EMG: OUT=A0 only (no LO+/LO- pins)
 */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

const int MPU_ELBOW_ADDRESS = 0x68; 
// Wrist sensor (AD0 connected to 3V3)
const int MPU_WRIST_ADDRESS = 0x69; 

// Create MPU6050 objects for each sensor
Adafruit_MPU6050 mpuElbow;
Adafruit_MPU6050 mpuWrist;
// Pins
#define EMG_OUTPUT A0

// Thresholds
int emgRestStop = 35;           // EMG too high at rest
int emgRestWarning = 45;        // EMG warning at rest
int emgMovementStop = 200;      // EMG too low during movement
int emgMovementWarning = 300;   // EMG warning during movement
float stopAngle = 20.0;        // Angle too low
float warningAngle = 60.0;     // Angle warning

// Readings
float angle = 0;
int emgValue = 0;
String status = "OK";
bool moving = false;
float lastAngle = 0;
unsigned long lastMoveTime = 0;

void setup() {
  Serial.begin(9600);
  
  Serial.println("========================================");
  Serial.println("   Hardware Test - Rehab Monitor");
  Serial.println("========================================");
  Serial.println();
  
  // Initialize I2C
  Wire.begin();
  
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
  Serial.println("Testing AD8232 EMG Sensor...");
  int testRead = analogRead(EMG_OUTPUT);
  Serial.print("  EMG Raw Value: ");
  Serial.println(testRead);
  if (testRead > 0 && testRead < 1023) {
    Serial.println("✓ AD8232 EMG SENSOR CONNECTED");
  } else {
    Serial.println("? EMG sensor reading unusual");
    Serial.println("  Check: VCC, GND, OUTPUT(A0)");
  }
  Serial.println();
  
  Serial.println("========================================");
  Serial.println("Starting continuous readings...");
  Serial.println("Thresholds loaded:");
  Serial.println("  STOP: Angle<110°, EMG Rest>55, EMG Moving<200");
  Serial.println("  WARNING: Angle<125°, EMG Rest>45, EMG Moving<300");
  Serial.println("========================================");
  Serial.println();
  delay(2000);
}

void loop() {
  // Read angle from MPU6050s
  readAngle();
  
  // Read EMG
  readEMG();
  
  // Detect movement
  detectMovement();
  
  // Check thresholds
  checkThresholds();
  
  // Print readings
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
  } else if (angle < warningAngle) {
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
  
  delay(100); // Update every 500ms
}

void readAngle() {
  sensors_event_t elbowAccel, elbowGyro, elbowTemp;
  sensors_event_t wristAccel, wristGyro, wristTemp;
  
  // Get new sensor events from both IMUs.
  // getEvent populates all three variables at once.
  mpuElbow.getEvent(&elbowAccel, &elbowGyro, &elbowTemp);
  mpuWrist.getEvent(&wristAccel, &wristGyro, &wristTemp);
  
  // Get the Z-axis accelerometer data from each sensor.
  float elbowAngleZ = elbowAccel.acceleration.z;
  float elbowAngleY = elbowAccel.acceleration.y;
  
  float wristAngleZ = wristAccel.acceleration.z;
  float wristAngleY = wristAccel.acceleration.y;
  
  // Calculate the tilt angle relative to gravity for each sensor.
  // We use atan2 and a scaling factor to get a more stable angle.
  float elbowTilt = atan2(elbowAngleY, elbowAngleZ) * 180 / PI;
  float wristTilt = atan2(wristAngleY, wristAngleZ) * 180 / PI;
  
  // Calculate the relative angle between the two sensors.
  // We use abs() to get a positive value.
  angle = abs(elbowTilt - wristTilt);
  
  // Print the calculated elbow angle to the Serial Monitor
 
  // A small delay to keep the serial output readable
  delay(100);
}

void readEMG() {
  // Read raw value (0-1023 for Arduino Uno)
  int rawValue = analogRead(EMG_OUTPUT);
  // Map to 0-1000 scale
  emgValue = rawValue;
}

void detectMovement() {
  if (abs(angle - lastAngle) > 3.0) {
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
  } else if (angle < warningAngle) {
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