/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FLICD_CLIENTH
#define _FLICD_CLIENTH

#define FLIC_BUFSIZE      32

/* PIPE packet format
 * 32 byte payloads
 * {
 *   char operation;
 *   char status;
 *   char button;
 *   char [29] str;     // Make sure to have a null in offset 28
 * }
 */

//
// Operation
//
#define FLIC_PING         0   // periodic message over pipe for sanity
#define FLIC_INFO_GENERAL 1   // getInfo response
#define FLIC_CONNECT      2   // connect response
#define FLIC_STATUS       3   // button status changes (online/offline)
#define FLIC_UPDOWN       4   // down/up/hold message
static const char *FLIC_OPS[]={"FLIC_PING",
                        "FLIC_INFO_GENERAL",
                        "FLIC_CONNECT",
                        "FLIC_STATUS",
                        "FLIC_UPDOWN"};
//
// Status
//
#define FLIC_STATUS_OK    0
#define FLIC_STATUS_DOWN  0
#define FLIC_STATUS_UP    1
#define FLIC_STATUS_HOLD  5
#define FLIC_STATUS_FATAL 255

//
// For button neutral messages
//
#define FLIC_BUTTON_ALL   255

extern int flicd_client_main(int argc, char *argv[]);
extern int flicd_client_init(const char *server, int port);
extern int flicd_client_handle_line(int sockfd, const char *incmd);

#endif
