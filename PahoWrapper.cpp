/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#include <windows.h>
#include <winhttp.h>
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
  reconnect();
#ifdef DEBUG_PRINT_MQTT
  if(logfile) { fprintf(logfile,"MQTT Server: %s\n",mqttServer); }
#endif
  const char *base=config->getMqttTopicBase();
  for(int i=0;i<8;i++) {
    const char *name=config->getFlicName(i);
    //
    if(name) {
      topicState[i]=(char*)malloc(strlen(base)+strlen(name)+20);
      sprintf(topicState[i],"%s%s/state",base,name);
      //
      topicAvailability[i]=(char*)malloc(strlen(base)+strlen(name)+20);
      sprintf(topicAvailability[i],"%s%s/availability",base,name);
#ifdef DEBUG_PRINT_MQTT
      if(logfile) {
        fprintf(logfile,"Button(%d): %s state=%s availability=%s\n",i,name,topicState[i],topicAvailability[i]);
      }
#endif
    } else {
      topicState[i]=0;
      topicAvailability[i]=0;
    }
  }
}

void PahoWrapper::writeState(int bno, const char *msg) {
  send(topicState[bno], msg);
}

void PahoWrapper::markAvailable(bool avail) {
  int i;
  for(i=0;i<8;i++) {
    if(topicAvailability[i]) {
      send(topicAvailability[i], avail?"Online":"Offline");
    }
  }
}

void PahoWrapper::send(const char *topic, const char *msg) {
#ifdef DEBUG_PRINT_MQTT
  if(logfile) { 
    fprintf(logfile,"PAHO - Writing message '%s' to topic '%s'\n",msg, topic); 
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
  pubmsg.retained = 0;
  InterlockedIncrement(&pahoOutstanding);
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
  int rc;
  //
  // https://eclipse.dev/paho/files/mqttdoc/MQTTAsync/html/publish.html
  //
  if(pahoClient) { 
    MQTTAsync_destroy(&pahoClient); 
    pahoClient=0; 
    Sleep(1000); 
  }
  MQTTAsync_create(&pahoClient, mqttServer, "Curb2MQTT/1.0", MQTTCLIENT_PERSISTENCE_NONE, NULL);
  MQTTAsync_setCallbacks(pahoClient, NULL, _pahoOnConnLost, NULL, NULL);
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;
  conn_opts.onSuccess = _pahoOnConnect;
  conn_opts.onFailure = _pahoOnConnectFailure;
  conn_opts.context = (void*)this;
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
  InterlockedDecrement(&pahoOutstanding);
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


