// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_NETWORK_H
#define CGM_NETWORK_H

#define SOCK_MAX_HTTP_LINE    8190

#define SOCK_LOCAL_CONNECT_TIMEOUT  3 // in seconds  
#define SOCK_CONNECT_TIMEOUT  60      // in seconds 
#define SOCK_TIMEOUT_INTERVAL 20000   // timeout on send/receive socket operations
#define SOCK_TIMEOUT_GETLOG   45000   // timeout on send/receive socket operations
#define SOCK_HTTP_INCOMING_REQUEST_TIMEOUT  30  // in seconds, how much time is allowed for HTTP clients to send their requests

#define SOCK_NO_ERROR         -9
#define SOCK_WDOG_SHUTDOWN	  -10
#define SOCK_IO_ERROR	      -11
#define SOCK_TIMEOUT	      -12

#define SOCK_DNS_ERROR        -14
#define SOCK_PROTOCOL_ERROR   -15

int net_setNonBlockingMode(int sock);

int net_reQueryMxRecord(char * domain, struct in_addr * mxIP);

int net_connectMX(char * domain, SOCKET * outSock);
int net_connect(char * host, int port, SOCKET * outSock, int localHost);

int net_sendBytes(int sock, char * buf, int bufSize, int *outLen, int timeout);
int net_recvBytes(int sock, char *buf, int bufSize, int * outLen, int timeout);
int net_recvSmtpResponse(int sock, int * smtpCode, int timeout);

int net_recvHttpResponse(int sock, char * response, int maxLen, int timeout);
void build_http_response(char * response, int contentLength);

int net_get_url(char * url, char * buf, int bufSize);
#endif