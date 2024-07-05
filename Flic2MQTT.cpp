/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdio.h>
#include "global.h"
#include "Config.h"
#include "PahoWrapper.h"
#include "flicd_client.h"

Config *myConfig=new Config();
PahoWrapper *myPaho=0;
int thePipeR=0,thePipeW=0;

DWORD firstTick;
DWORD continueTick;
DWORD availabilityTick;
FILE *logfile;
int epochNum=0;
int packetCount;
int packetCountEpoch;

extern int flicd_client_main(int argc, char *argv[]);
extern int flicd_client_init(const char *server, int port);
extern int flicd_client_handle_line(int sockfd, const char *incmd);

void Usage() {
  fprintf(stderr,"flic2MQTT                               # this message\n");
  fprintf(stderr,"flic2MQTT -interact host [port]         # run a flic simpleclient to host[port]\n");
  fprintf(stderr,"flic2MQTT -mqtt                         # run in mqtt mode with settings from Flic2MQTT.config\n");
}

static int winsock_init() {
  int ret;
  WSADATA wsaData;
  ret = WSAStartup(MAKEWORD(2,2), &wsaData);
  if(ret != 0) {
    printf("WSAStartup failed with error: %d\n", ret);
    return 1;
  }
  return 0;
}

static void winsock_cleanup() {
  WSACleanup();
}

int main(int argc, char *argv[]) {
  int status;
  int pipes[2];

  winsock_init();

  if(argc>1 && !strcmp(argv[1],"-interact")) {
    //
    // for interactive mode pass remainder of command line to the flicd simple client
    //
    flicd_client_main(argc-1,&argv[1]);
    return(0);
  }
  if(argc==2 && !strcmp(argv[1],"-mqtt")) {
    fprintf(stderr,"MQTT mode.  lets go!\n");
  } else {
    Usage();
    return(0);
  }

  status=_pipe(pipes,1024,O_BINARY);
  if(status==-1) {
    fprintf(stderr, "Error creating pipes\n");
    return -1;
  }
  thePipeR=pipes[0];
  thePipeW=pipes[1];

  //
  // Load Config
  //
  fprintf(stderr, "Loading Config File\n");
  myConfig->readConfig("Flic2MQTT.config");
  logfile=myConfig->getLogfile();

  //
  // Initialize Flic
  //
  int sockfd=flicd_client_init(myConfig->getFlicdServer(), myConfig->getFlicdPort());
  if(sockfd<0) {
    if(logfile) { fprintf(logfile,"Error connecting to flicd server\n"); }
    return -1;
  }

  //
  // Initialize MQTT
  //
  PahoWrapper *pt=new PahoWrapper(myConfig);
  pt->markAvailable(false);
  myPaho=pt;

  status=flicd_client_handle_line(sockfd, "getInfo");
  fprintf(stderr,"handle_line status=%d\n",status);
  //
  // Loop as long as the flicd returns a normal condition.
  //
  for(status=1000;status==1000;) {
    //
    // If Paho connection is down, reconnect it.
    //
    if(!myPaho->isUp()) { myPaho->reconnect(); }

    firstTick=continueTick=availabilityTick=0;
    epochNum++;
    packetCount=packetCountEpoch=0;
#ifdef DEBUG_PRINT_MAIN
    if(logfile) { 
      time_t clock;
      time(&clock);
      fprintf(logfile,"Epoch %d begins: %s\n", epochNum, asctime(localtime(&clock)));
   }
#endif
    char piper[32];
    _read(thePipeR,piper,32);
    fprintf(stderr,"piper got %d %d %d %s\n",piper[0],piper[1],piper[2],&piper[3]);
    //
    // Why did looper end?
    //
    DWORD tick=GetTickCount();
    if(!firstTick) { firstTick=tick; }
#ifdef DEBUG_PRINT_MAIN
    if(logfile) { 
      int h, m, s, frac;
      frac=(tick-firstTick);
      h=frac/(1000*60*60); frac=frac%(1000*60*60);
      m=frac/(1000*60);    frac=frac%(1000*60);
      s=frac/(1000);       frac=frac%(1000);
      frac=frac/100;
      fprintf(logfile, "Looper ended with status %d (normal=1000) Epoch %d after epochTime=%d:%02d:%02d.%01d packets=%d\n",status,epochNum,h,m,s,frac,packetCountEpoch);
      fprintf(logfile, "tick=%ul firstTick=%ul\n",tick,firstTick);
      fflush(logfile); 
    }
#endif
    char *msg=0;
    int wait=0;
    //
    // mark to MQTT server that we are offline and mark locally that device states are unknown to resend when we come back
    //
    myPaho->markAvailable(false);

    // If we had an expected exit note it and restart after a bit.
    if(status==1000) { 
      wait=3; 
      msg="NORMAL_CLOSURE.  Start over in 3 minutes"; 
    } else if(status==12002) { 
      wait=5; 
      msg="ERROR_WINHTTP_TIMEOUT.  Try again in 5 mins";
      status=1000;
    } else if(status==12007) { 
      wait=5; 
      msg="ERROR_WINHTTP_NAME_NOT_RESOLVED.  Try again in 5 mins";
      status=1000;
    } else if(status==12030) { 
      wait=5; 
      msg="ERROR_WINHTTP_CONNECTION_ERROR.  Try again in 5 mins";
      status=1000;
    } else if(status==711711) {
      wait=1; 
      msg="ERROR_PAHO_DOWN.  Try again in 1 min";
      status=1000;
    }
#ifdef DEBUG_PRINT_MAIN
    if(logfile && msg) { fprintf(logfile, "%s\n",msg); }
#endif
    Sleep(wait*60*1000); 
  }
  if(logfile) { fflush(logfile); } // should be in Config.cpp destructor?
}

