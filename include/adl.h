// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_ADL_H

#include "adl_structures.h"

#define ADL_INVALID_TEMP -1000 // -1C
//#define MAX_ADAPTERS_PER_GPU 16

#define ADL_STATUS_INITIAL         7   // never used
#define ADL_STATUS_NOT_AVAILABLE   8   // not loaded ok on startup, unable tofind the library
#define ADL_STATUS_CLOSED   	   9   // loaded ok on startup
#define ADL_STATUS_OPENED   	  10   // loaded ok on startup

#define ADL_STATUS_CALL_FAILED    11   // loading failed for some reason
#define ADL_STATUS_OPEN_FAILED    12   // loading failed for some reason
#define ADL_STATUS_OPEN_OK        13   // loading failed for some reason
#define ADL_STATUS_CLOSE_OK       14   // loading failed for some reason
#define ADL_STATUS_EXCEPTION      15   // was loaded ok, then it failed, assume driver crash

typedef struct _hwAdapterInfo
{
	int adapterIndex;
	int adapterID;
	int bus;
	int gpu;
	int displayMapped;
	int displayConnected;
	int fanRPM;
	int temperature;
	int supportFanRPM;
	int supportFanPercentage;

	ADLPMActivity current;  // engine/memory/vddc, utilization, current OD performance level
} hwAdapterInfo;

typedef struct _adlInfo
{	
	int iNumberAdapters;
	int iMaxTemp;
	int iZeroFan;
	int iADLStatus;
	int iMinActivity;

	hwAdapterInfo * adapters;
} ADLInfo;


void reset_adl_stats();
char * adl_status_str();

int adl_open();
void adl_close();

void getADLStats(ADLInfo * adlInfo);
void displayADLStats(ADLInfo * adlInfo);

int initADLInfo(void);
int refreshADLInfo(void);
DWORD WINAPI adlPollingThread(LPVOID pvParam);

#endif CGM_ADL_H