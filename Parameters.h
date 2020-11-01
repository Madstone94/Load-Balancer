#ifndef parameters_H_
#define parameters_H_
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
// exists because single file was too cumbersome
// easier than passing a bunch of variables
typedef struct Params {
    int threads;
    uint16_t port;
    int maxrequest;
    // amount of servers and their ports
    int total;
    uint16_t* servers;
}Parameters;
// constructor
Parameters newparams(int argc, char** argv,const char* version);
int findport(int argc,char**argv);
int findservers(int argc,char**argv);
bool checknumber(char* number);
bool isflag(char* argument);
#endif