#include "mqtt_header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MQTTClient mqttClient;

// Function to publish a message to the MQTT broker
void publishToMQTT(const char *payload, const char *topic)
{
  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  pubmsg.payload = (void *)payload;
  pubmsg.payloadlen = strlen(payload);
  pubmsg.qos = 0;
  pubmsg.retained = 0;

  MQTTClient_deliveryToken token;
  MQTTClient_publishMessage(mqttClient, topic, &pubmsg, &token);
  MQTTClient_waitForCompletion(mqttClient, token, 1000L); // Wait for message delivery
}

// Function to initialize the MQTT connection
void initMQTT()
{

  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

  MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

  MQTTClient_create(&mqttClient, MQTT_ADDRESS, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;

  conn_opts.username = MQTT_USERNAME;
  conn_opts.password = MQTT_PASSWORD;

  ssl_opts.enableServerCertAuth = 1;

  conn_opts.ssl = &ssl_opts;

  if (MQTTClient_connect(mqttClient, &conn_opts) != MQTTCLIENT_SUCCESS)
  {
    fprintf(stderr, "Failed to connect to MQTT broker.\n");
    exit(EXIT_FAILURE);
  }
  printf("Connected to MQTT broker at %s\n", MQTT_ADDRESS);
}