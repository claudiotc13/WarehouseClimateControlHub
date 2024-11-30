#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "MQTTClient.h"

// MQTT Broker configuration
#define MQTT_ADDRESS "ssl://370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud:8883"
#define MQTT_CLIENTID "UDPtoMQTTClient"
#define MQTT_USERNAME "comscs2324jpt45"
#define MQTT_PASSWORD "josepedro2"

// Function declarations
void initMQTT();
void publishToMQTT(const char *payload, const char *topic);

#endif // MQTT_HANDLER_H