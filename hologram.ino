#include <WiFi.h>
#include <WebServer.h>

const char* WIFI_SSID = "DESKTOP-5J8MM72 4221_";
const char* WIFI_PASS = "11111113";

#define LED_BUILTIN_PIN 8
#define DATA_PIN   0
#define CLOCK_PIN  1
#define LATCH_PIN  2

// ---------- IR receiver ----------
#define IR_PIN     21         // IR receiver OUT -> GPIO 4
#define IR_ACTIVE_LOW true   // Most IR receivers are active-LOW

WebServer server(80);

#define MAX_LINES 64
uint16_t lines[MAX_LINES];
uint16_t lineCount   = 0;
uint32_t frameDelay  = 100;    // ms between lines
uint32_t startDelay  = 0;      // ms to wait after IR trigger before playing
bool     playing     = false;
uint32_t lastStep    = 0;
uint16_t playIndex   = 0;

// ---------- Trigger state machine ----------
enum TrigState {
  TS_WAITING_IR,       // idle, waiting for IR signal
  TS_START_DELAY,      // IR received, waiting for startDelay to elapse
  TS_PLAYING           // running the frame
};
TrigState trigState = TS_WAITING_IR;
uint32_t  trigTime = 0;

String postBody = "";

// WiFi LED
enum WifiLedState { WIFI_LED_CONNECTING, WIFI_LED_ONLINE, WIFI_LED_OFFLINE };
WifiLedState wifiLedState = WIFI_LED_CONNECTING;
uint32_t wifiLedTimer = 0;
bool     wifiLedOn    = false;

void updateWifiLed() {
  uint32_t now = millis();
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    if (wifiLedState != WIFI_LED_ONLINE) {
      wifiLedState = WIFI_LED_ONLINE;
      digitalWrite(LED_BUILTIN_PIN, HIGH);
      wifiLedOn = true;
    }
    return;
  }
  if (wifiLedState == WIFI_LED_ONLINE) {
    wifiLedState = WIFI_LED_OFFLINE;
    digitalWrite(LED_BUILTIN_PIN, LOW);
    wifiLedOn = false;
    wifiLedTimer = now;
    return;
  }
  if (now - wifiLedTimer >= 500) {
    wifiLedTimer = now;
    wifiLedOn = !wifiLedOn;
    digitalWrite(LED_BUILTIN_PIN, wifiLedOn ? HIGH : LOW);
  }
}

void writeShiftRegister(uint16_t data) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (data >> 8) & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data & 0xFF);
  digitalWrite(LATCH_PIN, HIGH);
}

void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleBody() {
  postBody = server.hasArg("plain") ? server.arg("plain") : "";
}

void handlePing() {
  sendCORSHeaders();
  server.send(200, "application/json", "{\"ok\":1}");
}

void handleStatus() {
  sendCORSHeaders();
  uint16_t s = (lineCount > 0) ? lines[0] : 0;
  String json = "{\"state\":" + String(s) + ",\"lines\":" + String(lineCount) + "}";
  server.send(200, "application/json", json);
}

void handleSet() {
  sendCORSHeaders();
  long v = 0;
  if (server.hasArg("value")) v = server.arg("value").toInt();
  else {
    String body = server.hasArg("plain") ? server.arg("plain") : postBody;
    v = body.toInt();
  }
  if (v < 0) v = 0;
  if (v > 65535) v = 65535;

  // /set acts as immediate override (no trigger needed)
  trigState = TS_PLAYING;
  playing   = true;

  uint16_t val = (uint16_t)v;
  writeShiftRegister(val);
  lines[0] = val;
  lineCount = 1;
  frameDelay = 100;

  String json = "{\"state\":" + String(val) + "}";
  server.send(200, "application/json", json);
}

// ---------- /frames : load a frame, wait for IR ----------
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

  // frame delay
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

  // start delay (after IR trigger)
  long sd = 0;
  int sk = body.indexOf("\"startDelay\"");
  if (sk >= 0) {
    int c = body.indexOf(':', sk);
    if (c > 0) {
      sd = body.substring(c + 1).toInt();
      if (sd < 0) sd = 0;
    }
  }
  startDelay = (uint32_t)sd;

  // lines
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
  Serial.print("Frame delay: ");
  Serial.print(frameDelay);
  Serial.print(" ms, Start delay: ");
  Serial.print(startDelay);
  Serial.println(" ms");

  // *** Arm for IR trigger instead of playing immediately ***
  trigState = TS_WAITING_IR;

  // Turn LEDs off while waiting
  writeShiftRegister(0);

  String resp = "{\"ok\":1,\"lines\":" + String(lineCount) +
                ",\"delay\":" + String(frameDelay) +
                ",\"startDelay\":" + String(startDelay) + "}";
  server.send(200, "application/json", resp);
}

void handleStop() {
  sendCORSHeaders();
  trigState = TS_WAITING_IR;
  playing   = false;
  writeShiftRegister(0);
  lines[0] = 0;
  lineCount = 1;
  server.send(200, "application/json", "{\"ok\":1,\"stopped\":1}");
}

void handleOptions() {
  sendCORSHeaders();
  server.send(204);
}

// ---------- IR read ----------
bool irSignalActive() {
  int v = digitalRead(IR_PIN);
  return IR_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
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

  // IR pin
  pinMode(IR_PIN, INPUT_PULLUP);   // receiver idles HIGH
  Serial.print("IR pin ready on GPIO ");
  Serial.println(IR_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
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
  digitalWrite(LED_BUILTIN_PIN, HIGH);
  wifiLedState = WIFI_LED_ONLINE;

  Serial.print("Connected! Device IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ping",   HTTP_GET,     handlePing);
  server.on("/status", HTTP_GET,     handleStatus);
  server.on("/set",    HTTP_GET,     handleSet);
  server.on("/frames", HTTP_POST,    handleFrames, handleBody);
  server.on("/stop",   HTTP_GET,     handleStop);

  server.on("/ping",   HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/set",    HTTP_OPTIONS, handleOptions);
  server.on("/frames", HTTP_OPTIONS, handleOptions);
  server.on("/stop",   HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("HTTP server started on port 80");
  Serial.println("Waiting for IR trigger...");
}

void loop() {
  server.handleClient();
  updateWifiLed();

  uint32_t now = millis();

  // ---- Trigger state machine ----
  switch (trigState) {

    case TS_WAITING_IR:
      if (irSignalActive() && lineCount > 0) {
        Serial.println("IR signal detected!");
        trigTime = now;
        playIndex = 0;
        lastStep  = now;

        if (startDelay == 0) {
          Serial.println("Starting frame (no start delay)");
          trigState = TS_PLAYING;
          playing   = true;
          writeShiftRegister(lines[0]);
        } else {
          Serial.print("Start delay: ");
          Serial.print(startDelay);
          Serial.println(" ms");
          trigState = TS_START_DELAY;
        }
      }
      break;

    case TS_START_DELAY:
      if (now - trigTime >= startDelay) {
        Serial.println("Start delay elapsed - playing frame (once)");
        trigState = TS_PLAYING;
        playing   = true;
        playIndex = 0;
        lastStep  = now;
        if (lineCount > 0) writeShiftRegister(lines[0]);
      }
      break;

    case TS_PLAYING:
      if (playing && lineCount > 0) {
        if (now - lastStep >= frameDelay) {
          lastStep = now;
          playIndex++;

          if (playIndex >= lineCount) {
            // Frame finished (each line played once).
            // Turn LEDs off and go back to waiting for the next IR trigger.
            Serial.println("Frame complete. Waiting for next IR...");
            writeShiftRegister(0);
            playing   = false;
            trigState = TS_WAITING_IR;
          } else {
            writeShiftRegister(lines[playIndex]);
          }
        }
      }
      break;
  }
}
