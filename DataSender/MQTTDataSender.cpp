#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTDataSender.hpp"
#include "MQTTClient.h"

MQTTDataSender::MQTTDataSender(const char* address, const char* clientID, int qos, long timeout)
{
    this->address = address;
    this->clientID = clientID;
    this->qos = qos;
    this->timeout = timeout;
}

void MQTTDataSender::sendData(const std::string& tag , const float& data) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int ch;

    this->topic = tag.data();

    MQTTClient_create(&client, this->address, this->clientID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    printf("Connecting to server %s\n", this->address);
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char payload[16] = {0};
    sprintf(payload, "%f\0", data);

    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = this->qos;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, this->topic, &pubmsg, &token);
    printf("Waiting for up to %ld seconds for publication of %s\n"
        "on topic %s for client with ClientID: %s\n",
        (this->timeout / 1000), payload, this->topic, this->clientID);
    rc = MQTTClient_waitForCompletion(client, token, this->timeout);
    printf("Message with delivery token %d delivered\n", token);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}