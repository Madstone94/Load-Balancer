#include<err.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include "Parameters.h"
#include "threader.h"

// prototypes
void create_server(Parameters params);
void create_socket(struct sockaddr_in server_address, socklen_t addresslength, Parameters params);
void connectclient(ssize_t server_socket, const Parameters params);

int main(int argc,char **argv) {
    if (argc < 1) {
        printf("missing arguments\n");
        exit(EXIT_FAILURE);
    } else {
        Parameters params = newparams(argc,argv,"HTTP/1.1");
        create_server(params);
    }
}

void create_server(Parameters params) {
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(params.port);
    socklen_t addresslength = sizeof(server_address);
	create_socket(server_address,addresslength,params);
}

void create_socket(struct sockaddr_in server_address, socklen_t addresslength, Parameters params) {
    ssize_t server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        exit(EXIT_FAILURE);
    }
    int enable = 1;
    int ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    ret = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if (ret < 0) {
        exit(EXIT_FAILURE);
    }
    ret = listen(server_socket,500);
    if (ret < 0) {
        exit(EXIT_FAILURE);
    }
	connectclient(server_socket,params);
}

void connectclient(ssize_t server_socket, const Parameters params) {
    Balancer* balancer = malloc(sizeof(Balancer) + sizeof(params.total*sizeof(int)));
    balancer->connections = params.total;
    balancer->max = params.maxrequest;
    sem_init(&balancer->checker,0,1); 
    init(balancer,params);
    while (1) {
        printf("[+] server is waiting...\n");
        struct sockaddr client_address ;
        socklen_t client_addresslength = sizeof(client_address);
        printf("SERVER:%ld\n",server_socket);
        int client_socket = accept(server_socket, &client_address, &client_addresslength);
        printf("new request: %ld\n",client_socket);
        
        addrequest(client_socket,balancer);
        balancer->current++;
        if (balancer->current >= balancer->max) {
            // send signal to updater
            sem_post(&balancer->checker);
            balancer->current = 0;
        }
    }
}