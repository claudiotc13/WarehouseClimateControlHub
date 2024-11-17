#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "MQTTClient.h"

#define PORT 8888
#define MAX_MESSAGE_SIZE 1024
#define MAX_CLIENTS 10

//  MQTT Broker configuration
#define MQTT_ADDRESS   "ssl://370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud:8883"
#define MQTT_CLIENTID  "UDPtoMQTTClient"
#define MQTT_TOPIC     "/comcs/g45/UDPTESTE"
#define MQTT_USERNAME  "comscs2324jpt45"
#define MQTT_PASSWORD  "josepedro2"

MQTTClient mqttClient;

typedef struct
{
  struct sockaddr_in address;
  int socket;
  int id;
} Client;

Client clients[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS];
int clientCount = 0;
// pthread_mutex_t mutex;

void publishToMQTT(const char *payload) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(mqttClient, MQTT_TOPIC, &pubmsg, &token);
    MQTTClient_waitForCompletion(mqttClient, token, 1000L);  // Wait for message delivery
    printf("Published message: %s\n", payload);
}




void initMQTT() {

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

    // Specify the paths to the necessary certificates and keys
   // ssl_opts.trustStore = "/home/root/myCA/myCA.crt";  // CA certificate (to verify the broker's certificate)
   // ssl_opts.keyStore = "/home/root/myCA/certs/server.crt";  // Client certificate (if client authentication is required)
   // ssl_opts.privateKey = "/home/root/myCA/private/server.key";  // Client private key (if client authentication is required)
   // ssl_opts.verify = 0;  // Enable server certificate verification

    MQTTClient_create(&mqttClient, MQTT_ADDRESS, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    ssl_opts.enableServerCertAuth = 1;

    conn_opts.ssl = &ssl_opts;

    // MQTTClient_create(&mqttClient, MQTT_ADDRESS, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);



    if (MQTTClient_connect(mqttClient, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker.\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to MQTT broker at %s\n", MQTT_ADDRESS);
}



void printClientInfo(int clientIndex) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clients[clientIndex].address.sin_addr), ip, INET_ADDRSTRLEN);
    printf("Client %d:\n", clientIndex);
    printf("  ID: %d\n", clients[clientIndex].id);
    printf("  Socket: %d\n", clients[clientIndex].socket);
    printf("  Address: %s\n", ip);
    printf("  Port: %d\n", ntohs(clients[clientIndex].address.sin_port));
}


void *handleClient(void *arg)
{
  int id = *((int *)arg);
  char buffer[MAX_MESSAGE_SIZE];
  ssize_t bytesRead;

  printf("Thread do Client %d começou a executar\n", id + 1);

  while (1)
  {
    // Receive message from client
    socklen_t adrlen = sizeof(clients[id].address);
    bytesRead = recvfrom(clients[id].socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clients[id].address, &adrlen);
    if (bytesRead == -1)
    {
      perror("Receive failed");
      exit(EXIT_FAILURE);
    }

    buffer[bytesRead] = '\0';
    printf("Msg recebida do Client %d: %s\n", id + 1, buffer);

    publishToMQTT(buffer);

    // Broadcast the received message to all clients
    /*
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < clientCount; ++i) {
      if (i != id) {
        sendto(clients[i].socket, buffer, bytesRead, 0, (struct sockaddr*)&clients[i].address, sizeof(clients[i].address));
      }
    }
    pthread_mutex_unlock(&mutex);
    */

    // Send a message to the client (for demonstration purposes)

    sprintf(buffer, "Server: Message from server to Client %d\n", id + 1);
    sendto(clients[id].socket, buffer, strlen(buffer), 0, (struct sockaddr *)&clients[id].address, sizeof(clients[id].address));
    printf("Msg enviada para o Client %d: %s\n", id + 1, buffer);
    memset(buffer, 0, strlen(buffer));
  }
}

int main()
{
  int serverSocket;
  struct sockaddr_in serverAddr, currentAddr;
  char buffer[MAX_MESSAGE_SIZE];
  socklen_t clientLen = sizeof(struct sockaddr_in);
  ssize_t bytesRead;


  // Initialize MQTT connection
  initMQTT();

  // Create socket
  if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }
  if (serverSocket == 0)
  {
    perror("Socket creation suscessfull");
  }

  // Configure server address
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);

  // Bind socket
  if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
  {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Initialize mutex
  // pthread_mutex_init(&mutex, NULL);

  printf("Server listening on port %d...\n", PORT);

  // Create a thread for each client
  while (1)
  {
    // Receive message from client
    
    bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&currentAddr, &clientLen);
    if (bytesRead == -1)
    {
      perror("Receive failed");
      exit(EXIT_FAILURE);
    }

    printf("AQUIIII: \n");
    // buffer[bytesRead] = '\0';
    // printf("Client %d connected: %s", clientCount + 1, buffer);
    int isNewClient = 1;
        for (int i = 0; i < clientCount; i++) {
            if (memcmp(&(clients[i].address.sin_addr), &(currentAddr.sin_addr), sizeof(struct in_addr)) == 0) {
                isNewClient = 0;
                break;
            }
        }


  if (isNewClient) {

    // Add client to the list
    clients[clientCount].socket = serverSocket;
    clients[clientCount].id = clientCount;
    clients[clientCount].address = currentAddr;

    // Print client information
    printClientInfo(clientCount);


    // Create a thread to handle the client
    printf(" antes de criar thread quando é client novo \n");
    pthread_create(&threads[clientCount], NULL, handleClient, (void *)&clients[clientCount].id);


    clientCount++;

     printf(" criou thread do client novo \n");
  }
    // Send a welcome message to the client (for demonstration purposes)
   // sprintf(buffer, "Server: Welcome, you are Client %d\n", clientCount);
    // sendto(clients[clientCount - 1].socket, buffer, strlen(buffer), 0, (struct sockaddr *)&clients[clientCount - 1].address, sizeof(clients[clientCount - 1].address));
  }

  close(serverSocket);
  // pthread_mutex_destroy(&mutex);

  return 0;
}
