#include "HX711.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// --- HX711 (Weight) ---
#define HX711_DOUT  12
#define HX711_SCK   14
#define BP_PIN      A0
HX711 scale;
const float CALIBRATION_FACTOR = -7050.0;

// --- DS18B20 (Temperature) ---
#define ONE_WIRE_BUS 16
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- DFPlayer Mini (MP3) ---
#define RX_PIN 5
#define TX_PIN 4
SoftwareSerial mp3Serial(RX_PIN, TX_PIN);
DFRobotDFPlayerMini mp3;
// --- WiFi ---
const char* ssid     = "iot";
const char* password = "12345678";
const char* POST_URL = "http://iotcloud22.in/4467/post_value.php";
const char* CMD_URL  = "http://iotcloud22.in/4467/light.json";
WiFiClient client;

// --- Timing ---
unsigned long lastPollMs   = 0;
unsigned long lastRepeatMs = 0;
const unsigned long POLL_INTERVAL   = 1500;
const unsigned long REPEAT_INTERVAL = 10000;

// --- Track State ---
int currentCmdTrack  = 0;
int lastSeenCmdTrack = 0;

// =====================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  // HX711
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();
  Serial.println("HX711 Ready.");

  // DS18B20
  sensors.begin();
  Serial.print("DS18B20 sensors: ");
  Serial.println(sensors.getDeviceCount());

  // MP3
  mp3Serial.begin(9600);
  delay(500);
  if (!mp3.begin(mp3Serial)) {
    Serial.println("ERROR: MP3 init failed!");
    while (1) { delay(2000); }
  }
  mp3.volume(20);
  mp3.EQ(DFPLAYER_EQ_NORMAL);
  mp3.outputDevice(DFPLAYER_DEVICE_SD);
  Serial.println("MP3 Ready.");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  Serial.println(WiFi.status()==WL_CONNECTED ? "WiFi OK" : "WiFi FAILED");
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect(); WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 10000)
    delay(300);
  return WiFi.status() == WL_CONNECTED;
}
void sendData(float weight, float tempC, int bpm) {
  if (!ensureWiFi()) return;
  HTTPClient http;
  http.begin(client, POST_URL);
  http.addHeader("Content-Type","application/x-www-form-urlencoded");
  http.setTimeout(5000);
  String postData = "value1=" + String(weight,2)
                  + "&value2=" + String(tempC,2)
                  + "&value3=" + String(bpm);
  int code = http.POST(postData);
  Serial.println(code==HTTP_CODE_OK ? "POST: SUCCESS" : "POST: FAILED");
  http.end();
}

int fetchTrackCommand() {
  if (!ensureWiFi()) return 0;
  HTTPClient http;
  int trackNum = 0;
  if (!http.begin(client, CMD_URL)) return 0;
  http.setTimeout(5000);
  if (http.GET() == HTTP_CODE_OK) {
    StaticJsonDocument<256> doc; // FIX: increased buffer
    if (!deserializeJson(doc, http.getString())) {
      trackNum = doc["track"].as<int>();
    }
  }
  http.end();
  return (trackNum >= 1 && trackNum <= 4) ? trackNum : 0;
}

// =====================================================
void loop() {
  float weight = scale.get_units(5) / 36.0;
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  int raw = analogRead(BP_PIN);
  int bpm = (raw < 620) ? 0 : raw / 9.7;

  Serial.println("=============================");
  Serial.print("Weight : "); Serial.print(weight,2); Serial.println(" kg");
  Serial.print("BPM    : "); Serial.println(bpm);
  Serial.print("Temp   : "); Serial.print(tempC,2); Serial.println(" C");

  unsigned long now = millis();

  // Poll cloud every 1.5s
  if (now - lastPollMs >= POLL_INTERVAL) {
    lastPollMs = now;
    int cmdTrack = fetchTrackCommand();
    if (cmdTrack != lastSeenCmdTrack) {
      lastSeenCmdTrack = cmdTrack;
      if (cmdTrack > 0) {
        currentCmdTrack = cmdTrack;
        mp3.play(currentCmdTrack);
        lastRepeatMs = now;
      } else { currentCmdTrack = 0; }
    }
  }

  // Repeat MP3 every 10s
  if (currentCmdTrack > 0 && (now - lastRepeatMs >= REPEAT_INTERVAL)) {
    mp3.play(currentCmdTrack);
    lastRepeatMs = now;
  }

  sendData(weight, tempC, bpm);
  delay(200);
}
