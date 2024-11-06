/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#include <windows.h>
#include <stdio.h>
#include "global.h"
#include "Config.h"

FILE       *Config::getLogfile()               { return logfile;             }
const char *Config::getMqttServer()            { return mqttServer;          }
const char *Config::getMqttTopicBase()         { return mqttTopicBase;       }
const char *Config::getFlicdServer()           { return flicdServer;         }
int         Config::getFlicdPort()             { return flicdPort;           }
const char *Config::getFlicName(int i)         { return flicName[i];         }
const char *Config::getFlicMac(int i)          { return flicMac[i];          }

Config::Config() { 
  logfile=0;
  logfileName=0;
  flicdServer=0;
  mqttServer=0;
  mqttTopicBase=0;
  flicdPort=0;
  for(int i=0;i<8;i++) { flicName[i]=0; flicMac[i]=0; }
}

//
// Read Flic2MQTT.config and populate settings
//
void Config::readConfig(const char *fname) {
  FILE *f;
  int i, ch;
  char buf[4096];
  char cc,*p,*q;

  if(logfile && logfile!=stderr && logfile!=stdout) { fclose(logfile); }
  logfile=0;
  if(logfileName)      { free(logfileName);      logfileName=0;      }
  if(mqttServer)       { free(mqttServer);       mqttServer=0;       }
  if(mqttTopicBase)    { free(mqttTopicBase);    mqttTopicBase=0;    }
  if(flicdServer)      { free(flicdServer);      flicdServer=0;      }
  flicdPort=5551;
  for(i=0;i<8;i++) {
    if(flicName[i]) { free(flicName[i]);  flicName[i]=0; }
    if(flicMac[i])  { free(flicMac[i]);   flicMac[i]=0;  }
  }

  f=fopen(fname,"r");
  if(!f) {
    fprintf(stderr,"Could not open Flic2MQTT.config\n");
    exit(1);
  }

  for(i=0;i<4095;i++) {
     ch=fgetc(f);
     if(ch==EOF) {
        buf[i]=0;
        i=4095;
     } else {
        buf[i]=(char)ch;
     }
  }
  fclose(f);
  if(ch!=EOF) {
    fprintf(stderr,"Excessively long Flic2MQTT.config\n");
    exit(1);
  }

  // hash out all line contents after a hashmark
  for(i=0;buf[i];i++) {
    if(buf[i]=='#') {
      int j;
      for(j=i+1;buf[j] && (buf[j]!='\r') && (buf[j]!='\n');j++) {
        buf[j]='#';
      }
      i=j-1;
    }
  }

  p=strstr(buf,"LOGFILE=");
  if(p) {
    for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
    cc=*q; 
    *q=0;
    logfileName=strdup(&p[8]);
    *q=cc;
    if(!strcmp(logfileName,"stderr")) {
      logfile=stderr;
    } else if(!strcmp(logfileName,"stdout")) {
      logfile=stdout;
    } else {
      //
      //  Cycle the old log files
      //
      char from[1024], to[1024];
      for(i=8;i>=0;i--) {
        if(i) { sprintf(from,"%s.%d",logfileName,i); } else { sprintf(from,"%s",logfileName); }
        sprintf(to,"%s.%d",logfileName,i+1);
        remove(to);
        rename(from,to);
      }
      //
      // open the new log file
      //
      logfile=fopen(logfileName,"w");
    }
  }

  p=strstr(buf,"MQTT_SERVER=");
  for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
  cc=*q;
  *q=0;
  mqttServer=strdup(&p[12]);
  *q=cc;

  p=strstr(buf,"MQTT_TOPIC_BASE=");
  for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
  cc=*q;
  *q=0;
  mqttTopicBase=strdup(&p[16]);
  *q=cc;

  p=strstr(buf,"FLICD_SERVER=");
  for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
  cc=*q;
  *q=0;
  flicdServer=strdup(&p[13]);
  *q=cc;

  p=strstr(buf,"FLICD_PORT=");
  for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
  cc=*q;
  *q=0;
  flicdPort=atoi(&p[11]);
  *q=cc;

  char chartmp[24];
  for(i=0;i<8;i++) {
    sprintf(chartmp,"FLIC_NAME_%02d=",i);
    p=strstr(buf,chartmp);
    if(p) {
      for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
      cc=*q;
      *q=0;
      flicName[i]=strdup(&p[strlen(chartmp)]);
      *q=cc;
    }
    sprintf(chartmp,"FLIC_MAC_%02d=",i);
    p=strstr(buf,chartmp);
    if(p) {
      for(q=p;(*q) && (*q!='\r') && (*q!='\n');) { q=q+1; }  // find end of config parameter
      cc=*q;
      *q=0;
      flicMac[i]=strdup(&p[strlen(chartmp)]);
      *q=cc;
    }
  }

#ifdef DEBUG_PRINT_CONFIG
  if(logfile) { 
    fprintf(logfile,"LOGFILE=%s\n",logfileName);
    fprintf(logfile,"MQTT_SERVER=%s\n",mqttServer);
    fprintf(logfile,"MQTT_TOPIC_BASE=%s\n",mqttTopicBase);
    fprintf(logfile,"FLICD_SERVER=%s\n",flicdServer);
    fprintf(logfile,"FLICD_PORT=%d\n",flicdPort);
    for(i=0;i<8;i++) {
      fprintf(logfile,"FLIC_NAME_%02d=%s\n",i,flicName[i]);
      fprintf(logfile,"FLIC_MAC_%02d=%s\n",i,flicMac[i]);
    }
  }
#endif
}

