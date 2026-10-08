#include <WiFi.h>
#include <WebServer.h>

// ---------- WiFi Credentials ----------
const char* WIFI_SSID = "DESKTOP-5J8MM72 4221_";
const char* WIFI_PASS = "11111113";

// ---------- 74HC595 Pins ----------
#define DATA_PIN   0
#define CLOCK_PIN  1
#define LATCH_PIN  2

WebServer server(80);

// ---------- Frame storage ----------
// Each frame: up to 8 lines of 16-bit values + delay in ms
#define MAX_FRAMES      32
#define MAX_LINES       32

struct Frame {
  uint16_t lines[MAX_LINES];
  uint8_t  lineCount;
  uint32_t delayMs;
};

Frame    frames[MAX_FRAMES];
uint16_t frameCount    = 0;
bool     playing       = false;
uint32_t lastFrameTime = 0;
uint16_t playIndex     = 0;

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

// ---------- /ping ----------
void handlePing() {
  sendCORSHeaders();
  server.send(200, "application/json", "{\"ok\":1}");
}

// ---------- /frames : receive JSON payload ----------
// Expected JSON:
// {
//   "delay": 100,
//   "frames": [
//     { "lines": ["00000000","11111111"], "delay": 100 },
//     { "lines": ["10101010"], "delay": 200 }
//   ]
// }
void handleFrames() {
  sendCORSHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.println("Received frames payload:");
  Serial.println(body);

  // --- Minimal JSON parsing (no external library) ---
  // Find "delay":
  int delayKey = body.indexOf("\"delay\"");
  long globalDelay = 100;
  if (delayKey >= 0) {
    int colon = body.indexOf(':', delayKey);
    if (colon > 0) {
      globalDelay = body.substring(colon + 1).toInt();
      if (globalDelay <= 0) globalDelay = 100;
    }
  }

  // Parse frames array
  frameCount = 0;
  int framesKey = body.indexOf("\"frames\"");
  if (framesKey < 0) {
    server.send(400, "application/json", "{\"error\":\"no frames\"}");
    return;
  }

  int arrStart = body.indexOf('[', framesKey);
  if (arrStart < 0) {
    server.send(400, "application/json", "{\"error\":\"bad frames\"}");
    return;
  }

  int pos = arrStart + 1;
  while (pos < body.length() && frameCount < MAX_FRAMES) {
    int objStart = body.indexOf('{', pos);
    if (objStart < 0) break;
    int objEnd = body.indexOf('}', objStart);
    if (objEnd < 0) break;

    String obj = body.substring(objStart + 1, objEnd);

    // per-frame delay
    uint32_t frameDelay = globalDelay;
    int dk = obj.indexOf("\"delay\"");
    if (dk >= 0) {
      int c = obj.indexOf(':', dk);
      if (c > 0) {
        long v = obj.substring(c + 1).toInt();
        if (v > 0) frameDelay = (uint32_t)v;
      }
    }

    // lines array
    int linesKey = obj.indexOf("\"lines\"");
    if (linesKey >= 0) {
      int la = obj.indexOf('[', linesKey);
      int le = obj.indexOf(']', la);
      if (la > 0 && le > la) {
        String linesStr = obj.substring(la + 1, le);

        uint8_t lc = 0;
        int p = 0;
        while (p < linesStr.length() && lc < MAX_LINES) {
          int q1 = linesStr.indexOf('"', p);
          if (q1 < 0) break;
          int q2 = linesStr.indexOf('"', q1 + 1);
          if (q2 < 0) break;

          String bits = linesStr.substring(q1 + 1, q2);
          bits.trim();

          // Convert binary string -> uint16_t
          uint16_t val = 0;
          for (int i = 0; i < bits.length() && i < 16; i++) {
            val <<= 1;
            if (bits[i] == '1') val |= 1;
          }
          frames[frameCount].lines[lc++] = val;
          p = q2 + 1;
        }
        frames[frameCount].lineCount = lc;
      }
    }

    frames[frameCount].delayMs = frameDelay;
    frameCount++;

    pos = objEnd + 1;
    int nextObj = body.indexOf('{', pos);
    int arrEnd  = body.indexOf(']', pos);
    if (arrEnd >= 0 && (nextObj < 0 || nextObj > arrEnd)) break;
  }

  Serial.print("Parsed frames: ");
  Serial.println(frameCount);

  // Start playback
  playIndex     = 0;
  lastFrameTime = millis();
  playing       = (frameCount > 0);

  if (playing) {
    writeShiftRegister(frames[0].lines[0]);
  }

  String resp = "{\"ok\":1,\"frames\":" + String(frameCount) + "}";
  server.send(200, "application/json", resp);
}

// ---------- /stop ----------
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

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  writeShiftRegister(0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Device IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ping",   HTTP_GET,     handlePing);
  server.on("/frames", HTTP_POST,    handleFrames);
  server.on("/stop",   HTTP_GET,     handleStop);
  server.on("/ping",   HTTP_OPTIONS, handleOptions);
  server.on("/frames", HTTP_OPTIONS, handleOptions);
  server.on("/stop",   HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("HTTP server started on port 80");
}

// ---------- Loop ----------
void loop() {
  server.handleClient();

  if (playing && frameCount > 0) {
    uint32_t now = millis();
    Frame &f = frames[playIndex];

    // Walk through this frame's lines using its delay
    static uint8_t lineIdx = 0;
    if (now - lastFrameTime >= f.delayMs) {
      lastFrameTime = now;
      lineIdx++;
      if (lineIdx >= f.lineCount) {
        lineIdx = 0;
        playIndex++;
        if (playIndex >= frameCount) playIndex = 0;
      }
      if (frames[playIndex].lineCount > 0) {
        writeShiftRegister(frames[playIndex].lines[lineIdx]);
      }
    }
  }
}
