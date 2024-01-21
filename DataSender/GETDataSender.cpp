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

    //Send the dataz
    std::cout << "Sending data: " << formattedData << '\n';


}

