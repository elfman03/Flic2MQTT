# Flic2MQTT
Listens to Flic buttons (via a flicd) and posts to MQTT.  It uses a modified flic sdk 'simpleclient' to create a socket connection to a flid daemon, registers for events from selected buttons.  As button events are received from flicd, it converts these events into MQTT topic posts.  My primary intended use is to run this on the same server running flicd (connecting to flicd via 127.0.0.1) then send the resulting MQTT messages to a separate server on my IOT network running openhab. In this way I can avoid using the FlicButton binding in OpenHAB and re-enable the firewall that prevents my IOT network from initiating connections to my regular network.  

it also provides a command line mode to run in 'simpleclient' mode to enable assorted maintenance tasks like scanning/registering new buttons, getting their MAC addresses, deleting buttons, and performing general maintenance.

My Flic buttons are all Flic-1 (actually "powered by Flic" FCCID: 2ACR9-FLIC)

|Command Line                    |Function                                            |
|--------------------------------|----------------------------------------------------|
|flic2MQTT                       |this message                                        |
|flic2MQTT -interact host [port] |run a flic 'simpleclient' to host[port]             |
|flic2MQTT -mqtt                 |run in mqtt mode with settings from Flic2MQTT.config|

NOTES:
* Requires a working flicd daemon
* Currently only windows builds supported
* 'simpleclient' is ported from https://github.com/50ButtonsEach/fliclib-linux-hci/tree/master/simpleclient
* Depends on Apache PAHO MQTT client library

INSTALL: 
* pull this git repo 
* pull/build - PAHO C Library - https://github.com/eclipse/paho.mqtt.c.git
* Modify Makefile to indicate where your built PAHO library is
* Get a Visual Studio Development prompt 
  *(I use 2022 community edition)
  * e.j.: C:\Windows\System32\cmd.exe /k "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
* make
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
# Where is my mqtt server and what should the topic pattern be (will augment button name)
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
