// Author ugo (Ugo Di Girolamo)

#ifndef _MQTTCONTROLLER__H_
#define _MQTTCONTROLLER__H_

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ESP8266WiFi.h>

#include "Action.h"

/**
 * A class for interacting with the Mqtt broker.
 *
 * Publishes status updates, and subscribes to a feed requesting changes.
 *
 * Also reports a pulse and subscribes to a feed for requesting changes to the
 * value of the pulseStatus, so that Mqtt connectivity can be tested.
 */
class MqttController {
 public:
  MqttController();

  void setup(WiFiClient *client);

  void ensureConnected();
  Action* processMqttInput();

  void publishToZoneFeed(String json);
  void publishToStatusFeed(String json);

  void publishPulse();

 private:
  // The MQTT client
  Adafruit_MQTT_Client* mqtt;
  // czii feeds for publishing.
  Adafruit_MQTT_Publish* zone_mqtt_feed;
  Adafruit_MQTT_Publish* status_mqtt_feed;
  // czii/zonetemp feed for subscribing to zone info changes.
  Adafruit_MQTT_Subscribe* mqtt_sub_feed;

  // The "pulse status" of this controller.
  // It has no function except testing connectivity.
  // It is periodically reported using the pulsePublishFeed, and
  // can be modified using the pulseUpdateSubFeed.
  int pulseStatus;
  unsigned long lastPulseSendTimeMillis;
  Adafruit_MQTT_Publish* pulsePublishFeed;
  Adafruit_MQTT_Subscribe* pulseUpdateSubFeed;
};

#endif  // _MQTTCONTROLLER__H_
