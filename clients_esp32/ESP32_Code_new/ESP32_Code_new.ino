#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

#define DHTTYPE DHT11

DHT dht(15, DHTTYPE);

//---- WiFi settings
// const char* ssid = "IoTDEI2";
// const char* password = "#8tud3nt2024";
// const char* ssid = "Vodafone-1F01D8";
// const char* password = "7DYYUGDUDMTDYNU9";
// const char* ssid = "MEO-CF7AD9";
// const char* password = "398DD18B34";
const char *ssid = "NOS-45C4";
const char *password = "6YM3RY6T";

// Server to get the current time
const char *ntpServer = "time.google.com";
const long utcOffsetInSeconds = 0; // Set to your timezone offset (e.g., 3600 for GMT+1)

// MQTT credentials
const char *mqtt_server = "370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud";
const char *mqtt_username = "comscs2324jpt45";
const char *mqtt_password = "josepedro2";
const int mqtt_port = 8883;

// UDP server
const char *udpAddress = "192.168.1.107"; // check the IP address of the server and change it here
const int udpPort = 8888;                 // check the port of the server and change it here

WiFiUDP udp;

// create NTPclient
NTPClient timeClient(udp, ntpServer, utcOffsetInSeconds, 30000);
static unsigned long lastSync = 0;

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
char humidade[100];
char temp[100];
char comando[100];
int value = 0;

void setup()
{
    delay(5000);
    Serial.begin(9600);

    Serial.print("\nConnecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    delay(1000);
    randomSeed(micros());

    Serial.println("\nWiFi connected\nIP address: ");
    Serial.println(WiFi.localIP());

    while (!Serial)
        delay(1);

    espClient.setInsecure();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    dht.begin();
    delay(2000);

    timeClient.begin();
    delay(1000);

    while (!timeClient.update())
    {
        Serial.println("Waiting for NTP synchronization...");
        delay(1000);
    }

    Serial.println("Time synchronized:");
    Serial.println(timeClient.getFormattedTime()); // Display time in HH:MM:SS format

    strcpy(comando, "start");
}

void loop()
{
    if (!client.connected())
        reconnect();
    client.loop();

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    char tempo_bom[15];

    // Buffer to test UDP
    uint8_t buffer[50];
    timeClient.begin();
    timeClient.update();
    unsigned long now = timeClient.getEpochTime();

    if (strcmp(comando, "start") == 0)
    {
        if (now - lastSync >= 1)
        { // Send every second
            lastSync = now;
            Serial.println(timeClient.getFormattedTime()); // Display updated time every second

            sprintf(tempo_bom, "%s", timeClient.getFormattedTime());
            sprintf(humidade, "%f", h);
            sprintf(temp, "%f", t);

            Serial.println(humidade);
            Serial.println(temp);

            // Create JSON object for WeatherObserved data
            StaticJsonDocument<200> doc;
            doc["id"] = "ZE";
            doc["type"] = "WeatherObserved";
            doc["temperature"] = temp;
            doc["humidity"] = humidade;
            doc["currenttime"] = tempo_bom;

            char jsonBuffer[256];

            serializeJson(doc, jsonBuffer);

            Serial.println("mensagem enviada para o broker");

            publishMessage("/comcs/g45/humidade", String(jsonBuffer), true);

            // This initializes udp and transfer buffer
            udp.beginPacket(udpAddress, udpPort);
            udp.write((const uint8_t *)jsonBuffer, strlen(jsonBuffer));
            udp.endPacket();

            memset(jsonBuffer, 0, sizeof(jsonBuffer));
            memset(buffer, 0, 50);

            Serial.print("Waiting for Server ACK \n");
            udp.parsePacket();

            if (udp.read(buffer, 50) > 0)
            {
                Serial.print("Server to client: ");
                Serial.println((char *)buffer);
                memset(buffer, 0, 50);
            }
        }
    }
    else if (strcmp(comando, "stop") == 0)
    {
        Serial.println("Envio de mensagens parado até novo comando");
        delay(1000);
    }
}

void reconnect()
{
    // Loop until we’re reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection…");
        String clientId = "ESP32-G45-";
        clientId += String(random(0xffff), HEX);

        // Attempt to connect
        if (client.connect(clientId.c_str(), mqtt_username, mqtt_password))
        {
            Serial.println("connected");
            client.subscribe("/comcs/g45/commands");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// This function is called every time we have a message from the broker
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Callback - ");
    Serial.print("Message:");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println("");

    strncpy(comando, (char *)payload, length);
    comando[length] = '\0';
}

// Publishing as string
void publishMessage(const char *topic, String payload, boolean retained)
{
    if (client.publish(topic, payload.c_str(), retained))
        Serial.println("Message published [" + String(topic) + "]: " + payload);
}