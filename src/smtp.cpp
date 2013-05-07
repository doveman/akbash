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
#include <Mswsock.h>
#include <time.h>
#include <math.h>
#include <process.h>

#include "smtp.h"
#include "log.h"
#include "listen.h"
#include "wdmain.h"
#include "network.h"
#include "miner_monitor.h"

#define SMTP_RETRY_DELAY   5000
#define SMTP_RETRY_COUNT   10

#define SMTP_MAX_MSG_SIZE    8192

#define HELO_STR "EHLO %s\r\n\x0"
#define MAIL_STR "MAIL FROM:<%s>\r\n\x0"
#define RCPT_STR "RCPT TO:<%s>\r\n\x0"
#define DATA_STR "DATA\r\n\x0"
#define QUIT_STR "QUIT\r\n\x0"

// date, email, email, email, email, msg ID, ip, restart datatime, version, ip, email, restart datetime
#define MSG_FORMAT_WDOG_RESTART "Date: %s\r\nFrom: <%s>\r\nSender: <%s>\r\nReply-To: <%s>\r\nTo: <%s>\r\nMessage-ID: %s\r\nSubject: akbash (%s): watchdog has been restarted at: %s\r\nKeywords: akbash, cgminer, bitcoin mining, BTC\r\nImportance: High\r\nPriority: urgent\r\nLanguage: en\r\nContent-Type: text/html; charset=UTF-8\r\nX-Mailer: Akbash ver. %s\r\nX-Mailer-Info: http://www.petermoss.com/akbash \r\nX-Originating-IP: %s\r\nXPriority: 1\r\nX-Sender: %s\r\n\r\n%s\r\n.\r\n"

// date, email, email, email, email, msg ID, ip, version, ip, email, restart date time
#define MSG_FORMAT_RESTART "Date: %s\r\nFrom: <%s>\r\nSender: <%s>\r\nReply-To: <%s>\r\nTo: <%s>\r\nMessage-ID: %s\r\nSubject: akbash (%s): miner has been restarted at: %s\r\nKeywords: akbash, cgminer, bitcoin mining, BTC\r\nImportance: High\r\nPriority: urgent\r\nLanguage: en\r\nContent-Type: text/html; charset=UTF-8\r\nX-Mailer: Akbash ver. %s\r\nX-Mailer-Info: http://www.petermoss.com/akbash \r\nX-Originating-IP: %s\r\nXPriority: 1\r\nX-Sender: %s\r\n\r\n%s\r\n.\r\n"

// date, email, email, email, email, msg ID, ip, version, ip, email, reboot date time
#define MSG_FORMAT_REBOOT "Date: %s\r\nFrom: <%s>\r\nSender: <%s>\r\nReply-To: <%s>\r\nTo: <%s>\r\nMessage-ID: %s\r\nSubject: akbash (%s): OS is about to be rebooted\r\nKeywords: akbash, cgminer, bitcoin mining, BTC\r\nImportance: High\r\nPriority: urgent\r\nLanguage: en\r\nContent-Type: text/html; charset=UTF-8\r\nX-Mailer: Akbash ver. %s\r\nX-Mailer-Info: http://www.petermoss.com/akbash \r\nX-Originating-IP: %s\r\nXPriority: 1\r\nX-Sender: %s\r\n\r\n%s\r\n.\r\n"

// date, email, email, email, email, msg ID, ip, version, ip, email, status msg
#define MSG_FORMAT_STATUS "Date: %s\r\nFrom: <%s>\r\nSender: <%s>\r\nReply-To: <%s>\r\nTo: <%s>\r\nMessage-ID: %s\r\nSubject: akbash (%s): avg: %.2f, hw: %d\r\nKeywords: cgminer, bitcoin mining, BTC\r\nImportance: High\r\nPriority: urgent\r\nLanguage: en\r\nContent-Type: text/html; charset=UTF-8\r\nX-Mailer: Akbash ver. %s\r\nX-Mailer-Info: http://www.petermoss.com/akbash \r\nX-Originating-IP: %s\r\nXPriority: 1\r\nX-Sender: %s\r\n\r\n%s\r\n.\r\n"

char _smtp_email[SMTP_MAX_USER_LEN];
char _smtp_email_domain[SMTP_MAX_FQDN];

char _smtp_localhost_name[SMTP_MAX_HOST_NAME_LEN];
int _smtp_disable_email_notifications = 0;

typedef struct _emailMsg
{
	int    msgType;
	char * msg;
} EmailMsg;

#define PROCESS_RESTART_EMAIL_TYPE 1
#define WDOG_RESTART_EMAIL_TYPE    2
#define OS_REBOOT_EMAIL_TYPE       3
#define WDOG_STATUS_EMAIL_TYPE     4

void build_smtp_date(char *buf);    // buf should be SMTP_MAX_DATE_LEN bytes
void build_smtp_msgId(char * buf);  // buf should be SMTP_MAX_MSGID_LEN bytes
//DWORD WINAPI sendEmailThread(LPVOID pvMsg);
void sendEmailThread(void * pvMsg);

void send_smtp_wdog_restarted_msg(void)
{
	char buf[SMTP_MAX_MSG_SIZE] = {0};
	char dateStr[SMTP_MAX_DATE_LEN] = {0};
	char msgId[SMTP_MAX_MSGID_LEN] = {0};
	char * pMsg = NULL;
	int msgLen = 0;
	char msgBody[REASONS_STRING_MAX_LENGTH*2+1];
	EmailMsg * msg = NULL;

	if (_smtp_disable_email_notifications == 1) 
	{
		debug_log(LOG_SVR, "send_smtp_wdog_restarted_msg(): sending of email notifications is disabled.");
		return;
	} else
	{
		debug_log(LOG_INF, "send_smtp_wdog_restarted_msg(): attempting to send \'watchdog restarted\' email notification.");
	}

	memset(buf, 0, sizeof(buf));
	memset(dateStr, 0, sizeof(dateStr));
	memset(msgId, 0, sizeof(msgId));
	memset(msgBody, 0, sizeof(msgBody));

	build_smtp_msgId(msgId);
	build_smtp_date(dateStr);

	sprintf( msgBody, 
		     "<html><body><font face=\"Courier\">akbash watchdog has been restarted at: <br><br>%s<br><br>%s<br></font></body></html>",
			 startedOn,			 
			 SEND_DONATIONS_TO
		   );

	// date, email, email, email, email, msg ID, ip, restart datatime, version, ip, email, restart datetime
	sprintf( buf, 
		     MSG_FORMAT_WDOG_RESTART, 
			 dateStr,
			 _smtp_email,_smtp_email,_smtp_email,_smtp_email,
			 msgId,
			 cfg->wdogRigName,
			 startedOn,
			 WDOG_VERSION,
			 cfg->wdogListenIP,
			_smtp_email,
			msgBody
		);

	msgLen = strlen(buf);
	pMsg = (char *) calloc(1, msgLen+1);
	if (pMsg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_wdog_restarted_msg(): failed to allocate memory for message object.");
		return;
	}

	strncpy_s(pMsg, msgLen+1, buf, msgLen);

	msg = (EmailMsg *) calloc(1, sizeof(EmailMsg));
	if (msg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_wdog_restarted_msg(): failed to allocate memory for EmailMsg object.");
		return;
	}

	msg->msgType = WDOG_RESTART_EMAIL_TYPE;
	msg->msg = pMsg;

	_beginthread( sendEmailThread, 0, (void *) msg );
} // end of send_smtp_wdog_restarted_msg()

void send_smtp_restarted_msg(int reason)
{
	char buf[SMTP_MAX_MSG_SIZE] = {0};
	char dateStr[SMTP_MAX_DATE_LEN] = {0};
	char msgId[SMTP_MAX_MSGID_LEN] = {0};
	char restartedAt[32] = {0};
	char * pMsg = NULL;
	int msgLen = 0;
	char reasonsStr[REASONS_STRING_MAX_LENGTH+1];
	char msgBody[REASONS_STRING_MAX_LENGTH*2+1];
	EmailMsg * msg = NULL;

	time_t stTime;
	struct tm * stTimeTm;

	if (_smtp_disable_email_notifications == 1) 
	{
		debug_log(LOG_SVR, "send_smtp_restarted_msg(): sending of email notifications is disabled.");
		return;
	} else
	{
		debug_log(LOG_INF, "send_smtp_restarted_msg(): attempting to send \'miner restarted\' email notification.");
	}

	memset(buf, 0, sizeof(buf));
	memset(dateStr, 0, sizeof(dateStr));
	memset(msgId, 0, sizeof(msgId));
	memset(restartedAt, 0, sizeof(restartedAt));
	memset(reasonsStr, 0, sizeof(reasonsStr));
	memset(msgBody, 0, sizeof(msgBody));

	build_smtp_msgId(msgId);
	build_smtp_date(dateStr);

	// date, email, email, email, email, msg ID, ip, version, ip, email, restart date time

	time(&stTime);
	stTimeTm = localtime(&stTime);

	sprintf( restartedAt, 
		     "%02d/%02d/%04d %02d:%02d:%02d",
			 stTimeTm->tm_mon+1,
			 stTimeTm->tm_mday,
			 stTimeTm->tm_year+1900,
			 stTimeTm->tm_hour,
			 stTimeTm->tm_min,
			 stTimeTm->tm_sec 
		   );


	formatReasons(reasonsStr, sizeof(reasonsStr), reason);

	sprintf( msgBody, 
		     "<html><body><font face=\"Courier\">Miner process has been restarted at: <br><br>%s<br><br>due the following reason:<br><br>%s<br><br>%s</font></body></html>",
			 restartedAt,
			 reasonsStr,
			 SEND_DONATIONS_TO
		   );
		    
	sprintf( buf, MSG_FORMAT_RESTART, 
				dateStr,
				_smtp_email,_smtp_email,_smtp_email,_smtp_email,
				msgId,
				cfg->wdogRigName,
				restartedAt,
				WDOG_VERSION,
				cfg->wdogListenIP,
				_smtp_email,
				msgBody
			);

	msgLen = strlen(buf);
	pMsg = (char *) calloc(1, msgLen+1);
	if (pMsg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_restarted_msg(): failed to allocate memory for message object.");
		return;
	}

	strncpy_s(pMsg, msgLen+1, buf, msgLen);

	msg = (EmailMsg *) calloc(1, sizeof(EmailMsg));
	if (msg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_restarted_msg(): failed to allocate memory for EmailMsg object.");
		return;
	}
		
	msg->msgType = PROCESS_RESTART_EMAIL_TYPE;
	msg->msg = pMsg;

	_beginthread( sendEmailThread, 0, (void *) msg );	
} // end of send_smtp_restarted_msg()

void send_smtp_rebooted_msg(int reason)
{
	char buf[SMTP_MAX_MSG_SIZE] = {0};
	char dateStr[SMTP_MAX_DATE_LEN] = {0};
	char msgId[SMTP_MAX_MSGID_LEN] = {0};
	char rebootedAt[32] = {0};
	char * pMsg = NULL;
	int msgLen = 0;
	char reasonsStr[REASONS_STRING_MAX_LENGTH+1];
	char msgBody[REASONS_STRING_MAX_LENGTH*2+1];
	EmailMsg * msg = NULL;

	time_t stTime;
	struct tm * stTimeTm;

	if (_smtp_disable_email_notifications == 1) 
	{
		debug_log(LOG_SVR, "send_smtp_rebooted_msg(): sending of email notifications is disabled.");
		return;
	} else
	{
		debug_log(LOG_SVR, "send_smtp_rebooted_msg(): attempting to send \'OS is being rebooted\' email notification.");
	}

	memset(buf, 0, sizeof(buf));
	memset(dateStr, 0, sizeof(dateStr));
	memset(msgId, 0, sizeof(msgId));
	memset(rebootedAt, 0, sizeof(rebootedAt));
	memset(reasonsStr, 0, sizeof(reasonsStr));
	memset(msgBody, 0, sizeof(msgBody));
	build_smtp_msgId(msgId);
	build_smtp_date(dateStr);

	// date, email, email, email, email, msg ID, ip, version, ip, email, restart date time

	time(&stTime);
	stTimeTm = localtime(&stTime);

	sprintf( rebootedAt, 
		     "%02d/%02d/%04d %02d:%02d:%02d",
			 stTimeTm->tm_mon+1,
			 stTimeTm->tm_mday,
			 stTimeTm->tm_year+1900,
			 stTimeTm->tm_hour,
			 stTimeTm->tm_min,
			 stTimeTm->tm_sec 
		   );

	formatReasons(reasonsStr, sizeof(reasonsStr), reason);

	sprintf( msgBody, 
		     "<html><body><font face=\"Courier\">OS is about to be rebooted at: <br><br>%s<br><br> due the following reason:<br><br>%s<br><br>%s</font></body></html>",
			 rebootedAt,
			 reasonsStr,
			 SEND_DONATIONS_TO
		   );


	sprintf( buf, MSG_FORMAT_REBOOT, 
				dateStr,
				_smtp_email,_smtp_email,_smtp_email,_smtp_email,
				msgId,
				cfg->wdogRigName,
				WDOG_VERSION,
				cfg->wdogListenIP,
				_smtp_email,
				msgBody
			);

	msgLen = strlen(buf);
	pMsg = (char *) calloc(1, msgLen+1);
	if (pMsg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_rebooted_msg(): failed to allocate memory for message object.");
		return;
	}

	strncpy_s(pMsg, msgLen+1, buf, msgLen);

	msg = (EmailMsg *) calloc(1, sizeof(EmailMsg));
	if (msg == NULL)
	{
		debug_log(LOG_ERR, "send_smtp_rebooted_msg(): failed to allocate memory for EmailMsg object.");
		return;
	}

	msg->msgType = OS_REBOOT_EMAIL_TYPE;
	msg->msg = pMsg;
		
	sendEmailThread((void*) msg);
} // end of send_smtp_rebooted_msg()

void build_smtp_msgId(char * buf)  // buf should be SMTP_MAX_MSGID_LEN bytes
{
	__int64 ltime = 0;
	DWORD tick = GetTickCount();
	DWORD tid = GetCurrentThreadId();
	DWORD pid = GetCurrentProcessId();

	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

	char localhost_name[SMTP_MAX_HOST_NAME_LEN+1] = {0};
	char fqdn_name[SMTP_MAX_HOST_NAME_LEN+1] = {0};
	struct hostent * FAR host = NULL;

	int rc = gethostname(localhost_name, SMTP_MAX_HOST_NAME_LEN);
		
	if (rc == SOCKET_ERROR)
	{
		int rc = WSAGetLastError();
		strcpy(localhost_name, "localhost");
	}

	host = (struct hostent * FAR) gethostbyname(localhost_name);
	if (host)
	{
		strcpy(fqdn_name, host->h_name);
	} else
	{
		strcpy(fqdn_name, localhost_name);
	}

	GetSystemTimes(&idleTime, &kernelTime, &userTime);
	_time64(&ltime);

	sprintf( buf, "<%06d.%06d.%08d.%03d%03d%03d.%012d@",
		     tid, pid, tick, 
		     idleTime.dwHighDateTime,
		     kernelTime.dwHighDateTime,
		     userTime.dwHighDateTime,
		     ltime
		   );
	strcat(buf, fqdn_name);
	strcat(buf,">");
} // end of build_smtp_msgId()

// Build SMTP date field in the following format: Mon, 20 Jun 12 10:59:59 -0500
void build_smtp_date(char *buf)  // buf should be SMTP_MAX_DATE_LEN bytes
{
    int secondsDiff = _timezone;
	long hours = 0;
	div_t divResult;
	int tzHours = 0;
	int tzMinutes = 0;
	char tzBuf[6] = {0};
	struct tm *newtime;
	__int64 ltime;
	char wday[4] = {0};
	char mon[4] = {0};

	divResult = div((int) secondsDiff, (int) 3600);
	tzHours = divResult.quot;
	tzMinutes = divResult.rem*60;
	sprintf(tzBuf, "-%02d%02d", divResult.quot, divResult.rem/60);
	tzBuf[5] = 0;

	_time64( &ltime );

	// Obtain coordinated universal time:
	newtime = _localtime64( &ltime ); // C4996

	if (newtime->tm_wday == 0) strcpy(wday, "Sun");
	if (newtime->tm_wday == 1) strcpy(wday, "Mon");
	if (newtime->tm_wday == 2) strcpy(wday, "Tue");
	if (newtime->tm_wday == 3) strcpy(wday, "Wed");
	if (newtime->tm_wday == 4) strcpy(wday, "Thu");
	if (newtime->tm_wday == 5) strcpy(wday, "Fri");
	if (newtime->tm_wday == 6) strcpy(wday, "Sat");

	if (newtime->tm_mon == 0) strcpy(mon, "Jan");
	if (newtime->tm_mon == 1) strcpy(mon, "Feb");
	if (newtime->tm_mon == 2) strcpy(mon, "Mar");
	if (newtime->tm_mon == 3) strcpy(mon, "Apr");
	if (newtime->tm_mon == 4) strcpy(mon, "May");
	if (newtime->tm_mon == 5) strcpy(mon, "Jun");
	if (newtime->tm_mon == 6) strcpy(mon, "Jul");
	if (newtime->tm_mon == 7) strcpy(mon, "Aug");
	if (newtime->tm_mon == 8) strcpy(mon, "Sep");
	if (newtime->tm_mon == 9) strcpy(mon, "Oct");
	if (newtime->tm_mon == 10) strcpy(mon, "Nov");
	if (newtime->tm_mon == 11) strcpy(mon, "Dec");

	sprintf( buf, 
		     "%s, %02d %s %02d %02d:%02d:%02d %s",
			 wday,
			 newtime->tm_mday,
			 mon,
			 newtime->tm_year-100,
			 newtime->tm_hour,
			 newtime->tm_min,
			 newtime->tm_sec,
			 tzBuf
		   );
} // end of build_smtp_date()

int initSmtp(char * email, int disableFlag)
{
	char * p = NULL;
	int domainLen = 0;	
	DNS_STATUS dns_rc = 0;
	DNS_RECORD * pQueryResultsSet;	
	char smtp_fqdn[SMTP_MAX_FQDN] = {0};

	if (disableFlag == 1)
	{
		debug_log(LOG_SVR, "initSmtp(): sending of email notifications will be disabled. disableFlag: %d", disableFlag);
		_smtp_disable_email_notifications = 1;
		return 1;
	} else
		_smtp_disable_email_notifications = 0;

	if (email == NULL || strlen(email) < 2)
	{
		debug_log(LOG_SVR, "initSmtp(): email address: \'%s\' does not to be valid)", email == NULL ? "n/a" : email);
		debug_log(LOG_SVR, "initSmtp(): sending of email notifications will be disabled.");
		_smtp_disable_email_notifications = 1;
		return 2;
	}

	strcpy_s(_smtp_email, sizeof(_smtp_email), email);

	p = strstr(_smtp_email, "@");
	if (p == NULL)
	{
		debug_log(LOG_SVR, "initSmtp(): unable to parse domain from email address: \'%s\' (_smtp_email: \'%s\')", email, _smtp_email);
		debug_log(LOG_SVR, "initSmtp(): sending of email notifications will be disabled.");
		_smtp_disable_email_notifications = 1;
		return 2;
	}

	p += 1;  //  read to the end of string

	domainLen = strlen(p);
	
	strcpy_s(_smtp_email_domain, sizeof(_smtp_email_domain), p);

	debug_log(LOG_DBG, "initSmtp(): querying mx records for email domain: \'%s\'", _smtp_email_domain);

    dns_rc = DnsQuery_A(_smtp_email_domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);

	if (dns_rc == 0)
	{
		strncpy_s(smtp_fqdn, sizeof(smtp_fqdn), pQueryResultsSet->Data.MX.pNameExchange,  strlen(pQueryResultsSet->Data.MX.pNameExchange));

		debug_log(LOG_DBG, "initSmtp(): email domain: \'%s\', mail server: \'%s\'", _smtp_email_domain, smtp_fqdn);
		DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);

		debug_log(LOG_DBG, "initSmtp(): looking up an IP for \'%s\'", smtp_fqdn);
		dns_rc = DnsQuery_A(smtp_fqdn, DNS_TYPE_A, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);
		if (dns_rc == 0)
		{
			struct  in_addr ipAddr;
			ipAddr.S_un.S_addr = pQueryResultsSet->Data.A.IpAddress;

			debug_log(LOG_DBG, "initSmtp(): mail exchanger: \'%s\', ip: %s",  smtp_fqdn, inet_ntoa(ipAddr));
			DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);
		} else
		{
			debug_log(LOG_SVR, "initSmtp(): failed to resolve ip for mail exchanger: \'%s\', rc: %d)",  smtp_fqdn, dns_rc);
			debug_log(LOG_SVR, "initSmtp(): sending of email notifications will be disabled.");
			_smtp_disable_email_notifications = 1;
			return 1;
		}
	} else
	{
		debug_log(LOG_SVR, "initSmtp(): failed to resolve mx record for domain: \'%s\', rc: %d)",  _smtp_email_domain, dns_rc);
		debug_log(LOG_SVR, "initSmtp(): sending of email notifications will be disabled.");
		_smtp_disable_email_notifications = 1;

		return 1;
	}

	if (gethostname(_smtp_localhost_name, SMTP_MAX_HOST_NAME_LEN) != 0)  // string for EHLO command
	{
		strcpy(_smtp_localhost_name, "localhost");
	}

	debug_log(LOG_DBG, "initSmtp(): will use localhostname: \'%s\' in EHLO command", _smtp_localhost_name );

	debug_log(LOG_INF, "initSmtp(): initialization is complete; email notifications are enabled.");
	return 0;
} // end of initSmtp()

int reQueryMxRecord(struct  in_addr * mxIP)
{
	DNS_STATUS dns_rc = 0;
	DNS_RECORD * pQueryResultsSet;	
	char smtp_fqdn[SMTP_MAX_FQDN+1] = {0};

	debug_log(LOG_DBG, "reQueryMxRecord(): querying mx records for email domain: \'%s\'", _smtp_email_domain);

    dns_rc = DnsQuery_A(_smtp_email_domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);

	if (dns_rc == 0)
	{
		strncpy_s(smtp_fqdn, sizeof(smtp_fqdn), pQueryResultsSet->Data.MX.pNameExchange,  strlen(pQueryResultsSet->Data.MX.pNameExchange));

		debug_log(LOG_DBG, "reQueryMxRecord(): email domain: \'%s\', mail server: \'%s\'", _smtp_email_domain, smtp_fqdn);
		DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);

		debug_log(LOG_DBG, "reQueryMxRecord(): looking up an IP for \'%s\'", smtp_fqdn);
		dns_rc = DnsQuery_A(smtp_fqdn, DNS_TYPE_A, DNS_QUERY_STANDARD, NULL, &pQueryResultsSet, NULL);
		if (dns_rc == 0)
		{
			mxIP->S_un.S_addr = pQueryResultsSet->Data.A.IpAddress;

			debug_log(LOG_DBG, "reQueryMxRecord(): mail exchanger: \'%s\', ip: %s",  smtp_fqdn, inet_ntoa(*mxIP));
			DnsRecordListFree(pQueryResultsSet, DnsFreeRecordList);
		} else
		{
			debug_log(LOG_SVR, "reQueryMxRecord(): failed to resolve ip for mail exchanger: \'%s\', rc: %d)",  smtp_fqdn, dns_rc);
			debug_log(LOG_SVR, "reQueryMxRecord(): sending of email notifications will be disabled.");
			_smtp_disable_email_notifications = 1;
			return 1;
		}
	} else
	{
		debug_log(LOG_SVR, "reQueryMxRecord(): failed to resolve mx record for domain: \'%s\', rc: %d)",  _smtp_email_domain, dns_rc);
		debug_log(LOG_SVR, "reQueryMxRecord(): sending of email notifications will be disabled.");
		_smtp_disable_email_notifications = 1;

		return 1;
	}

	return 0;
} // end of reQueryMxRecord()

//DWORD WINAPI sendEmailThread(LPVOID pvMsg)
void sendEmailThread(void * pvMsg)
{	
	EmailMsg * eMsg = (EmailMsg *) pvMsg;

	char * msg = NULL;
	int msgLen = 0;

	int    optval  = 0;
	int    rc      = -99;
	int    len     = 0;  
	int    outLen  = 0;  // bytes sent/received
	int    lastError = 0;

	char buf[SMTP_MAX_TEXT_LINE_LEN+1] = {0};

	int retryCount = 0;
	int retryDelay = 0;

	unsigned long ip = 0;
	SOCKET   sock    = INVALID_SOCKET;

	if (eMsg == NULL)
	{
		debug_log(LOG_SVR, "sendEmailThread(): nothing to send, eMsg is empty");
   	    debug_log(LOG_SVR, "sendEmailThread(): exiting thread: 0x%04x", GetCurrentThreadId());
		return;
	}

	msg = (char *) eMsg->msg;

	if (msg != NULL)
		msgLen = strlen(msg);

	if (msg == NULL || msgLen <= 0)
	{
		debug_log(LOG_SVR, "sendEmailThread(): nothing to send, message len: %d", msgLen);
   	    debug_log(LOG_SVR, "sendEmailThread(): exiting thread: 0x%04x", GetCurrentThreadId());
		free(eMsg);
		return;
	}

	if (eMsg->msgType == PROCESS_RESTART_EMAIL_TYPE)
		debug_log(LOG_INF, "sendEmailThread(): sending \'Monitored process restarted\' email message, len: %d", msgLen);

	if (eMsg->msgType == WDOG_RESTART_EMAIL_TYPE)
		debug_log(LOG_INF, "sendEmailThread(): sending \'Watchdog restarted\' email message, len: %d", msgLen);

	if (eMsg->msgType == OS_REBOOT_EMAIL_TYPE)
		debug_log(LOG_INF, "sendEmailThread(): sending \'OS is about to be rebooted\' email message, len: %d", msgLen);

	if (eMsg->msgType == WDOG_STATUS_EMAIL_TYPE)
		debug_log(LOG_INF, "sendEmailThread(): sending \'Watchdog Status\' email message, len: %d", msgLen);

start_again:

	if (net_connectMX(_smtp_email_domain, &sock) != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): failed to connect to mail server for email domain: %s", _smtp_email_domain);
   	    debug_log(LOG_SVR, "sendEmailThread(): exiting thread: 0x%04x", GetCurrentThreadId());
		goto the_end;
	}

	debug_log(LOG_DBG, "sendEmailThread(): connected to email server for domain: \'%s\' waiting for 220 response...", _smtp_email_domain);

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 220) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 220 response from the server, rc: %d", rc);	   
			closesocket(sock);

			if (rc == 421 || rc == 420 && retryCount < SMTP_RETRY_COUNT)
			{			
				retryCount++;
				retryDelay += SMTP_RETRY_DELAY;
				debug_log(LOG_SVR, "sendEmailThread(): will attempt to re-connect in %d seconds...retry count: %d", retryDelay/1000, retryCount);	   

				Sleep(retryDelay);
				goto start_again;
			}

			goto the_end;
		}
	} else
		goto the_end;

	memset(buf, 0, sizeof(buf)); 
	sprintf(buf, HELO_STR , _smtp_localhost_name);

	debug_log(LOG_DBG, "sendEmailThread(): received 220 response. Sending EHLO %s command.", _smtp_localhost_name);

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	len = strlen(buf);
	rc = net_sendBytes(sock, buf, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send EHLO command, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 250) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 250 response after EHLO, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;
 
	memset(buf, 0, sizeof(buf)); 
	sprintf( buf, MAIL_STR , _smtp_email);

	debug_log(LOG_DBG, "sendEmailThread(): received 250 response. Sending MAIL FROM: <%s> command.", _smtp_email);

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	len = strlen(buf);
	rc = net_sendBytes(sock, buf, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send MAIL FROM command, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 250) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 250 response after MAIL FROM, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;

	memset(buf, 0, sizeof(buf)); 
	sprintf( buf, RCPT_STR , _smtp_email);

	debug_log(LOG_DBG, "sendEmailThread(): received 250 response. Sending RCPT TO:<%s> command.", _smtp_email);

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	len = strlen(buf);
	rc = net_sendBytes(sock, buf, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send RCPT TO command, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 250) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 250 response after RCPT TO, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;

	memset(buf, 0, sizeof(buf)); 
	memcpy( buf, DATA_STR , 6);
	
	debug_log(LOG_DBG, "sendEmailThread(): received 250 response. Sending DATA command.");

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	len = strlen(buf);
	rc = net_sendBytes(sock, buf, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send DATA command, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 354) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 354 response after DATA, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;

	debug_log(LOG_DBG, "sendEmailThread(): received 354 response. Sending email headers and body.");

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	debug_log(LOG_DBG, "sendEmailThread(): received 354 response. Sending email headers and body. len: %d", msgLen);
	rc = net_sendBytes(sock, msg, msgLen, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send msg body, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 250) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 250 response after sending of message body, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;

	memset( buf, 0, SMTP_MAX_TEXT_LINE_LEN);
	memcpy( buf, QUIT_STR , 6);

	debug_log(LOG_DBG, "sendEmailThread(): received 250 response. Sending QUIT command");

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	len = strlen(buf);
	rc = net_sendBytes(sock, buf, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_SVR, "sendEmailThread(): unable to send QUIT command, rc: %d", rc);
		goto the_end;
	}

	if (waitForShutdown(1)) goto the_end;  // shutdown is set

	if (net_recvSmtpResponse(sock, &rc, SOCK_TIMEOUT_INTERVAL) == SOCK_NO_ERROR)
	{
		if (rc != 221) 
		{
			debug_log(LOG_SVR, "sendEmailThread(): unable to receive 221 response after QUIT, rc: %d", rc);
			goto the_end;
		}
	} else
		goto the_end;

	debug_log(LOG_DBG, "sendEmailThread(): received 221 response.");

the_end:
	free(msg);
	free(eMsg);
	debug_log(LOG_INF, "sendEmailThread(): closing connection.");
	closesocket(sock);

	debug_log(LOG_INF, "sendEmailThread(): exiting email thread: 0x%04x", GetCurrentThreadId());
	//return 0;
} // end of "sendEmailThread()" 

DWORD WINAPI statusEmailThread(LPVOID pvMsg)
{
	char buf[SMTP_MAX_MSG_SIZE] = {0};
	char dateStr[SMTP_MAX_DATE_LEN] = {0};
	char msgId[SMTP_MAX_MSGID_LEN] = {0};
	char status[WDOG_STATUS_STR_LEN] = {0};
	char * pMsg = NULL;
	int msgLen = 0;
	double avg = 0.0;
	double util = 0.0;
	int hw = 0;
	
	time_t stTime;
	struct tm * stTimeTm;

	int waitIntervalMin = (int) pvMsg;

	long long waitInterval = waitIntervalMin*60*1000; // miliseconds	

	debug_log(LOG_SVR, "statusEmailThread(): will send \'watchdog status\' email message every %d minutes", waitIntervalMin);

	wait(45000); // sleep to throttle emails so that yahoo does not mark them as spam

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		build_smtp_msgId(msgId);
		build_smtp_date(dateStr);

		time(&stTime);
		stTimeTm = localtime(&stTime);

		avg = 0.0;
		hw = 0;
		util = 0.0;

		getWatchdogStatus(status, sizeof(status), &avg, &util, &hw);

		// date, email, email, email, email, msg ID, ip, version, ip, email, status msg
		sprintf( buf, MSG_FORMAT_STATUS, 
					dateStr,
					_smtp_email,_smtp_email,_smtp_email,_smtp_email,
					msgId,
					cfg->wdogRigName, // subject
					avg,
					hw,
					WDOG_VERSION,
					cfg->wdogListenIP,
					_smtp_email,
					status
			  );

		msgLen = strlen(buf);
		pMsg = (char *) calloc(1, msgLen+1);
		if (pMsg == NULL)
		{
			debug_log(LOG_ERR, "send_smtp_wdog_restarted_msg(): failed to allocate memory for message object.");
		} else
		{
			strncpy_s(pMsg, msgLen+1, buf, msgLen);

			EmailMsg * msg = (EmailMsg *) calloc(1, sizeof(EmailMsg));
			msg->msgType = WDOG_STATUS_EMAIL_TYPE;
			msg->msg = pMsg;

			sendEmailThread((void *) msg );
		}

		wait(waitInterval);

		debug_log(LOG_INF, "statusEmailThread(): woke up, time to send \'watchdog status\' email message");
	}  

	debug_log(LOG_SVR, "statusEmailThread(): exiting status email thread: 0x%04x", GetCurrentThreadId());
    ExitThread(0);
	return 0;
} // end of statusEmailThread()
