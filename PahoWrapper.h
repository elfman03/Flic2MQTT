/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PAHOWRAPPER
#define _PAHOWRAPPER
#include <MQTTAsync.h>

class Config;

class PahoWrapper {

private:
  FILE *logfile;
  MQTTAsync pahoClient;
  const char *mqttServer;
  char *topicLWT;
  char *topicState[8];
  char *topicStateHold[8];
  LONG volatile pahoOutstanding;
  bool volatile pahoUp;
  
  //
  void send(const char *topic, int retain, const char *msg);

public:
  PahoWrapper(Config *config);
  LONG getOutstanding();
  bool isUp();
  void markAvailable(bool avail);
  void writeState(int butt, bool hold, const char *msg);
  void reconnect();

  void pahoOnConnLost(char *cause);
  void pahoOnConnectFailure(MQTTAsync_failureData* response);
  void pahoOnConnect(MQTTAsync_successData* response);
  void pahoOnSend(MQTTAsync_successData* response);
  void pahoOnSendFailure(MQTTAsync_failureData* response);

};

#endif
