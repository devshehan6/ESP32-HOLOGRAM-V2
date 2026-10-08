#include <WiFi.h>
#include <WebServer.h>

// ---------- WiFi Credentials ----------
const char* WIFI_SSID = "DESKTOP-5J8MM72 4221_";
const char* WIFI_PASS = "11111113";

// ---------- Built-in LED ----------
// Most ESP32 dev boards use GPIO 2 for the on-board LED.
// If yours is different (some use GPIO 5, 15, etc.), change this.
#define LED_BUILTIN_PIN 2

// ---------- 74HC595 Pins ----------
#define DATA_PIN   0
#define CLOCK_PIN  1
#define LATCH_PIN  2

WebServer server(80);

#define MAX_LINES 64
uint16_t lines[MAX_LINES];
uint16_t lineCount  = 0;
uint32_t frameDelay = 100;
bool     playing    = false;
uint32_t lastStep   = 0;
uint16_t playIndex  = 0;

String postBody = "";

// ---------- WiFi LED state machine ----------
enum WifiLedState { WIFI_LED_CONNECTING, WIFI_LED_ONLINE, WIFI_LED_OFFLINE };
WifiLedState wifiLedState = WIFI_LED_CONNECTING;
uint32_t     wifiLedTimer = 0;
bool         wifiLedOn    = false;

void updateWifiLed() {
  uint32_t now = millis();

  // Detect current WiFi status
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    if (wifiLedState != WIFI_LED_ONLINE) {
      wifiLedState = WIFI_LED_ONLINE;
      digitalWrite(LED_BUILTIN_PIN, HIGH);   // Solid ON
      wifiLedOn = true;
    }
    return;
  }

  // Not connected
  if (wifiLedState == WIFI_LED_ONLINE) {
    // Just dropped
    wifiLedState = WIFI_LED_OFFLINE;
    digitalWrite(LED_BUILTIN_PIN, LOW);
    wifiLedOn = false;
    wifiLedTimer = now;
    return;
  }

  // Connecting → blink slowly (500 ms on/off)
  if (now - wifiLedTimer >= 500) {
    wifiLedTimer = now;
    wifiLedOn = !wifiLedOn;
    digitalWrite(LED_BUILTIN_PIN, wifiLedOn ? HIGH : LOW);
  }
}

// ---------- Shift register ----------
void writeShiftRegister(uint16_t data) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (data >> 8) & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data & 0xFF);
  digitalWrite(LATCH_PIN, HIGH);
}

// ---------- CORS ----------
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleBody() {
  if (server.hasArg("plain")) postBody = server.arg("plain");
  else postBody = "";
}

void handlePing() {
  sendCORSHeaders();
  server.send(200, "application/json", "{\"ok\":1}");
}

// ---------- /frames ----------
void handleFrames() {
  sendCORSHeaders();

  String body = server.hasArg("plain") ? server.arg("plain") : postBody;
  Serial.print("Body length: ");
  Serial.println(body.length());
  Serial.println(body);

  if (body.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }

  long d = 100;
  int dk = body.indexOf("\"delay\"");
  if (dk >= 0) {
    int c = body.indexOf(':', dk);
    if (c > 0) {
      d = body.substring(c + 1).toInt();
      if (d <= 0) d = 100;
    }
  }
  frameDelay = (uint32_t)d;

  lineCount = 0;
  int lk = body.indexOf("\"lines\"");
  if (lk < 0) {
    server.send(400, "application/json", "{\"error\":\"no lines key\"}");
    return;
  }
  int la = body.indexOf('[', lk);
  int le = body.indexOf(']', la);
  if (la < 0 || le < 0) {
    server.send(400, "application/json", "{\"error\":\"bad lines array\"}");
    return;
  }

  String arr = body.substring(la + 1, le);
  int p = 0;
  while (p < (int)arr.length() && lineCount < MAX_LINES) {
    int q1 = arr.indexOf('"', p);
    if (q1 < 0) break;
    int q2 = arr.indexOf('"', q1 + 1);
    if (q2 < 0) break;

    String bits = arr.substring(q1 + 1, q2);
    bits.trim();

    uint16_t val = 0;
    for (int i = 0; i < (int)bits.length() && i < 16; i++) {
      val <<= 1;
      if (bits[i] == '1') val |= 1;
    }
    lines[lineCount++] = val;
    p = q2 + 1;
  }

  Serial.print("Lines parsed: ");
  Serial.println(lineCount);

  playIndex = 0;
  lastStep  = millis();
  playing   = (lineCount > 0);
  if (playing) writeShiftRegister(lines[0]);

  String resp = "{\"ok\":1,\"lines\":" + String(lineCount) + ",\"delay\":" + String(frameDelay) + "}";
  server.send(200, "application/json", resp);
}

void handleStop() {
  sendCORSHeaders();
  playing = false;
  writeShiftRegister(0);
  server.send(200, "application/json", "{\"ok\":1,\"stopped\":1}");
}

void handleOptions() {
  sendCORSHeaders();
  server.send(204);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_BUILTIN_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN_PIN, LOW);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  writeShiftRegister(0);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  // Blink the built-in LED while connecting
  uint32_t blinkTimer = millis();
  bool blinkOn = false;
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - blinkTimer >= 250) {
      blinkTimer = millis();
      blinkOn = !blinkOn;
      digitalWrite(LED_BUILTIN_PIN, blinkOn ? HIGH : LOW);
    }
    delay(10);
    Serial.print(".");
  }
  Serial.println();

  // Connected → solid ON
  digitalWrite(LED_BUILTIN_PIN, HIGH);
  wifiLedState = WIFI_LED_ONLINE;

  Serial.print("Connected! Device IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ping",   HTTP_GET,     handlePing);
  server.on("/frames", HTTP_POST,    handleFrames, handleBody);
  server.on("/stop",   HTTP_GET,     handleStop);

  server.on("/ping",   HTTP_OPTIONS, handleOptions);
  server.on("/frames", HTTP_OPTIONS, handleOptions);
  server.on("/stop",   HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();
  updateWifiLed();     // Reflect WiFi state on the built-in LED

  if (playing && lineCount > 0) {
    uint32_t now = millis();
    if (now - lastStep >= frameDelay) {
      lastStep = now;
      playIndex++;
      if (playIndex >= lineCount) playIndex = 0;
      writeShiftRegister(lines[playIndex]);
    }
  }
}
