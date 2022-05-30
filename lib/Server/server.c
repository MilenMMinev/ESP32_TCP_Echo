#include "server.h"

/**
 * @brief Utility to log socket errors
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket number
 * @param[in] err Socket errno
 * @param[in] message Message to print
 */
static void log_socket_error(const char *tag, const int sock, const int err, const char *message)
{
    ESP_LOGE(tag, "[sock=%d]: %s\n"
                  "error=%d: %s", sock, message, err, strerror(err));
}

// Parse Api routes
int get_api_route(char* in_data, int len)
{
    if (strncmp(in_data, API_ROUTE_CNT, strlen(API_ROUTE_CNT)) == 0)
        return API_CNT;
    if (strncmp(in_data, API_ROUTE_MESSEGES_CNT, strlen(API_ROUTE_MESSEGES_CNT)) == 0)
        return API_MESSEGES_CNT;
    if (strncmp(in_data, API_ROUTE_MESSEGES_SIZE, strlen(API_ROUTE_MESSEGES_SIZE)) == 0)
        return API_MESSEGES_SIZE;
    ESP_LOGI("asd", "asd get_api_route_echo");
    return API_ECHO;

}

int count_active_clients(int* sockets, int max_sockets){
    int cnt = 0;
    for(int i = 0; i < max_sockets; i++){
        if(sockets[i] != INVALID_SOCK){
            cnt++;
        }
    }
    return cnt;
}


/**
 * @brief Tries to receive data from specified sockets in a non-blocking way,
 *        i.e. returns immediately if no data.
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket for reception
 * @param[out] data Data pointer to write the received data
 * @param[in] max_len Maximum size of the allocated space for receiving data
 * @return
 *          >0 : Size of received data
 *          =0 : No data available
 *          -1 : Error occurred during socket read operation
 *          -2 : Socket is not connected, to distinguish between an actual socket error and active disconnection
 */
static int try_receive(const char *tag, const int sock, char * data, size_t max_len)
{
    int len = recv(sock, data, max_len, 0);
    if (len < 0) {
        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   // Not an error
        }
        if (errno == ENOTCONN) {
            ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
            return -2;  // Socket has been disconnected
        }
        log_socket_error(tag, sock, errno, "Error occurred during receiving");
        return -1;
    }

    if (data[len-1] != '\n' && data[len-1] != '\r'){
        ESP_LOGW(tag, "[sock=%d]: Invalid text input", sock);
        return -10;
    }

    return len;
}

/**
 * @brief Sends the specified data to the socket. This function blocks until all bytes got sent.
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket to write data
 * @param[in] data Data to be written
 * @param[in] len Length of the data
 * @return
 *          >0 : Size the written data
 *          -1 : Error occurred during socket write operation
 */
static int socket_send(const char *tag, const int sock, const char * data, const size_t len)
{
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
            log_socket_error(tag, sock, errno, "Error occurred during sending");
            return -1;
        }
        to_write -= written;
    }
    return len;
}


void tcp_server_task(void *pvParameters)
{
    static char rx_buffer[128];
    static char clients_cnt_buffer[10];
    int cnt_active_clients = 0;
    static const char *TAG = "nonblocking-socket-server";
    SemaphoreHandle_t *server_ready = pvParameters;
    struct addrinfo hints = { .ai_socktype = SOCK_STREAM };
    struct addrinfo *address_info;
    int listen_sock = INVALID_SOCK;
    const size_t max_socks = TCP_SERVER_MAX_CLIENTS - 1;
    static int sock[TCP_SERVER_MAX_CLIENTS - 1];
    static int sock_message_cnt[TCP_SERVER_MAX_CLIENTS - 1];
    static int sock_message_size[TCP_SERVER_MAX_CLIENTS - 1];


    ESP_LOGI(TAG, "max_socks: %d", max_socks);

    //Init sockets as Invalid(free). Init messages counters
    for (int i=0; i<max_socks; ++i) {
        sock[i] = INVALID_SOCK;
        sock_message_cnt[i] = 0;
        sock_message_size[i] = 0;
    }

    // Translating the hostname or a string representation of an IP to address_info
    int res = getaddrinfo(TCP_SERVER_BIND_ADDRESS, TCP_SERVER_BIND_PORT, &hints, &address_info);
    if (res != 0 || address_info == NULL) {
        ESP_LOGE(TAG, "couldn't get hostname for `%s` "
                      "getaddrinfo() returns %d, addrinfo=%p", TCP_SERVER_BIND_ADDRESS, res, address_info);
        goto error;
    }

    // Creating a listener socket
    listen_sock = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);

    if (listen_sock < 0) {
        log_socket_error(TAG, listen_sock, errno, "Unable to create socket");
        goto error;
    }
    ESP_LOGI(TAG, "Listener socket created");

    // Marking the socket as non-blocking
    int flags = fcntl(listen_sock, F_GETFL);
    if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_socket_error(TAG, listen_sock, errno, "Unable to set socket non blocking");
        goto error;
    }
    ESP_LOGI(TAG, "Socket marked as non blocking");

    // Binding socket to the given address
    int err = bind(listen_sock, address_info->ai_addr, address_info->ai_addrlen);
    if (err != 0) {
        log_socket_error(TAG, listen_sock, errno, "Socket unable to bind");
        goto error;
    }
    ESP_LOGI(TAG, "Socket bound on %s:%s", TCP_SERVER_BIND_ADDRESS, TCP_SERVER_BIND_PORT);

    // Set queue (backlog) of pending connections to one (can be more)
    err = listen(listen_sock, TCP_SERVER_MAX_CLIENTS);
    if (err != 0) {
        log_socket_error(TAG, listen_sock, errno, "Error occurred during listen");
        goto error;
    }
    ESP_LOGI(TAG, "Socket listening");
    xSemaphoreGive(*server_ready);

    // Main loop for accepting new connections and serving all connected clients
    while (1) {
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);

        // Find a free socket
        int new_sock_index = 0;
        for (new_sock_index=0; new_sock_index<max_socks; ++new_sock_index) {
            if (sock[new_sock_index] == INVALID_SOCK) {
                break;
            }
        }

        // We accept a new connection only if we have a free socket
        if (new_sock_index < max_socks) {
            // Try to accept a new connection
            sock[new_sock_index] = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock[new_sock_index] < 0) {
                if (errno == EWOULDBLOCK) { // The listener socket did not accepts any connection
                                            // continue to serve open connections and try to accept again upon the next iteration
                    ESP_LOGV(TAG, "No pending connections...");
                } else {
                    log_socket_error(TAG, listen_sock, errno, "Error when accepting connection");
                    goto error;
                }
            } else {
                // We have a new client connected -> print it's address
                ESP_LOGI(TAG, "[sock=%d]: Connection accepted.", sock[new_sock_index]);
                sock_message_cnt[new_sock_index] = 0;
                sock_message_size[new_sock_index] = 0;

                // ...and set the client's socket non-blocking
                flags = fcntl(sock[new_sock_index], F_GETFL);
                if (fcntl(sock[new_sock_index], F_SETFL, flags | O_NONBLOCK) == -1) {
                    log_socket_error(TAG, sock[new_sock_index], errno, "Unable to set socket non blocking");
                    goto error;
                }
                ESP_LOGI(TAG, "[sock=%d]: Socket marked as non blocking", sock[new_sock_index]);
            }
        }

        // We serve all the connected clients in this loop
        for (int i=0; i<max_socks; ++i) {
            if (sock[i] != INVALID_SOCK) {

                // This is an open socket -> try to serve it
                int len = try_receive(TAG, sock[i], rx_buffer, sizeof(rx_buffer));
                if (len < 0) {
                    // Error occurred within this client's socket -> close and mark invalid
                    ESP_LOGI(TAG, "[sock=%d]: try_receive() returned %d -> closing the socket", sock[i], len);
                    close(sock[i]);
                    sock[i] = INVALID_SOCK;
                } else if (len > 0) {

                    int api_route = get_api_route(rx_buffer, len);

                    if (api_route == API_CNT){
                        cnt_active_clients = count_active_clients(sock, max_socks);
                        len = sprintf(clients_cnt_buffer, "%d", cnt_active_clients);
                        strcpy(rx_buffer, clients_cnt_buffer);
                    }
                    else if(api_route == API_MESSEGES_CNT){
                        char* pos = rx_buffer;
                        for (int i = 0 ; i < max_socks ; i++) {
                            if (i) {
                                pos += sprintf(pos, ", ");
                            }
                            pos += sprintf(pos, "%d", sock_message_cnt[i]);
                        }
                        len = strlen(rx_buffer);
                        ESP_LOGI(TAG, "[sock=%d]: Received Messages Cnt. Response: %.*s", sock[i], len, rx_buffer);
                    }
                    else if(api_route == API_MESSEGES_SIZE){
                        char* pos = rx_buffer;
                        for (int i = 0 ; i < max_socks ; i++) {
                            if (i) {
                                pos += sprintf(pos, ", ");
                            }
                            pos += sprintf(pos, "%d", sock_message_size[i]);
                        }
                        len = strlen(rx_buffer);
                        ESP_LOGI(TAG, "[sock=%d]: Received Messages sizes. Response: %.*s", sock[i], len, rx_buffer);
                    }
                    else if (api_route == API_ECHO){
                        // Received some data -> echo back
                        ESP_LOGI(TAG, "[sock=%d]: Received %.*s", sock[i], len, rx_buffer);
                    }

                    len = socket_send(TAG, sock[i], rx_buffer, len);
                    if (len < 0) {
                        // Error occurred on write to this socket -> close it and mark invalid
                        ESP_LOGI(TAG, "[sock=%d]: socket_send() returned %d -> closing the socket", sock[i], len);
                        close(sock[i]);
                        sock[i] = INVALID_SOCK;
                    } else {
                        ESP_LOGI(TAG, "Successfully echoed to this socket");
                        // Successfully echoed to this socket
                        if (api_route == API_ECHO){
                            ESP_LOGI(TAG, "Incrementing messages stats");
                            sock_message_cnt[i] ++;
                            sock_message_size[i] += len;
                        }
                        ESP_LOGI(TAG, "[sock=%d]: Written %.*s", sock[i], len, rx_buffer);
                    }
                }

            } // one client's socket
        } // for all sockets

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(YIELD_TO_ALL_MS));
    }

error:
    if (listen_sock != INVALID_SOCK) {
        close(listen_sock);
    }

    for (int i=0; i<max_socks; ++i) {
        if (sock[i] != INVALID_SOCK) {
            close(sock[i]);
        }
    }

    free(address_info);
    vTaskDelete(NULL);
}