# ESP32_TCP_Echo
A simpleTCP server that echos back sent data and responds to basic api calls.
The app relies heavily on the official  [examples](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets) by esp-idf

It supports multiple simultaneous clients.
It echoes back sent data except for some key words that serve as routes to the API endpoint:
- **/clients/cnt** Lists number of currently active clients.
- **/messages_cnts** Lists number of valid messages received by client.
- **/messages_sizes** Lists total number of bytes sent/received by client.
 

## Environment:
Tested on espressif32 using VS code + Platformio

## How to run:

The ESP works in AP mode. Once it boots it creates a new Free Wifi Network.
The TCP server starts on 192.168.4.1:11122 by default.

## TODO:
There are concurrency problems with the API methods leading them to sometimes yield incorrect data when multiple clients are sending/receiving simultaneously.

## Tests:

There are Python unittests in tests/tests.py which test the functionality of the server.
