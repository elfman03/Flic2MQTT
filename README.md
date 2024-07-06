# Flic2MQTT
Listens to Flic buttons (via a flicd) and posts to MQTT

|Command Line                    |Function                                            |
|--------------------------------|----------------------------------------------------|
|flic2MQTT                       |this message                                        |
|flic2MQTT -interact host [port] |run a flic 'simpleclient' to host[port]             |
|flic2MQTT -mqtt                 |run in mqtt mode with settings from Flic2MQTT.config|

NOTE:  Currently only windows builds supported

INSTALL: 
* pull this git repo 
* pull/build - PAHO C Library - https://github.com/eclipse/paho.mqtt.c.git
* Modify Makefile to indicate where your built PAHO library is
* Get a Visual Studio Development prompt 
  *(I use 2022 community edition)
  * e.j.: C:\Windows\System32\cmd.exe /k "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
* > make
* Copy paho-mqtt3a.dll to current directory
* run flic2mqtt


MQTT topics created/updated:
|Topic                             | Value          | Description                |
|----------------------------------|----------------|----------------------------|
|/flic2mqtt/LWT                    | Online/Offline | is flic2mqtt running?      |
|/flic2mqtt/button_name/state      | On/Off         | On while actively pressed  |
|/flic2mqtt/button_name/click      | timestamp      | triggers on click event    |
|/flic2mqtt/button_name/hold       | timestamp      | triggers on hold event     |
|/flic2mqtt/button_name/clickclick | timestamp      | triggers on double click   |
|/flic2mqtt/button_name/clickhold  | timestamp      | triggers on click then hold|

Configure Flic2MQTT.config:
```
#
# where should logs go?
#
LOGFILE=stdout
#
# Where is my mqtt server and what should the topic pattern be (will augment circuit name)
#
MQTT_SERVER=192.168.100.250
MQTT_TOPIC_BASE=/flic2mqtt
#
# Where is my flicd server?
#
FLICD_SERVER=127.0.0.1
#FLICD_PORT=5551
#
#
# The button names I want to track and their flic identifiers (can have up to 8)
#
FLIC_NAME_00=butt0
FLIC_MAC_00=xx:xx:xx:xx:xx:xx
#
FLIC_NAME_01=butt1
FLIC_MAC_01=xx:xx:xx:xx:xx:xx
#
FLIC_NAME_02=butt2
FLIC_MAC_02=xx:xx:xx:xx:xx:xx
#
FLIC_NAME_03=butt3
FLIC_MAC_03=xx:xx:xx:xx:xx:xx
```
