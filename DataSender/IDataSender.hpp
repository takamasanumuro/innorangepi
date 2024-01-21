#pragma once
#include <any> // Type erasure for virtual functions to allow more generic input parameters
#include <iostream>

/*Let's define an abstract interface in order to provide a common functionality to send data to a remote server.
Different protocols can be used like parameterized HTTP GET or MQTT.
The implementation details must be left for the concrete classes, which will select which protocol to use
and any specific requirements shall be passed to their constructor.
*/

class IDataSender {
public:
    virtual ~IDataSender() {}

    virtual void sendData(const std::string& tag, const float& data) = 0;
};


