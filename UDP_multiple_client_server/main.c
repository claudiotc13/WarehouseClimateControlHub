// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON.h"

#define PORT 8888
#define MAXLINE 1024
#define ACK_MESSAGE "ACK"
#define ARRAY_SIZE 2
// #define MSG_CONFIRM 0 //possibly needed for MAC users

typedef struct
{
  int sockfd;
  struct sockaddr_in *client_addr;
  char message[MAXLINE];
} ClientData;

typedef struct
{
  char time[20];
  float temperature;
  float humidity;
} SensorData;

SensorData sensor_data_1[ARRAY_SIZE];
SensorData sensor_data_2[ARRAY_SIZE];

int sensor_data_index_1 = 0; // Global index for sensor_data_array_1
int sensor_data_index_2 = 0; // Global index for sensor_data_array_2

void *handle_client(void *arg);
void *handle_arduino_values(void *arg);

// Driver code
int main()
{
  int sockfd;
  struct sockaddr_in servaddr, cliaddr;

  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  // Filling server information
  servaddr.sin_family = AF_INET;         // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY; // listens for connections in every available networkinterface
  servaddr.sin_port = htons(PORT);

  // Bind the socket with the server address
  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    perror("bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Server is running on port %d\n", PORT);

  int i = 0;

  pthread_t handle_values_thread;
  if (pthread_create(&handle_values_thread, NULL, handle_arduino_values, NULL) != 0)
  {
    perror("pthread_create failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  for (;;)
  {
    ClientData *client_data = (ClientData *)malloc(sizeof(ClientData));
    if (!client_data)
    {
      perror("malloc failed");
      continue;
    }

    socklen_t slen = sizeof(cliaddr);
    int n = recvfrom(sockfd, client_data->message, MAXLINE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &slen);

    if (n < 0)
    {
      perror("recvfrom failed");
      free(client_data);
      continue;
    }

    client_data->sockfd = sockfd;
    client_data->client_addr = &cliaddr;
    client_data->message[n] = '\0';

    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client, (void *)client_data) != 0)
    {
      perror("pthread_create failed");
      free(client_data);
    }
    else
    {
      pthread_detach(tid);
    }
  }

  pthread_join(handle_values_thread, NULL);
  close(sockfd);
  return 0;
}

void *handle_client(void *arg)
{
  ClientData *client_data = (ClientData *)arg;
  int sockfd = client_data->sockfd;
  struct sockaddr_in *client_addr = client_data->client_addr;
  char *message = client_data->message;

  printf("Received message from client: %s\n", message);

  int ack_len = sendto(sockfd, ACK_MESSAGE, strlen(ACK_MESSAGE), MSG_CONFIRM, (const struct sockaddr *)client_addr, sizeof(struct sockaddr_in));

  if (ack_len < 0)
  {
    perror("Acknowledgment send failed");
  }
  else
  {
    printf("Acknowledgment sent to client\n");
  }

  cJSON *json = cJSON_Parse(message);
  if (!json)
  {
    perror("cJSON_Parse failed");
    free(client_data);
    return NULL;
  }

  // get the temperature and humidity values
  cJSON *temperature = cJSON_GetObjectItemCaseSensitive(json, "temperature");
  cJSON *humidity = cJSON_GetObjectItemCaseSensitive(json, "humidity");
  cJSON *time = cJSON_GetObjectItemCaseSensitive(json, "currenttime");
  cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
  float temp_value, humidity_value;
  char time_str[20], id_str[20];

  // transform the values to float
  if (cJSON_IsString(temperature) && (temperature->valuestring != NULL))
  {
    temp_value = atof(temperature->valuestring);
  }
  else
  {
    printf("Temperature not found or invalid\n");
  }

  if (cJSON_IsString(humidity) && (humidity->valuestring != NULL))
  {
    humidity_value = atof(humidity->valuestring);
  }
  else
  {
    printf("Humidity not found or invalid\n");
  }

  if (cJSON_IsString(time) && (time->valuestring != NULL))
  {
    strncpy(time_str, time->valuestring, sizeof(time_str) - 1);
    time_str[sizeof(time_str) - 1] = '\0'; // Ensure null-termination
  }
  else
  {
    printf("Time not found or invalid\n");
  }

  if (cJSON_IsString(id) && (id->valuestring != NULL))
  {
    strncpy(id_str, id->valuestring, sizeof(id_str) - 1);
    id_str[sizeof(id_str) - 1] = '\0';
  }

  if (strcmp(id_str, "LAU") == 0)
  {
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';
    data.temperature = temp_value; // Example temperature value
    data.humidity = humidity_value;
    write_to_sensor_data_1(&data);
  }
  else if (strcmp(id_str, "ZE") == 0)
  {
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';
    data.temperature = temp_value;
    data.humidity = humidity_value;
    write_to_sensor_data_2(&data);
  }

  cJSON_Delete(json);

  free(client_data);
  return NULL;
}

void *handle_arduino_values(void *arg)
{
  while (1)
  {
    if (strcmp(sensor_data_1[1].time, sensor_data_2[1].time) == 0)
    {
      float temp_diff = sensor_data_1[1].temperature - sensor_data_2[1].temperature;
      float hum_diff = sensor_data_1[1].humidity - sensor_data_2[1].humidity;
      printf("Temperature difference: %.2f\n", temp_diff);
      printf("Humidity difference: %.2f\n", hum_diff);
    }
    else if (strcmp(sensor_data_1[1].time, sensor_data_2[0].time) == 0)
    {
      float temp_diff = sensor_data_1[1].temperature - sensor_data_2[0].temperature;
      float hum_diff = sensor_data_1[1].humidity - sensor_data_2[0].humidity;
      printf("Temperature difference: %.2f\n", temp_diff);
      printf("Humidity difference: %.2f\n", hum_diff);
    }
    else if (strcmp(sensor_data_1[0].time, sensor_data_2[1].time) > 0)
    {
      float temp_diff = sensor_data_1[0].temperature - sensor_data_2[1].temperature;
      float hum_diff = sensor_data_1[0].humidity - sensor_data_2[1].humidity;
      printf("Temperature difference: %.2f\n", temp_diff);
      printf("Humidity difference: %.2f\n", hum_diff);
    }
    else if (strcmp(sensor_data_1[0].time, sensor_data_2[0].time) > 0)
    {
      float temp_diff = sensor_data_1[0].temperature - sensor_data_2[0].temperature;
      float hum_diff = sensor_data_1[0].humidity - sensor_data_2[0].humidity;
      printf("Temperature difference: %.2f\n", temp_diff);
      printf("Humidity difference: %.2f\n", hum_diff);
    }

    /*
    printf("Sensor Data Array 1:\n");
    for (int i = 0; i < sensor_data_index_1; i++)
    {
      printf("Time: %s, Temperature: %.2f, Humidity: %.2f\n",
             sensor_data_1[i].time,
             sensor_data_1[i].temperature,
             sensor_data_1[i].humidity);
    }

    printf("Sensor Data Array 2:\n");
    for (int i = 0; i < sensor_data_index_2; i++)
    {
      printf("Time: %s, Temperature: %.2f, Humidity: %.2f\n",
             sensor_data_2[i].time,
             sensor_data_2[i].temperature,
             sensor_data_2[i].humidity);
    }

    sleep(1); // Sleep for a second before printing again
    */
  }

  return NULL;
}

void write_to_sensor_data_1(SensorData *data)
{
  sensor_data_1[0] = sensor_data_1[1];
  sensor_data_1[1] = *data;
  // sensor_data_index_1++;
}

void write_to_sensor_data_2(SensorData *data)
{
  sensor_data_2[0] = sensor_data_2[1];
  sensor_data_2[1] = *data;
  // sensor_data_index_2++;
}