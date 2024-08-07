/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 * NOTE: 
 *
 * File Source: based on https://github.com/50ButtonsEach/fliclib-linux-hci/tree/master/simpleclient)
 * File License CC0 (see https://github.com/50ButtonsEach/fliclib-linux-hci/blob/master/README.md)
 * 
 *
 * Winsock inspiration from https://learn.microsoft.com/en-us/windows/win32/winsock/complete-client-code
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <sys/types.h>
#include "flicd_client.h"

#ifdef __GNUC__
linux #include <unistd.h>
linux #include <poll.h>
linux #include <sys/param.h>
linux #include <sys/uio.h>
linux #include <sys/ioctl.h>
linux #include <sys/socket.h>
linux #include <arpa/inet.h>
linux #include <netdb.h>
#endif

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#define STDIN_FILENO 0

#include <io.h>
#include <time.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "flicd_client_protocol_packets.h"
extern int thePipeW;     // the pipe to use to send flic info to the main thread

using namespace std;
using namespace FlicClientProtocol;

static DWORD theFlicdReaderTid;             // Thread identifier of Flicd reader
static HANDLE theFlicdReaderHandle;         // Handle of Flicd reader

static const char* CreateConnectionChannelErrorStrings[] = {
  "NoError",
  "MaxPendingConnectionsReached"
};

static const char* ConnectionStatusStrings[] = {
  "Disconnected",
  "Connected",
  "Ready"
};

static const char* DisconnectReasonStrings[] = {
  "Unspecified",
  "ConnectionEstablishmentFailed",
  "TimedOut",
  "BondingKeysMismatch"
};

static const char* RemovedReasonStrings[] = {
  "RemovedByThisClient",
  "ForceDisconnectedByThisClient",
  "ForceDisconnectedByOtherClient",
  
  "ButtonIsPrivate",
  "VerifyTimeout",
  "InternetBackendError",
  "InvalidData",
  
  "CouldntLoadDevice",
  
  "DeletedByThisClient",
  "DeletedByOtherClient",
  "ButtonBelongsToOtherPartner",
  "DeletedFromButton"
};

static const char* ClickTypeStrings[] = {
  "ButtonDown",
  "ButtonUp",
  "ButtonClick",
  "ButtonSingleClick",
  "ButtonDoubleClick",
  "ButtonHold"
};

static const char* BdAddrTypeStrings[] = {
  "PublicBdAddrType",
  "RandomBdAddrType"
};

static const char* LatencyModeStrings[] = {
  "NormalLatency",
  "LowLatency",
  "HighLatency"
};

static const char* ScanWizardResultStrings[] = {
  "WizardSuccess",
  "WizardCancelledByUser",
  "WizardFailedTimeout",
  "WizardButtonIsPrivate",
  "WizardBluetoothUnavailable",
  "WizardInternetBackendError",
  "WizardInvalidData",
  "WizardButtonBelongsToOtherPartner",
  "WizardButtonAlreadyConnectedToOtherDevice"
};

static const char* BluetoothControllerStateStrings[] = {
  "Detached",
  "Resetting",
  "Attached"
};

static uint8_t hex_digit_to_int(char hex) {
  if (hex >= '0' && hex <= '9')
    return hex - '0';
  if (hex >= 'a' && hex <= 'f')
    return hex - 'a' + 10;
  if (hex >= 'A' && hex <= 'F')
    return hex - 'A' + 10;
  return 0;
}

static uint8_t hex_to_byte(const char* hex) {
  return (hex_digit_to_int(hex[0]) << 4) | hex_digit_to_int(hex[1]);
}

static string bytes_to_hex_string(const uint8_t* data, int len) {
  string str(len * 2, '\0');
  for (int i = 0; i < len; i++) {
    static const char tbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    str[i * 2] = tbl[data[i] >> 4];
    str[i * 2 + 1] = tbl[data[i] & 0xf];
  }
  return str;
}

struct Bdaddr {
  uint8_t addr[6];
  
  Bdaddr() {
    memset(addr, 0, 6);
  }
  
  Bdaddr(const Bdaddr& o) {
    *this = o;
  }
  Bdaddr(const uint8_t* a) {
    *this = a;
  }
  Bdaddr(const char* a) {
    *this = a;
  }
  Bdaddr& operator=(const Bdaddr& o) {
    memcpy(addr, o.addr, 6);
    return *this;
  }
  Bdaddr& operator=(const uint8_t* a) {
    memcpy(addr, a, 6);
    return *this;
  }
  Bdaddr& operator=(const char* a) {
    for (int i = 0, pos = 15; i < 6; i++, pos -= 3) {
      addr[i] = hex_to_byte(&a[pos]);
    }
    return *this;
  }
  
  string to_string() const {
    string str;
    for (int i = 5; i >= 0; i--) {
      str += bytes_to_hex_string(addr + i, 1);
      if (i != 0) {
        str += ':';
      }
    }
    return str;
  }
  
  bool operator==(const Bdaddr& o) const { return memcmp(addr, o.addr, 6) == 0; }
  bool operator!=(const Bdaddr& o) const { return memcmp(addr, o.addr, 6) != 0; }
  bool operator<(const Bdaddr& o) const { return memcmp(addr, o.addr, 6) < 0; }
};

static void write_packet(int fd, void* buf, int len) {
  //uint8_t new_buf[2 + len];  // bogus code from upstream
  uint8_t new_buf[4096];
  assert(len<4094);
  new_buf[0] = len & 0xff;
  new_buf[1] = len >> 8;
  memcpy(new_buf + 2, buf, len);
  fprintf(stderr,"Sending flicd client command : %s\n",FLICD_CMDS[*(unsigned char *)buf]);
  
  int pos = 0;
  int left = 2 + len;
  while(left) {
    int res = send(fd, (const char *)(new_buf + pos), left, 0);
    if (res < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("write");
      exit(1);
    }
    pos += res;
    left -= res;
  }
}

static Bdaddr read_bdaddr(const char *buf) {
  char addr[32];
  sscanf(buf, "%s", addr);
  if (strlen(addr) != 17) {
    fprintf(stderr, "Warning: Invalid length of bd addr\n");
  }
  return Bdaddr(addr);
}

static void print_help() {
  static const char help_text[] =
  "Available commands:\n"
  "getInfo - get various info about the server state and previously verified buttons\n"
  "startScanWizard - start scan wizard\n"
  "cancelScanWizard - cancel scan wizard\n"
  "startScan - start a raw scanning of Flic buttons\n"
  "stopScan - stop raw scanning\n"
  "connect xx:xx:xx:xx:xx:xx id - first parameter is the bluetooth address of the button, second is an integer identifier you set to identify this connection\n"
  "disconnect id - disconnect or abort pending connection\n"
  "changeModeParameters id latency_mode auto_disconnect_time - change latency mode (NormalLatency/LowLatency/HighLatency) and auto disconnect time for this connection\n"
  "forceDisconnect xx:xx:xx:xx:xx:xx - disconnect this button, even if other client program are connected\n"
  "getButtonInfo xx:xx:xx:xx:xx:xx - get button info for a verified button\n"
  "createBatteryStatusListener xx:xx:xx:xx:xx:xx id - first parameter is the bluetooth address of the button, second is an integer you set to identify this listener\n"
  "removeBatteryStatusListener id - removes a battery listener\n"
  "delete xx:xx:xx:xx:xx:xx - delete button\n"
  "help - prints this help text\n"
  "quit - exit\n"
  "\n";
  fprintf(stderr, help_text);
}

static void pipe_send(char operation, char status, char button, const char *str) {
  int i;
  char piper[32];
  //
  // assemble a packet and write it to the main thread pipe
  //
  piper[0]=operation;
  piper[1]=status;
  piper[2]=button;
  for(i=0;str[i] && i<28;i++) { piper[i+3]=str[i]; }  // copy up to 28 bytes of incoming string
  for(i=i+3;i<32;i++)         { piper[i]=0;        }  // fill remainder of message with nulls
  //
  _write(thePipeW,piper,32);
}

static DWORD WINAPI flicd_client_reader(LPVOID param) {
  int sockfd=*((int*)param);
  free(param);
  char *readbuf=(char *)malloc(65536+4);
  assert(readbuf);

  while(1) {
    int nbytes;

    int err=WSAETIMEDOUT;
    for(;err==WSAETIMEDOUT;) {
      err=0;
      nbytes = recv(sockfd, readbuf, 2, 0);
      if (nbytes < 0) {
        err=WSAGetLastError();
        if(err==WSAETIMEDOUT) {
          if(thePipeW) {
            pipe_send(FLIC_PING, FLIC_STATUS_OK, FLIC_BUTTON_ALL, "NO_UPDATE");
          } else {
            //fprintf(stderr,"Expected timeout receiving header from flicd!\n");
          }
        } else {
          if(thePipeW) {
            pipe_send(FLIC_PING, FLIC_STATUS_FATAL, FLIC_BUTTON_ALL, "BAD_HEADER");
          }
          perror("FATAL: read sockfd header");
          return 1;
        }
      }
      if (nbytes == 1) {
          if(thePipeW) {
            pipe_send(FLIC_PING, FLIC_STATUS_FATAL, FLIC_BUTTON_ALL, "SHORT_HEADER");
          }
          fprintf(stderr,"FATAL: just one byte came");
          return 1;
      }
    }

    int packet_len = readbuf[0] | (readbuf[1] << 8);
    int read_pos = 0;
    int bytes_left = packet_len;
    //fprintf(stderr,"flicd wants to send %d bytes\n",packet_len);
    
    while (bytes_left > 0) {
      nbytes = recv(sockfd, readbuf + read_pos, bytes_left, 0);
      if (nbytes < 0) {
        if(thePipeW) {
          pipe_send(FLIC_PING, FLIC_STATUS_FATAL, FLIC_BUTTON_ALL, "BAD_PAYLOAD");
        }
        perror("read sockfd data");
        return 1;
      }
      read_pos += nbytes;
      bytes_left -= nbytes;
    }
    //fprintf(stderr,"flicd sent %d bytes - event=%s\n",read_pos,FLICD_EVTS[readbuf[0]]);
    
    void* pkt = (void*)readbuf;
    switch (readbuf[0]) {
      case EVT_ADVERTISEMENT_PACKET_OPCODE: {
        EvtAdvertisementPacket* evt = (EvtAdvertisementPacket*)pkt;
        printf("ADV: %s %s %d %s %s%s%s\n",
               Bdaddr(evt->bd_addr).to_string().c_str(),
               string(evt->name, (size_t)evt->name_length).c_str(),
               evt->rssi,
               (evt->is_private ? "private" : "public"),
               (evt->already_verified ? "verified" : "unverified"),
               (evt->already_connected_to_this_device ? " already connected to this device" : ""),
               (evt->already_connected_to_other_device ? " already connected to other device" : "")
        );
        break;
      }
      case EVT_CREATE_CONNECTION_CHANNEL_RESPONSE_OPCODE: {
        EvtCreateConnectionChannelResponse* evt = (EvtCreateConnectionChannelResponse*)pkt;
        if(thePipeW) {
          pipe_send(FLIC_CONNECT, FLIC_STATUS_OK, evt->base.conn_id, ConnectionStatusStrings[evt->error]);
        } else {
          printf("Create conn: %d %s %s\n", evt->base.conn_id, CreateConnectionChannelErrorStrings[evt->error], ConnectionStatusStrings[evt->connection_status]);
        }
        break;
      }
      case EVT_CONNECTION_STATUS_CHANGED_OPCODE: {
        EvtConnectionStatusChanged* evt = (EvtConnectionStatusChanged*)pkt;
        if(thePipeW) {
          pipe_send(FLIC_STATUS, FLIC_STATUS_OK, evt->base.conn_id, ConnectionStatusStrings[evt->connection_status]);
        } else {
          printf("Connection status changed: %d %s", evt->base.conn_id, ConnectionStatusStrings[evt->connection_status]);
          if (evt->connection_status == Disconnected) {
            printf(" %s\n", DisconnectReasonStrings[evt->disconnect_reason]);
          } else {
            printf("\n");
          }
        }
        break;
      }
      case EVT_CONNECTION_CHANNEL_REMOVED_OPCODE: {
        EvtConnectionChannelRemoved* evt = (EvtConnectionChannelRemoved*)pkt;
        printf("Connection removed: %d %s\n", evt->base.conn_id, RemovedReasonStrings[evt->removed_reason]);
        break;
      }
      case EVT_BUTTON_UP_OR_DOWN_OPCODE:
      case EVT_BUTTON_CLICK_OR_HOLD_OPCODE:
      case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE:
      case EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OR_HOLD_OPCODE: {
        EvtButtonEvent* evt = (EvtButtonEvent*)pkt;
        if(thePipeW) {
          if (readbuf[0]==EVT_BUTTON_UP_OR_DOWN_OPCODE) {
            pipe_send(FLIC_UPDOWN, evt->click_type, evt->base.conn_id, ClickTypeStrings[evt->click_type]);
          } else if (readbuf[0]==EVT_BUTTON_CLICK_OR_HOLD_OPCODE && evt->click_type==ButtonHold) {
            pipe_send(FLIC_UPDOWN, evt->click_type, evt->base.conn_id, ClickTypeStrings[evt->click_type]);
          } else if (readbuf[0]==EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE && evt->click_type==ButtonSingleClick) {
            pipe_send(FLIC_UPDOWN, evt->click_type, evt->base.conn_id, ClickTypeStrings[evt->click_type]);
          } else if (readbuf[0]==EVT_BUTTON_SINGLE_OR_DOUBLE_CLICK_OPCODE && evt->click_type==ButtonDoubleClick) {
            pipe_send(FLIC_UPDOWN, evt->click_type, evt->base.conn_id, ClickTypeStrings[evt->click_type]);
          }
        } else {
          static const char* types[] = {"Button up/down", "Button click/hold", "Button single/double click", "Button single/double click/hold"};
          printf("%s: %d, %s, %s, %d seconds ago\n", types[readbuf[0]-EVT_BUTTON_UP_OR_DOWN_OPCODE], evt->base.conn_id, ClickTypeStrings[evt->click_type], (evt->was_queued ? "queued" : "not queued"), evt->time_diff);
        }
        break;
      }
      case EVT_NEW_VERIFIED_BUTTON_OPCODE: {
        EvtNewVerifiedButton* evt = (EvtNewVerifiedButton*)pkt;
        printf("New verified button: %s\n", Bdaddr(evt->bd_addr).to_string().c_str());
        break;
      }
      case EVT_GET_INFO_RESPONSE_OPCODE: {
        EvtGetInfoResponse* evt = (EvtGetInfoResponse*)pkt;
        if(thePipeW) {
          pipe_send(FLIC_INFO_GENERAL, FLIC_STATUS_OK, FLIC_BUTTON_ALL, BluetoothControllerStateStrings[evt->bluetooth_controller_state]);
        } else {
          printf("Got info: %s, %s (%s), max pending connections: %d, max conns: %d, current pending conns: %d, currently no space: %c\n",
               BluetoothControllerStateStrings[evt->bluetooth_controller_state],
               Bdaddr(evt->my_bd_addr).to_string().c_str(),
               BdAddrTypeStrings[evt->my_bd_addr_type],
               evt->max_pending_connections,
               evt->max_concurrently_connected_buttons,
               evt->current_pending_connections,
               evt->currently_no_space_for_new_connection ? 'y' : 'n');
          puts(evt->nb_verified_buttons > 0 ? "Verified buttons:" : "No verified buttons yet");
          for(int i = 0; i < evt->nb_verified_buttons; i++) {
            printf("%s\n", Bdaddr(evt->bd_addr_of_verified_buttons[i]).to_string().c_str());
          }
        }
        break;
      }
      case EVT_NO_SPACE_FOR_NEW_CONNECTION_OPCODE: {
        EvtNoSpaceForNewConnection* evt = (EvtNoSpaceForNewConnection*)pkt;
        printf("No space for new connection, max: %d\n", evt->max_concurrently_connected_buttons);
        break;
      }
      case EVT_GOT_SPACE_FOR_NEW_CONNECTION_OPCODE: {
        EvtGotSpaceForNewConnection* evt = (EvtGotSpaceForNewConnection*)pkt;
        printf("Got space for new connection, max: %d\n", evt->max_concurrently_connected_buttons);
        break;
      }
      case EVT_BLUETOOTH_CONTROLLER_STATE_CHANGE_OPCODE: {
        EvtBluetoothControllerStateChange* evt = (EvtBluetoothControllerStateChange*)pkt;
        printf("Bluetooth state change: %d\n", evt->state);
        break;
      }
      case EVT_GET_BUTTON_INFO_RESPONSE_OPCODE: {
        EvtGetButtonInfoResponse* evt = (EvtGetButtonInfoResponse*)pkt;
        printf("Button info response: %s %s %s %s %d %d\n",
               Bdaddr(evt->bd_addr).to_string().c_str(),
               bytes_to_hex_string(evt->uuid, sizeof(evt->uuid)).c_str(),
               string(evt->color, (size_t)evt->color_length).c_str(),
               string(evt->serial_number, (size_t)evt->serial_number_length).c_str(),
               evt->flic_version,
               evt->firmware_version
        );
        break;
      }
      case EVT_SCAN_WIZARD_FOUND_PRIVATE_BUTTON_OPCODE: {
        printf("Found private button. Please hold down it for 7 seconds to make it public.\n");
        break;
      }
      case EVT_SCAN_WIZARD_FOUND_PUBLIC_BUTTON_OPCODE: {
        EvtScanWizardFoundPublicButton* evt = (EvtScanWizardFoundPublicButton*)pkt;
        printf("Found public button %s %s, connecting...\n", Bdaddr(evt->bd_addr).to_string().c_str(), string(evt->name, (size_t)evt->name_length).c_str());
        break;
      }
      case EVT_SCAN_WIZARD_BUTTON_CONNECTED_OPCODE: {
        printf("Connected, now pairing and verifying...\n");
        break;
      }
      case EVT_SCAN_WIZARD_COMPLETED_OPCODE: {
        EvtScanWizardCompleted* evt = (EvtScanWizardCompleted*)pkt;
        printf("Scan wizard done with status %s\n", ScanWizardResultStrings[evt->result]);
        break;
      }
      case EVT_BATTERY_STATUS_OPCODE: {
        EvtBatteryStatus* evt = (EvtBatteryStatus*)pkt;
        printf("Battery status report for id %d, percentage: %d%%, timestamp: %s\n", evt->listener_id, evt->battery_percentage, ctime((time_t*)&evt->timestamp));
        break;
      }
      case EVT_BUTTON_DELETED_OPCODE: {
        EvtButtonDeleted* evt = (EvtButtonDeleted*)pkt;
        printf("Button %s deleted %s\n", Bdaddr(evt->bd_addr).to_string().c_str(), evt->deleted_by_this_client ? "by this client" : "not by this client");
        break;
      }
      default: {
        fprintf(stderr,"Unknown Packet opcode: %d\n",readbuf[0]);
      }
    }
  }
}

static int linein(char *buf, int sz) {
  int len,ch=' ';
  //
  // read till newline converting tabs to spaces and killing \r\n
  //
  for(len=0;ch!='\n';) {
    ch=getchar();
    if(len>=sz) { fprintf(stderr,"OVERFLOW\n"); buf[0]=0; return -2; }
    if(ch==EOF) { fprintf(stderr,"EOF\n");      buf[0]=0; return -1; }
    buf[len]=(unsigned char)ch;
    if(ch=='\t') { buf[len]=' '; }
    if(ch=='\r') { buf[len]=0; }
    if(ch=='\n') { buf[len]=0; }
    if(buf[len]) { len++; }
  }
  return len;
}

int flicd_client_handle_line(int sockfd, const char *incmd) {
  //
  // split cmdline into up to three words ignoring excess spaces
  //
  char words[4][64];
  words[0][0]=0;
  words[1][0]=0;
  words[2][0]=0;
  words[3][0]=0;
  int wordnum=0, windex=0;
  for (int i=0;incmd[i];i++) {
    if(incmd[i]!=' ') { 
      words[wordnum][windex]=incmd[i]; 
      windex++;
      assert(windex<63);  // sanity check overflow
    }
    if(incmd[i]==' ' && windex) { 
      words[wordnum][windex]=0;
      wordnum++; 
      windex=0;
      assert(wordnum<5); // sanity check overflow
    }
  }
  if(windex) { 
    words[wordnum][windex]=0;
    wordnum++;
    assert(wordnum<5);  // sanity check overflow
  }

  //fprintf(stderr,"wordnum=%d w0=%s w1=%s w2=%s\n",wordnum,words[0],words[1],words[2],words[3]);

  if (strcmp("startScanWizard", words[0]) == 0) {
    assert(wordnum==1);
    CmdCreateScanWizard cmd;
    cmd.opcode = CMD_CREATE_SCAN_WIZARD_OPCODE;
    cmd.scan_wizard_id = 0;
    write_packet(sockfd, &cmd, sizeof(cmd));
    
    printf("Please click and hold down your Flic button!\n");
  }
  if (strcmp("cancelScanWizard", words[0]) == 0) {
    assert(wordnum==1);
    CmdCancelScanWizard cmd;
    cmd.opcode = CMD_CANCEL_SCAN_WIZARD_OPCODE;
    cmd.scan_wizard_id = 0;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("startScan", words[0]) == 0) {
    assert(wordnum==1);
    CmdCreateScanner cmd;
    cmd.opcode = CMD_CREATE_SCANNER_OPCODE;
    cmd.scan_id = 0;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("stopScan", words[0]) == 0) {
    assert(wordnum==1);
    CmdRemoveScanner cmd;
    cmd.opcode = CMD_REMOVE_SCANNER_OPCODE;
    cmd.scan_id = 0;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("connect", words[0]) == 0) {
    assert(wordnum==3);
    CmdCreateConnectionChannel cmd;
    cmd.opcode = CMD_CREATE_CONNECTION_CHANNEL_OPCODE;
    memcpy(cmd.bd_addr, read_bdaddr(words[1]).addr, 6);
    sscanf(words[2],"%u", &cmd.conn_id);
    cmd.latency_mode = NormalLatency;
    cmd.auto_disconnect_time = 0x1ff;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("disconnect", words[0]) == 0) {
    assert(wordnum==2);
    CmdRemoveConnectionChannel cmd;
    cmd.opcode = CMD_REMOVE_CONNECTION_CHANNEL_OPCODE;
    sscanf(words[1],"%u", &cmd.conn_id);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("forceDisconnect", words[0]) == 0) {
    assert(wordnum==2);
    CmdForceDisconnect cmd;
    cmd.opcode = CMD_FORCE_DISCONNECT_OPCODE;
    memcpy(cmd.bd_addr, read_bdaddr(words[1]).addr, 6);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("changeModeParameters", words[0]) == 0) {
    assert(wordnum==4);
    CmdChangeModeParameters cmd;
    cmd.opcode = CMD_CHANGE_MODE_PARAMETERS_OPCODE;
    sscanf(words[1],"%u", &cmd.conn_id);
    char latency_mode[32];
    uint32_t auto_disconnect_time;
    sscanf(words[2],"%s", latency_mode);
    sscanf(words[3],"%u", &auto_disconnect_time);
    int mode = 0;
    for (int i = 0; i < 3; i++) {
      if (strcmp(latency_mode, LatencyModeStrings[i]) == 0) {
        mode = i;
      }
    }
    cmd.latency_mode = (enum LatencyMode)mode;
    cmd.auto_disconnect_time = auto_disconnect_time;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("getButtonInfo", words[0]) == 0) {
    assert(wordnum==2);
    CmdGetButtonInfo cmd;
    cmd.opcode = CMD_GET_BUTTON_INFO_OPCODE;
    memcpy(cmd.bd_addr, read_bdaddr(words[1]).addr, 6);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("getInfo", words[0]) == 0) {
    assert(wordnum==1);
    CmdGetInfo cmd;
    cmd.opcode = CMD_GET_INFO_OPCODE;
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("createBatteryStatusListener", words[0]) == 0) {
    assert(wordnum==3);
    CmdCreateBatteryStatusListener cmd;
    cmd.opcode = CMD_CREATE_BATTERY_STATUS_LISTENER_OPCODE;
    memcpy(cmd.bd_addr, read_bdaddr(words[1]).addr, 6);
    sscanf(words[2], "%u", &cmd.listener_id);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("removeBatteryStatusListener", words[0]) == 0) {
    assert(wordnum==2);
    CmdRemoveBatteryStatusListener cmd;
    cmd.opcode = CMD_REMOVE_BATTERY_STATUS_LISTENER_OPCODE;
    sscanf(words[1], "%u", &cmd.listener_id);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("delete", words[0]) == 0) {
    assert(wordnum==2);
    CmdDeleteButton cmd;
    cmd.opcode = CMD_DELETE_BUTTON_OPCODE;
    memcpy(cmd.bd_addr, read_bdaddr(words[1]).addr, 6);
    write_packet(sockfd, &cmd, sizeof(cmd));
  }
  if (strcmp("help", words[0]) == 0) {
    print_help();
  }
  if (strcmp("quit", words[0]) == 0) {
    assert(wordnum==1);
    return(1);
  }
  return(0);
}

int flicd_client_init(const char *host, int port) {
  fprintf(stderr,"host=%s len=%d port=%d\n",host,(int)strlen(host),port);

  // check for valid host
  //
  struct hostent* server = gethostbyname(host);
  if (server == NULL) {
    fprintf(stderr, "ERROR: no such host: %s\n",host);
    return -1;
  }

  // create socket and check for validity
  //
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "ERROR: failure creating socket\n");
    perror("socket");
    return sockfd;
  }

  // connect and check for valid connection
  //
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);
  
  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERROR: failure connecting socket\n");
    perror("connect");
    close(sockfd);
    return -1;
  }

  //
  // Set up one minute timeout on flicd socket
  //
  DWORD to=60000;
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to))<0) {
    fprintf(stderr, "ERROR: setting recieve timeout on socket\n");
    perror("setsockopt");
    close(sockfd);
    return -1;
  }

  // Create reader thread and verify successful creation
  //
  int *tmp=(int*)malloc(sizeof(int));
  *tmp=sockfd;
  theFlicdReaderHandle=CreateThread(NULL,0,flicd_client_reader,tmp,0,&theFlicdReaderTid);
  if(!theFlicdReaderHandle) {
    fprintf(stderr, "ERROR: fail creating flicd client reader thread\n");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int flicd_client_main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s host [port]\n", argv[0]);
    return 1;
  }
  int port=5551;
  if(argc >= 3) { port=atoi(argv[2]); }
  int sockfd=flicd_client_init(argv[1], port);

  if(sockfd<0) { return -1; }
 
  print_help();
  
  while(1) {
    int ret;
    char incmd[128];
   
    ret=linein(incmd,128);
    if(ret==-1) {
      fprintf(stderr,"EOF\n");
      return 0;
    }
    if(ret==-2) {
      fprintf(stderr,"OVERFLOW\n");
      return 1;
    }
    fprintf(stderr,"stdin command: %s\n",incmd); 
    ret=flicd_client_handle_line(sockfd, incmd);
    if(ret) { 
      return 0; 
    }
  }
}
