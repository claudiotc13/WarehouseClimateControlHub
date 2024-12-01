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

#define PORT 8888         // Port number
#define MAXLINE 1024      // Maximum length of the message
#define ACK_MESSAGE "ACK" // Acknowledgment message
#define ARRAY_SIZE 2      // Size of the sensor data array

// Structure to store the client data
typedef struct
{
  int sockfd;
  struct sockaddr_in *client_addr;
  char message[MAXLINE];
} ClientData;

// Structure to store the sensor data
typedef struct
{
  char time[20];
  float temperature;
  float humidity;
} SensorData;

SensorData sensor_data_1[ARRAY_SIZE]; // Array to store the last two values sent by arduino1
SensorData sensor_data_2[ARRAY_SIZE]; // Array to store the last two values sent by arduino2

int sensor_data_index_1 = 0; // Global index for sensor_data_array_1
int sensor_data_index_2 = 0; // Global index for sensor_data_array_2

int arduino1_connected = 0; // Flag to check if arduino1 is connected
int arduino2_connected = 0; // Flag to check if arduino2 is connected
int both_connected = 0;     // Flag to check if both arduinos are connected

void *handle_client(void *arg);                                            // Function to handle the data sent by the client
void *handle_arduino_values(void *arg);                                    // Function to handle the temperature and humidity sent by the arduino sensors
void send_alert_exceeded_values_temp();                                    // Function to send an alert to the MQTT broker when the temperature values exceed the limits
void initMQTT();                                                           // Function to initialize the MQTT connection
void publishToMQTT(const char *payload, const char *topic);                // Function to publish a message to the MQTT broker
void send_alert_exceeded_values_hum();                                     // Function to send an alert to the MQTT broker when the humidity values exceed the limits
void write_to_sensor_data_1(SensorData *data);                             // Function to write the data to the sensor_data_1 array
void write_to_sensor_data_2(SensorData *data);                             // Function to write the data to the sensor_data_2 array
void handle_differences(float temp1, float temp2, float hum1, float hum2); // Function to handle the differences between the values sent by the two arduinos

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

  // create a thread to handle the values sent by the arduino sensors
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

    // create a thread to handle the data sent by the client
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

// Thread function to handle the data sent by the client
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

  // get the values from the json object
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

  // transform the values to float
  if (cJSON_IsString(humidity) && (humidity->valuestring != NULL))
  {
    humidity_value = atof(humidity->valuestring);
  }
  else
  {
    printf("Humidity not found or invalid\n");
  }

  // transform the values to string
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

  // check which arduino client sent the data
  // There are two arduinos with ids "LAU" and "ZE"
  if (strcmp(id_str, "LAU") == 0)
  {
    arduino1_connected = 1;
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';

    // check if the values exceed the limits
    if (temp_value > 50 || temp_value < 0)
    {
      send_alert_exceeded_values_temp(&temp_value);
    }

    // check if the values exceed the limits
    if (humidity_value > 80 || humidity_value < 20)
    {
      send_alert_exceeded_values_hum(&humidity_value);
    }

    data.temperature = temp_value;
    data.humidity = humidity_value;

    // write the data to the sensor_data array to later be handled by the handle_arduino_values thread
    write_to_sensor_data_1(&data);
  }
  else if (strcmp(id_str, "ZE") == 0)
  {
    arduino2_connected = 1;
    SensorData data;
    strncpy(data.time, time_str, sizeof(data.time) - 1);
    data.time[sizeof(data.time) - 1] = '\0';

    // check if the values exceed the limits
    if (temp_value > 50 || temp_value < 0)
    {
      send_alert_exceeded_values_temp(&temp_value);
    }

    // check if the values exceed the limits
    if (humidity_value > 80 || humidity_value < 20)
    {
      send_alert_exceeded_values_hum(&humidity_value);
    }
    data.temperature = temp_value;
    data.humidity = humidity_value;

    // write the data to the sensor_data array to later be handled by the handle_arduino_values thread
    write_to_sensor_data_2(&data);
  }

  // check if both arduinos are connected to the server
  if (arduino1_connected && arduino2_connected)
  {
    both_connected = 1;
  }

  cJSON_Delete(json);

  free(client_data);
  return NULL;
}

// Thread function to handle the temperature and humidity sent by the arduino sensors
// written to the sensor_data arrays
void *handle_arduino_values(void *arg)
{
  // wait till both arduinos are connected and have sent data
  while (!both_connected && sensor_data_1[1].time == NULL && sensor_data_2[1].time == NULL)
  {
    usleep(100000);
  }

  while (1)
  {
    // with this if statement we can handle all the possible cases of the time values of the two arduinos
    // if one of the arduinos sends the data before the other, we can still handle the differences
    if (strcmp(sensor_data_1[1].time, sensor_data_2[1].time) == 0)
    {
      handle_differences(sensor_data_1[1].temperature, sensor_data_2[1].temperature, sensor_data_1[1].humidity, sensor_data_2[1].humidity);
    }
    else if (strcmp(sensor_data_1[1].time, sensor_data_2[0].time) == 0)
    {
      handle_differences(sensor_data_1[1].temperature, sensor_data_2[0].temperature, sensor_data_1[1].humidity, sensor_data_2[0].humidity);
    }
    else if (strcmp(sensor_data_1[0].time, sensor_data_2[1].time) == 0)
    {
      handle_differences(sensor_data_1[0].temperature, sensor_data_2[1].temperature, sensor_data_1[0].humidity, sensor_data_2[1].humidity);
    }
  }

  return NULL;
}

// Function to write the data to the sensor_data_1 array
// to keep track of the last two values sent by the arduino
void write_to_sensor_data_1(SensorData *data)
{
  sensor_data_1[0] = sensor_data_1[1];
  sensor_data_1[1] = *data;
}

// Function to write the data to the sensor_data_2 array
// to keep track of the last two values sent by the arduino
void write_to_sensor_data_2(SensorData *data)
{
  sensor_data_2[0] = sensor_data_2[1];
  sensor_data_2[1] = *data;
}

// Function to send an alert to the MQTT broker when the temperature values exceed the limits
void send_alert_exceeded_values_temp()
{
  char alert_message[50] = "Temperature value exceeded the limits\0";
  char topic[50] = "/comcs/g45/temperatureAlert";
  publishToMQTT(&alert_message, &topic);
}

// Function to send an alert to the MQTT broker when the humidity values exceed the limits
void send_alert_exceeded_values_hum()
{
  char alert_message[50] = "Humidity value exceeded the limits\0";
  char topic[50] = "/comcs/g45/humidityAlert";
  publishToMQTT(&alert_message, &topic);
}

// Function to handle the differences between the values sent by the two arduinos
// and send an alert to the MQTT broker if the difference exceeds a certain value
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
