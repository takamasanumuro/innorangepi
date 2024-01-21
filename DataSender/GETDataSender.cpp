#pragma once
#include "GETDataSender.hpp"

std::string GETDataSender::_formatData(const std::string& tag, const float& value) {

    //key=value
    std::string formattedData = tag + "=";
    formattedData += std::to_string(value);
    return formattedData;
}

void GETDataSender::sendData(const std::string& tag, const float& data) {

    //Format the data
    std::string formattedData = _formatData(tag, data);

    std::cout << "Sending data: " << formattedData << '\n';

    extern void curlURL(char *url);

    std::string url = "http://" + _ip + ":" + _port + "/ScadaBR/httpds?";
    url += formattedData;

    curlURL((char*)url.c_str());


}

