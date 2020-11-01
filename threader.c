#include "threader.h"

//static so its not visible outside this file
static pthread_cond_t recieved = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

void init(Balancer* balancer,Parameters params) {
    const bool debug = true;
    // extra 1 for check thread
    pthread_t* threads = malloc((balancer->connections+1)*sizeof(pthread_t));
    balancer->thread = threads;
    for (int x = 0; x < balancer->connections; x++) {
        balancer->threadid = x;
        // create connection threads
        server* serv = malloc(sizeof(server));
        serv->port = params.servers[x];
        serv->id = x;
        serv->status = true;
        balancer->servers[x] = serv;
        if (debug == true ) {
            printf("server #%d\n",x);
            printf("server ID: %d\n",balancer->servers[x]->id);
            printf("server port at x %d: %d\n",x,balancer->servers[x]->port);
        }
        pthread_create((threads+x), NULL, connection, (void*) balancer);
    }
    // checker thread
    pthread_create(&balancer->thread[balancer->connections+1], NULL, update, (void*) balancer);
}

void* connection(void* balancer) {
    Balancer* controller = (Balancer*) balancer;
    const int id = controller->threadid;
    printf("thread %d online\n",id);
    request* newrequest;
    int rc = pthread_mutex_lock(&mutex);
    while (1) {
        if (controller->waiting > 0) {
            newrequest = getrequest(controller);
            if (newrequest->client_socket > 0) {
                printf("thread %d taking care of socket %ld\n",id,newrequest->client_socket);
                findserver(controller,newrequest,id);
            }
            free(newrequest);
        } else {
            printf("thread %d going to sleep\n",id);
            rc = pthread_cond_wait(&recieved, &mutex);
            printf("thread %d waking up\n",id);
        }
    }
}

void* update(void* balancer) {
    const bool debug = true;
    Balancer* controller = (Balancer*) balancer;
    const int id = controller->threadid;
    struct timespec time;
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    int seconds = 0;
    fd_set copy,ports;
    FD_ZERO(&ports);
    while (1) {
        // connects to the servers
        for (int x = 0; x < controller->connections;x++) {

            ssize_t result = client_connect(controller->servers[x]->port);
            if (result < 0) {
                printf("server %d is dead\n",x);
                printf("reason: failure to connect\n");
                controller->servers[x]->status = false;
                continue;
            }
            FD_SET(result,&ports);
            copy = ports;
            int sent = send(result,"GET /healthcheck HTTP/1.1\r\nContent-Length : 0\r\n\r\n",55,0);
            if (sent < 0) {
                printf("server %d is dead\n",x);
                printf("reason: failure of request to reach\n");
                controller->servers[x]->status = false;
                continue;
            } else {
                int tcount = select(result,&copy,NULL,NULL,&timeout);
                if (tcount <= 0) {
                    printf("server %d is dead\n",x);
                    printf("reason: failure to respond\n");
                    controller->servers[x]->status = false;
                    continue;
                } else {
                    updatehealth(result,controller,x);
                }
            }
        }
        // waits
        while (sem_timedwait( &controller->checker, &time ) == -1) {
            clock_gettime(CLOCK_REALTIME, &time);
            time.tv_sec += 1;
            seconds++;
            if (seconds==60) {
                seconds = 0;
                printf("checker going to sleep...\n");
                sem_post(&controller->checker);
            }
        }
    }
    return NULL;
}

void updatehealth(uint16_t socket, Balancer* balancer, const int index) {
    const bool debug = true;
    double fails,requests;
    char buffer[16384];
    int bytes = recv(socket,buffer,16384,0);
    printf("buffer gotten:\n");
    buffer[bytes+1] = "\0";
    printf("%s\n",buffer);
    char* temp;
    char* token = strtok_r(buffer,"\r\n\r\n",&temp);
    token = strtok_r(NULL,"\r\n\r\n",&temp);
    token = strtok_r(NULL,"\r\n\r\n",&temp);
    if (token != NULL) {
        fails = atoi(token);
		printf("fails: %lf\n",requests);
    } else {
		printf("server %d is dead\n",index);
        printf("reason: failure to send full healthcheck\n");
        balancer->servers[index]->status = false;
        return;
    }
    token = strtok_r(NULL,"\n",&temp);
    if (token != NULL) {
        requests = atoi(token);
		printf("requests: %lf\n",requests);
    } else {
        printf("server %d is dead\n",index);
        printf("reason: failure to send full healthcheck\n");
        balancer->servers[index]->status = false;
        return;
    }
    balancer->servers[index]->failure = (fails / requests);
    balancer->servers[index]->requests = requests+1;
    if (debug == true) {
        printf("updated health for server #%d\n",index);
        printf("health: %lf \n",balancer->servers[index]->failure);
    }
    balancer->servers[index]->status = true;
}

void addrequest(ssize_t client_socket, Balancer* balancer) {
    request* newrequest = malloc(sizeof(request));
    newrequest->client_socket = client_socket;
    newrequest->next = NULL;
    int rc = pthread_mutex_lock(&mutex);
    if (balancer->waiting == 0) {
        balancer->waitlist = newrequest;
        balancer->newest = newrequest;
    } else {
        balancer->newest->next = newrequest;
        balancer->newest = newrequest;
    }
    (balancer->waiting)++;
    rc = pthread_mutex_unlock(&mutex);
    rc = pthread_cond_signal(&recieved);
}

request* getrequest(Balancer* balancer) {
    request* newrequest;
    int rc = pthread_mutex_lock(&mutex);
    if (balancer->waiting > 0) {
        newrequest = balancer->waitlist;
        balancer->waitlist = newrequest->next;
        if (balancer->waitlist == NULL) { /* this was the last request on the list */
            balancer->newest = NULL;
        }
        /* decrease the total number of pending requests */
        balancer->waiting--;
    } else {
        newrequest = NULL;
    }
    rc = pthread_mutex_unlock(&mutex);
    return newrequest;
}

// finds the server best suited
void findserver(Balancer* balancer, request* newrequest, const int id) {
    int lowest = 0;
    for (int x = 0; x < balancer->connections; x++) {
    // checks if online
        if (balancer->servers[x]->status == true) {
            if (balancer->servers[x]->requests < balancer->servers[lowest]->requests ) {
                printf("server %d:\n",x);
                printf("server %d requests: %d\n",x ,balancer->servers[x]->requests);
                printf("server %d failure rate: %f\n", x, balancer->servers[x]->failure);
                printf("server %d:\n",lowest);
                printf("server %d requests: %d\n",lowest,balancer->servers[lowest]->requests);
                printf("server %d failure rate: %f\n",lowest,balancer->servers[lowest]->failure);
                printf("server %d wins\n",x);
                lowest = x;
            } else if ( balancer->servers[x]->requests == balancer->servers[lowest]->requests ){
				if (balancer->servers[x]->failure < balancer->servers[lowest]->failure) {
					lowest = x;
				}
				continue;
            }
        } else {
            printf("server %d is currently dead, finding other server\n",x);
        }
    }
    forward(balancer,newrequest,id,lowest);
}

// creates the connection
void forward(Balancer* balancer, request* newrequest, const int id, const int index) {
    ssize_t result = client_connect(balancer->servers[index]->port);
    if (result < 0) {
        // server is dead
        balancer->servers[index]->status = false;
        findserver(balancer,newrequest,id);
    }
    balancer->servers[index]->requests++;
    bridge(newrequest->client_socket,result);
}

// connects to server
ssize_t client_connect(uint16_t port) {
    struct sockaddr_in servaddr;
    ssize_t server_socket = socket(AF_INET,SOCK_STREAM,0);
    if (server_socket < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    /* For this assignment the IP address can be fixed */
    inet_pton(AF_INET,"127.0.0.1",&(servaddr.sin_addr));
    if(connect(server_socket,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
        return -1;
    return server_socket;
}

void bridge(ssize_t sockfd1,ssize_t sockfd2) {
    fd_set set;
    struct timeval timeout;
    int fromfd, tofd;
    while(1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO (&set);
        FD_SET (sockfd1, &set);
        FD_SET (sockfd2, &set);
        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        // select return the number of file descriptors ready for reading in set
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                printf("error during select, exiting\n");
                return;
            case 0:
                printf("both channels are idle, waiting again\n");
                continue;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    printf("this should be unreachable\n");
                    return;
                }
        }
        if (transferdata(fromfd, tofd) <= 0)
            return;
    }
}

int transferdata(ssize_t fromfd, ssize_t tofd) {
    char recvline[4096];
    int n = recv(fromfd, recvline, 4096, 0);
    if (n < 0) {
        printf("connection error receiving\n");
        return -1;
    } else if (n == 0) {
        printf("receiving connection ended\n");
        return 0;
    }
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        printf("connection error sending\n");
        return -1;
    } else if (n == 0) {
        printf("sending connection ended\n");
        return 0;
    }
    return n;
}