// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windns.h>
#include <WinInet.h>
#include <time.h>

#include "wdmain.h"
#include "log.h"
#include "network.h"
#define SLEEP_EX_INTERVAL 100

int net_reQueryMxRecord(char * domain, struct  in_addr * mxIP)
{
	DNS_STATUS dns_rc = 0;
	DNS_RECORD * pQueryResultsSet;	
	char smtp_fqdn[SMTP_MAX_FQDN+1] = {0};

	debug_log(LOG_DBG, "net_reQueryMxRecord(): querying mx records for email domain: \'%s\'", domain);

    dns_rc = DnsQuery_A(domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);

	if (dns_rc == 0)
	{
		strncpy_s(smtp_fqdn, sizeof(smtp_fqdn), pQueryResultsSet->Data.MX.pNameExchange,  strlen(pQueryResultsSet->Data.MX.pNameExchange));

		debug_log(LOG_DBG, "net_reQueryMxRecord(): email domain: \'%s\', mail server: \'%s\'", domain, smtp_fqdn);
		DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);

		debug_log(LOG_DBG, "net_reQueryMxRecord(): looking up an IP for \'%s\'", smtp_fqdn);
		dns_rc = DnsQuery_A(smtp_fqdn, DNS_TYPE_A, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);
		if (dns_rc == 0)
		{
			mxIP->S_un.S_addr = pQueryResultsSet->Data.A.IpAddress;

			debug_log(LOG_DBG, "net_reQueryMxRecord(): mail exchanger: \'%s\', ip: %s",  smtp_fqdn, inet_ntoa(*mxIP));
			DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);
		} else
		{
			debug_log(LOG_ERR, "net_reQueryMxRecord(): failed to resolve ip for mail exchanger: \'%s\', rc: %d)",  smtp_fqdn, dns_rc);
			debug_log(LOG_SVR, "net_reQueryMxRecord(): sending of email notifications will be disabled.");
			return SOCK_DNS_ERROR;
		}
	} else
	{
		debug_log(LOG_ERR, "net_reQueryMxRecord(): failed to resolve mx record for domain: \'%s\', rc: %d)",  domain, dns_rc);
		debug_log(LOG_SVR, "net_reQueryMxRecord(): sending of email notifications will be disabled.");
		return SOCK_DNS_ERROR;
	}

	return SOCK_NO_ERROR;
} // end of net_reQueryMxRecord()

int net_setNonBlockingMode(int sock)
{
	u_long mode = 1; // non-blocking

	if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
	{
		debug_log(LOG_ERR, "net_sendBytes(): putting socket into non-blocking mode failed: %d", WSAGetLastError());			
		return SOCK_IO_ERROR;
	}

	return SOCK_NO_ERROR;
} // end of net_setNonBlockingMode()


int net_get_url(char * url, char * buf, int bufSize)
{
	char * ua = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:11.0) Gecko/20100101 Firefox/11.0";
	HINTERNET hUrl = NULL;
	int rc = 0;
	DWORD recvBytes = 0;
	unsigned long dwCust  = 0;
	
	HINTERNET hInet = InternetOpenA(ua, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (hInet == NULL)
	{
		debug_log(LOG_ERR, "net_get_url(): failed to open Internet handle, rc: %d", WSAGetLastError());	
		return SOCK_IO_ERROR;
	}

	hUrl = InternetOpenUrl(hInet, url, NULL,0,INTERNET_FLAG_HYPERLINK, dwCust);

	if (hUrl == NULL)
	{
		debug_log(LOG_ERR, "net_get_url(): failed to open Url handle, rc: %d", WSAGetLastError());	
		InternetCloseHandle(hInet);		
		return SOCK_IO_ERROR;
	}

	memset(buf, 0, bufSize);
	rc = InternetReadFile(hUrl, buf, bufSize, &recvBytes);
	if (rc == FALSE)
	{
		debug_log(LOG_ERR, "net_get_url(): failed InternetReadFile() rc: %d", WSAGetLastError());	
		InternetCloseHandle(hUrl);		
		InternetCloseHandle(hInet);		
		return SOCK_IO_ERROR;
	}

	if (InternetCloseHandle(hUrl) == FALSE)
	{
		debug_log(LOG_ERR, "net_get_url(): failed to close hUrl handle, rc: %d", WSAGetLastError());	
		return SOCK_IO_ERROR;
	}

	if (InternetCloseHandle(hInet) == FALSE)
	{
		debug_log(LOG_ERR, "net_get_url(): failed to close hInet handle, rc: %d", WSAGetLastError());	
		InternetCloseHandle(hUrl);		
		return SOCK_IO_ERROR;
	}

	return SOCK_NO_ERROR;
} // end of net_get_url()

int net_connect(char * host, int port, SOCKET * outSock, int localHost)
{
	int retryCount = 0;
	fd_set writeFD;
	struct timeval tm_out;
	SOCKET sock;
	struct sockaddr_in serv;
	int rc = 0;
	struct hostent *ip;

	*outSock = INVALID_SOCKET;

	ip = (struct hostent *) gethostbyname(host);

	if (ip == NULL)
	{
		debug_log(LOG_ERR, "net_connect(): gethostbyname(%s) failed: %d", host, WSAGetLastError());

		return SOCK_DNS_ERROR;
	}

	// Create a new socket 
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) 
	{
		debug_log(LOG_ERR, "net_connect(): creating socket failed, rc: %d", WSAGetLastError());
		return SOCK_IO_ERROR;
	}

	if (net_setNonBlockingMode(sock) != SOCK_NO_ERROR)
	{
		debug_log(LOG_ERR, "net_connect(): putting socket into non-blocking mode failed");
		closesocket(sock);
		return SOCK_IO_ERROR;
	}
 
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr = *((struct in_addr *)ip->h_addr);
	serv.sin_port = htons(port);
	

	if (waitForShutdown(1)) 
	{
		closesocket(sock);
		return SOCK_WDOG_SHUTDOWN;
	}

	rc = connect(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr));
	if (rc == SOCKET_ERROR) 
	{
		rc = WSAGetLastError();

		if (rc == WSAEWOULDBLOCK)
		{
			if (waitForShutdown(1)) 
			{
				closesocket(sock);
				return SOCK_WDOG_SHUTDOWN;
			}

			FD_ZERO(&writeFD);
			FD_CLR(sock, &writeFD);
			FD_SET(sock, &writeFD);
			if (localHost)
				tm_out.tv_sec = SOCK_LOCAL_CONNECT_TIMEOUT;
			else
				tm_out.tv_sec = SOCK_CONNECT_TIMEOUT;
			tm_out.tv_usec = 0;

			rc = select(0,  0, &writeFD, 0, &tm_out);

			if (rc == 0)
			{
				debug_log(LOG_SVR, "net_connect(): host \'%s\' (%s) did not respond in %d seconds", host, inet_ntoa(serv.sin_addr), localHost ? SOCK_LOCAL_CONNECT_TIMEOUT : SOCK_CONNECT_TIMEOUT);
				closesocket(sock);
				return SOCK_TIMEOUT;
			}

		} else
		{
			debug_log(LOG_SVR, "net_connect(): connect() to \'%s\' (%s) failed: %d", host, inet_ntoa(serv.sin_addr), rc);
			closesocket(sock);
			return SOCK_IO_ERROR;
		}
	}

	if (localHost == 0)
		debug_log(LOG_INF, "net_connect(): connected to: \'%s@%d\'", host, port);

	*outSock = sock;
	return SOCK_NO_ERROR;
} // end of net_connect()


int net_connectMX(char * domain, SOCKET * outSock)
{
   	struct  in_addr ipAddr;
	int retryCount = 0;
	unsigned long ip = 0;
	fd_set writeFD;
	struct timeval tm_out;
	SOCKET sock;
	struct sockaddr_in serv;
	int rc = 0;

	*outSock = INVALID_SOCKET;

	if (net_reQueryMxRecord(domain, &ipAddr) != SOCK_NO_ERROR)
	{
		debug_log(LOG_ERR, "net_connectMX(): re-querying of MX for domain: %s failed", domain);
		return SOCK_DNS_ERROR;
	}

	// Create a new socket 
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) 
	{
		debug_log(LOG_ERR, "net_connectMX(): creating socket failed, rc: %d", WSAGetLastError());
		return SOCK_IO_ERROR;
	}

	if (net_setNonBlockingMode(sock) != SOCK_NO_ERROR)
	{
		debug_log(LOG_ERR, "net_connectMX(): putting socket into non-blocking mode failed");
		closesocket(sock);
		return SOCK_IO_ERROR;
	}
 
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port   = htons(25); // SMTP port 
	memcpy(&(serv.sin_addr), &ipAddr, sizeof(ipAddr));  // MX ip address
	//ip = inet_addr("127.0.0.1");
	//memcpy(&(serv.sin_addr), &ip, sizeof(ip));
	

	if (waitForShutdown(1)) 
	{
		closesocket(sock);
		return SOCK_WDOG_SHUTDOWN;
	}

	rc = connect(sock, (struct sockaddr *)&serv, sizeof(struct sockaddr));
	if (rc == SOCKET_ERROR) 
	{
		rc = WSAGetLastError();

		if (rc == WSAEWOULDBLOCK)
		{
			if (waitForShutdown(1)) 
			{
				closesocket(sock);
				return SOCK_WDOG_SHUTDOWN;
			}

			FD_ZERO(&writeFD);
			FD_CLR(sock, &writeFD);
			FD_SET(sock, &writeFD);
			tm_out.tv_sec = SOCK_CONNECT_TIMEOUT;
			tm_out.tv_usec = 0;

			rc = select(0,  0, &writeFD, 0, &tm_out);
			if (rc == 0)
			{
				debug_log(LOG_SVR, "net_connectMX(): host \'%s\' did not respond in %d seconds", inet_ntoa(serv.sin_addr), SOCK_CONNECT_TIMEOUT);
				closesocket(sock);
				return SOCK_TIMEOUT;
			}

		} else
		{
			debug_log(LOG_SVR, "net_connectMX(): connect() to \'%s\' failed: %d", inet_ntoa(serv.sin_addr), rc);
			closesocket(sock);
			return SOCK_IO_ERROR;
		}
	}

	debug_log(LOG_INF, "net_connectMX(): successfully connected to: \'%s@25\'", inet_ntoa(serv.sin_addr));

	*outSock = sock;
	return SOCK_NO_ERROR;
} // end of net_connectMX()

int net_sendBytes(int sock, char * buf, int bufSize, int *outLen, int timeout)
{
	WSAOVERLAPPED sendOverlapped;
	WSABUF dataBuf;

	DWORD bytesSent = 0;

	int index       = 0;
	DWORD flags     = 0;
	int rc          = 0;
	int lastError   = 0;

	if (bufSize < 1)
		return SOCK_NO_ERROR;

	// Make sure the RecvOverlapped struct is zeroed out
	SecureZeroMemory((PVOID) & sendOverlapped, sizeof (WSAOVERLAPPED));
	SecureZeroMemory((PVOID) & dataBuf, sizeof (WSABUF));

	// Create an event handle and setup an overlapped structure.
	sendOverlapped.hEvent = WSACreateEvent();
	if (sendOverlapped.hEvent == NULL) 
	{
		debug_log(LOG_ERR, "net_sendBytes(): WSACreateEvent failed: %d", WSAGetLastError());		
		closesocket(sock);
		return SOCK_IO_ERROR;
	}

	dataBuf.len = bufSize; 
	dataBuf.buf = buf;

	while (index < bufSize) 
	{		
		bytesSent = 0;
		flags = 0;

wait_for_send:
		rc = WSASend(sock, &dataBuf, 1, &bytesSent, flags, &sendOverlapped, NULL);	
		if (rc != 0)
		{
			if (rc == SOCKET_ERROR) 
			{
				lastError = WSAGetLastError();				

				if (lastError == WSAEWOULDBLOCK)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(sendOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_send;
				} else
				{
					if (lastError != WSA_IO_PENDING)
					{
						debug_log(LOG_ERR, "net_sendBytes(): WSASend failed with error: %d", lastError);
						closesocket(sock);
						WSACloseEvent(sendOverlapped.hEvent);
						return SOCK_IO_ERROR;					
					}
				}
			}

wait_for_io: 
			rc = WSAWaitForMultipleEvents(1, &sendOverlapped.hEvent, TRUE, timeout, TRUE);
			if (rc == WSA_WAIT_TIMEOUT)
			{
				debug_log(LOG_ERR, "net_sendBytes(): overlapped I/O timeout: %d", timeout);
				closesocket(sock);
				WSACloseEvent(sendOverlapped.hEvent);
				return SOCK_TIMEOUT;
			}
			if (rc == WSA_WAIT_IO_COMPLETION) 
			{
				if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
				{
					closesocket(sock);
					WSACloseEvent(sendOverlapped.hEvent);
					return SOCK_WDOG_SHUTDOWN;
				}

				SleepEx(SLEEP_EX_INTERVAL, TRUE);
				goto wait_for_io;
			}

wait_for_ov:
			rc = WSAGetOverlappedResult(sock, &sendOverlapped, &bytesSent, FALSE, &flags);
			if (rc == FALSE) 
			{
				lastError = WSAGetLastError();

				debug_log(LOG_ERR, "net_sendBytes(): WSAGetOverlappedResult failed with error: %d", lastError);

				if (lastError == WSA_IO_INCOMPLETE)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(sendOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}
					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_ov;
				}

				debug_log(LOG_ERR, "net_sendBytes(): overlapped I/O failed with error: %d", lastError);
				closesocket(sock);
				WSACloseEvent(sendOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}

			// ------------------------
			// Reset the signaled event
			// ------------------------
			rc = WSAResetEvent(sendOverlapped.hEvent);
			if (rc == FALSE) 
			{
				debug_log(LOG_ERR, "net_sendBytes(): WSAResetEvent failed with error = %d", WSAGetLastError());
				closesocket(sock);
				WSACloseEvent(sendOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}
		}

		index += bytesSent;

		dataBuf.len = bufSize-index;
		dataBuf.buf = buf+index;

	} // while (index < bufSize) 

	WSACloseEvent(sendOverlapped.hEvent);
	
	*outLen = index;

	return SOCK_NO_ERROR;
} // end of net_sendBytes()

int net_recvSmtpResponse(int sock, int * smtpCode, int timeout)
{
	WSAOVERLAPPED recvOverlapped;
	WSABUF dataBuf;

	DWORD recvBytes = 0;
	int index       = 0;
	DWORD flags     = 0;
	int rc          = 0;
	int lastError   = 0;

	int bytesLeft = SMTP_MAX_TEXT_LINE_LEN-1;
	int bufSize = SMTP_MAX_TEXT_LINE_LEN;

	char buf[SMTP_MAX_TEXT_LINE_LEN] = {0};
	char * p = NULL;

	// Make sure the RecvOverlapped struct is zeroed out
	SecureZeroMemory((PVOID) &recvOverlapped, sizeof (WSAOVERLAPPED));
	SecureZeroMemory((PVOID) &dataBuf, sizeof (WSABUF));
	SecureZeroMemory((PVOID) &buf, sizeof (buf));

	// Create an event handle and setup an overlapped structure.
	recvOverlapped.hEvent = WSACreateEvent();
	if (recvOverlapped.hEvent == NULL) 
	{
		debug_log(LOG_ERR, "net_recvSmtpResponse(): WSACreateEvent failed: %d", WSAGetLastError());	
		closesocket(sock);
		return SOCK_IO_ERROR;
	}

	while (index < bufSize) 
	{		
		recvBytes = 0;
		flags = 0;

		dataBuf.len = 1; 
	    dataBuf.buf = &buf[index];

wait_for_recv:  // submit the request

		rc = WSARecv(sock, &dataBuf, 1, &recvBytes, &flags, &recvOverlapped, NULL);
		if (rc != 0)
		{
			if (rc == SOCKET_ERROR) 
			{
				lastError = WSAGetLastError();

				if (lastError == WSAEWOULDBLOCK)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_recv;
				} else
				{
					if (lastError != WSA_IO_PENDING)
					{
						debug_log(LOG_ERR, "net_recvSmtpResponse(): WSARecv failed with error: %d", lastError);
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_IO_ERROR;					
					}
				}
			}

wait_for_io: // Wait for I/O completion

			rc = WSAWaitForMultipleEvents(1, &recvOverlapped.hEvent, TRUE, timeout, TRUE);
			if (rc == WSA_WAIT_TIMEOUT)
			{
				debug_log(LOG_ERR, "net_recvSmtpResponse(): overlapped I/O timeout: %d", timeout);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_TIMEOUT;
			}
			if (rc == WSA_WAIT_IO_COMPLETION) 
			{
				if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
				{
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_WDOG_SHUTDOWN;
				}

				SleepEx(SLEEP_EX_INTERVAL, TRUE);
				goto wait_for_io;
			}

wait_for_ov: // Check the result of the I/O overlapped operation

			rc = WSAGetOverlappedResult(sock, &recvOverlapped, &recvBytes, FALSE, &flags);
			if (rc == FALSE) 
			{
				lastError = WSAGetLastError();
				if (lastError == WSA_IO_INCOMPLETE)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_ov;
				}

				debug_log(LOG_ERR, "net_recvSmtpResponse(): overlapped I/O failed with error: %d", lastError);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}

			// ------------------------
			// Reset the signaled event
			// ------------------------
			rc = WSAResetEvent(recvOverlapped.hEvent);
			if (rc == FALSE) 
			{
				debug_log(LOG_ERR, "net_recvSmtpResponse(): WSAResetEvent failed with error = %d", WSAGetLastError());
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}
		}

		// Check what we received...

		if (buf[index] == '\r')
		{
			index += recvBytes;

			recvBytes = 0;
			flags = 0;

			dataBuf.len = 1; 
			dataBuf.buf = &buf[index];

			// Receive another byte
wait_for_recv2:
			rc = WSARecv(sock, &dataBuf, 1, &recvBytes, &flags, &recvOverlapped, NULL);
			if (rc != 0)
			{
				if (rc == SOCKET_ERROR) 
				{
					lastError = WSAGetLastError();

					if (lastError == WSAEWOULDBLOCK)
					{
						if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
						{
							closesocket(sock);
							WSACloseEvent(recvOverlapped.hEvent);
							return SOCK_WDOG_SHUTDOWN;
						}

						SleepEx(SLEEP_EX_INTERVAL, TRUE);
						goto wait_for_recv2;
					} else
					{
						if (lastError != WSA_IO_PENDING)
						{
							debug_log(LOG_ERR, "net_recvSmtpResponse(): WSARecv failed with error: %d", lastError);
							closesocket(sock);
							WSACloseEvent(recvOverlapped.hEvent);
							return SOCK_IO_ERROR;					
						}
					}
				}
wait_for_io2: 
				rc = WSAWaitForMultipleEvents(1, &recvOverlapped.hEvent, TRUE, timeout, TRUE);
				if (rc == WSA_WAIT_TIMEOUT)
				{
					debug_log(LOG_ERR, "net_recvSmtpResponse(): overlapped I/O timeout: %d", timeout);
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_TIMEOUT;
				}
				if (rc == WSA_WAIT_IO_COMPLETION) 
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_io2;
				}

wait_for_ov2:
				rc = WSAGetOverlappedResult(sock, &recvOverlapped, &recvBytes, FALSE, &flags);
				if (rc == FALSE) 
				{
					lastError = WSAGetLastError();
					if (lastError == WSA_IO_INCOMPLETE)
					{
						if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
						{
							closesocket(sock);
							WSACloseEvent(recvOverlapped.hEvent);
							return SOCK_WDOG_SHUTDOWN;
						}

						SleepEx(SLEEP_EX_INTERVAL, TRUE);
						goto wait_for_ov2;
					}

					debug_log(LOG_ERR, "net_recvSmtpResponse(): overlapped I/O failed with error: %d", lastError);
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_IO_ERROR;
				}

				// ------------------------
				// Reset the signaled event
				// ------------------------
				rc = WSAResetEvent(recvOverlapped.hEvent);
				if (rc == FALSE) 
				{
					debug_log(LOG_ERR, "net_recvSmtpResponse(): WSAResetEvent failed with error = %d", WSAGetLastError());
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_IO_ERROR;
				}
			}


			if ( buf[index] == '\n')
			{
				// we got to the end of line (CRLF found)
				index += recvBytes;

				if (buf[3] == '-')
				{
					net_recvSmtpResponse(sock, smtpCode, timeout); // receive another line in multiline replies
				} // endif 

				break;
			}
		} else
		{
			// Continue receiving the line until we get CRLF, we need to receive those bytes to cler the buffer.
			index += recvBytes;

			recvBytes = 0;
			flags = 0;

			dataBuf.len = 1; 
			dataBuf.buf = &buf[index];
		}

	} // while (index < bufSize) 

    buf[3] = 0; // get frist three bytes
    *smtpCode = atoi(buf);

	WSACloseEvent(recvOverlapped.hEvent);

	return SOCK_NO_ERROR;
} // end of "net_recvSmtpResponse()" 


int net_recvBytes(int sock, char *buf, int bufSize, int * outLen, int timeout)
{
	WSAOVERLAPPED recvOverlapped;
	WSABUF dataBuf;

	DWORD recvBytes = 0;
	int index       = 0;
	DWORD flags     = 0;
	int rc          = 0;
	int lastError   = 0;

	// Make sure the RecvOverlapped struct is zeroed out
	SecureZeroMemory((PVOID) & recvOverlapped, sizeof (WSAOVERLAPPED));
	SecureZeroMemory((PVOID) & dataBuf, sizeof (WSABUF));

	// Create an event handle and setup an overlapped structure.
	recvOverlapped.hEvent = WSACreateEvent();
	if (recvOverlapped.hEvent == NULL) 
	{
		debug_log(LOG_ERR, "net_recvBytes(): WSACreateEvent failed: %d", WSAGetLastError());			
		return SOCK_IO_ERROR;
	}

	dataBuf.len = bufSize-1; // one for ending null
	dataBuf.buf = buf;

	*buf = '\0';
	while (index < bufSize) 
	{		
		recvBytes = 0;
		flags = 0;

wait_for_recv:
		rc = WSARecv(sock, &dataBuf, 1, &recvBytes, &flags, &recvOverlapped, NULL);
		if (rc != 0)
		{
			if (rc == SOCKET_ERROR) 
			{
				lastError = WSAGetLastError();

				if (lastError == WSAEWOULDBLOCK)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_recv;
				} else
				{
					if (lastError != WSA_IO_PENDING)
					{
						debug_log(LOG_ERR, "net_recvBytes(): WSARecv failed with error: %d", lastError);
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_IO_ERROR;					
					}
				}
			}

wait_for_io: 
			rc = WSAWaitForMultipleEvents(1, &recvOverlapped.hEvent, TRUE, timeout, TRUE);
			if (rc == WSA_WAIT_TIMEOUT)
			{
				debug_log(LOG_ERR, "net_recvBytes(): overlapped I/O timeout: %d", timeout);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_TIMEOUT;
			}
			if (rc == WSA_WAIT_IO_COMPLETION) 
			{
				if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
				{
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_WDOG_SHUTDOWN;
				}

				SleepEx(SLEEP_EX_INTERVAL, TRUE);
				goto wait_for_io;
			}

wait_for_ov:
			rc = WSAGetOverlappedResult(sock, &recvOverlapped, &recvBytes, FALSE, &flags);
			if (rc == FALSE) 
			{
				lastError = WSAGetLastError();
				if (lastError == WSA_IO_INCOMPLETE)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_ov;
				}

				debug_log(LOG_ERR, "net_recvBytes(): overlapped I/O failed with error: %d", lastError);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}

			// ------------------------
			// Reset the signaled event
			// ------------------------
			rc = WSAResetEvent(recvOverlapped.hEvent);
			if (rc == FALSE) 
			{
				debug_log(LOG_ERR, "net_recvBytes(): WSAResetEvent failed with error = %d", WSAGetLastError());
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}
		}

		if (recvBytes == 0)
			break;

		if ((index + (int) recvBytes) > bufSize)
			break;

		index += recvBytes;

		buf[index] = '\0';

		if ((bufSize-index) <= 0)
			break;

		dataBuf.len = bufSize-index;
		dataBuf.buf = buf+index;
	} // while (index < bufSize) 

	WSACloseEvent(recvOverlapped.hEvent);
	
	*outLen = index;

	return SOCK_NO_ERROR;
} // end of net_recvBytes()

int net_recvHttpLine(int sock, char * line, int maxLineLen, int timeout)
{
	WSAOVERLAPPED recvOverlapped;
	WSABUF dataBuf;

	DWORD recvBytes = 0;
	int index       = 0;
	DWORD flags     = 0;
	int rc          = 0;
	int lastError   = 0;

	time_t start = 0;
	time_t end   = 0;

	char * p = line;

	time(&start);

	// Make sure the RecvOverlapped struct is zeroed out
	SecureZeroMemory((PVOID) &recvOverlapped, sizeof (WSAOVERLAPPED));
	SecureZeroMemory((PVOID) &dataBuf, sizeof (WSABUF));

	// Create an event handle and setup an overlapped structure.
	recvOverlapped.hEvent = WSACreateEvent();
	if (recvOverlapped.hEvent == NULL) 
	{
		debug_log(LOG_ERR, "net_recvHttpLine(): WSACreateEvent failed: %d", WSAGetLastError());			
		closesocket(sock);
		return SOCK_IO_ERROR;
	}

	while (index < maxLineLen) 
	{		
		recvBytes = 0;
		flags = 0;

		dataBuf.len = 1; 
	    dataBuf.buf = p;

wait_for_recv:  // submit the request

		rc = WSARecv(sock, &dataBuf, 1, &recvBytes, &flags, &recvOverlapped, NULL);
		if (rc != 0)
		{
			if (rc == SOCKET_ERROR) 
			{
				lastError = WSAGetLastError();

				if (lastError == WSAEWOULDBLOCK)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_recv;
				} else
				{
					if (lastError != WSA_IO_PENDING)
					{
						debug_log(LOG_ERR, "net_recvHttpLine(): WSARecv failed with error: %d", lastError);
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_IO_ERROR;					
					}
				}
			}

wait_for_io: // Wait for I/O completion

			rc = WSAWaitForMultipleEvents(1, &recvOverlapped.hEvent, TRUE, timeout, TRUE);
			if (rc == WSA_WAIT_TIMEOUT)
			{
				debug_log(LOG_ERR, "net_recvHttpLine(): overlapped I/O timeout: %d", timeout);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_TIMEOUT;
			}
			if (rc == WSA_WAIT_IO_COMPLETION) 
			{
				if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
				{
					closesocket(sock);
					WSACloseEvent(recvOverlapped.hEvent);
					return SOCK_WDOG_SHUTDOWN;
				}

				SleepEx(SLEEP_EX_INTERVAL, TRUE);
				goto wait_for_io;
			}

wait_for_ov: // Check the result of the I/O overlapped operation

			rc = WSAGetOverlappedResult(sock, &recvOverlapped, &recvBytes, FALSE, &flags);
			if (rc == FALSE) 
			{
				lastError = WSAGetLastError();
				if (lastError == WSA_IO_INCOMPLETE)
				{
					if (waitForShutdown(WAIT_FOR_SHUTDOWN_INTERVAL)) 
					{
						closesocket(sock);
						WSACloseEvent(recvOverlapped.hEvent);
						return SOCK_WDOG_SHUTDOWN;
					}

					SleepEx(SLEEP_EX_INTERVAL, TRUE);
					goto wait_for_ov;
				}

				debug_log(LOG_ERR, "net_recvHttpLine(): overlapped I/O failed with error: %d", lastError);
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}

			// ------------------------
			// Reset the signaled event
			// ------------------------
			rc = WSAResetEvent(recvOverlapped.hEvent);
			if (rc == FALSE) 
			{
				debug_log(LOG_ERR, "net_recvHttpLine(): WSAResetEvent failed with error = %d", WSAGetLastError());
				closesocket(sock);
				WSACloseEvent(recvOverlapped.hEvent);
				return SOCK_IO_ERROR;
			}
		}

		// Check what we received...

		if (*p == '\n')
		{
			p += recvBytes;
			index += recvBytes;
			break;
		} else
		{
			// Continue receiving the line until we get LF, we need to receive those bytes to cler the buffer.
			p += recvBytes;
			index += recvBytes;

			recvBytes = 0;
			flags = 0;

			dataBuf.len = 1; 
			dataBuf.buf = p;
		}

		time(&end);
		if (end-start > timeout/1000)
		{
			debug_log(LOG_SVR, "net_recvHttpLine(): timeout waiting for LF character, probably a telnet session pretending to be an HTTP client");
			closesocket(sock);
			WSACloseEvent(recvOverlapped.hEvent);
			return SOCK_IO_ERROR;
		}

	} // while (index < maxLineLen) 

	WSACloseEvent(recvOverlapped.hEvent);

	if (index < maxLineLen)
		return SOCK_NO_ERROR;

	return SOCK_PROTOCOL_ERROR;
} // end of "net_recvHttpLine()" 

int net_recvHttpResponse(int sock, char * response, int maxLen, int timeout)
{
	char line[SOCK_MAX_HTTP_LINE+1];
	int rc = 0;
	int len = 0;
	time_t start = 0;
	time_t end   = 0;

	time(&start);

	for(;;)
	{
		memset(line, 0, sizeof(line));
		rc = net_recvHttpLine(sock, line, SOCK_MAX_HTTP_LINE, timeout);
		//debug_log("net_recvHttpResponse(): net_recvHttpLine() line: %s", line);
		if ( rc != SOCK_NO_ERROR) 
		{
			debug_log(LOG_ERR, "net_recvHttpResponse(): net_recvHttpLine() failed: %d", rc);
			return SOCK_PROTOCOL_ERROR;
		}

		len = strlen(line);
		if (len > 3)
		{
			if (line[0] == 'G' && line[1] == 'E' && line[2] == 'T')
			{
				strcpy(response, line);
				//break;
			}
		} else
		{
			if (len == 1 && line[0] == '\n')
				break;
			if (len == 2 && line[0] == '\r' && line[1] == '\n')
				break;
		}

		time(&end);
		if (end-start > SOCK_HTTP_INCOMING_REQUEST_TIMEOUT)
		{
			debug_log(LOG_SVR, "net_recvHttpResponse(): %d seconds timeout waiting for HTTP request headers; probably a telnet session pretending to be an HTTP client", SOCK_HTTP_INCOMING_REQUEST_TIMEOUT);
			closesocket(sock);
			return SOCK_IO_ERROR;
		}
		SleepEx(SLEEP_EX_INTERVAL, TRUE);
	}
	return SOCK_NO_ERROR;
} // end of net_recvHttpResponse()

void build_http_response(char * response, int contentLength)
{
	char dateStr[32];

	build_smtp_date(dateStr);

	sprintf( response, 
		     "HTTP/1.0 200 OK\r\nDate: %s\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n",
		     dateStr,
			 contentLength
			);
} // end of build_http_response()