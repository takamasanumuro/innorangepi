//Test for DataSEnd interface
#include <iostream>
#include "IDataSender.hpp"
#include "GETDataSender.hpp"

int main(int argc, char** argv) {

    std::cout << "DataSender" << std::endl;
    
    float value = 3.14f;
    IDataSender* dataSender = new GETDataSender("44.221.0.169", "8080");
    dataSender->sendData("altitude", value);

    return 0;
}