#if defined WIN32
#include <winsock.h>
#include <windows.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include "protocol.h"

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif


#if defined WIN32 && !defined(socklen_t)
typedef int socklen_t;
#endif



void clearwinsock(void) {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(const char *errorMessage) {
    if (errorMessage) {
        printf("%s", errorMessage);
    }
}

static float random_in_range(float min, float max) {
    float r = (float)rand() / (float)RAND_MAX;
    return min + r * (max - min);
}


static float htonf(float value) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = value;
    u.i = htonl(u.i);
    return u.f;
}

float get_temperature(void) {
    return random_in_range(-10.0f, 40.0f);
}

float get_humidity(void) {
    return random_in_range(20.0f, 100.0f);
}

float get_wind(void) {
    return random_in_range(0.0f, 100.0f);
}

float get_pressure(void) {
    return random_in_range(950.0f, 1050.0f);
}



int is_supported_city(const char *city) {
    char tmp[64];
    size_t i;

    for (i = 0; i < sizeof(tmp) - 1 && city[i] != '\0'; ++i) {
        tmp[i] = (char)tolower((unsigned char)city[i]);
    }
    tmp[i] = '\0';


    if (strcmp(tmp, "bari") == 0) return 1;
    if (strcmp(tmp, "roma") == 0) return 1;
    if (strcmp(tmp, "milano") == 0) return 1;
    if (strcmp(tmp, "napoli") == 0) return 1;
    if (strcmp(tmp, "torino") == 0) return 1;
    if (strcmp(tmp, "palermo") == 0) return 1;
    if (strcmp(tmp, "genova") == 0) return 1;
    if (strcmp(tmp, "bologna") == 0) return 1;
    if (strcmp(tmp, "firenze") == 0) return 1;
    if (strcmp(tmp, "venezia") == 0) return 1;

    return 0;
}

int validate_request(const weather_request_t *req) {
    if (!req) return STATUS_INVALID_REQUEST;


    char type_lower = (char)tolower((unsigned char)req->type);


    if (type_lower != 't' && type_lower != 'h' &&
        type_lower != 'w' && type_lower != 'p') {
        return STATUS_INVALID_REQUEST;
    }


    if (req->city[0] == '\0') {
        return STATUS_INVALID_REQUEST;
    }


    if (!is_supported_city(req->city)) {
        return STATUS_CITY_NOT_FOUND;
    }

    return STATUS_OK;
}

void build_response(const weather_request_t *req, weather_response_t *resp) {
    int status = validate_request(req);
    resp->status = (unsigned int)status;

    if (status == STATUS_OK) {

        char type_lower = (char)tolower((unsigned char)req->type);
        resp->type = type_lower;
        
        switch (type_lower) {
            case 't':
                resp->value = get_temperature();
                break;
            case 'h':
                resp->value = get_humidity();
                break;
            case 'w':
                resp->value = get_wind();
                break;
            case 'p':
                resp->value = get_pressure();
                break;
            default:

                resp->status = STATUS_INVALID_REQUEST;
                resp->type = '\0';
                resp->value = 0.0f;
                break;
        }
    } else {
        resp->type = '\0';
        resp->value = 0.0f;
    }
}


static void get_ip_string(const struct sockaddr_in *addr, char *buffer, size_t size) {
    if (!addr || !buffer || size == 0) {
        return;
    }

    const char *ip = inet_ntoa(addr->sin_addr);
    if (ip) {
        strncpy(buffer, ip, size - 1);
        buffer[size - 1] = '\0';
    } else {
        strncpy(buffer, "unknown", size - 1);
        buffer[size - 1] = '\0';
    }
}

int handle_client(int client_socket, struct sockaddr_in *client_addr) {
    weather_request_t req;
    weather_response_t resp;
    int total_bytes_recv = 0;
    int bytes_to_receive = (int)sizeof(req);

    memset(&req, 0, sizeof(req));

    while (total_bytes_recv < bytes_to_receive) {
        int bytes_recv = recv(client_socket,
                              (char *)&req + total_bytes_recv,
                              bytes_to_receive - total_bytes_recv,
                              0);
        if (bytes_recv <= 0) {
            errorhandler("Errore nella ricezione dei dati o connessione chiusa.\n");
            return -1;
        }
        total_bytes_recv += bytes_recv;
    }


    req.city[sizeof(req.city) - 1] = '\0';


    char client_ip[INET_ADDRSTRLEN];
    get_ip_string(client_addr, client_ip, sizeof(client_ip));

    printf("Richiesta '%c %s' dal client ip %s\n",
           req.type, req.city, client_ip);


    build_response(&req, &resp);

    weather_response_t resp_network;
    resp_network.status = htonl(resp.status);
    resp_network.type = resp.type;
    resp_network.value = htonf(resp.value);

    if (send(client_socket, (const char *)&resp_network, sizeof(resp_network), 0) != sizeof(resp_network)) {
        errorhandler("Errore nell'invio dei dati.\n");
        return -1;
    }

    return 0;
}



static void print_usage(const char *progname) {
    printf("Uso: %s [-p port]\n", progname);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_SERVER_PORT;
    int i;


    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                printf("Porta non valida.\n");
                return 1;
            }
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

#if defined WIN32

    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
        printf("Errore nell'inizializzazione di Winsock.\n");
        return 1;
    }
#endif


    srand((unsigned int)time(NULL));


    int my_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket < 0) {
        errorhandler("Errore nella creazione della socket.\n");
        clearwinsock();
        return 1;
    }


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    server_addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(my_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        errorhandler("Errore nel binding della socket.\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }


    if (listen(my_socket, QUEUE_SIZE) < 0) {
        errorhandler("Errore nell'impostazione della socket in ascolto.\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    printf("Server in ascolto sulla porta %d.\n", port);


    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = (socklen_t)sizeof(client_addr);
        int client_socket = accept(my_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            errorhandler("Errore nell'accettazione della connessione.\n");
            break;
        }


        if (handle_client(client_socket, &client_addr) != 0) {
            printf("Errore nella gestione del client.\n");
        }

        closesocket(client_socket);
    }

    printf("Server terminated.\n");

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
