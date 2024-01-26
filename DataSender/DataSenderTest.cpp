//Test for DataSEnd interface
#include <iostream>
#include "GETDataSender.hpp"
#include "MQTTDataSender.hpp"
#include "SocketDataSender.hpp"
#include <memory>

//Random C++ generator
#include <random>

int main(int argc, char** argv) {

    std::cout << "DataSender" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(21.0f, 25.2f);
    float value = dis(gen);

    IDataSender* dataSender = new GETDataSender("44.221.0.169", "8080");
    dataSender->sendData("altitude", value);
    delete(dataSender);

    printf("Switching to MQTT\n");
    dataSender = new MQTTDataSender("tcp://broker.hivemq.com:1883", "takataka", 1, 10000L);
    dataSender->sendData("innoboat/tensao/bateria", value);
    delete(dataSender);

    dataSender = new MQTTDataSender("tcp://localhost:1883", "takataka", 1, 10000L);
    dataSender->sendData("innoboat/tensao/bateria", value);
    delete(dataSender);

    dataSender = new SocketDataSender("127.0.0.1", "5555");
    dataSender->sendData("latitude", value);
    delete(dataSender);

    return 0;
}