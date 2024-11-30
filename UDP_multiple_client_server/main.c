// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON.h"
#include "MQTTClient.h"
#include <math.h>
#include "mqtt_header.h"

#define PORT 8888
#define MAXLINE 1024
#define ACK_MESSAGE "ACK"
#define ARRAY_SIZE 2

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

int arduino1_connected = 0;
int arduino2_connected = 0;
int both_connected = 0;

void *handle_client(void *arg);
void *handle_arduino_values(void *arg);
void send_alert_exceeded_values_temp(float *temp_value);
void initMQTT();
void publishToMQTT(const char *payload, const char *topic);
void send_alert_exceeded_values_hum(float *humidity_value);
void write_to_sensor_data_1(SensorData *data);
void write_to_sensor_data_2(SensorData *data);
void handle_differences(float temp1, float temp2, float hum1, float hum2);

// Driver code
int main()
{
  int sockfd;
  struct sockaddr_in servaddr, cliaddr;

  initMQTT();

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
    printf("LAU ENTROU\n");
    arduino1_connected = 1;
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';
    if (temp_value > 50 || temp_value < 0)
    {
      send_alert_exceeded_values_temp(&temp_value);
    }

    if (humidity_value > 80 || humidity_value < 20)
    {
      send_alert_exceeded_values_hum(&humidity_value);
    }

    data.temperature = temp_value;
    data.humidity = humidity_value;
    write_to_sensor_data_1(&data);
  }
  else if (strcmp(id_str, "ZE") == 0)
  {
    printf("ZE ENTROU\n");
    arduino2_connected = 1;
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';
    if (temp_value > 50 || temp_value < 0)
    {
      send_alert_exceeded_values_temp(&temp_value);
    }

    if (humidity_value > 80 || humidity_value < 20)
    {
      send_alert_exceeded_values_hum(&humidity_value);
    }
    data.temperature = temp_value;
    data.humidity = humidity_value;
    write_to_sensor_data_2(&data);
  }

  if (arduino1_connected && arduino2_connected)
  {
    both_connected = 1;
  }

  cJSON_Delete(json);

  free(client_data);
  return NULL;
}

void *handle_arduino_values(void *arg)
{
  // wait till both arduinos are connected and have sent data
  while (!both_connected && sensor_data_1[1].time == NULL && sensor_data_2[1].time == NULL)
  {
    usleep(100000);
  }

  while (1)
  {
    if (strcmp(sensor_data_1[1].time, sensor_data_2[1].time) == 0)
    {
      printf("first if\n");
      handle_differences(sensor_data_1[1].temperature, sensor_data_2[1].temperature, sensor_data_1[1].humidity, sensor_data_2[1].humidity);
    }
    else if (strcmp(sensor_data_1[1].time, sensor_data_2[0].time) == 0)
    {
      printf("second if\n");
      handle_differences(sensor_data_1[1].temperature, sensor_data_2[0].temperature, sensor_data_1[1].humidity, sensor_data_2[0].humidity);
    }
    else if (strcmp(sensor_data_1[0].time, sensor_data_2[1].time) > 0)
    {
      printf("third if\n");
      handle_differences(sensor_data_1[0].temperature, sensor_data_2[1].temperature, sensor_data_1[0].humidity, sensor_data_2[1].humidity);
    }
    else if (strcmp(sensor_data_1[0].time, sensor_data_2[0].time) > 0)
    {
      printf("fourth if\n");
      handle_differences(sensor_data_1[0].temperature, sensor_data_2[0].temperature, sensor_data_1[0].humidity, sensor_data_2[0].humidity);
    }
  }

  return NULL;
}

void write_to_sensor_data_1(SensorData *data)
{
  sensor_data_1[0] = sensor_data_1[1];
  sensor_data_1[1] = *data;
}

void write_to_sensor_data_2(SensorData *data)
{
  sensor_data_2[0] = sensor_data_2[1];
  sensor_data_2[1] = *data;
}

void send_alert_exceeded_values_temp(float *temp_value)
{
  char alert_message[50] = "Temperature value exceeded the limits: \0";
  char topic[50] = "/comcs/g45/temperatureAlert";
  publishToMQTT(&alert_message, &topic);
}

void send_alert_exceeded_values_hum(float *humidity_value)
{
  char alert_message[50] = "Humidity value exceeded the limits: \0";
  char topic[50] = "/comcs/g45/humidityAlert";
  publishToMQTT(&alert_message, &topic);
}

void handle_differences(float temp1, float temp2, float hum1, float hum2)
{
  char topicTemp[50] = "/comcs/g45/temperatureDiffAlert";
  char topicHum[50] = "/comcs/g45/humidityDiffAlert";
  float temp_diff = fabs(temp1 - temp2);
  float hum_diff = fabs(hum1 - hum2);

  printf("Temperature difference: %.2f\n", temp_diff);
  printf("Humidity difference: %.2f\n", hum_diff);

  if (temp_diff > 2)
  {
    char alert_message[50];
    sprintf(alert_message, "Temperature difference exceeded %.2f", temp_diff);
    publishToMQTT(alert_message, &topicTemp);
  }

  if (hum_diff > 5)
  {
    char alert_message[50];
    sprintf(alert_message, "Humidity difference exceeded %.2f", hum_diff);
    publishToMQTT(alert_message, &topicHum);
  }
}
