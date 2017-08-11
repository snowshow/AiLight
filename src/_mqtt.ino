/**
 * Ai-Thinker RGBW Light Firmware - MQTT Module
 *
 * The MQTT module holds all the code to manage all functions for communicating
 * with the MQTT broker.
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sacha Telgenhof <stelgenhof at gmail dot com>
 * (https://www.sachatelgenhof.nl)
 * Copyright (c) 2016 - 2017 Sacha Telgenhof
 */

/**
 * @brief Publish a message to an MQTT topic
 *
 * @param topic the MQTT topic to publish the message to
 * @param message the message to be published
 */
void mqttPublish(const char *topic, const char *message) {
  // Don't do anything if we are not connected to the MQTT broker
  if (!mqtt.connected()) {
    return;
  }

  if ((os_strlen(topic) > 0) && (os_strlen(message) > 0)) {
    mqtt.publish(topic, MQTT_QOS_LEVEL, MQTT_RETAIN, message);

    DEBUGLOG("[MQTT] Published message to '%s'\n", topic);
  }
}

/**
 * @brief Subscribe to an MQTT topic
 *
 * @param topic the MQTT topic to subscribe to
 * @param qos the desired QoS level (defaults to MQTT_QOS_LEVEL)
 */
void mqttSubscribe(const char *topic, uint8_t qos = MQTT_QOS_LEVEL) {
  // Don't do anything if we are not connected to the MQTT broker
  if (!mqtt.connected()) {
    return;
  }

  if (os_strlen(topic) > 0) {
    mqtt.subscribe(topic, qos);

    DEBUGLOG("[MQTT] Subscribed to topic '%s'\n", topic);
  }
}

/**
 * @brief Unsubscribe from an MQTT topic
 *
 * @param topic the MQTT topic to unsubscribe from
 */
void mqttUnsubscribe(const char *topic) {
  // Don't do anything if we are not connected to the MQTT broker
  if (!mqtt.connected()) {
    return;
  }

  if (os_strlen(topic) > 0) {
    mqtt.unsubscribe(topic);

    DEBUGLOG("[MQTT] Unsubscribed from topic '%s'\n", topic);
  }
}

/**
 * @brief Register MQTT callback functions
 *
 * @param the callback function to register
 */
void mqttRegister(void (*callback)(uint8_t, const char *, const char *)) {
  _mqtt_callbacks.push_back(callback);
}

/**
 * @brief Event handler for when a connection to the MQTT has been established.
 *
 * @param bool sessionPresent
 */
void onMQTTConnect(bool sessionPresent) {
  DEBUGLOG("[MQTT] Connected\n");

  // Notify subscribers (connected)
  for (uint8_t i = 0; i < _mqtt_callbacks.size(); i++) {
    (*_mqtt_callbacks[i])(MQTT_EVENT_CONNECT, NULL, NULL);
  }

  // MQTT discovery for Home Assistant
  if (cfg.mqtt_ha_use_discovery && !cfg.mqtt_ha_is_discovered) {
    static const int BUFFER_SIZE = JSON_OBJECT_SIZE(8);
    StaticJsonBuffer<BUFFER_SIZE> mqttJsonBuffer;
    JsonObject &md_root = mqttJsonBuffer.createObject();

    md_root["name"] = cfg.hostname;
    md_root["platform"] = "mqtt_json";
    md_root["state_topic"] = cfg.mqtt_state_topic;
    md_root["command_topic"] = cfg.mqtt_command_topic;
    md_root["brightness"] = true;
    md_root["rgb"] = true;
    md_root["white_value"] = true;
    md_root["color_temp"] = true;

    // Build the payload
    char md_buffer[md_root.measureLength() + 1];
    md_root.printTo(md_buffer, sizeof(md_buffer));

    // Construct the topic name for HA MQTT discovery
    char *dc_topic = new char[128];
    sprintf_P(dc_topic, PSTR("%s/light/%s/config"), cfg.mqtt_ha_disc_prefix,
              cfg.hostname);

    mqttPublish(dc_topic, md_buffer);

    cfg.mqtt_ha_is_discovered = true;
    EEPROM_write(cfg);
  }
}

/**
 * @brief Event handler for when the connection to the MQTT broker has been
 * disconnected.
 */
void onMQTTDisconnect(AsyncMqttClientDisconnectReason reason) {
  DEBUGLOG("[MQTT] Disconnected. Reason: %d\n", reason);

  // Notify subscribers (disconnected)
  for (uint8_t i = 0; i < _mqtt_callbacks.size(); i++) {
    (*_mqtt_callbacks[i])(MQTT_EVENT_DISCONNECT, NULL, NULL);
  }

  mqttReconnectTimer.once(RECONNECT_TIME, mqttConnect);
}

/**
 * @brief Event handler for when a message is received from the MTT broker
 *
 * @param topic the MQTT topic to which the message has been published
 * @param payload the contents/message that has been published
 * @param properties additional properties of the published message
 * @param length size of the published message
 * @param index ?
 * @param total ?
 */
void onMQTTMessage(char *topic, char *payload,
                   AsyncMqttClientMessageProperties properties, size_t length,
                   size_t index, size_t total) {

  // Convert payload into char variable
  char message[length + 1];
  os_memcpy(message, payload, length);
  message[length] = 0;

  DEBUGLOG("[MQTT] Received message on '%s'\n", topic, message);

  // Notify subscribers (message received)
  for (uint8_t i = 0; i < _mqtt_callbacks.size(); i++) {
    (*_mqtt_callbacks[i])(MQTT_EVENT_MESSAGE, topic, message);
  }
}

/**
 * @brief Handles the connection to the MQTT broker.
 */
void mqttConnect() {

  DEBUGLOG("[MQTT] Connecting to broker '%s:%i'", cfg.mqtt_server,
           cfg.mqtt_port);

  if ((os_strlen(cfg.mqtt_user) > 0) && (os_strlen(cfg.mqtt_password) > 0)) {
    DEBUGLOG(" as user '%s'\n", cfg.mqtt_user);
  }

  mqtt.connect();
}

/**
 * @brief Bootstrap function for the MQTT connection
 */
void setupMQTT() {
  mqtt.onConnect(onMQTTConnect);
  mqtt.onDisconnect(onMQTTDisconnect);
  mqtt.onMessage(onMQTTMessage);

  mqtt.setServer(cfg.mqtt_server, cfg.mqtt_port);
  mqtt.setKeepAlive(MQTT_KEEPALIVE);
  mqtt.setCleanSession(false);
  mqtt.setClientId(cfg.hostname);
  mqtt.setWill(cfg.mqtt_lwt_topic, 2, true, MQTT_STATUS_OFFLINE);
  mqtt.setCredentials(cfg.mqtt_user, cfg.mqtt_password);
}
