#pragma once
#include "IDataSender.hpp"
#include <string>

//Concrete implementation that sends data via parameterized GET requests

class GETDataSender : public IDataSender {
public:

    GETDataSender(std::string ip, std::string port) : _ip(ip), _port(port) {}
    virtual void sendData(const std::string& tag, const float& data) override;
    

private:

    std::string _formatData(const std::string& tag, const float& value);
    
    std::string params;
    std::string _ip;
    std::string _port;

};
