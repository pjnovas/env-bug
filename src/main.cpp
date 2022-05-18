#include "config.h"

#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Settings
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *clientId = WIFI_HOSTNAME;

const char *mqtt_server = MQTT_HOST;
const unsigned int mqtt_port = MQTT_PORT;

const char *mqtt_user = MQTT_USER;
const char *mqtt_pass = MQTT_PASS;
const char *mqtt_topic = MQTT_TOPIC;

const unsigned int publishInterval = PUBLISH_INTERVAL;
const unsigned int publishInterval_ms = publishInterval * 1000;

// Instances
OneWire ds(SENSOR_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

// State
byte sensorAddr[8];
unsigned long lastUpdate = 0;

void Log(String message)
{
#ifdef DEBUG
  Serial.println(message);
#endif
}

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  delay(10);
#endif
  Log("STARTED");

  ds.search(sensorAddr);
  OneWire::crc8(sensorAddr, 7);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Log("WIFI;TRY");

    if (WiFi.status() == WL_CONNECT_FAILED)
    {
      Log("WIFI;OFF");
      Log(String(WiFi.status()));
      return;
    }

    delay(1000);
  }

  Log("WIFI;ON");

  client.setServer(mqtt_server, mqtt_port);
  client.setKeepAlive(publishInterval * 1.5);
}

float getTemperature()
{
  byte data[12];

  ds.reset();
  ds.select(sensorAddr);
  ds.write(0x44); // start conversion

  delay(1000);

  ds.reset();
  ds.select(sensorAddr);
  ds.write(0xBE); // Read Scratchpad

  for (byte i = 0; i < 9; i++)
  {
    data[i] = ds.read();
  }

  OneWire::crc8(data, 8);

  int16_t raw = (data[1] << 8) | data[0];
  return (float)raw / 16.0;
}

bool mqttReady()
{
  if (client.connected())
    return true;

  Log("MQTT;TRY");

  String willTopic = String(clientId) + "/status";
  const char *willTopic_ = willTopic.c_str();

  if (client.connect(clientId, mqtt_user, mqtt_pass, willTopic_, 0, true, "offline", true))
  {
    Log("MQTT;ON");
    return client.publish(willTopic_, "online", true);
  }

  Log("MQTT;OFF");
  delay(1000); // wait for reconnect
  return false;
}

bool isMeasureTime()
{
  return lastUpdate == 0 || (millis() - lastUpdate) > publishInterval_ms;
}

bool publishTemp()
{
  float celsius = getTemperature();

  char tempStr[8];
  dtostrf(celsius, 6, 2, tempStr);

  Log("MQTT;PUB");
  return client.publish(mqtt_topic, tempStr);
}

void loop()
{
  if (isMeasureTime())
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Log("WIFI;OFF");
      return;
    }

    if (!mqttReady())
      return;

    if (publishTemp())
    {
      lastUpdate = millis();
    }

    client.loop();
  }
}
