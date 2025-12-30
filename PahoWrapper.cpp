/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef __LINUX__
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#define LONG long
#define Sleep(xxx) usleep(xxx*1000)
#else
#include <windows.h>
#include <winhttp.h>
#endif
#include <stdio.h>
#include <MQTTAsync.h>
#include "global.h"
#include "Config.h"
#include "PahoWrapper.h"

extern "C" {
  //
  // Paho C bridging back to C++
  //
  static void _pahoOnConnLost(void *context, char *cause)                           { ((PahoWrapper*)(context))->pahoOnConnLost(cause);          }
  static void _pahoOnConnectFailure(void* context, MQTTAsync_failureData* response) { ((PahoWrapper*)(context))->pahoOnConnectFailure(response); }
  static void _pahoOnSendFailure(void* context, MQTTAsync_failureData* response)    { ((PahoWrapper*)(context))->pahoOnSendFailure(response);    }
  static void _pahoOnConnect(void* context, MQTTAsync_successData* response)        { ((PahoWrapper*)(context))->pahoOnConnect(response);        }
  static void _pahoOnSend(void* context, MQTTAsync_successData* response)           { ((PahoWrapper*)(context))->pahoOnSend(response);           }
}

LONG PahoWrapper::getOutstanding() { return pahoOutstanding; }
bool PahoWrapper::isUp()           { return pahoUp;          }

PahoWrapper::PahoWrapper(Config *config) {
  pahoClient=0;
  pahoOutstanding=0;
  pahoUp=false;
  logfile=config->getLogfile();
  mqttServer=config->getMqttServer();
#ifdef DEBUG_PRINT_MQTT
  if(logfile) { fprintf(logfile,"MQTT Server: %s\n",mqttServer); }
#endif

  //
  // Load enough config info to know the LWT
  //
  const char *base=config->getMqttTopicBase();
  topicLWT=(char*)malloc(strlen(base)+20);
  sprintf(topicLWT,"%s/LWT",base);
#ifdef DEBUG_PRINT_MQTT
  if(logfile) { fprintf(logfile,"LWT=%s\n",topicLWT); }
#endif

  //
  // connect
  //
  reconnect();
  //
  for(int i=0;i<8;i++) {
    const char *name=config->getFlicName(i);
    //
    if(name) {
      topicState[i]=(char*)malloc(strlen(base)+strlen(name)+20);
      topicStateClick[i]=(char*)malloc(strlen(base)+strlen(name)+24);
      topicStateHold[i]=(char*)malloc(strlen(base)+strlen(name)+24);
      topicStateClickClick[i]=(char*)malloc(strlen(base)+strlen(name)+24);
      topicStateClickHold[i]=(char*)malloc(strlen(base)+strlen(name)+24);
      sprintf(topicState[i],"%s/%s/STATE",base,name);
      sprintf(topicStateClick[i],"%s/%s/CLICK",base,name);
      sprintf(topicStateHold[i],"%s/%s/HOLD",base,name);
      sprintf(topicStateClickClick[i],"%s/%s/CLICKCLICK",base,name);
      sprintf(topicStateClickHold[i],"%s/%s/CLICKHOLD",base,name);
      //
#ifdef DEBUG_PRINT_MQTT
      if(logfile) {
        fprintf(logfile,"Button(%d): %s state=%s click=%s hold=%s clickclick=%s clickhold=%s\n",i,name,topicState[i],topicStateClick[i],topicStateHold[i],topicStateClickClick[i],topicStateClickHold[i]);
      }
#endif
    } else {
      topicState[i]=0;
      topicStateClick[i]=0;
      topicStateHold[i]=0;
      topicStateClickClick[i]=0;
      topicStateClickHold[i]=0;
    }
  }
}

void PahoWrapper::writeState(int bno, int mode, const char *msg) {
  if(mode==BUTT_STATE) {
    send(topicState[bno], 0, msg);
  } else if(mode==BUTT_CLICK) {
    send(topicStateClick[bno], 0, msg);
  } else if(mode==BUTT_HOLD) {
    send(topicStateHold[bno], 0, msg);
  } else if(mode==BUTT_CLICKCLICK) {
    send(topicStateClickClick[bno], 0, msg);
  } else {
    send(topicStateClickHold[bno], 0, msg);
  }
}

void PahoWrapper::markAvailable(bool avail) {
  if(avail) {
    send(topicLWT, 1, "Online");
  } else {
    send(topicLWT, 1, "Offline");
  }
}

void PahoWrapper::send(const char *topic, int retain, const char *msg) {
#ifdef DEBUG_PRINT_MQTT
  if(logfile) { 
    fprintf(logfile,"PAHO - Writing retain=%d message '%s' to topic '%s'\n", retain, msg, topic); 
    fflush(logfile);
  }
#endif
  //
  // https://eclipse.dev/paho/files/mqttdoc/MQTTAsync/html/publish.html
  //
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
  int rc;
  opts.onSuccess = _pahoOnSend;
  opts.onFailure = _pahoOnSendFailure;
  opts.context = (void*)this;
  pubmsg.payload = (void*)msg;
  pubmsg.payloadlen = strlen(msg);
  pubmsg.qos = 1;
  pubmsg.retained = retain;
#ifdef __LINUX__
  __sync_fetch_and_add(&pahoOutstanding,1);
#else
  InterlockedIncrement(&pahoOutstanding);
#endif
  if ((rc = MQTTAsync_sendMessage(pahoClient, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS) {
    pahoUp=false;
    fprintf(stderr,"PAHO_ERROR - Failed to start sendMessage, return code %d (%s : %s)\n", rc,topic,msg);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile,"PAHO_ERROR - Failed to start sendMessage, return code %d (%s: %s)\n", rc,topic,msg);
    }
#endif
  }
}

void PahoWrapper::reconnect() {
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  MQTTAsync_willOptions will_opts= MQTTAsync_willOptions_initializer;
  int rc;
  //
  // https://eclipse.dev/paho/files/mqttdoc/MQTTAsync/html/publish.html
  //
  if(pahoClient) { 
    MQTTAsync_destroy(&pahoClient); 
    pahoClient=0; 
    Sleep(1000); 
  }
  MQTTAsync_create(&pahoClient, mqttServer, "Flic2MQTT/1.0", MQTTCLIENT_PERSISTENCE_NONE, NULL);
  MQTTAsync_setCallbacks(pahoClient, NULL, _pahoOnConnLost, NULL, NULL);
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  conn_opts.onSuccess = _pahoOnConnect;
  conn_opts.onFailure = _pahoOnConnectFailure;
  conn_opts.context = (void*)this;
  will_opts.topicName=topicLWT;
  will_opts.message="Offline";
  will_opts.qos=1;
  will_opts.retained=1;
  conn_opts.will = &will_opts;
  pahoUp=false;
  if ((rc = MQTTAsync_connect(pahoClient, &conn_opts)) == MQTTASYNC_SUCCESS) {
    for(int i=0;i<5;i++) {
      if(!pahoUp) { 
        Sleep(1000); 
#ifdef DEBUG_PRINT_MQTT
    if(logfile) { fprintf(logfile, "PAHO - Waiting for connection... %d/5\n", i+1); }
#endif
      }
    }
  } else {
    fprintf(stderr, "PAHO_ERROR - Failed to start connect, return code %d\n", rc);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile, "PAHO_ERROR - Failed to start connect, return code %d\n", rc);
    }
#endif
  }
  if(!pahoUp) {
    fprintf(stderr, "PAHO_ERROR - Paho did not connect correctly after 5 seconds...\n", rc);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile, "PAHO_ERROR - Paho did not connect correctly after 5 seconds...\n", rc);
    }
#endif
  }
}

void PahoWrapper::pahoOnConnLost(char *cause) {
  pahoUp=false;
  fprintf(stderr,"PAHO_ERROR - Connection Lost - cause %s\n", cause);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile, "PAHO_ERROR - Connection Lost - cause %s\n", cause);
    }
#endif
}

void PahoWrapper::pahoOnConnectFailure(MQTTAsync_failureData* response) {
  pahoUp=false;
  fprintf(stderr,"PAHO_ERROR - Connect failed, rc %d\n", response ? response->code : 0);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile, "PAHO_ERROR - Connect failed, rc %d\n", response ? response->code : 0);
    }
#endif
}

void PahoWrapper::pahoOnSendFailure(MQTTAsync_failureData* response) {
  pahoUp=false;
  fprintf(stderr,"PAHO_ERROR - Send failed, rc %d\n", response ? response->code : 0);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile, "PAHO_ERROR - Send failed, rc %d\n", response ? response->code : 0);
    }
#endif
}

void PahoWrapper::pahoOnConnect(MQTTAsync_successData* response) {
  pahoOutstanding=0;
  pahoUp=true;
#ifdef DEBUG_PRINT_MQTT
  if(logfile) {
    fprintf(logfile,"PAHO - onConnect complete\n");
  }
#endif
}

void PahoWrapper::pahoOnSend(MQTTAsync_successData* response)
{
#ifdef __LINUX__
  __sync_fetch_and_sub(&pahoOutstanding,1);
#else
  InterlockedDecrement(&pahoOutstanding);
#endif
  if(pahoOutstanding>16) {
    pahoUp=false;
    fprintf(stderr,"PAHO_ERROR - Outstanding Paho MQTT messages is high: %d\n", pahoOutstanding);
#ifdef DEBUG_PRINT_MQTT
    if(logfile && logfile!=stderr) {
      fprintf(logfile,"PAHO_WARNING - Outstanding Paho MQTT messages is high: %d\n", pahoOutstanding);
    }
#endif
  }
#ifdef DEBUG_PRINT_MQTT
  //if(logfile) { fprintf(logfile,"PAHO - onSend complete outstanding=%d\n",pahoOutstanding); }
#endif
}


