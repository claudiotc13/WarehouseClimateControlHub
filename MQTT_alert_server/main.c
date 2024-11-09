#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define your MQTT connection details
#define MQTT_ADDRESS   "370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud"
#define MQTT_PORT      8883
#define MQTT_CLIENTID  "UDPtoMQTTClient"
#define MQTT_TOPIC     "teste2"
#define MQTT_USERNAME  "comscs2324jpt45"
#define MQTT_PASSWORD  "josepedro2"

// Log callback function
void log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
    printf("Log: %s\n", str);
}

int main(int argc, char *argv[]) {
    // Initialize the Mosquitto library
    mosquitto_lib_init();

    // Create a new Mosquitto client instance
    struct mosquitto *mosq = mosquitto_new(MQTT_CLIENTID, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create client instance.\n");
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    // Set username and password
    mosquitto_username_pw_set(mosq, MQTT_USERNAME, MQTT_PASSWORD);

    // Set log callback
    mosquitto_log_callback_set(mosq, log_callback);

    // Set TLS/SSL connection
    mosquitto_tls_set(mosq, NULL, NULL, NULL, NULL, NULL);  // Using default CA certificates

    // Connect to the broker
    if (mosquitto_connect(mosq, MQTT_ADDRESS, MQTT_PORT, 60)) {
        fprintf(stderr, "Could not connect to broker.\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    // Start loop for managing connection
    mosquitto_loop_start(mosq);

    // Publish a message to the specified topic
    const char *message = "Hello, MQTT";  // Define the message content
    int ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen("Hello, MQTT"), "Hello, MQTT", 0, true);
    if (ret) {
        fprintf(stderr, "Failed to publish message.\n");
    } else {
        printf("Message published successfully.\n");
    }


    int counter = 0;

    while (1)
    {


        counter++;
        
        sleep(1);

        const char *cmd = "mosquitto_pub -h '370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud' -p 8883 -u 'comscs2324jpt45' -P 'josepedro2' -t '/comcs/g45/Alerta' -m 'ALERTA DEU MERDA'";
        const char *cmd2 = "mosquitto_pub -h '370134814ab545c6ae9d743848a77f33.s1.eu.hivemq.cloud' -p 8883 -u 'comscs2324jpt45' -P 'josepedro2' -t '/comcs/g45/Alerta2' -m 'ALERTA DEU MERDA2'";
        if (counter == 5) {
           // Execute the command using system()
           int ret2 = system(cmd);
           // Check the result of the command
           if (ret2 == -1) {
           perror("Error executing mosquitto_pub");
           return 1;
           }
        } else if (counter == 10){
           // Execute the command using system()
           int ret2 = system(cmd2);
           counter = 0;
           // Check the result of the command
           if (ret2 == -1) {
           perror("Error executing mosquitto_pub");
           return 1;
           }
        }
     
    }
    

  

    // Wait a moment to ensure message is sent
    mosquitto_loop_forever(mosq, -1, 1);  // This keeps the loop running

    // Stop the loop and clean up
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}
