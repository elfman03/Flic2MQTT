##
## Copyright (C) 2023, Chris Elford
## 
## SPDX-License-Identifier: MIT
##

OPTS=/MD /EHsc /Zi
OBJS=Config.obj PahoWrapper.obj flicd_client.obj
ELIBS=ws2_32.lib mswsock.lib advapi32.lib
PAHO_I=../paho.mqtt.c/src
PAHO_L=../paho.mqtt.c/src/Release/paho-mqtt3a.lib

Flic2MQTT.exe: Flic2MQTT.cpp $(OBJS)
	cl $(OPTS) /I $(PAHO_I) Flic2MQTT.cpp /link /DEBUG $(OBJS) $(PAHO_L) $(ELIBS)

Config.obj: Config.cpp Config.h global.h
	cl $(OPTS) /c Config.cpp

flicd_client.obj: flicd_client.cpp flicd_client_protocol_packets.h
	cl $(OPTS) /c flicd_client.cpp

PahoWrapper.obj: PahoWrapper.cpp PahoWrapper.h Config.h global.h
	cl $(OPTS) /I $(PAHO_I) /c PahoWrapper.cpp

clean:
	cmd /c del /q Config.obj
	cmd /c del /q PahoWrapper.obj
	cmd /c del /q flicd_client.obj
	cmd /c del /q Flic2MQTT.obj Flic2MQTT.exe

test:
	.\Flic2MQTT.exe
