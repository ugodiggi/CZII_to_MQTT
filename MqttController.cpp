// Copyright 2018 Urban Compass Inc. All rights reserved
// Author ugo (Ugo Di Girolamo)

#include "MqttController.h"

// MqttConf.h is not in git (see also .gitignore).
// It must contain your MQTT credential/configuration, i.e.
//   #define MQTT_SERVER       "MQTT_SERVER_URL"     // Local OpenHab and MQ server
//   #define MQTT_SERVERPORT   1883                  // use 8883 for SSL
//   #define MQTT_USERNAME     ""                    // MQTT server username
//   #define MQTT_PASSWORD     ""                    // MQTT server password
#include "MqttConf.h"

#include "Util.h"

MqttController::MqttController() {
  mqtt = NULL;
  zone_mqtt_feed = NULL;
  status_mqtt_feed = NULL;
  mqtt_sub_feed = NULL;

  pulseStatus = 100;
  lastPulseSendTimeMillis = 0;
  pulsePublishFeed = NULL;
  pulseUpdateSubFeed = NULL;
}

void MqttController::setup(WiFiClient *client) {
  // Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
  mqtt = new Adafruit_MQTT_Client(
      client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);

  // Setup 'czii' (Comfort Zone II) feeds for publishing.
  zone_mqtt_feed = new Adafruit_MQTT_Publish(mqtt, "czii/zone");
  status_mqtt_feed = new Adafruit_MQTT_Publish(mqtt, "czii/status");

  // Setup 'czii/zonetemp' feed for subscribing to zone info changes.
  mqtt_sub_feed = new Adafruit_MQTT_Subscribe(mqtt, "czii/zonetemp");
  mqtt->subscribe(mqtt_sub_feed);

  // Setup pulse publish/update subscriber feeds.
  pulsePublishFeed = new Adafruit_MQTT_Publish(mqtt, "czii/pulse");
  pulseUpdateSubFeed = new Adafruit_MQTT_Subscribe(mqtt, "czii/updatePulse");
  mqtt->subscribe(pulseUpdateSubFeed);
}

//  connect and reconnect as necessary to the MQTT server.
//  Should be called in the loop function and it will take care if connecting.
//
void MqttController::ensureConnected() {
  // Stop if already connected.
  if (mqtt->connected()) {
    return;
  }

  Serial.println();
  Serial.println(F("Connecting to MQTT..."));

  uint8_t retries = 3;
  int8_t ret;
  while ((ret = mqtt->connect()) != 0) { // connect will return 0 for connected
    digitalWrite(BUILTIN_LED, LOW);  // Flash LED while connecting to WiFi

    Serial.println(mqtt->connectErrorString(ret));
    Serial.println(F("Retrying MQTT connection in 5 seconds..."));

    mqtt->disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1) {
        delay(100);
      }
    }

    digitalWrite(BUILTIN_LED, HIGH);
  }

  if (mqtt->connect() == 0) {
    Serial.println(F("MQTT Connected."));
  }
  else {
    Serial.println(F("MQTT Connection Failed!!!!"));
  }

  Serial.println();
}

//
//  Process input from the MQTT subscription feeds
//
Action* MqttController::processMqttInput() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  this->ensureConnected();

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt->readSubscription(10))) {
    if (subscription == mqtt_sub_feed) {
      Serial.print(F("mqtt_sub_feed received : "));
      String value = (char *)mqtt_sub_feed->lastread;
      Serial.println(value);
      return actionFromJson(value);
    } else if (subscription == pulseUpdateSubFeed) {
      Serial.print(F("pulseUpdateSubFeed received : "));
      String value = (char *)pulseUpdateSubFeed->lastread;
      Serial.println(value);
      long val = value.toInt();
      if (val >= 0 && val < 1000) {
        pulseStatus = val;
        lastPulseSendTimeMillis = 0;
      } else {
        Serial.println(F("pulseUpdateSubFeed - unrecognized value."));
      }
    }
  }
  return NULL;
}

void MqttController::publishToZoneFeed(String json) {
  info_print("MQTT Zone: length=" + String(json.length()) + ", JSON=");
  info_println(json);
  if (zone_mqtt_feed == NULL) {
    info_println(F("publishing to zone_mqtt_feed is disabled."));
  } else if (!zone_mqtt_feed->publish(json.c_str())) {
    info_println(F("zone_mqtt_feed.publish Failed"));
  }
}

void MqttController::publishToStatusFeed(String json) {
  info_print("MQTT Status: length=" + String(json.length()) + ", JSON=");
  info_println(json);
  if (status_mqtt_feed == NULL) {
    info_println(F("publishing to status_mqtt_feed is disabled."));
  } else if (!status_mqtt_feed->publish(json.c_str())) {
    info_println(F("status_mqtt_feed.publish Failed"));
  }
}

void MqttController::publishPulse() {
  unsigned long now = millis();
  if (now - lastPulseSendTimeMillis > 5000) {
    String msg = String(pulseStatus);
    info_println("Sending pulseStatus: " + msg);
    pulsePublishFeed->publish(msg.c_str());
    lastPulseSendTimeMillis = now;
  }
}
