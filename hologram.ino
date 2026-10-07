#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ===== WiFi Credentials =====
const char* ssid     = "DESKTOP-5J8MM72 4221_";
const char* password = "11111113";

// ===== Web Server on port 80 =====
WebServer server(80);

// ===== Handle Root Page (index.html) =====
void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "index.html not found in LittleFS");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// ===== Handle Text from HTML (via POST) =====
void handleText() {
  if (server.hasArg("plain")) {
    String receivedText = server.arg("plain");
    
    Serial.println("========== RECEIVED FROM HTML ==========");
    Serial.print("Text: ");
    Serial.println(receivedText);
    Serial.println("========================================");
    
    server.send(200, "text/plain", "OK: " + receivedText);
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

// ===== 404 Handler =====
void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 Hologram Project Starting ===");

  // Mount LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }
  Serial.println("LittleFS Mounted Successfully");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Open this IP in your browser to see hologram control page");

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/send", HTTP_POST, handleText);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP Server Started");
}

void loop() {
  server.handleClient();
}
