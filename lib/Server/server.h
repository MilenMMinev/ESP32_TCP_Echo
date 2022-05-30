#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys/socket.h"
#include "netdb.h"
#include "errno.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"



#define TCP_SERVER_BIND_ADDRESS "0.0.0.0"
#define TCP_SERVER_BIND_PORT "11122"
#define TCP_SERVER_MAX_CLIENTS 5

#define INVALID_SOCK (-1)
#define YIELD_TO_ALL_MS 50

#define API_ROUTE_CNT "/clients/cnt\r"
#define API_ROUTE_MESSEGES_CNT "/messages_cnts\r"
#define API_ROUTE_MESSEGES_SIZE "/messages_sizes\r"


enum API_ROUTES{API_ECHO, API_CNT, API_MESSEGES_CNT, API_MESSEGES_SIZE};


void tcp_server_task(void *pvParameters);

#endif /* SERVER_H */
