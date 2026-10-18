#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "credentials.h"

namespace {
constexpr char kStatusTopic[] = "hottubHVAC/status";
constexpr char kTemperatureTopicPrefix[] = "home/HVAC/temp";
constexpr uint8_t kDallasPin = D3;
constexpr uint8_t kMaxTemperatureSensors = 10;
constexpr unsigned long kReconnectIntervalMs = 5000;
constexpr unsigned long kHeartbeatIntervalMs = 30000;
constexpr unsigned long kTemperatureIntervalMs = 30000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
OneWire oneWire(kDallasPin);
DallasTemperature dallasSensors(&oneWire);
uint8_t temperatureSensorCount = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastTemperatureReport = 0;

void publishTemperatures();

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
  lastTemperatureReport = millis() - kTemperatureIntervalMs;
  Serial.println(" connected");
  Serial.println("MQTT OK");
  return true;
}

void printSensorAddress(uint8_t index) {
  DeviceAddress address;
  if (!dallasSensors.getAddress(address, index)) {
    return;
  }

  Serial.print("Dallas sensor ");
  Serial.print(index + 1);
  Serial.print(": ");
  for (uint8_t byteIndex = 0; byteIndex < sizeof(DeviceAddress); ++byteIndex) {
    if (address[byteIndex] < 16) {
      Serial.print('0');
    }
    Serial.print(address[byteIndex], HEX);
  }
  Serial.println();
}

void publishTemperatures() {
  dallasSensors.requestTemperatures();

  for (uint8_t index = 0; index < temperatureSensorCount; ++index) {
    const float temperatureC = dallasSensors.getTempCByIndex(index);
    if (temperatureC == DEVICE_DISCONNECTED_C) {
      Serial.print("Dallas sensor ");
      Serial.print(index + 1);
      Serial.println(" disconnected");
      continue;
    }

    const float temperatureF = DallasTemperature::toFahrenheit(temperatureC);
    char topic[32];
    char payload[12];
    snprintf(topic, sizeof(topic), "%s%u", kTemperatureTopicPrefix, index + 1);
    snprintf(payload, sizeof(payload), "%.2f", temperatureF);

    const bool published = mqttClient.publish(topic, payload, true);
    Serial.print(topic);
    Serial.print(" = ");
    Serial.print(payload);
    Serial.println(published ? " F" : " F (publish failed)");
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BOOT OK");

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(mqttServer, mqttPort);

  dallasSensors.begin();
  temperatureSensorCount = min(static_cast<uint8_t>(dallasSensors.getDeviceCount()),
                               kMaxTemperatureSensors);
  Serial.print("Dallas sensors found: ");
  Serial.println(temperatureSensorCount);
  for (uint8_t index = 0; index < temperatureSensorCount; ++index) {
    printSensorAddress(index);
  }

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

  if (mqttClient.connected() &&
      millis() - lastTemperatureReport >= kTemperatureIntervalMs) {
    lastTemperatureReport = millis();
    publishTemperatures();
  }
}
