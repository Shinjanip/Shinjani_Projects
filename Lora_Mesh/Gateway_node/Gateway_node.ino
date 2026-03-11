#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// LoRa Pins (Standard ESP32)
#define SS 5
#define RST 14
#define DIO0 2

// Wi-Fi Credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server URL (Optional: Replace with your API or Webhook)
const char* serverName = "http://your-api-endpoint.com/data";

long lastSeenIds[20]; // Larger cache for the gateway
int idIndex = 0;

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Initialize LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Init Failed!");
    while (1);
  }
  Serial.println("Gateway Online. Listening for Mesh packets...");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Parse the Packet ID to avoid duplicates
    long msgId = getValue(incoming, ':', 1).toInt();

    if (!isDuplicate(msgId)) {
      lastSeenIds[idIndex] = msgId;
      idIndex = (idIndex + 1) % 20;

      Serial.println("\n--- NEW MESH DATA ---");
      Serial.println("Raw: " + incoming);
      
      // Extract specific data points
      String origin = getValue(incoming, '|', 2); // e.g., "ORG:NODE_01"
      String data = getValue(incoming, '|', 3);   // e.g., "DATA:F0,H1"
      
      Serial.println("From: " + origin);
      Serial.println("Sensor Data: " + data);
      Serial.println("RSSI: " + String(LoRa.packetRssi()));

      // Send to Cloud/Server
      if (WiFi.status() == WL_CONNECTED) {
        sendToCloud(origin, data);
      }
    }
  }
}

bool isDuplicate(long id) {
  for (int i = 0; i < 20; i++) {
    if (lastSeenIds[i] == id) return true;
  }
  return false;
}

void sendToCloud(String node, String sensorData) {
  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Constructing a simple POST body
  String httpRequestData = "node=" + node + "&payload=" + sensorData;
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// Helper to parse the pipe-separated string
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}