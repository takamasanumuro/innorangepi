//Test for DataSEnd interface
#include <iostream>
#include "IDataSender.hpp"
#include "GETDataSender.hpp"


int main(int argc, char** argv) {

    std::cout << "DataSender" << std::endl;
    
    float value = 3.14f;
    IDataSender* dataSender = new GETDataSender("localhost", "8080");
    dataSender->sendData("temperature", value);


    return 0;
}