// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_WDMAIN_H
#define CGM_WDMAIN_H

#include "config.h"

#define WAIT_FOR_SHUTDOWN_INTERVAL 20  

#define RECVSIZE 65000

extern CGMConfig * cfg;

extern char * sysCmd;
extern int restart;
extern char startedOn[32]; 
extern char title[256];


void age(char * buf, int bufsize);

void wait(long long howLong);
int waitForShutdown(long timeout);

int getADLMutex();
void releaseADLMutex();

int getPoolMutex();
void releasePoolMutex();

int getBTCMutex();
void releaseBTCMutex();

int getLTCMutex();
void releaseLTCMutex();

int getGpuMutex();
void releaseGpuMutex();

#endif