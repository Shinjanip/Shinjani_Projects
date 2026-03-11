#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <PulseSensorPlayground.h> 
#include <Adafruit_ADS1X15.h>
#include <Arduino.h>

// I2C addresses for the MPU6050 sensors
const int MPU_ELBOW_ADDRESS = 0x68; 
const int MPU_WRIST_ADDRESS = 0x69; 

// PulseSensor Pin (A0 on most ESP32 dev boards)
// Ensure this pin matches the ADC pin on your specific ESP32 board
const int PulseWire = 34; // This is a common ADC pin, but check your board's pinout.
const int LED_BUILTIN = 2; // The built-in LED on ESP32
int Threshold = 550; // Determine which Signal to "count as a beat" and which to ignore

// --- SENSOR OBJECTS ---
Adafruit_MPU6050 mpuElbow;
Adafruit_MPU6050 mpuWrist;
PulseSensorPlayground pulseSensor; // Object for the Pulse Sensor
Adafruit_ADS1115 ads; // Object for the ADS1115 EMG sensor

// --- THRESHOLDS ---
struct StopThresholds {
    int bicepsRestMax = 55;
    int tricepsRestMax = 58;
    int bicepsMoveMin = 200;
    int tricepsMoveMin = 180;
    int elbowAngleMin = 110;
    int coActivationMin = 60;
    int maxHeartRate = 120;
};

struct WarningThresholds {
    int bicepsRestMax = 45;
    int tricepsRestMax = 48;
    int bicepsMoveMin = 300;
    int tricepsMoveMin = 280;
    int elbowAngleMin = 125;
};

const StopThresholds stopThresholds;
const WarningThresholds warningThresholds;

// --- GLOBALS ---
float elbowAngle = 0.0;
int bicepsEmg = 0;
int tricepsEmg = 0;
int heartRate = 0;
bool isOkay = true;

// --- FUNCTION PROTOTYPES ---
void readAllSensors();
void calculateElbowAngle();
void evaluateThresholds();
void displaySensorData();

void setup() {
    Serial.begin(115200);

    // Initialize I2C communication on default pins
    Wire.begin();

    // MPU6050 sensor setup
    Serial.println("Initializing Elbow IMU...");
    if (!mpuElbow.begin(MPU_ELBOW_ADDRESS)) {
        Serial.println("Failed to find Elbow MPU6050 chip at address 0x68");
        while (1) { delay(10); }
    }
    Serial.println("Elbow MPU6050 Found!");

    Serial.println("Initializing Wrist IMU...");
    if (!mpuWrist.begin(MPU_WRIST_ADDRESS)) {
        Serial.println("Failed to find Wrist MPU6050 chip at address 0x69");
        while (1) { delay(10); }
    }
    Serial.println("Wrist MPU6050 Found!");
    
    // ADS1115 setup for EMG sensors
    Serial.println("Initializing ADS1115 for EMG...");
    if (!ads.begin()) {
        Serial.println("Failed to find ADS1115 chip");
        while (1) { delay(10); }
    }
    Serial.println("ADS1115 Found!");

    // Pulse Sensor setup
    Serial.println("Initializing Pulse Sensor...");
    pulseSensor.analogInput(PulseWire);
    pulseSensor.blinkOnPulse(LED_BUILTIN);
    pulseSensor.setThreshold(Threshold);
    if (!pulseSensor.begin()) {
        Serial.println("PulseSensor object not created or signal not found!");
    } else {
        Serial.println("PulseSensor object created!");
    }
    
    Serial.println("Sensors initialized. Starting real-time monitoring...");
}

void loop() {
    isOkay = true;
    readAllSensors();
    evaluateThresholds();
    displaySensorData();

    // If no flags were raised, print the "You are okay!" message
    if (isOkay) {
        Serial.println("You are okay! 👍");
    }

    // A short delay to keep the loop from running too fast
    delay(100);
}

// Reads all sensor values
void readAllSensors() {
    // Read EMG sensors
    bicepsEmg = ads.readADC_SingleEnded(0);
    tricepsEmg = ads.readADC_SingleEnded(1);

    // Read heart pulse sensor. The library's BPM calculation is handled internally.
    heartRate = pulseSensor.getBeatsPerMinute();
    
    // Calculate elbow angle
    calculateElbowAngle();
}

// Calculates the elbow angle using the Adafruit library's accelerometer data
void calculateElbowAngle() {
    sensors_event_t elbowAccel, elbowGyro, elbowTemp;
    sensors_event_t wristAccel, wristGyro, wristTemp;
    
    mpuElbow.getEvent(&elbowAccel, &elbowGyro, &elbowTemp);
    mpuWrist.getEvent(&wristAccel, &wristGyro, &wristTemp);
    
    float elbowTilt = atan2(elbowAccel.acceleration.y, elbowAccel.acceleration.z) * 180 / PI;
    float wristTilt = atan2(wristAccel.acceleration.y, wristAccel.acceleration.z) * 180 / PI;
    
    elbowAngle = abs(elbowTilt - wristTilt);
}

// Displays sensor data in a formatted string
void displaySensorData() {
    Serial.print("Angle:");
    Serial.print(elbowAngle, 2);
    Serial.print(" | Biceps EMG:");
    Serial.print(bicepsEmg);
    Serial.print(" | Triceps EMG:");
    Serial.print(tricepsEmg);
    Serial.print(" | Heart Rate:");
    Serial.println(heartRate);
}

// Evaluates the sensor data against the predefined thresholds
void evaluateThresholds() {
    bool isRest = (bicepsEmg < 50 && tricepsEmg < 50);
    bool isMovement = !isRest;
    
    // --- Check for STOP conditions (Red Flags) ---
    if (isRest) {
        if (bicepsEmg > stopThresholds.bicepsRestMax) {
            Serial.println(">>> RED FLAG: Biceps EMG at rest is too high!");
            isOkay = false;
            return;
        }
        if (tricepsEmg > stopThresholds.tricepsRestMax) {
            Serial.println(">>> RED FLAG: Triceps EMG at rest is too high!");
            isOkay = false;
            return;
        }
    }
    
    if (isMovement) {
        if (bicepsEmg < stopThresholds.bicepsMoveMin) {
            Serial.println(">>> RED FLAG: Biceps EMG during movement is too low!");
            isOkay = false;
            return;
        }
        if (tricepsEmg < stopThresholds.tricepsMoveMin) {
            Serial.println(">>> RED FLAG: Triceps EMG during movement is too low!");
            isOkay = false;
            return;
        }
        if (elbowAngle < stopThresholds.elbowAngleMin) {
            Serial.println(">>> RED FLAG: Elbow angle is too low!");
            isOkay = false;
            return;
        }
        if (bicepsEmg > stopThresholds.coActivationMin && tricepsEmg > stopThresholds.coActivationMin) {
            Serial.println(">>> RED FLAG: Co-activation detected (both muscles engaged simultaneously)!");
            isOkay = false;
            return;
        }
    }
    
    if (heartRate > stopThresholds.maxHeartRate) {
        Serial.println(">>> RED FLAG: Heart rate is too high!");
        isOkay = false;
        return;
    }
    
    // --- Check for Warning conditions (Yellow Flags) ---
    if (isRest) {
        if (bicepsEmg > warningThresholds.bicepsRestMax) {
            Serial.println(">> YELLOW FLAG: Biceps EMG at rest is elevated.");
            isOkay = false;
        }
        if (tricepsEmg > warningThresholds.tricepsRestMax) {
            Serial.println(">> YELLOW FLAG: Triceps EMG at rest is elevated.");
            isOkay = false;
        }
    }
    
    if (isMovement) {
        if (bicepsEmg < warningThresholds.bicepsMoveMin) {
            Serial.println(">> YELLOW FLAG: Biceps EMG during movement is low.");
            isOkay = false;
        }
        if (tricepsEmg < warningThresholds.tricepsMoveMin) {
            Serial.println(">> YELLOW FLAG: Triceps EMG during movement is low.");
            isOkay = false;
        }
        if (elbowAngle < warningThresholds.elbowAngleMin) {
            Serial.println(">> YELLOW FLAG: Elbow angle is low.");
            isOkay = false;
        }
    }
}