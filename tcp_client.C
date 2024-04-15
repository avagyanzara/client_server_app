#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ISVALIDSOCKET(s)  ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

#define DEFAULT_COMMAND       0
#define CONNECT_COMMAND       1
#define DISCONNECT_COMMAND    2
#define EXECUTE_SHELL_COMMAND 3

struct Command
{
    unsigned int type;
    char* ip_address;
    char* port;
    char* shell_command;
    SOCKET socket;
};

/// Functionality 
void get_input(char* line);
void init_command(struct Command* command); 
char* copy_string(const char* str);
void parse(char* line, struct Command* command);
void configure_remote_address(struct Command* command, struct addrinfo** peer_address);
SOCKET create_socket(struct addrinfo* peer_address);
void connect(SOCKET socket_peer, struct addrinfo** peer_address);
void execute_connect(struct Command* command);
void send_message(SOCKET socket, const char* message);
void receive_message(SOCKET socket, char* read);
void execute_shell(struct Command* command);
void execute_disconnect(struct Command* command);
void execute(struct Command* command);
void print(struct Command* command);

int main()
{
    struct Command command;
    init_command(&command);

    while(1) {
        printf("Client> ");
        char line[1024];
        get_input(line);
        parse(line, &command);
        execute(&command);
    }
    return 0;
}

void get_input(char* line)
{
    memset(line, '\0', 1024);
    fgets(line, 1024, stdin);
    char* res = strchr(line, '\n');
    if(res == NULL) {
        line[1023] = '\0';
    } else {
        *res = '\0';
    }
}

void init_command(struct Command* command) 
{
    command->type  = DEFAULT_COMMAND;
    command->ip_address    = NULL;
    command->port          = NULL;
    command->shell_command = NULL;
    command->socket        = -1;
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

void parse(char* line, struct Command* command)
{
    const char* connect_str    = "connect";
    const char* disconnect_str = "disconnect";
    const char* shell_str      = "shell";

    if(strncasecmp(line, connect_str, strlen(connect_str)) == 0) {
        if(command->ip_address != NULL) {
            printf("Connection is already established\n");
            return;
        }
        command->type = CONNECT_COMMAND;
        char* word = strtok(line, " ");
        unsigned int index = 1;

        while(word != NULL && index < 4) {
            if(index == 2) {
                command->ip_address = copy_string(word);
            } else if(index == 3) {
                command->port = copy_string(word);
            } 
            word = strtok(NULL, " ");
            ++index;
        }

        if(word != NULL) {
            fprintf(stderr, "Command is not correct - connect <IP_ADDRESS> <PORT>\n");
            exit(1);
        }
    } else if(strcasecmp(line, disconnect_str) == 0) {
        command->type = DISCONNECT_COMMAND;
    } else if(strncasecmp(line, shell_str, strlen(shell_str)) == 0) {
        command->type = EXECUTE_SHELL_COMMAND;
        if(strlen(line) == strlen(shell_str)) {
            fprintf(stderr, "Command is not correct - shell <command>\n");
            exit(1);
        }
        command->shell_command = copy_string(line);
    } else {
        fprintf(stderr, "Command is not correct - connect | disconnect | shell\n");
        exit(1);
    }
}

void configure_remote_address(struct Command* command, struct addrinfo** peer_address)
{
    //printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(command->ip_address, command->port, &hints, peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
	exit(1);
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo((*peer_address)->ai_addr, (*peer_address)->ai_addrlen, address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer), NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);
}

SOCKET create_socket(struct addrinfo* peer_address)
{
    SOCKET socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    return socket_peer;
}

void connect(SOCKET socket_peer, struct addrinfo** peer_address)
{
    if(connect(socket_peer, (*peer_address)->ai_addr, (*peer_address)->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(*peer_address);
    printf("Connected.\nTo send data, enter text followed by enter.\n");
}

void execute_connect(struct Command* command)
{
    struct addrinfo* peer_address;
    configure_remote_address(command, &peer_address);
    command->socket = create_socket(peer_address);
    connect(command->socket, &peer_address);
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
    if(bytes_sent <= 0) {
        fprintf(stderr, "send() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
}

void receive_message(SOCKET socket, char* read)
{
    unsigned int len = 0;
    int bytes_received = recv(socket, &len, sizeof(len), 0);
    if(bytes_received < 1) {
        fprintf(stderr, "ERROR: Connection closed by server.\n");
        exit(1);
    }

    bytes_received = recv(socket, read, len, 0);
    read[len] = '\0';

    if(bytes_received < 1) {
        fprintf(stderr, "ERROR: Connection closed by peer.\n");
        exit(1);
    }
}

void execute_shell(struct Command* command)
{
    SOCKET socket_peer = command->socket;
    const char* shell_command = command->shell_command;
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    fd_set writes;
    FD_ZERO(&writes);
    FD_SET(socket_peer, &writes);
 
    if(select(socket_peer+1, 0, &writes, 0, &timeout) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    if(FD_ISSET(socket_peer, &writes)) {
        send_message(socket_peer, shell_command);
        
        char read[4096];
        receive_message(socket_peer, read);
        printf("Result: %s", read);
    }

}

void execute_disconnect(struct Command* command)
{
    SOCKET socket_peer = command->socket;

    if(socket_peer == -1) {
        fprintf(stderr, "connection is not established yet.\n");
        exit(1);
    }
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    fd_set writes;
    FD_ZERO(&writes);
    FD_SET(socket_peer, &writes);
 
    if(select(socket_peer+1, 0, &writes, 0, &timeout) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    if(FD_ISSET(socket_peer, &writes)) {
        send_message(socket_peer, "disconnect");
    }

    printf("Disconnect...\n");
    FD_CLR(socket_peer, &writes);
    CLOSESOCKET(socket_peer);
    exit(0);
}

void execute(struct Command* command)
{
    assert(command != NULL && "command cannot be NULL");
    switch(command->type) {
    case CONNECT_COMMAND:
	execute_connect(command);
        break;
    case DISCONNECT_COMMAND:
	execute_disconnect(command);
        break;
    case EXECUTE_SHELL_COMMAND:
	execute_shell(command);
        break;
    default:
	assert(false && "invalid command type");
        break;
    }
}

void print(struct Command* command)
{
    printf("Command: ");
    switch(command->type) {
    case CONNECT_COMMAND:
        printf("connect %s %s\n", command->ip_address, command->port);
        break;
    case DISCONNECT_COMMAND:
        printf("disconnect\n");
        break;
    case EXECUTE_SHELL_COMMAND:
        printf("%s\n", command->shell_command);
        break;
    default:
        printf("incorrect\n");
        break;
    }
}
