// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_LISTEN_H
#define CGM_LISTEN_H

#include <windows.h>
#include "config.h"
#include "msprocess.h"

#define WDOG_VERSION "1.0.8"
#define WDOG_STATUS_STR_LEN 15100

extern int restartCount;

void incrementRestartCount();
int shouldWeReboot(CGMConfig * cfg);

void getWatchdogStatus(char * status, int statusSize, double * avg, double * util, int * hw);

BOOL systemReboot();
void reboot(int reason);
DWORD WINAPI listenForCommands( LPVOID lpParam );

#endif