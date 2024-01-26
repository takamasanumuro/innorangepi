#include "IDataSender.hpp"  

class SocketDataSender : public IDataSender {

public:
    SocketDataSender(const char* host, const char* port) : _host(host), _port(port) {}
    void sendData(const std::string& tag, const float& data) override;

private:
    const char* _host;
    const char* _port;
};

