/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CONFIGH
#define _CONFIGH

class Config {

private:
  FILE *logfile;
  char *logfileName;
  char *mqttServer;
  char *mqttTopicBase;
  char *flicName[8];
  int flicId[8];
  
public:
  Config();
  void readConfig(const char *fname);
  FILE *getLogfile();
  const char *getMqttServer();
  const char *getMqttTopicBase();
  const char *getFlicName(int i);
  int getFlicId(int i);
};

#endif
