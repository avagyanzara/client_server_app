#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define ISVALIDSOCKET(s)  ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

static unsigned int CONNECTION_COUNT = 0;
#define MAX_CONNECTION_COUNT 5

/// Utility functions
void execute_shell_command(const char* str, char* res);
void send_shell_cmd_result(char* read, SOCKET socket, const char* shell_str);
char* copy_string(const char* str);
void print_address(const char* msg, struct sockaddr_storage* client_address);

/// TCP server utilities
SOCKET create_socket(struct addrinfo* bind_address);
void configure_local_address(struct addrinfo** bind_address);
void listen_socket(SOCKET socket_listen);
void accept_connections(SOCKET socket_listen);
int execute_command(SOCKET socket, fd_set* master);
SOCKET accept_client(struct sockaddr_storage* client_address, SOCKET socket_listen);
void close_server_socket(SOCKET socket, fd_set* master);
void send_message(SOCKET socket, const char* message);
int receive_message(SOCKET socket, char* read);

int main() 
{    
    struct addrinfo* bind_address;
    configure_local_address(&bind_address);
    SOCKET socket_listen = create_socket(bind_address);
    listen_socket(socket_listen);
    accept_connections(socket_listen);

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);
    return 0;
}

void execute_shell_command(const char* str, char* res)
{
    FILE* pipein;
    if((pipein = popen(str, "r")) == NULL) {
        fprintf(stderr, "popen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    fgets(res, 2048, pipein);
    res[2047] = '\0';
}

int execute_command(SOCKET socket, fd_set* master)
{
    char read[4096];
    int rc = receive_message(socket, read);
    if(rc != 0) {
        close_server_socket(socket, master);
        return rc;
    }

    if(strcasecmp(read, "disconnect") == 0) {
        close_server_socket(socket, master);
        return 0;
    }

    const char* shell_str = "shell ";
    if(strncasecmp(read, shell_str, strlen(shell_str)) == 0) {
        send_shell_cmd_result(read, socket, shell_str);
    }
    return 0;
}

char* copy_string(const char* str)
{
    const unsigned int len = strlen(str);
    char* res = (char*)malloc(len + 1);
    if(res == NULL) {
        fprintf(stderr, "Cannot allocate memory.\n");
        exit(1);
    }
    strcpy(res, str);
    res[len] = '\0';    
    return res;
}

void send_message(SOCKET socket, const char* message)
{
    unsigned int len = strlen(message);
    unsigned int bytes_sent = send(socket, &len, sizeof(len), 0);
    if(bytes_sent <= 0) {
        fprintf(stderr, "send() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    bytes_sent = send(socket, message, len, 0);
    if(bytes_sent < 1) {
        fprintf(stderr, "send() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    //printf("Send %d bytes.\n", bytes_sent);
}

int receive_message(SOCKET socket, char* read)
{
    unsigned int len = 0;
    int bytes_received = recv(socket, &len, sizeof(len), 0);
    if(bytes_received < 1) {
        fprintf(stderr, "ERROR: Connection closed by peer.\n");
        return -1;
    }

    bytes_received = recv(socket, read, len, 0);
    if(bytes_received < 1) {
        fprintf(stderr, "ERROR: Connection closed by peer.\n");
        return -1;
    }

    read[len] = '\0';
    return 0;
}

void close_server_socket(SOCKET socket, fd_set* master)
{
    //printf("Closing connection...\n");
    if(master != NULL) {
        FD_CLR(socket, master);
    }
    CLOSESOCKET(socket);
    --CONNECTION_COUNT;
}

void configure_local_address(struct addrinfo** bind_address)
{
    //printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(0, "1234", &hints, bind_address);
}

SOCKET create_socket(struct addrinfo* bind_address)
{
    //printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    
    if(bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    freeaddrinfo(bind_address);
    return socket_listen;
}

void listen_socket(SOCKET socket_listen)
{
    //printf("Listening...\n");
    if(listen(socket_listen, 1024) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
}

SOCKET accept_client(struct sockaddr_storage* client_address, SOCKET socket_listen)
{
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client = accept(socket_listen, (struct sockaddr*)client_address, &client_len);
    if(CONNECTION_COUNT >= MAX_CONNECTION_COUNT) {
        const char* msg = "Unable to establist a connection.";
        send_message(socket_client, msg);
        CLOSESOCKET(socket_client);
        return socket_client;
    }
    ++CONNECTION_COUNT;
    return socket_client;    
}

void print_address(const char* msg, struct sockaddr_storage* client_address)
{
    char address_buffer[100];
    getnameinfo((struct sockaddr*) client_address, sizeof(*client_address), address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s %s\n", msg, address_buffer);    
}

void send_shell_cmd_result(char* read, SOCKET socket, const char* shell_str)
{
    const char* cmd = copy_string(read+strlen(shell_str));
    char output[2048];
    execute_shell_command(cmd, output);
    //printf("Execute shell command: %s\n", output);
    send_message(socket, output);
}

void accept_connections(SOCKET socket_listen)
{
    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while(1) {
        fd_set reads;
        reads = master;
        if(select(max_socket+1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            exit(1);
        }
        
        for(SOCKET socket = 1; socket <= max_socket; ++socket) {
            if(!FD_ISSET(socket, &reads)) continue;
            if(socket == socket_listen) {
                struct sockaddr_storage client_address;
                SOCKET socket_client = accept_client(&client_address, socket_listen);
                if(!ISVALIDSOCKET(socket_client)) {
                    fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());

                    continue;
                }
                
                print_address("New connection from ", &client_address);
                FD_SET(socket_client, &master);
                if(socket_client > max_socket) {
                    max_socket = socket_client;
                }
            } else {
                execute_command(socket, &master);
            }
        }
    }
}
