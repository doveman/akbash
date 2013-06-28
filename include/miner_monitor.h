// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_HASHMONITOR_H
#define CGM_HASHMONITOR_H

#include "msprocess.h"

#define DEFAULT_RESTART_DELAY 15 // seconds

#define REASONS_STRING_MAX_LENGTH   1024

#define WDOG_RESTART_TEMPERATURE_THRESHOLD          1  
#define WDOG_RESTART_ACTIVITY_THRESHOLD             2 
#define WDOG_RESTART_FAN_DOWN			            4 
#define WDOG_RESTART_MINER_CRASH		            8 
#define WDOG_RESTART_GPU_HASH_THRESHOLD	           16 
#define WDOG_REBOOT_DRIVER_CRASH		           32 
#define WDOG_RESTART_MINER_NOT_RUNNING	           64 
#define WDOG_RESTART_MINER_NOT_RESPONDING         128 
#define WDOG_RESTART_WORKING_SET_THRESHOLD        256 
#define WDOG_RESTART_HANDLE_COUNT_THRESHOLD       512 
#define WDOG_RESTART_VIA_HTTP				     1024 
#define WDOG_RESTART_NOT_ALIVE				     2048  
#define WDOG_RESTART_PGA_HASH_THRESHOLD	         4096 
#define WDOG_REBOOT_MAX_RESTARTS	             8192 
#define WDOG_REBOOT_NOT_RUNNING_AFTER_RESTART   16384 
#define WDOG_REBOOT_VIA_HTTP				    32768 
#define WDOG_RESTART_GPU_ACTIVITY_THRESHOLD	    65536 
#define WDOG_RESTART_MINER_HW_ERRORS_THRESHOLD 131072 
#define WDOG_RESTART_PGA_HAS_BEEN_DISABLED     262144 
#define WDOG_RESTART_ACCEPTED_RATE_IS_FLAT     524288

void formatReasons(char * buf, int bufSize, int reason, int device);

void restartMiner(int delay, int reason, int device);

DWORD WINAPI monitorThread( LPVOID lpParam );

#endif