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
#include <assert.h>

Config *myConfig=new Config();
PahoWrapper *myPaho=0;
int thePipeR=0,thePipeW=0;

DWORD firstTick;         //  When did we start?
DWORD epochTick;         //  When did this epoch start?
DWORD availabilityTick;  //  When should we refresh availablity on MQTT server?
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

void timeFill(char *buf) {
  time_t clock;
  time(&clock);
  sprintf(buf,"%s", asctime(localtime(&clock)));
  buf[24]=0;
}

int looper(DWORD gotill) {
  char piper[32];
  int ret;
  int flicOp;
  int flicStat;
  int flicButt;
  const char *flicMsg=&piper[3];
  char timeStr[64];
  int butt_held[8];
  for(int i=0;i<8;i++) { butt_held[i]=0; }

  for(;;) {
    //
    // Check for exit loop condition
    //
    DWORD tick=GetTickCount();
    if(tick>gotill) { return 1000; }

    ret=_read(thePipeR,piper,32);
    assert(ret==32);

    flicOp=piper[0];
    flicStat=piper[1];
    flicButt=piper[2];

#ifdef DEBUG_PRINT_MAIN
    fprintf(logfile,"piper got %s %d %d %s\n",FLIC_OPS[flicOp],flicStat,flicButt,flicMsg);
#endif
    if(flicOp==FLIC_PING) {
    } else if(flicOp==FLIC_INFO_GENERAL) {
      myPaho->markAvailable(true);
    } else if(flicOp==FLIC_CONNECT) {
    } else if(flicOp==FLIC_STATUS) {
    } else if(flicOp==FLIC_UPDOWN) {
      if(flicStat==FLIC_STATUS_DOWN) { 
        //
        // We started pressing down
        //
        myPaho->writeState(flicButt, BUTT_STATE, "On"); 
        butt_held[flicButt]=0;  // if we are just starting to press, we were not held down
      } else if(flicStat==FLIC_STATUS_UP) {
        //
        // We stopped pressing down
        //
        myPaho->writeState(flicButt, BUTT_STATE, "Off"); 
      } else if(flicStat==FLIC_STATUS_HOLD) {
        //
        // We are holding it down
        //
        timeFill(timeStr);
        butt_held[flicButt]=1;
      } else if(flicStat==FLIC_STATUS_SINGLECLICK) {
        //
        // Flic detected a single click completion.  Send a click or hold event
        //
        timeFill(timeStr);
        if(butt_held[flicButt]) {
          myPaho->writeState(flicButt, BUTT_HOLD, timeStr); 
        } else {
          myPaho->writeState(flicButt, BUTT_CLICK, timeStr); 
        }
      } else if(flicStat==FLIC_STATUS_DOUBLECLICK) {
        //
        // Flic detected a double click completion.  Send a clickclick or clickhold event
        //
        timeFill(timeStr);
        if(butt_held[flicButt]) {
          myPaho->writeState(flicButt, BUTT_CLICKHOLD, timeStr); 
        } else {
          myPaho->writeState(flicButt, BUTT_CLICKCLICK, timeStr); 
        }
      }
    } else {
#ifdef DEBUG_PRINT_MAIN
      fprintf(logfile,"piper got UNKNOWN %s %d %d %s\n",FLIC_OPS[flicOp],flicStat,flicButt,flicMsg);
#endif
    }
    packetCountEpoch++;
  }
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

  // 
  // Kick off with info request and register for desired buttons
  //
  status=flicd_client_handle_line(sockfd, "getInfo");
#ifdef DEBUG_PRINT_MAIN
  fprintf(logfile,"main->flicd: getInfo : status=%d\n",status);
#endif

  for(int i=0;i<8;i++) {
    const char *mac=myConfig->getFlicMac(i);
    if(mac) {
      char cmd[32];
      sprintf(cmd,"connect %s %d",mac,i);
      status=flicd_client_handle_line(sockfd, cmd);
#ifdef DEBUG_PRINT_MAIN
      fprintf(logfile,"main->flicd: %s : status=%d\n",cmd,status);
#endif
    }
  }

  firstTick=epochTick=availabilityTick=0;
  packetCount=packetCountEpoch=0;
  epochNum=0;

  //
  // Loop as long as the flicd returns a normal condition.
  //
  for(status=1000;status==1000;) {
    //
    // If Paho connection is down, reconnect it.
    //
    if(!myPaho->isUp()) { myPaho->reconnect(); }

    epochNum++;
    packetCountEpoch=0;
#ifdef DEBUG_PRINT_MAIN
    if(logfile) { 
      time_t clock;
      time(&clock);
      fprintf(logfile,"Epoch %d begins: %s\n", epochNum, asctime(localtime(&clock)));
   }
#endif
    epochTick=GetTickCount();
    availabilityTick=epochTick+1000*60*60;   // one hour
    if(!firstTick) { firstTick=epochTick; }

    status=looper(availabilityTick);

    DWORD tick=GetTickCount();

#ifdef DEBUG_PRINT_MAIN
    if(logfile) { 
      int h, m, s, frac;
      frac=(tick-epochTick);
      h=frac/(1000*60*60); frac=frac%(1000*60*60);
      m=frac/(1000*60);    frac=frac%(1000*60);
      s=frac/(1000);       frac=frac%(1000);
      frac=frac/100;
      fprintf(logfile, "Looper ended with status %d (normal=1000) Epoch %d after epochTime=%d:%02d:%02d.%01d packets=%d\n",status,epochNum,h,m,s,frac,packetCountEpoch);
      fprintf(logfile, "firstTick=%ul epochTick=%ul tick=%ul\n",firstTick,epochTick,tick);
      fflush(logfile); 
    }
#endif

    //
    // Why did looper end?
    //
    //
    // Expected availiability deadline timeout (1000)
    //
    char msg[1024];
    if(status==1000) {
      flicd_client_handle_line(sockfd, "getInfo");
      sprintf(msg,"EPOCH %d - LOOPER END - 1000 - NORMAL LOOPER TIMEOUT TO REFRESH AVAILABILITY.",epochNum);
    } else {
      sprintf(msg,"EPOCH %d - LOOPER END - %d - UNEXPECTED RETURN.  DIE!!!!",epochNum,status);
    }
#ifdef DEBUG_PRINT_MAIN
    if(logfile && msg) { fprintf(logfile, "%s\n",msg); }
#endif
  }
  if(logfile) { fflush(logfile); } // should be in Config.cpp destructor?
}

