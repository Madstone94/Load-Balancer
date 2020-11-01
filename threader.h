#ifndef HTTPThreader_H_
#define HTTPThreader_H_
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include "Parameters.h"
#include <unistd.h>
#include <stdatomic.h>
#include <time.h> 
#include <semaphore.h>
#include <pthread.h>
#include <err.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// from kent state example, superior to my version
typedef struct request {
    ssize_t client_socket;
    struct request* next;
}request;

// server meta data
typedef struct server {
    // ID so it can find itself in balancer array
    int id;
    // port number
    uint16_t port;
    // expected load between checks
    int requests;
    // actual reported health of the server
    double failure;
    // if its reachable
    bool status;
}server;

typedef struct balancerObj {
    // time keeper
    time_t time;
    sem_t checker;
    // temp for thread id
    int threadid;
    // total connections of servers
    int connections;
    // max amount of requests before refresh
    int max;
    // current requests parsed
    int current;
    // current number
    int waiting;
    request* waitlist;
    request* newest;
    // only 1 flexible array necessary and this is kinda a useless array compared to servers
    pthread_t* thread;
    // flexible array of servers + metadata
    server* servers[];
}Balancer;

void init(Balancer* balancer,Parameters params);

// API
void addrequest(ssize_t client_socket, Balancer* balancer);
request* getrequest(Balancer* balancer);
// threads
void* connection(void* balancer);
void* update(void* balancer);
void bridge(ssize_t client_socket,ssize_t server_socket);
void updatehealth(uint16_t socket, Balancer* balancer, const int index);
// IO
void findserver(Balancer* balancer, request* newrequest, const int id);
void forward(Balancer* balancer, request* newrequest, const int id, const int index);
ssize_t client_connect(uint16_t port);
int transferdata(ssize_t fromfd, ssize_t tofd);
#endif