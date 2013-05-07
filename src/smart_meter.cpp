// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "wdmain.h"
#include "miner_api.h"

#define ONOFF_PEAK_MONITORNING_DELAY 10

// -------
// 1 - yes
// 0 - no
// -------
int isCurrentTimeGreaterThan(char * startTime, int addOffsetToNow)
{
	time_t now_t = 0;
	time_t start_t = 0;
	struct tm * start_tm = NULL;
	
	char start[6];
	char * pStart = NULL;
	char * end = NULL;
	int hours = 0;
	int minutes = 0;

	time(&now_t);

	strcpy_s(start, sizeof(start), startTime);
	
	now_t += addOffsetToNow;

	start_tm = localtime(&now_t);

	end = strstr(start, ":");

	//debug_log(LOG_SVR, "isCurrentTimeGreaterThan(%s, %d) start: %s end: %s", startTime, addOffsetToNow, start, end);

	if (end != NULL)
	{
		pStart = end+1;
		*end = 0;
    	//debug_log(LOG_SVR, "isCurrentTimeGreaterThan(%s, %d) pStart: %s end: %s", startTime, addOffsetToNow, pStart, end);

		hours = atoi(start);
		minutes = atoi(pStart);

		//debug_log(LOG_SVR, "isCurrentTimeGreaterThan(%s, %d): hours: %d, minutes: %d", startTime, addOffsetToNow, hours, minutes);

		start_tm->tm_hour = hours;
		start_tm->tm_min = minutes;
		start_tm->tm_sec = 0;
		start_t = mktime(start_tm);

		//debug_log(LOG_SVR, "isCurrentTimeGreaterThan(%s, %d): now_t: %lu, start_t: %lu", startTime, addOffsetToNow, now_t, start_t);

		if (now_t > start_t)
			return 1;
	}

	return 0;
} // end of isCurrentTimeGreaterThan()

void checkSmartMeteringSchedule(CGMConfig * cfg)
{
	CGMConfig * _cfg = cfg;

	if (cfg->smartMeterOnPeak == 0) // are we OFF peak?
	{
		if (isCurrentTimeGreaterThan(_cfg->smartMeterOffPeakStartTime, 0) == false)  // are we before start of OFF peak period
		{
			if (isCurrentTimeGreaterThan(_cfg->smartMeterOnPeakStartTime, 0)) // did we pass ON peak start time
			{
				debug_log(LOG_SVR, "checkSmartMeteringSchedule(): on-peak schedule (%s) detected, trying to disable GPUs", _cfg->smartMeterOnPeakStartTime);
			
				cfg->smartMeterOnPeak = 1;
				Sleep(10000);
				if (disableGPUs() == 0)
				{			
					cfg->smartMeterOnPeak = 0;  // We failed to disable GPUs
					debug_log(LOG_ERR, "checkSmartMeteringSchedule(): failed to disable GPUs");
				} else
				{
					debug_log(LOG_SVR, "checkSmartMeteringSchedule(): GPUs are disabled; GPU monitoring has been suspended.");
				}

			}
		}
	}

	if (cfg->smartMeterOnPeak == 1) // We are ON Peak
	{
		if (isCurrentTimeGreaterThan(_cfg->smartMeterOffPeakStartTime, 0))
		{
			if (isCurrentTimeGreaterThan(_cfg->smartMeterOffPeakStartTime, -ONOFF_PEAK_MONITORNING_DELAY*60))
			{
				debug_log(LOG_SVR, "checkSmartMeteringSchedule(): we are %d minutes into off-peak schedule (%s), enable GPU monitoring", ONOFF_PEAK_MONITORNING_DELAY, _cfg->smartMeterOffPeakStartTime);
				// ------------------------------------------------------
				// We are ONOFF_PEAK_MONITORNING_DELAY minutes into the off peak schedule.
				// Hopefully, the GPUs have raised their averages by now.
				// ------------------------------------------------------
				cfg->smartMeterOnPeak = 0;
			} else
			{
				if (cfg->smartMeterOnPeak == 1)  // were we on-peak before hitting this point?
				{
					debug_log(LOG_SVR, "checkSmartMeteringSchedule(): off-peak schedule (%s) has started, trying to enable GPUs", _cfg->smartMeterOffPeakStartTime);
					// ------------------------------
					// Off-peak schedule has started.
					// ------------------------------
					if (enableGPUs() == 0)
					{
						debug_log(LOG_ERR, "checkSmartMeteringSchedule(): failed to enable GPUs");
					} else
					{
						debug_log(LOG_SVR, "checkSmartMeteringSchedule(): GPUs were successfully enabled; will wait %d minutes before re-enabling GPU monitoring", ONOFF_PEAK_MONITORNING_DELAY);
					}
				}
			}
		}
	}

} // end of checkSmartMeteringSchedule()

DWORD WINAPI smartMeteringThread(LPVOID pvParam)
{
	CGMConfig * _cfg = (CGMConfig *) pvParam;

	int waitIntervalMin = _cfg->smartMeterPollingInterval;

	long long waitInterval = waitIntervalMin*60*1000; // miliseconds	

	debug_log(LOG_SVR, "smartMeteringThread(): will check smart metering schedule every %d minutes", waitIntervalMin);

	wait(waitInterval);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		checkSmartMeteringSchedule(_cfg);
		wait(waitInterval);
		debug_log(LOG_INF, "smartMeteringThread(): woke up, time to check smart metering schedule");
	}  

	debug_log(LOG_SVR, "smartMeteringThread(): exiting smart metering thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of smartMeteringThread()
