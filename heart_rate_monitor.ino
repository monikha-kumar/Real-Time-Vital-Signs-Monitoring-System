#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"
// --- WiFi Credentials ---
const char* ssid     = "iot";
const char* password = "12345678";

// --- Cloud Server URL ---
const char* postServerURL = "http://iotcloud22.in/4467/post_value1.php";

// --- Objects ---
MAX30105 particleSensor;
WiFiClient client;

// --- Timing ---
unsigned long lastSendMs = 0;
const unsigned long SEND_INTERVAL = 1000; // 1 second

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Heart Rate Monitor ===");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FAILED. Running offline.");
  }

  // MAX30105
  Wire.begin(4, 5); // SDA=GPIO4, SCL=GPIO5
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30105 not found!");
    while (1) { delay(1000); }
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x00);
  particleSensor.setPulseAmplitudeGreen(0x00);
  particleSensor.setPulseAmplitudeIR(0x1F);
  Serial.println("MAX30105 Ready.");
}

// --- WiFi Reconnect ---
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(300); Serial.print(".");
  }
  return WiFi.status() == WL_CONNECTED;
}

// =====================================================
void loop() {
  long irValue = particleSensor.getIR();

// FIX: Correct threshold for finger detection
  long G = (irValue < 50000) ? 0 : irValue / 724;
  String status = (irValue < 50000) ? "No finger" : "Finger detected";

  Serial.println("-----------------------------");
  Serial.print("IR Raw   : "); Serial.println(irValue);
  Serial.print("Heart Rate: "); Serial.print(G); Serial.println(" BPM");
  Serial.print("Status   : "); Serial.println(status);

  unsigned long now = millis();
  if (now - lastSendMs >= SEND_INTERVAL) {
    lastSendMs = now;
    if (ensureWiFi()) {
      HTTPClient http;
      http.begin(client, postServerURL);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      http.setTimeout(5000);
      int code = http.POST("value1=" + String(G));
      Serial.print("POST code: "); Serial.println(code);
      Serial.println(code == HTTP_CODE_OK ? "Cloud: SUCCESS" : "Cloud: FAILED");
      http.end();
    }
  }
  delay(100);
}
 
