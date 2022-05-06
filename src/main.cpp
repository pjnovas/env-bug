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

const unsigned int publishInterval = PUBLISH_INTERVAL_MS;

// Instances
OneWire ds(SENSOR_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

// State
byte sensorAddr[8];
unsigned long lastUpdate;

void setup()
{
  Serial.begin(115200);
  delay(10);

  ds.search(sensorAddr);
  OneWire::crc8(sensorAddr, 7);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(ssid, password);

  client.setServer(mqtt_server, mqtt_port);
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

void loop(void)
{
  if ((millis() - lastUpdate) > publishInterval)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WIFI OFF");
      return;
    }

    if (!client.connected() && !client.connect(clientId, mqtt_user, mqtt_pass))
    {
      Serial.println("FAILED CONN BROKER");
      delay(1000); // wait for reconnect
      return;
    }

    float celsius = getTemperature();

    char tempStr[8];
    dtostrf(celsius, 6, 2, tempStr);
    client.publish(mqtt_topic, tempStr);
    client.loop();

    lastUpdate = millis();
  }
}
