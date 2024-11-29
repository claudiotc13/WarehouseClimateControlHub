#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <NTPClient.h> // NTPClient By Fabrice Weinberg 

#define DHTTYPE DHT11

DHT dht(15, DHTTYPE);

//---- WiFi settings
//const char* ssid = "IoTDEI2";
//const char* password = "#8tud3nt2024";
// const char* ssid = "Vodafone-1F01D8";
// const char* password = "7DYYUGDUDMTDYNU9";
// const char* ssid = "MEO-CF7AD9";
// const char* password = "398DD18B34";
const char* ssid = "NOS-45C4";
const char* password = "6YM3RY6T";

// NTP Sever Sync ESP32 Times
const char* ntpServer = "time.google.com";   // time.google.com   // pool.ntp.org  // time.nist.gov
const long utcOffsetInSeconds = 0; // Set to your timezone offset (e.g., 3600 for GMT+1)

//---- MQTT Broker settings
const char* mqtt_server = "370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud"; // replace with your broker url
const char* mqtt_username = "comscs2324jpt45"; // replace with your broker id
const char* mqtt_password = "josepedro2"; // replace with your broker password
const int mqtt_port = 8883;

// here is broadcast UDP address
// const char * udpAddress = "172.18.154.4"; //the address of the SRV01
// const int udpPort = 8080;
// const char * udpAddress = "192.168.1.111";  //Server Zé
// const char * udpAddress = "192.168.8.186";  //Server Zé VM Alpine
const char * udpAddress = "192.168.1.107";  //Server Lau VM Alpine
// const char * udpAddress = "172.24.177.109";  //Server Zé2
// const char * udpAddress = "192.168.1.227"; //Server Lau
const int udpPort = 8888;        //the address of the SRV02
//create UDP instance
WiFiUDP udp;

//create NTPclient
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
    delay(1000);
    randomSeed(micros());
    Serial.println("\nWiFi connected\nIP address: ");
    Serial.println(WiFi.localIP());
    while (!Serial) delay(1);
    espClient.setInsecure(); // !! espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    dht.begin();
    Serial.println("\nAQUIIIIII\n");
    delay(2000);

    timeClient.begin();
    delay(1000);
    while (!timeClient.update()) {
     Serial.println("Waiting for NTP synchronization...");
     delay(1000);
    } 

    // Print the time
    Serial.println("Time synchronized:");
    Serial.println(timeClient.getFormattedTime()); // Display time in HH:MM:SS format
 

    strcpy(comando, "start");
}


void loop() {
    if (!client.connected())
        reconnect();
    client.loop();

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    char tempo_bom [15];



    // Buffer to test UDP
    uint8_t buffer[50];
    timeClient.begin();
    timeClient.update();

   // delay(1000);
    unsigned long now = timeClient.getEpochTime();
if (strcmp(comando, "start") == 0) {
  if (now - lastSync >= 1) { // Send every second
    lastSync = now;
    Serial.println(timeClient.getFormattedTime()); // Display updated time every second

    sprintf(tempo_bom, "%s",timeClient.getFormattedTime() );
    sprintf(humidade, "%f", h);
    sprintf(temp, "%f", t);
    

   
    Serial.println(humidade);
    Serial.println(temp);

    // Create JSON object for WeatherObserved data
    StaticJsonDocument<200> doc;
    doc["id"] = "ZE";
    doc["type"] = "WeatherObserved";
    doc["temperature"] = temp;  // Example temperature
    doc["humidity"] = humidade;     // Example humidity
    doc["currenttime"]=tempo_bom;

    char jsonBuffer[256];

    //StaticJsonDocument<200> doc2;
    //doc2["id"] = "LAU";
    //doc2["type"] = "WeatherObserved";
    //doc2["temperature"] = temp;  // Example temperature
    //doc2["humidity"] = humidade;     // Example humidity
    //doc2["currenttime"]=tempo_bom;

    //char jsonBuffer2[256];

    serializeJson(doc, jsonBuffer);
    //serializeJson(doc2, jsonBuffer2);

    Serial.println("mensagem enviada para o broker");

    publishMessage("/comcs/g45/humidade", String(jsonBuffer), true);
     //publishMessage("/comcs/g45/humidade", String(jsonBuffer2), true);
 
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
} else if (strcmp(comando, "stop") == 0) {
    Serial.println("Envio de mensagens parado até novo comando");
    delay(5000);
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