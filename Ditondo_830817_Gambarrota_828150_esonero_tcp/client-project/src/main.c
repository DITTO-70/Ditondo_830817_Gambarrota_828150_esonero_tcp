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
#include <ctype.h>
#include "protocol.h"


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

static float ntohf(float value) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = value;
    u.i = ntohl(u.i);
    return u.f;
}


static void format_city_name(char *dst, size_t dst_size, const char *src) {
    size_t i;
    if (dst_size == 0) return;
    dst[0] = '\0';

    for (i = 0; i + 1 < dst_size && src[i] != '\0'; ++i) {
        if (i == 0) {
            dst[i] = (char)toupper((unsigned char)src[i]);
        } else {
            dst[i] = (char)tolower((unsigned char)src[i]);
        }
    }
    dst[i] = '\0';
}


static void print_usage(const char *progname) {
    printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", progname);
}

int parse_arguments(int argc, char *argv[],
                    char *server_addr, int *port,
                    char *type, char *city, size_t city_size) {
    int i;


    if (server_addr) {
        strncpy(server_addr, DEFAULT_SERVER_ADDRESS, BUFFER_SIZE - 1);
        server_addr[BUFFER_SIZE - 1] = '\0';
    }
    if (port) {
        *port = DEFAULT_SERVER_PORT;
    }
    if (type) {
        *type = '\0';
    }
    if (city && city_size > 0) {
        city[0] = '\0';
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            if (server_addr) {
                strncpy(server_addr, argv[++i], BUFFER_SIZE - 1);
                server_addr[BUFFER_SIZE - 1] = '\0';
            } else {
                ++i;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            if (port) {
                *port = atoi(argv[++i]);
                if (*port <= 0 || *port > 65535) {
                    printf("Porta non valida.\n");
                    return -1;
                }
            } else {
                ++i;
            }
        } else if (strcmp(argv[i], "-r") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            const char *req = argv[++i];
            if (!req[0]) {
                printf("Richiesta vuota.\n");
                return -1;
            }

            if (type) {
                *type = req[0];
            }

            const char *city_part = req + 1;
            while (*city_part == ' ') {
                city_part++;
            }

            if (!*city_part) {
                printf("Città mancante nella richiesta.\n");
                return -1;
            }

            if (city && city_size > 0) {
                strncpy(city, city_part, city_size - 1);
                city[city_size - 1] = '\0';
            }
        } else {
            print_usage(argv[0]);
            return -1;
        }
    }


    if (!type || !city) {
        printf("Errore interno di parsing.\n");
        return -1;
    }
    if (*type == '\0' || city[0] == '\0') {
        printf("Parametro -r obbligatorio.\n");
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

int connect_to_server(const char *server_address, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        errorhandler("Errore nella creazione della socket.\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);


    if (inet_addr(server_address) != INADDR_NONE) {
        server_addr.sin_addr.s_addr = inet_addr(server_address);
    } else {

        struct hostent *host = gethostbyname(server_address);
        if (host == NULL) {
            errorhandler("Errore nella risoluzione del nome host.\n");
            closesocket(sock);
            return -1;
        }
        server_addr.sin_addr = *(struct in_addr *)host->h_addr;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        errorhandler("Errore nella connessione al server.\n");
        closesocket(sock);
        return -1;
    }

    return sock;
}

int send_weather_request(int socket_desc, const weather_request_t *req) {
    if (!req) return -1;
    int bytes_to_send = (int)sizeof(*req);
    int total_bytes_sent = 0;
    while (total_bytes_sent < bytes_to_send) {
        int bytes_sent = send(socket_desc,
                              (const char *)req + total_bytes_sent,
                              bytes_to_send - total_bytes_sent,
                              0);
        if (bytes_sent <= 0) {
            errorhandler("Errore nell'invio dei dati.\n");
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }
    return 0;
}

int recv_weather_response(int socket_desc, weather_response_t *resp) {
    if (!resp) return -1;
    int bytes_to_receive = (int)sizeof(*resp);
    int total_bytes_rcvd = 0;
    while (total_bytes_rcvd < bytes_to_receive) {
        int bytes_rcvd = recv(socket_desc,
                              (char *)resp + total_bytes_rcvd,
                              bytes_to_receive - total_bytes_rcvd,
                              0);
        if (bytes_rcvd <= 0) {
            errorhandler("Errore nella ricezione dei dati o connessione chiusa.\n");
            return -1;
        }
        total_bytes_rcvd += bytes_rcvd;
    }
    resp->status = ntohl(resp->status);
    resp->value = ntohf(resp->value);
    return 0;
}

void format_and_print_response(const char *server_ip,
                               const weather_response_t *resp,
                               const char *city) {
    if (!resp || !server_ip || !city) {
        printf("Risposta non valida.\n");
        return;
    }

    char city_pretty[64];
    format_city_name(city_pretty, sizeof(city_pretty), city);

    if (resp->status == STATUS_OK) {
        switch (resp->type) {
            case 't':
                printf("Ricevuto risultato dal server ip %s. %s: Temperatura = %.1f°C\n", 
                       server_ip, city_pretty, resp->value);
                break;
            case 'h':
                printf("Ricevuto risultato dal server ip %s. %s: Umidità = %.1f%%\n", 
                       server_ip, city_pretty, resp->value);
                break;
            case 'w':
                printf("Ricevuto risultato dal server ip %s. %s: Vento = %.1f km/h\n", 
                       server_ip, city_pretty, resp->value);
                break;
            case 'p':
                printf("Ricevuto risultato dal server ip %s. %s: Pressione = %.1f hPa\n", 
                       server_ip, city_pretty, resp->value);
                break;
            default:
                printf("Ricevuto risultato dal server ip %s. Risposta non valida\n", server_ip);
                break;
        }
    } else if (resp->status == STATUS_CITY_NOT_FOUND) {
        printf("Ricevuto risultato dal server ip %s. Città non disponibile\n", server_ip);
    } else if (resp->status == STATUS_INVALID_REQUEST) {
        printf("Ricevuto risultato dal server ip %s. Richiesta non valida\n", server_ip);
    } else {
        printf("Ricevuto risultato dal server ip %s. Risposta non valida\n", server_ip);
    }
}



int main(int argc, char *argv[]) {
    char server_addr[BUFFER_SIZE];
    int port;
    char type;
    char city[64];

    if (parse_arguments(argc, argv, server_addr, &port, &type, city, sizeof(city)) != 0) {

        return 1;
    }

#if defined WIN32

    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
        printf("Errore nell'inizializzazione di Winsock.\n");
        return 1;
    }
#endif

    int my_socket = connect_to_server(server_addr, port);
    if (my_socket < 0) {
        clearwinsock();
        return 1;
    }

    weather_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = type;
    strncpy(req.city, city, sizeof(req.city) - 1);
    req.city[sizeof(req.city) - 1] = '\0';

    if (send_weather_request(my_socket, &req) != 0) {
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    weather_response_t resp;
    if (recv_weather_response(my_socket, &resp) != 0) {
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    format_and_print_response(server_addr, &resp, city);

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
