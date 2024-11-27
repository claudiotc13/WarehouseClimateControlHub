#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 2000
#define MAXLINE 1024
#define MAX_CLIENTS 100

typedef struct MessageNode
{
  char message[MAXLINE];
  struct MessageNode *next;
} MessageNode;

typedef struct
{
  struct sockaddr_in addr;
  pthread_t thread_id;
  bool active;
  MessageNode *message_queue;
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_cond;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
int find_client(struct sockaddr_in *client_addr);
int add_client(struct sockaddr_in *client_addr, pthread_t thread_id);
void enqueue_message(ClientInfo *client, const char *message);
char *dequeue_message(ClientInfo *client);

int main()
{
  int sockfd;
  char buffer[MAXLINE];
  struct sockaddr_in servaddr, cliaddr;

  // Create socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  // Fill server information
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  // Bind the socket with the server address
  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    perror("bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Server is running on port %d\n", PORT);

  while (1)
  {
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
    buffer[n] = '\0';

    pthread_mutex_lock(&clients_mutex);
    int client_index = find_client(&cliaddr);
    if (client_index == -1)
    {
      // New client, create a new thread
      pthread_t thread_id;
      struct sockaddr_in *client_addr = malloc(sizeof(struct sockaddr_in));
      memcpy(client_addr, &cliaddr, sizeof(struct sockaddr_in));

      struct client_info
      {
        int sockfd;
        struct sockaddr_in *client_addr;
        char *message;
      };

      struct client_info *info = malloc(sizeof(struct client_info));
      info->sockfd = sockfd;
      info->client_addr = client_addr;
      info->message = strdup(buffer);

      if (pthread_create(&thread_id, NULL, handle_client, (void *)info) != 0)
      {
        perror("pthread_create failed");
        free(client_addr);
        free(info->message);
        free(info);
      }
      else
      {
        pthread_detach(thread_id);
        add_client(client_addr, thread_id);
      }
    }
    else
    {
      // Existing client, pass the message to the existing thread
      enqueue_message(&clients[client_index], buffer);
    }
    pthread_mutex_unlock(&clients_mutex);
  }

  close(sockfd);
  return 0;
}

void *handle_client(void *arg)
{
  struct client_info *info = (struct client_info *)arg;
  int sockfd = info->sockfd;
  struct sockaddr_in *client_addr = info->client_addr;
  char *message = info->message;

  printf("Received message from client: %s\n", message);

  // Send a response back to the client
  char response[MAXLINE];
  snprintf(response, MAXLINE, "Server received: %s", message);
  sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));

  free(message);
  free(client_addr);
  free(info);

  // Process messages from the queue
  while (1)
  {
    pthread_mutex_lock(&info->queue_mutex);
    while (info->message_queue == NULL)
    {
      pthread_cond_wait(&info->queue_cond, &info->queue_mutex);
    }
    char *queued_message = dequeue_message(info);
    pthread_mutex_unlock(&info->queue_mutex);

    if (queued_message)
    {
      printf("Processing queued message from client: %s\n", queued_message);
      snprintf(response, MAXLINE, "Server received: %s", queued_message);
      sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
      free(queued_message);
    }
  }

  return NULL;
}

int find_client(struct sockaddr_in *client_addr)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].active && memcmp(&clients[i].addr, client_addr, sizeof(struct sockaddr_in)) == 0)
    {
      return i;
    }
  }
  return -1;
}

int add_client(struct sockaddr_in *client_addr, pthread_t thread_id)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (!clients[i].active)
    {
      clients[i].addr = *client_addr;
      clients[i].thread_id = thread_id;
      clients[i].active = true;
      clients[i].message_queue = NULL;
      pthread_mutex_init(&clients[i].queue_mutex, NULL);
      pthread_cond_init(&clients[i].queue_cond, NULL);
      return i;
    }
  }
  return -1;
}

void enqueue_message(ClientInfo *client, const char *message)
{
  MessageNode *new_node = (MessageNode *)malloc(sizeof(MessageNode));
  strncpy(new_node->message, message, MAXLINE);
  new_node->next = NULL;

  pthread_mutex_lock(&client->queue_mutex);
  if (client->message_queue == NULL)
  {
    client->message_queue = new_node;
  }
  else
  {
    MessageNode *temp = client->message_queue;
    while (temp->next != NULL)
    {
      temp = temp->next;
    }
    temp->next = new_node;
  }
  pthread_cond_signal(&client->queue_cond);
  pthread_mutex_unlock(&client->queue_mutex);
}

char *dequeue_message(ClientInfo *client)
{
  pthread_mutex_lock(&client->queue_mutex);
  if (client->message_queue == NULL)
  {
    pthread_mutex_unlock(&client->queue_mutex);
    return NULL;
  }
  MessageNode *temp = client->message_queue;
  client->message_queue = client->message_queue->next;
  pthread_mutex_unlock(&client->queue_mutex);

  char *message = strdup(temp->message);
  free(temp);
  return message;
}