#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* TELEMETRY_URL = "https://YOUR-VERCEL-PROJECT.vercel.app/api/telemetry";
const char* DEVICE_TOKEN = "REPLACE_WITH_TELEMETRY_DEVICE_TOKEN";

const int RELAY_PIN = 23;
const int ALERT_LED = 2;
const int BTN_ATTACK = 4;
const int BTN_FAULT = 16;
const unsigned long SEND_INTERVAL_MS = 1500;

unsigned long lastSend = 0;
int blockedAttackCount = 0;
bool relayIsolated = false;
uint32_t eventId = 1;
uint32_t bootId = 0;
String eventType = "SYS";
String eventMessage = "ESP32 booted; security whitelist active.";
bool lastAttackButton = HIGH;
bool lastFaultButton = HIGH;

String escapeJson(const String& text) {
  String out;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') out += "\\n";
    else if (c != '\r') out += c;
  }
  return out;
}

void setEvent(const String& type, const String& message) {
  eventId++;
  eventType = type;
  eventMessage = message;
  Serial.printf("[%s] %s\n", type.c_str(), message.c_str());
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  Serial.print("Connecting to Wokwi-GUEST");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " not connected yet");
}

void sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }
  WiFiClientSecure client;
  // Wokwi demo only. For physical hardware, validate the server CA certificate.
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, TELEMETRY_URL)) {
    Serial.println("Telemetry POST: could not initialize HTTPS");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);
  String json = "{";
  json += "\"deviceId\":\"inkanini-wokwi-esp32\",";
  json += "\"blockedAttackCount\":" + String(blockedAttackCount) + ",";
  json += "\"relayIsolated\":" + String(relayIsolated ? "true" : "false") + ",";
  json += "\"protectionActive\":true,";
  json += "\"baudRate\":9600,";
  json += "\"lineQuality\":\"" + String(relayIsolated ? "FAULT" : "CLEAN") + "\",";
  json += "\"uptimeSeconds\":" + String(millis() / 1000) + ",";
  json += "\"voltage\":3.2,";
  json += "\"event\":{\"id\":\"inkanini-wokwi-" + String(bootId, HEX) + "-" + String(eventId) + "\",\"type\":\"" + eventType + "\",\"message\":\"" + escapeJson(eventMessage) + "\"}";
  json += "}";
  int status = http.POST(json);
  Serial.printf("Telemetry POST: HTTP %d\n", status);
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ALERT_LED, OUTPUT);
  pinMode(BTN_ATTACK, INPUT_PULLUP);
  pinMode(BTN_FAULT, INPUT_PULLUP);
  bootId = esp_random();
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(ALERT_LED, LOW);
  connectWiFi();
  sendTelemetry();
}

void loop() {
  bool attackButton = digitalRead(BTN_ATTACK);
  bool faultButton = digitalRead(BTN_FAULT);
  if (lastAttackButton == HIGH && attackButton == LOW) {
    blockedAttackCount++;
    setEvent("ATTACK", "Unauthorized Modbus register write (function 0x06) intercepted and dropped.");
    digitalWrite(ALERT_LED, HIGH);
  }
  if (lastFaultButton == HIGH && faultButton == LOW) {
    relayIsolated = !relayIsolated;
    digitalWrite(RELAY_PIN, relayIsolated ? LOW : HIGH);
    digitalWrite(ALERT_LED, relayIsolated ? HIGH : LOW);
    setEvent(relayIsolated ? "WARN" : "SECURE", relayIsolated ? "Bus heartbeat lost; watchdog isolated the relay." : "Bus restored; watchdog heartbeat is healthy.");
  }
  lastAttackButton = attackButton;
  lastFaultButton = faultButton;
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendTelemetry();
  }
  delay(15);
}
