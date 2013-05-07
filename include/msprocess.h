// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_PROCAPI_H
#define CGM_PROCAPI_H

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <winsock2.h>
#include <time.h>

#include <windows.h>
#include <tchar.h>
#include <psapi.h>
#include <winternl.h>

typedef struct 
{
	DWORD processID;
	DWORD werFaultID;
	CHAR szProcessName[(MAX_PATH+1)*2];  
//	CHAR szCmd[(MAX_PATH+1)*2];  
//	CHAR szImage[(MAX_PATH+1)*2];
//	CHAR szCWD[(MAX_PATH+1)*2];
	SIZE_T workingSetSize;
	DWORD  handleCount;
} cgmProcessInfo;

int getProcessID(const char * name);
void getProcessInfo(const char * name, cgmProcessInfo * procInfo);
int numberOfAkbashProcesses(char * name);

void killAllWERProcesses(void);
void killProcessByID(DWORD processID);
void restartMinerProcess(cgmProcessInfo * procInfo);

#endif
