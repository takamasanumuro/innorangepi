#include "IDataSender.hpp"

//#define ADDRESS     "tcp://broker.hivemq.com:1883"
//#define CLIENTID    "takataka"
//#define TOPIC       "innoboat/tensao/bateria"
//#define QOS         1
//#define TIMEOUT     10000L

class MQTTDataSender : public IDataSender
{
public:
    MQTTDataSender(const char* address, const char* clientID, int qos, long timeout);
    void sendData(const std::string& tag, const float& data) override;

private:
    const char* address;
    const char* clientID;
    const char* topic;
    int qos;
    long timeout;
};
