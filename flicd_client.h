/*
 * Copyright (C) 2024, Chris Elford
 * 
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FLICD_CLIENTH
#define _FLICD_CLIENTH

#define FLIC_INFO_GENERAL 0
#define FLIC_INFO_BUTTON  1
#define FLIC_CONNECT      2
#define FLIC_UPDOWN       3
#define FLIC_STATUS_OK    0
#define FLIC_STATUS_FATAL 255
#define FLIC_BUTTON_ALL   255
#define FLIC_BUFSIZE      32

extern int flicd_client_main(int argc, char *argv[]);
extern int flicd_client_init(const char *server, int port);
extern int flicd_client_handle_line(int sockfd, const char *incmd);

#endif
