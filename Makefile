##
## Copyright (C) 2023, Chris Elford
## 
## SPDX-License-Identifier: MIT
##

ifeq ($(shell uname), Linux)
#
# Linux
#

CC=g++ -D__LINUX__
OPTS=-g
OBJS=Config.o PahoWrapper.o flicd_client.o
ELIBS=-lpthread -lpaho-mqtt3a

Flic2MQTT: Flic2MQTT.cpp flicd_client.h Config.h PahoWrapper.h global.h $(OBJS)
	$(CC) $(OPTS) -o Flic2MQTT Flic2MQTT.cpp $(OBJS) $(ELIBS)

Config.o: Config.cpp Config.h global.h
	$(CC) $(OPTS) -c Config.cpp

PahoWrapper.o: PahoWrapper.cpp PahoWrapper.h Config.h global.h
	$(CC) $(OPTS) -c PahoWrapper.cpp

flicd_client.o: flicd_client.cpp flicd_client.h flicd_client_protocol_packets.h
	$(CC) $(OPTS) -c flicd_client.cpp

clean:
	rm -f Config.o
	rm -f PahoWrapper.o
	rm -f flicd_client.o
	rm -f Flic2MQTT.o 
	rm -f Flic2MQTT

test:
	./Flic2MQTT

else
#
# Windows
#

OPTS=/MD /EHsc /Zi
OBJS=Config.obj PahoWrapper.obj flicd_client.obj
ELIBS=ws2_32.lib mswsock.lib advapi32.lib
PAHO_I=../paho.mqtt.c/src
PAHO_L=../paho.mqtt.c/src/Release/paho-mqtt3a.lib

Flic2MQTT.exe: Flic2MQTT.cpp flicd_client.h Config.h PahoWrapper.h global.h $(OBJS)
	cl $(OPTS) /I $(PAHO_I) Flic2MQTT.cpp /link /DEBUG $(OBJS) $(PAHO_L) $(ELIBS)

Config.obj: Config.cpp Config.h global.h
	cl $(OPTS) /c Config.cpp

PahoWrapper.obj: PahoWrapper.cpp PahoWrapper.h Config.h global.h
	cl $(OPTS) /I $(PAHO_I) /c PahoWrapper.cpp

flicd_client.obj: flicd_client.cpp flicd_client.h flicd_client_protocol_packets.h
	cl $(OPTS) /c flicd_client.cpp

clean:
	cmd /c del /q Config.obj
	cmd /c del /q PahoWrapper.obj
	cmd /c del /q flicd_client.obj
	cmd /c del /q Flic2MQTT.obj Flic2MQTT.exe Flic2MQTT.pdb
	cmd /c del /q vc140.pdb

test:
	.\Flic2MQTT.exe

endif
