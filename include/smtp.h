// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_SMTP_H
#define CGM_SMTP_H

#define SMTP_MAX_DATE_LEN		  32
#define SMTP_MAX_MSGID_LEN		  SMTP_MAX_FQDN+102
#define SMTP_MAX_TEXT_LINE_LEN    1024
#define SMTP_MAX_HOST_NAME_LEN    256
#define SMTP_MAX_IP_LEN           16
#define SMTP_MAX_USER_LEN         256
#define SMTP_MAX_FQDN	          256
#define SMTP_CONNECTION_CLOSED	  100

#define SEND_DONATIONS_TO "If you have any questions or comments, please contact me at: <a href=\"mailto: peter@petermoss.com\">peter@petermoss.com</a><br><br>Please support akbash by donating to: <br><br><b>1CorsCrBEcbncfnR7BVKadUZbHP7xNZCjJ</b><br><br>Thank you<br><br>"

void build_smtp_date(char *buf);

int initSmtp(char * email, int disableFlag);

void send_smtp_wdog_restarted_msg(void);
void send_smtp_block_found_msg(void);
void send_smtp_restarted_msg(int reason);
void send_smtp_rebooted_msg(int reason);

DWORD WINAPI statusEmailThread(LPVOID pvMsg);

#endif