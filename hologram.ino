#include <WiFi.h>
#include <WebServer.h>

// ---------- WiFi Credentials ----------
const char* WIFI_SSID = "DESKTOP-5J8MM72 4221_";
const char* WIFI_PASS = "11111113";

// ---------- 74HC595 Pins ----------
#define DATA_PIN   0    // DS  - Pin 14 on 74HC595
#define CLOCK_PIN  1    // SHCP - Pin 11 on 74HC595
#define LATCH_PIN  2    // STCP - Pin 12 on 74HC595

// ---------- Server ----------
WebServer server(80);

// Current 16-bit output state
uint16_t currentState = 0x0000;

// ---------- Send 16-bit data to cascaded shift registers ----------
void writeShiftRegister(uint16_t data) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (data >> 8) & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data & 0xFF);
  digitalWrite(LATCH_PIN, HIGH);
  currentState = data;
}

// ---------- CORS ----------
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ---------- /ping : lightweight connectivity check ----------
void handlePing() {
  sendCORSHeaders();
  server.send(200, "application/json", "{\"ok\":1}");
}

// ---------- /status ----------
void handleStatus() {
  sendCORSHeaders();
  String json = "{\"state\":" + String(currentState) + "}";
  server.send(200, "application/json", json);
}

// ---------- /set?value=NNNN ----------
void handleSet() {
  sendCORSHeaders();
  if (!server.hasArg("value")) {
    server.send(400, "application/json", "{\"error\":\"missing value\"}");
    return;
  }
  long v = server.arg("value").toInt();
  if (v < 0) v = 0;
  if (v > 65535) v = 65535;
  writeShiftRegister((uint16_t)v);
  Serial.print("Set state -> ");
  Serial.println(v);
  String json = "{\"state\":" + String(currentState) + "}";
  server.send(200, "application/json", json);
}

void handleOptions() {
  sendCORSHeaders();
  server.send(204);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  writeShiftRegister(0x0000);

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! Device IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ping",   HTTP_GET,     handlePing);
  server.on("/status", HTTP_GET,     handleStatus);
  server.on("/set",    HTTP_GET,     handleSet);
  server.on("/ping",   HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/set",    HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();
}
