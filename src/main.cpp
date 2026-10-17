#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "credentials.h"

namespace {
constexpr char kStatusTopic[] = "hottubHVAC/status";
constexpr unsigned long kReconnectIntervalMs = 5000;
constexpr unsigned long kHeartbeatIntervalMs = 30000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.print("\nWi-Fi connected: ");
  Serial.println(WiFi.localIP());
  Serial.println("WIFI OK");
}

bool connectMqtt() {
  String clientId = "hottubHVAC-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  Serial.print("Connecting to MQTT");

  if (!mqttClient.connect(clientId.c_str(), mqttUsername, mqttPassword,
                          kStatusTopic, 1, true, "offline")) {
    Serial.print(" failed, state=");
    Serial.println(mqttClient.state());
    return false;
  }

  mqttClient.publish(kStatusTopic, "online", true);
  Serial.println(" connected");
  Serial.println("MQTT OK");
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BOOT OK");

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(mqttServer, mqttPort);
  connectWiFi();
}

void loop() {
  connectWiFi();

  if (!mqttClient.connected() && millis() - lastReconnectAttempt >= kReconnectIntervalMs) {
    lastReconnectAttempt = millis();
    connectMqtt();
  }

  mqttClient.loop();

  if (mqttClient.connected() && millis() - lastHeartbeat >= kHeartbeatIntervalMs) {
    lastHeartbeat = millis();
    mqttClient.publish(kStatusTopic, "hello from esp32");
    Serial.println("hello from esp32");
  }
}
