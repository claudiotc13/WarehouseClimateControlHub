#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#define DHTTYPE DHT11

DHT dht(15, DHTTYPE);

//---- WiFi settings
// const char* ssid = "IoTDEI2";
// const char* password = "#8tud3nt2024";
// const char* ssid = "Vodafone-1F01D8";
// const char* password = "7DYYUGDUDMTDYNU9";
const char* ssid = "MEO-CF7AD9";
const char* password = "398DD18B34";

//---- MQTT Broker settings
const char* mqtt_server = "370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud"; // replace with your broker url
const char* mqtt_username = "comscs2324jpt45"; // replace with your broker id
const char* mqtt_password = "josepedro2"; // replace with your broker password
const int mqtt_port = 8883;

// here is broadcast UDP address
// const char * udpAddress = "172.18.154.4"; //the address of the SRV01
// const int udpPort = 8080;
const char * udpAddress = "192.168.1.111";  //Server Zé
// const char * udpAddress = "172.24.177.109";  //Server Zé2
// const char * udpAddress = "192.168.1.227"; //Server Lau
const int udpPort = 8888;        //the address of the SRV02
//create UDP instance
WiFiUDP udp;


WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
char humidade[100];
char temp[100];
char comando[100];
int value = 0;

void setup() {
    delay(5000);
    Serial.begin(9600);
    Serial.print("\nConnecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("\nWiFi connected\nIP address: ");
    Serial.println(WiFi.localIP());
    while (!Serial) delay(1);
    espClient.setInsecure(); // !! espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    dht.begin();
}

void loop() {
    if (!client.connected())
        reconnect();
    client.loop();

    float h = dht.readHumidity();
    float t = dht.readTemperature();


// este buffer e para testar o UDP
    uint8_t buffer[50];


    delay(5000);

    sprintf(humidade, "%f", h);
    sprintf(temp, "%f", t);
    

   
    Serial.println(humidade);
    Serial.println(temp);

    // Create JSON object for WeatherObserved data
    StaticJsonDocument<200> doc;
    doc["id"] = "WeatherObserved:Warehouse";
    doc["type"] = "WeatherObserved";
    doc["temperature"] = temp;  // Example temperature
    doc["humidity"] = humidade;     // Example humidity

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    Serial.println("mensagem enviada para o broker");

    publishMessage("/comcs/g45/humidade", String(jsonBuffer), true);
 
    //This initializes udp and transfer buffer
    udp.beginPacket(udpAddress, udpPort);
    udp.write((const uint8_t*)jsonBuffer, strlen(jsonBuffer));
    udp.endPacket();
    memset(jsonBuffer, 0, sizeof(jsonBuffer));
    memset(buffer, 0, 50);

    Serial.print("Waiting for Server ACK \n");
    //processing incoming packet, must be called before reading the buffer
    udp.parsePacket();
    if(udp.read(buffer, 50) > 0){
        Serial.print("Server to client: ");
        Serial.println((char *)buffer);
        memset(buffer, 0, 50);
    }


   
}

void reconnect() {
    // Loop until we’re reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection…");
        String clientId = "ESP32-G45-"; // change the groupID
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("connected");
            client.subscribe("/comcs/g45/commands"); // change topic for your group
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

//=======================================
// This void is called every time we have a message from the broker
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Callback - ");
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println("");

    // Copy payload to comando and ensure null-termination
    strncpy(comando, (char*)payload, length);
    comando[length] = '\0'; // Null-terminate the string
}

//=======================================
// Publishing as string
void publishMessage(const char* topic, String payload, boolean retained) {
    if (client.publish(topic, payload.c_str(), retained))
        Serial.println("Message published [" + String(topic) + "]: " + payload);
}