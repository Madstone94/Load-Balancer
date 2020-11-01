#include "Parameters.h"

// constructor
Parameters newparams(int argc, char** argv,const char* version) {
    const bool debug = true;
    Parameters params = {
        .threads = 4,
        .port = findport(argc,argv),
        .maxrequest = 5,
        .total = findservers(argc,argv)
    };
    params.servers = malloc(params.total*sizeof(int));
    int flag;
    while ( (flag = getopt(argc, argv,"N:R:") ) > -1) {
        switch (flag) {
            case 'N':
                if (checknumber(optarg) == true) {
                    params.threads = atoi(optarg);
                    if (params.threads <= 0) {
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            case 'R':
                params.maxrequest = atoi(optarg);
                if (params.maxrequest <= 0) {
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':
            case '?':
                break;
        }
    }
    int place = 0;
    for (int x = 1; x < argc; x++) {
        if ( (isflag(argv[x-1]) == true) || (isflag(argv[x]) == true) ) {
            continue;
        } else {
            if (debug == true) {
                printf("server port: %d\n",atoi(argv[x]));
            }
            uint16_t port = atoi(argv[x]);
            if (port != params.port) {
                (*(params.servers + place)) = atoi(argv[x]);
                place++;
            } else {
                continue;
            }
        }
    }
    if (debug == true) {
        for (int y = 0; y < params.total; y++) {
            printf("servers foud:\n");
            printf("%d\n",params.servers[y]);
        }
    }
    return params;
}

int findservers(int argc,char**argv) {
    int servers = 0;
    for (int x = 1; x < argc; x++) {
        if ( (isflag(argv[x-1]) == true) || (isflag(argv[x]) == true) ) {
            continue;
        } else {
            servers++;
        }
    }
    // account for home port
    servers--;
    return servers;
}

int findport(int argc,char**argv) {
    int port = -1;
    for (int x = 1; x < argc; x++) {
        if ( (isflag(argv[x-1]) == true) || (isflag(argv[x]) == true) ) {
            continue;
        } else {
            port = atoi(argv[x]);
            return port;
        }
    }
    return port;
}

bool checknumber(char* number) {
    const bool debug = false;
    if (debug == true) {
        printf("Value: ");
    }
    if (strspn(number,"0123456789") == strlen(number)) {
        if (debug == true) {
            printf("Valid\n");
        }
        return true;
    } else {
        if (debug == true) {
            printf("Invalid\n");
        }
        return false;
    }
}

bool isflag(char* argument) {
    const bool debug = false;
    if (debug == true) {
        printf("flag: ");
    }
    if (strspn(argument,"-") > 0) {
        if (debug == true) {
            printf("true\n");
        }
        return true;
    } else {
        if (debug == true) {
            printf("false\n");
        }
        return false;
    }
}