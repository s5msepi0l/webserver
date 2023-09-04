#ifndef WEBSERVER_NETWORKING_CORE
#define WEBSERVER_NETWORKING_CORE

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#endif

#if defined(__linux__)
#define PLATFORM_LINUX
#endif

#ifdef PLATFORM_LINUX

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct {
    socklen_t len;
    struct sockaddr_in addr;
}socket_address;

typedef unsigned long int socket_fd;

socket_fd n_socket();

#endif

#endif