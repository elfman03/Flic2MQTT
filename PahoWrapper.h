/*
 * Copyright (C) 2023, Chris Elford
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
  char *topicState[8];
  char *topicAvailability[8];
  LONG volatile pahoOutstanding;
  bool volatile pahoUp;
  
  //
  void send(const char *topic, const char *msg);

public:
  PahoWrapper(Config *config);
  LONG getOutstanding();
  bool isUp();
  void markAvailable(bool avail);
  void writeState(int circuit, const char *msg);
  void reconnect();

  void pahoOnConnLost(char *cause);
  void pahoOnConnectFailure(MQTTAsync_failureData* response);
  void pahoOnConnect(MQTTAsync_successData* response);
  void pahoOnSend(MQTTAsync_successData* response);
  void pahoOnSendFailure(MQTTAsync_failureData* response);

};

#endif
