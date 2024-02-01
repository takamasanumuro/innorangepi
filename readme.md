g++ DataSenderTest.cpp MQTTDataSender.cpp GETDataSender.cpp curl.cpp SocketDataSender.cpp -fpermissive -o DataSenderTest -I/usr/local/include -L/usr/local/lib -lpaho-mqtt3c -lpaho-mqtt3as -lpaho-mqttpp3 -lcurl && ./DataSenderTest\n



gcc instrumentation_main.c util.c -lcurl -o instrumentation_main && sudo ./instrumentation_main