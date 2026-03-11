#include <SPI.h>
#include <LoRa.h>

// Define pins for ESP32
#define SCK 18
#define MISO 19
#define MOSI 23
#define SS 5
#define RST 14
#define DIO0 2

unsigned long lastSendTime = 0;
int interval = 5000;          // Send a message every 5 seconds
int msgCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Duplex Node Initializing...");

  // Setup LoRa SPI pins
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) { // Match your local frequency
    Serial.println("LoRa init failed. Check your wiring!");
    while (1);
  }

  Serial.println("LoRa init succeeded. Listening...");
}

void loop() {
  // --- PART 1: RECEIVE LOGIC ---
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received: ");
    while (LoRa.available()) {
      String incoming = LoRa.readString();
      Serial.print(incoming);
    }
    Serial.print(" | RSSI: ");
    Serial.println(LoRa.packetRssi());
  }

  // --- PART 2: TRANSMIT LOGIC ---
  if (millis() - lastSendTime > interval) {
    String message = "Node Data #" + String(msgCount);
    
    Serial.println("Sending: " + message);
    
    LoRa.beginPacket();
    LoRa.print(message);
    LoRa.endPacket();

    msgCount++;
    lastSendTime = millis();
    
    // The library automatically returns to standby/listen mode 
    // after endPacket() is called.
  }
}