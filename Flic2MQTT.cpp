/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#include <windows.h>
#include <winhttp.h>
#include <time.h>
#include <stdio.h>
#include "global.h"
#include "Config.h"
#include "PahoWrapper.h"

Config *myConfig=new Config();
PahoWrapper *myPaho;

DWORD firstTick;
DWORD continueTick;
DWORD availabilityTick;
FILE *logfile;
int epochNum=0;
int packetCount;
int packetCountEpoch;

void main() {
  int status;
  //
  // Load Config
  //
  fprintf(stderr, "Loading Config File\n");
  myConfig->readConfig("Flic2MQTT.config");
  logfile=myConfig->getLogfile();
  myPaho=new PahoWrapper(myConfig);
  myPaho->markAvailable(false);

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
    //
    // Fetch Access Token
    //
#ifdef DEBUG_PRINT_MAIN
    if(logfile) { 
      time_t clock;
      time(&clock);
      fprintf(logfile,"Epoch %d begins: %s\n", epochNum, asctime(localtime(&clock)));
      fprintf(logfile, "Retrieving Curb Access Token\n"); 
   }
#endif
    //
    // Create flicd connection
    //
#ifdef DEBUG_PRINT_MAIN
    if(logfile) { fprintf(logfile,"Creating flicd connection\n"); }
#endif
    //t.b.d.
    if(logfile) { fflush(logfile); }
    //status=myWS->looper(&handleUTF8);
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
    //myStateMan->unkState();

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

