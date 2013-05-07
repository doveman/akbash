// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <windows.h>

#include "adl_sdk.h"
#include "adl_functions.h"

#include "log.h"
#include "adl.h"
#include "wdmain.h"
#include "miner_api.h"

static HINSTANCE hDLL           = NULL;

static ADLInfo gADLInfo = {0,0,0,ADL_STATUS_NOT_AVAILABLE,NULL};

static	ADL_MAIN_CONTROL_CREATE		           ADL_Main_Control_Create                = NULL;
static	ADL_MAIN_CONTROL_DESTROY	           ADL_Main_Control_Destroy               = NULL;
static	ADL_MAIN_CONTROL_REFRESH	           ADL_Main_Control_Refresh               = NULL;
static	ADL_ADAPTER_NUMBEROFADAPTERS_GET       ADL_Adapter_NumberOfAdapters_Get       = NULL;
static	ADL_ADAPTER_ADAPTERINFO_GET	           ADL_Adapter_AdapterInfo_Get            = NULL;
static  ADL_DISPLAY_DISPLAYINFO_GET            ADL_Display_DisplayInfo_Get            = NULL;
static	ADL_OVERDRIVE5_CURRENTACTIVITY_GET     ADL_Overdrive5_CurrentActivity_Get     = NULL;
static	ADL_OVERDRIVE5_TEMPERATURE_GET	       ADL_Overdrive5_Temperature_Get         = NULL;
static	ADL_OVERDRIVE5_FANSPEED_GET	           ADL_Overdrive5_FanSpeed_Get            = NULL;
static	ADL_OVERDRIVE5_FANSPEEDINFO_GET	       ADL_Overdrive5_FanSpeedInfo_Get        = NULL;
static	ADL_ADAPTER_ID_GET		               ADL_Adapter_ID_Get                     = NULL;

// Memory allocation function
static void * __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void *lpBuffer = malloc(iSize);

	return lpBuffer;
}

// Optional Memory de-allocation function
static void __stdcall ADL_Main_Memory_Free (void **lpBuffer)
{
	if (*lpBuffer) {
		free (*lpBuffer);
		*lpBuffer = NULL;
	}
}

void reset_adl_stats()
{
	if (getADLMutex())
	{
		__try
		{
			gADLInfo.iADLStatus = ADL_STATUS_INITIAL;
			gADLInfo.iMaxTemp = 0;
			gADLInfo.iZeroFan = 0;
			gADLInfo.iMinActivity = 100;
			gADLInfo.iNumberAdapters = 0;
			gADLInfo.adapters = NULL;
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		releaseADLMutex();
	}
}

char * adl_status_str(int status)
{
	switch (status)
	{
		case ADL_STATUS_INITIAL :       return "ADL_STATUS_INITIAL";
		case ADL_STATUS_NOT_AVAILABLE :	return "ADL_STATUS_NOT_AVAILABLE";
		case ADL_STATUS_CLOSED :        return "ADL_STATUS_CLOSED";
		case ADL_STATUS_OPENED :        return "ADL_STATUS_OPENED";
		case ADL_STATUS_CALL_FAILED :   return "ADL_STATUS_CALL_FAILED";
		case ADL_STATUS_OPEN_FAILED :   return "ADL_STATUS_OPEN_FAILED";
		case ADL_STATUS_OPEN_OK :       return "ADL_STATUS_OPEN_OK";
		case ADL_STATUS_CLOSE_OK :      return "ADL_STATUS_CLOSE_OK";
		default :                       return "ADL_STATUS_EXCEPTION";
	}
} // end of adl_status_str()

char * adl_status_str()
{
	int status = ADL_STATUS_INITIAL;

	if (getADLMutex())
	{
		__try
		{
			status = gADLInfo.iADLStatus;
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		releaseADLMutex();
	}

	return adl_status_str(status);
} // end of adl_status_str()

void set_adl_status(int status)
{
	if (getADLMutex())
	{
		__try
		{
			//debug_log(LOG_SVR, "set_adl_status(%s): %s", adl_status_str(status), adl_status_str(gADLInfo.iADLStatus));

			if (gADLInfo.iADLStatus == ADL_STATUS_INITIAL)
			{
				if (status == ADL_STATUS_OPEN_FAILED || status == ADL_STATUS_CALL_FAILED)
					gADLInfo.iADLStatus = ADL_STATUS_NOT_AVAILABLE;
				else
					if (status == ADL_STATUS_OPEN_OK)
						gADLInfo.iADLStatus = ADL_STATUS_OPENED;
					else
						if (status == ADL_STATUS_CLOSE_OK)
							gADLInfo.iADLStatus = ADL_STATUS_CLOSED;
			} else
			{
				if (gADLInfo.iADLStatus == ADL_STATUS_OPENED)
				{
					if (status != ADL_STATUS_CLOSE_OK)
						gADLInfo.iADLStatus = ADL_STATUS_EXCEPTION;
					else
						gADLInfo.iADLStatus = ADL_STATUS_CLOSED;
				} else
				{
					if ( gADLInfo.iADLStatus == ADL_STATUS_CLOSED)
					{
						if (status != ADL_STATUS_OPEN_OK)
							gADLInfo.iADLStatus = ADL_STATUS_EXCEPTION;  // was CLOSED then OPENING failed.  Assume driver went bad.  reboot OS?				
						else
							gADLInfo.iADLStatus = ADL_STATUS_OPENED;
					}
				}
			}
		
			//debug_log(LOG_SVR, "set_adl_status(%s): %s", adl_status_str(status), adl_status_str(gADLInfo.iADLStatus));
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		releaseADLMutex();
	}
} // end of set_adl_status()

int adl_open()
{
	int i = 0;
	int j = 0;
	int result = 0;

	__try
	{
		hDLL = LoadLibraryA("atiadlxx.dll");
		if (hDLL == NULL)
		{
			//debug_log(LOG_SVR, "adl_open(): unable to load (atiadlxx.dll) ADL library, rc: %d, will try to load atiadlxy.dll", GetLastError());

			// A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
			// Try to load the 32 bit library (atiadlxy.dll) instead
			hDLL = LoadLibraryA("atiadlxy.dll");

			if (hDLL == NULL) 
			{
				debug_log(LOG_ERR, "adl_open(): failed to load (atiadlxy.dll) ADL library, rc: %d", GetLastError());

				set_adl_status(ADL_STATUS_OPEN_FAILED);
				return 0;
			}

			debug_log(LOG_DBG, "adl_open(): atiadlxy.dll loaded ok");
		}

		ADL_Main_Control_Create = (ADL_MAIN_CONTROL_CREATE) GetProcAddress(hDLL,"ADL_Main_Control_Create");
		ADL_Main_Control_Destroy = (ADL_MAIN_CONTROL_DESTROY) GetProcAddress(hDLL,"ADL_Main_Control_Destroy");
		ADL_Adapter_NumberOfAdapters_Get = (ADL_ADAPTER_NUMBEROFADAPTERS_GET) GetProcAddress(hDLL,"ADL_Adapter_NumberOfAdapters_Get");
		ADL_Adapter_AdapterInfo_Get = (ADL_ADAPTER_ADAPTERINFO_GET) GetProcAddress(hDLL,"ADL_Adapter_AdapterInfo_Get");
		ADL_Overdrive5_Temperature_Get = (ADL_OVERDRIVE5_TEMPERATURE_GET) GetProcAddress(hDLL,"ADL_Overdrive5_Temperature_Get");
		ADL_Display_DisplayInfo_Get = (ADL_DISPLAY_DISPLAYINFO_GET) GetProcAddress(hDLL, "ADL_Display_DisplayInfo_Get");
		ADL_Overdrive5_CurrentActivity_Get = (ADL_OVERDRIVE5_CURRENTACTIVITY_GET) GetProcAddress(hDLL, "ADL_Overdrive5_CurrentActivity_Get");
		ADL_Overdrive5_FanSpeed_Get = (ADL_OVERDRIVE5_FANSPEED_GET) GetProcAddress(hDLL, "ADL_Overdrive5_FanSpeed_Get");
		ADL_Main_Control_Refresh = (ADL_MAIN_CONTROL_REFRESH) GetProcAddress(hDLL, "ADL_Main_Control_Refresh");
		ADL_Overdrive5_FanSpeedInfo_Get = (ADL_OVERDRIVE5_FANSPEEDINFO_GET) GetProcAddress(hDLL, "ADL_Overdrive5_FanSpeedInfo_Get");
		ADL_Adapter_ID_Get = (ADL_ADAPTER_ID_GET) GetProcAddress(hDLL,"ADL_Adapter_ID_Get");

		if (!ADL_Main_Control_Create            || !ADL_Main_Control_Destroy       ||
			!ADL_Adapter_NumberOfAdapters_Get   || !ADL_Overdrive5_Temperature_Get ||
			!ADL_Adapter_AdapterInfo_Get        || !ADL_Display_DisplayInfo_Get    ||
			!ADL_Overdrive5_CurrentActivity_Get || !ADL_Main_Control_Refresh       ||
			!ADL_Overdrive5_FanSpeed_Get        || !ADL_Overdrive5_FanSpeedInfo_Get ||
			!ADL_Adapter_ID_Get
			) 
		{
			debug_log(LOG_ERR, "adl_open(): Unabled to get pointers to ADL functions");
			FreeLibrary(hDLL);
			hDLL = NULL;

			set_adl_status(ADL_STATUS_OPEN_FAILED);
			return 0;
		}

		// Initialise ADL. The second parameter is 1, which means:
		// retrieve adapter information only for adapters that are physically present and enabled in the system
		result = ADL_Main_Control_Create (ADL_Main_Memory_Alloc, 1);
		if (result != ADL_OK) 
		{
			debug_log(LOG_ERR, "adl_open(): ADL_Main_Control_Create() failed, rc: %d", result);
			FreeLibrary(hDLL);
			hDLL = NULL;
			set_adl_status(ADL_STATUS_OPEN_FAILED);
			return 0;
		}

		result = ADL_Main_Control_Refresh();
		if (result != ADL_OK) 
		{
			debug_log(LOG_ERR, "adl_open(): ADL_Main_Control_Refresh(), rc: %d", result);
			ADL_Main_Control_Destroy();
			FreeLibrary(hDLL);
			hDLL = NULL;
			set_adl_status(ADL_STATUS_OPEN_FAILED);
			return 0;
		}
	
		set_adl_status(ADL_STATUS_OPEN_OK);

		return 1;
	} __except(EXCEPTION_EXECUTE_HANDLER)
	{
		debug_log(LOG_ERR, "adl_open(): caught exception opening ADL library");
	}

	set_adl_status(ADL_STATUS_OPEN_FAILED);
	return 0;
} // end of adl_open()

void adl_close()
{
	int _status = ADL_STATUS_CALL_FAILED;

	if (getADLMutex())
	{
		__try
		{
			if (hDLL != NULL)
			{
				ADL_Main_Control_Destroy();
				FreeLibrary(hDLL);
				hDLL = NULL;
				ADL_Main_Control_Create = NULL;
				ADL_Main_Control_Destroy = NULL;
				ADL_Adapter_NumberOfAdapters_Get = NULL;
				ADL_Overdrive5_Temperature_Get = NULL;
			    ADL_Adapter_AdapterInfo_Get = NULL;
				ADL_Display_DisplayInfo_Get = NULL;
			    ADL_Overdrive5_CurrentActivity_Get = NULL;
				ADL_Main_Control_Refresh = NULL;
			    ADL_Overdrive5_FanSpeed_Get = NULL;
				ADL_Overdrive5_FanSpeedInfo_Get = NULL;
				ADL_Adapter_ID_Get = NULL;

				_status = ADL_STATUS_CLOSE_OK;
			}
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
			debug_log(LOG_ERR, "adl_close(): caught exception closing ADL library");
		}
		releaseADLMutex();
	}

	set_adl_status(_status);
} // end of adl_close()

void getADLStats(ADLInfo * adlInfo)
{
	int i = 0;

	if (adlInfo != NULL)
	{
		if (getADLMutex())
		{
			__try
			{
				adlInfo->adapters = (hwAdapterInfo *) calloc(1, sizeof(hwAdapterInfo) * gADLInfo.iNumberAdapters);
				if (adlInfo->adapters == NULL)
				{
					debug_log(LOG_ERR, "getADLStats(): failed to allocate memory for adapters array, numOfAdapters: %d", gADLInfo.iNumberAdapters);
				} else
				{
					adlInfo->iNumberAdapters = gADLInfo.iNumberAdapters;
					adlInfo->iMaxTemp        = gADLInfo.iMaxTemp;
					adlInfo->iZeroFan        = gADLInfo.iZeroFan;
					adlInfo->iMinActivity    = gADLInfo.iMinActivity;
					adlInfo->iADLStatus      = gADLInfo.iADLStatus;

					for(i=0; i < gADLInfo.iNumberAdapters; i++)
					{
						adlInfo->adapters[i].adapterIndex                     = gADLInfo.adapters[i].adapterIndex;
						adlInfo->adapters[i].adapterID                        = gADLInfo.adapters[i].adapterID;
						adlInfo->adapters[i].bus                              = gADLInfo.adapters[i].bus;
						adlInfo->adapters[i].gpu                              = gADLInfo.adapters[i].gpu;
						adlInfo->adapters[i].displayConnected                 = gADLInfo.adapters[i].displayConnected;
						adlInfo->adapters[i].displayMapped                    = gADLInfo.adapters[i].displayMapped;
						adlInfo->adapters[i].temperature                      = gADLInfo.adapters[i].temperature;
						adlInfo->adapters[i].fanRPM                           = gADLInfo.adapters[i].fanRPM;
						adlInfo->adapters[i].supportFanPercentage             = gADLInfo.adapters[i].supportFanPercentage;
						adlInfo->adapters[i].supportFanRPM                    = gADLInfo.adapters[i].supportFanRPM;
						adlInfo->adapters[i].current.iEngineClock             = gADLInfo.adapters[i].current.iEngineClock;
						adlInfo->adapters[i].current.iMemoryClock             = gADLInfo.adapters[i].current.iMemoryClock;
						adlInfo->adapters[i].current.iVddc                    = gADLInfo.adapters[i].current.iVddc;
						adlInfo->adapters[i].current.iActivityPercent         = gADLInfo.adapters[i].current.iActivityPercent;
						adlInfo->adapters[i].current.iCurrentPerformanceLevel = gADLInfo.adapters[i].current.iCurrentPerformanceLevel;
					}
				}
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
				debug_log(LOG_ERR, "getADLStats(): caught exception reading from a local ADL cache");
			}

			releaseADLMutex();
		}
	}
} // end of getADLStats()

void updateADLStats(ADLInfo * adlInfo)
{
	int i = 0;

	if (adlInfo != NULL && adlInfo->iNumberAdapters > 0)
	{
		if (getADLMutex())
		{
			__try
			{				
				int numberofAdapters = 0;

				if (gADLInfo.adapters == NULL)
				{
					gADLInfo.adapters = (hwAdapterInfo *) calloc(1, sizeof(hwAdapterInfo) * adlInfo->iNumberAdapters);
					if (gADLInfo.adapters == NULL)
					{
						releaseADLMutex();
						debug_log(LOG_ERR, "updateADLStats(): failed to allocate memory for adapters array, numOfAdapters: %d", adlInfo->iNumberAdapters);
						return;
					}
					gADLInfo.iNumberAdapters = adlInfo->iNumberAdapters;
				}

				if (adlInfo->iNumberAdapters > gADLInfo.iNumberAdapters)
				{
					debug_log(LOG_SVR, "updateADLStats(): adapters detected: %d, adapters in local cache: %d", adlInfo->iNumberAdapters, gADLInfo.iNumberAdapters);
					numberofAdapters = gADLInfo.iNumberAdapters;
				} else
				{
					numberofAdapters = adlInfo->iNumberAdapters;
				}

				gADLInfo.iMaxTemp     = adlInfo->iMaxTemp;
				gADLInfo.iZeroFan     = adlInfo->iZeroFan;
				gADLInfo.iMinActivity = adlInfo->iMinActivity;

				for(i=0; i < numberofAdapters; i++)
				{
					gADLInfo.adapters[i].adapterIndex                     = adlInfo->adapters[i].adapterIndex;
					gADLInfo.adapters[i].adapterID                        = adlInfo->adapters[i].adapterID;
					gADLInfo.adapters[i].bus                              = adlInfo->adapters[i].bus;
					gADLInfo.adapters[i].gpu                              = adlInfo->adapters[i].gpu;
					gADLInfo.adapters[i].displayConnected                 = adlInfo->adapters[i].displayConnected;
					gADLInfo.adapters[i].displayMapped                    = adlInfo->adapters[i].displayMapped;
					gADLInfo.adapters[i].temperature                      = adlInfo->adapters[i].temperature;
					gADLInfo.adapters[i].fanRPM                           = adlInfo->adapters[i].fanRPM;
					gADLInfo.adapters[i].supportFanPercentage             = adlInfo->adapters[i].supportFanPercentage;
					gADLInfo.adapters[i].supportFanRPM                    = adlInfo->adapters[i].supportFanRPM;
					gADLInfo.adapters[i].current.iEngineClock             = adlInfo->adapters[i].current.iEngineClock;
					gADLInfo.adapters[i].current.iMemoryClock             = adlInfo->adapters[i].current.iMemoryClock;
					gADLInfo.adapters[i].current.iVddc                    = adlInfo->adapters[i].current.iVddc;
					gADLInfo.adapters[i].current.iActivityPercent         = adlInfo->adapters[i].current.iActivityPercent;
					gADLInfo.adapters[i].current.iCurrentPerformanceLevel = adlInfo->adapters[i].current.iCurrentPerformanceLevel;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
				debug_log(LOG_ERR, "getADLStats(): caught exception updating a local ADL cache");
			}

			releaseADLMutex();
		}
	}
}

void displayADLStats(ADLInfo * adlInfo)
{
	int i = 0;

	if (adlInfo == NULL) return;

	debug_log( LOG_DBG, "displayADLStats(): number of adapters: %d, max temperature: %d, min activity: %d, %s", 
			   adlInfo->iNumberAdapters, 
			   adlInfo->iMaxTemp,
			   adlInfo->iMinActivity,
			   adlInfo->iZeroFan == 1 ? "some fans stopped spinning" : "all fans are spinning"
			 );

	for(i=0; i < adlInfo->iNumberAdapters; i++)
	{
		debug_log( LOG_DBG, "displayADLStats(): id: %d, idx: %d, bus: %2d, gpu: %2d, t: %dC, f: %d %s, e: %d, m: %d, v: %0.2f, util: %2d%%, lvl: %d, disp[map: %d, con: %d]", 
			       adlInfo->adapters[i].adapterID,
				   adlInfo->adapters[i].adapterIndex, 
				   adlInfo->adapters[i].bus,
				   adlInfo->adapters[i].gpu,
				   adlInfo->adapters[i].temperature,
				   adlInfo->adapters[i].fanRPM,
				   adlInfo->adapters[i].supportFanRPM == 1 ? "rpm" : adlInfo->adapters[i].supportFanPercentage == 1 ? "%" : "n/a",
				   adlInfo->adapters[i].current.iEngineClock/100,
			       adlInfo->adapters[i].current.iMemoryClock/100,
			       ((double) adlInfo->adapters[i].current.iVddc*1.0)/1000.0,
			       adlInfo->adapters[i].current.iActivityPercent,
			       adlInfo->adapters[i].current.iCurrentPerformanceLevel,
				   adlInfo->adapters[i].displayMapped,
				   adlInfo->adapters[i].displayConnected
				 );
	}
} // end of displayADLStats()


int initADLInfo(void)
{
	int i = 0;
	int j = 0;
	int result = 0;
	int gpuNumber = 0;
	ADLInfo adlInfo;
	int last_adapter = -1;

	LPAdapterInfo     lpAdapterInfo    = NULL;
    LPADLDisplayInfo  lpAdlDisplayInfo = NULL;
	
	if (adl_open() == 0)
	{				
		debug_log(LOG_ERR, "initADLInfo(): failed to open ADL library, rc: %d", result);		
		return 0;
	}
		
	memset(&adlInfo, 0, sizeof(adlInfo));
	adlInfo.iADLStatus = ADL_STATUS_INITIAL;
	adlInfo.iMaxTemp = 0;
	adlInfo.iZeroFan = 0;
	adlInfo.iMinActivity = 101;
	adlInfo.iNumberAdapters = 0;
	adlInfo.adapters = NULL;

	result = ADL_Adapter_NumberOfAdapters_Get(&adlInfo.iNumberAdapters);
	if (result != ADL_OK) 
	{				
		debug_log(LOG_ERR, "initADLInfo(): ADL_Adapter_NumberOfAdapters_Get() failed, rc: %d", result);		
		set_adl_status(ADL_STATUS_CALL_FAILED);
		adl_close();
		return 0;
	}

	if (adlInfo.iNumberAdapters <= 0)
	{
		debug_log(LOG_SVR, "initADLInfo(): no video adapters were detected. GPU hardware monitoring will be disabled.");		
		set_adl_status(ADL_STATUS_CALL_FAILED);
		adl_close();
		return 0;
	}

	adlInfo.adapters = (hwAdapterInfo *) calloc(1, sizeof(hwAdapterInfo)*adlInfo.iNumberAdapters);
	lpAdapterInfo = (AdapterInfo *) calloc(1, sizeof (AdapterInfo)*adlInfo.iNumberAdapters);

	if (lpAdapterInfo == NULL || adlInfo.adapters == NULL)
	{
		debug_log(LOG_DBG, "initADLInfo(): failed toallocate memory for adapters structures, num of adapters: %d", adlInfo.iNumberAdapters);		
		set_adl_status(ADL_STATUS_CALL_FAILED);
		adl_close();
		return 0;
	}

	debug_log(LOG_DBG, "initADLInfo(): detected %d virtual adapters", adlInfo.iNumberAdapters);		
	debug_log(LOG_DBG, "initADLInfo(): retrieving adapters info...");		

    // Get the AdapterInfo structure for all adapters in the system
    result = ADL_Adapter_AdapterInfo_Get(lpAdapterInfo, sizeof (AdapterInfo) * adlInfo.iNumberAdapters);
	if (result != ADL_OK)
	{
		debug_log(LOG_SVR, "initADLInfo(): ADL_Adapter_AdapterInfo_Get() failed, rc: %d", result);
		free(lpAdapterInfo);
		set_adl_status(ADL_STATUS_CALL_FAILED);
		adl_close();
		return 0;
	}

	// ----------------------------------
	// Get display info for each adapter.
	// ----------------------------------
	debug_log(LOG_DBG, "initADLInfo(): checking display info for each adapter...");		
	for (i=0; i < adlInfo.iNumberAdapters; i++)
	{
		int iNumDisplays = 0;
		int j = 0;
		
		lpAdapterInfo[i].iFunctionNumber = 0;
		lpAdapterInfo[i].iPresent = 0;

		result = ADL_Display_DisplayInfo_Get(lpAdapterInfo[i].iAdapterIndex, &iNumDisplays, &lpAdlDisplayInfo, 0);
		if (result != ADL_OK)
		{
			debug_log(LOG_SVR, "initADLInfo(): ADL_Display_DisplayInfo_Get() failed, rc: %d", result);
			free(lpAdapterInfo);
			set_adl_status(ADL_STATUS_CALL_FAILED);
			adl_close();
			return 0;
		}

		//debug_log(LOG_INF, "initADLInfo(): adapter: %d, num of displays: %d", i, iNumDisplays);

		for ( j = 0; j < iNumDisplays; j++ )
		{
			// For each display, check its status. Use the display only if it's connected AND mapped (iDisplayInfoValue: bit 0 and 1 )
			if (  ( ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED ) != 
				( ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED     &
											lpAdlDisplayInfo[ j ].iDisplayInfoValue ) )
				continue;   // Skip the not connected or not mapped displays

			if (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED &	lpAdlDisplayInfo[ j ].iDisplayInfoValue)
			{
				if ( i != lpAdlDisplayInfo[ j ].displayID.iDisplayLogicalAdapterIndex )
					lpAdapterInfo[i].iPresent = 1;
			} 

			if (ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED &	lpAdlDisplayInfo[ j ].iDisplayInfoValue)
			{
				// Is the display mapped to this adapter? This test is too restrictive and may not be needed.
				if ( i != lpAdlDisplayInfo[ j ].displayID.iDisplayLogicalAdapterIndex )
					lpAdapterInfo[i].iFunctionNumber = 1;
			} 
		}

		if (lpAdlDisplayInfo)
		{
			ADL_Main_Memory_Free((void **) &lpAdlDisplayInfo);
			lpAdlDisplayInfo = NULL;
		}
	}


	debug_log(LOG_DBG, "initADLInfo(): mapping gpus to adapters...");		

	for(i=0; i < adlInfo.iNumberAdapters; i++)
	{
		int lpAdapterID = -1;

		/* Get unique identifier of the adapter, 0 means not AMD */
		result = ADL_Adapter_ID_Get(lpAdapterInfo[i].iAdapterIndex, &lpAdapterID);
		if (result != ADL_OK) 
		{
			adlInfo.adapters[i].adapterIndex     = lpAdapterInfo[i].iAdapterIndex;
			adlInfo.adapters[i].bus              = lpAdapterInfo[i].iBusNumber;
			adlInfo.adapters[i].displayConnected = lpAdapterInfo[i].iPresent;
			adlInfo.adapters[i].displayMapped    = lpAdapterInfo[i].iFunctionNumber;
			adlInfo.adapters[i].adapterID        = -1;
			debug_log(LOG_DBG, "initADLInfo():  failed to get adapter Id for adapter: %d, unknown device, rc %d", lpAdapterInfo[i].iAdapterIndex, result);
			continue;
		}

		adlInfo.adapters[i].adapterIndex     = lpAdapterInfo[i].iAdapterIndex;
		adlInfo.adapters[i].bus              = lpAdapterInfo[i].iBusNumber;
		adlInfo.adapters[i].displayConnected = lpAdapterInfo[i].iPresent;
		adlInfo.adapters[i].displayMapped    = lpAdapterInfo[i].iFunctionNumber;
		adlInfo.adapters[i].adapterID        = lpAdapterID;

		debug_log( LOG_DBG, "initADLInfo(): i: %d, adapter index: %d, adapter ID: %d, bus: %d", 
			       i, adlInfo.adapters[i].adapterIndex , adlInfo.adapters[i].adapterID, adlInfo.adapters[i].bus
				 );

		if (last_adapter == -1)
			last_adapter = lpAdapterID;

		/* Each adapter may have multiple entries */
		if (lpAdapterID == last_adapter)
		{			
			adlInfo.adapters[i].gpu = gpuNumber;
			continue;
		}

		last_adapter = lpAdapterID;

		gpuNumber++;
		adlInfo.adapters[i].gpu = gpuNumber;
	}

	// -----------------------
	// Query fan capabilities.
	// -----------------------
	for(i=0; i < adlInfo.iNumberAdapters; i++)
	{
		ADLFanSpeedInfo speedInfo;

		memset(&speedInfo, 0, sizeof(speedInfo));
		speedInfo.iSize = sizeof(speedInfo);

		result = ADL_Overdrive5_FanSpeedInfo_Get(adlInfo.adapters[i].adapterIndex, 0, &speedInfo);
		adlInfo.adapters[i].supportFanPercentage = 0;
		adlInfo.adapters[i].supportFanRPM = 0;

		if (result != ADL_OK)
		{
			debug_log( LOG_ERR, 
						"initADLInfo(): failed to read fan speed info on gpu: %d, bus: %d, index: %d, id: %d, ADL rc: %d", 
						adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
				     );
		} else
		{
			if (speedInfo.iFlags & ADL_DL_FANCTRL_SUPPORTS_RPM_READ)
			{
				adlInfo.adapters[i].supportFanRPM = 1;
				debug_log( LOG_DBG, 
							"initADLInfo(): fan on gpu: %d support RPM speed queries, bus: %d, index: %d, id: %d, ADL rc: %d", 
							adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
 						 );
			} else
			{
				if (speedInfo.iFlags & ADL_DL_FANCTRL_SUPPORTS_PERCENT_READ)
				{
					adlInfo.adapters[i].supportFanPercentage = 1;
  				    debug_log( LOG_DBG, 
					  		   "initADLInfo(): fan on gpu: %d support percentage queries, bus: %d, index: %d, id: %d, ADL rc: %d", 
							   adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
 						     );
				} else
				{
					debug_log( LOG_ERR, 
								"initADLInfo(): fan on gpu: %d does not support RPM or percentage queries, bus: %d, index: %d, id: %d, ADL rc: %d", 
								adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
							 );
					adlInfo.adapters[i].fanRPM = -2;
				}
			}
		}
	}
			
	updateADLStats(&adlInfo);
	adl_close();

	debug_log( LOG_DBG, "initADLInfo(): freeing memory...");
	free(adlInfo.adapters);
	free(lpAdapterInfo);

	return 1;
} // end of initADLInfo()

int refreshADLInfo(void)
{
	int i       = 0;
	int j       = 0;
	int result  = 0;
	int maxTemp = 0;
	int minUtil = 101;

	ADLInfo adlInfo;

	ADLTemperature temp;
	ADLFanSpeedValue fan;
	
	getADLStats(&adlInfo);

	if (adlInfo.iADLStatus == ADL_STATUS_EXCEPTION)
	{
		debug_log(LOG_ERR, "refreshADLInfo(): ADL library is in ADL_STATUS_EXCEPTION state, will skip H/W reading...");
		return 0;
	}

	if (adl_open() == 0)
	{				
		debug_log(LOG_ERR, "refreshADLInfo(): failed to open ADL library, rc: %d", result);		
		return 0;
	}
			
	debug_log(LOG_DBG, "refreshADLInfo(): reading temperature, fan speed and gpu utilization...");		

	// ----------------
	// Refresh RT info.
	// ----------------
	for(i=0; i < adlInfo.iNumberAdapters; i++)
	{
		if (adlInfo.adapters[i].adapterID != -1 && adlInfo.adapters[i].adapterID != 0)
		{
			// ---------------------
			// Read the temperature.
			// ---------------------
			temp.iSize = sizeof(ADLTemperature);
			temp.iTemperature = 0;
			result = ADL_Overdrive5_Temperature_Get(adlInfo.adapters[i].adapterIndex, 0, &temp);
			if (result != ADL_OK)
			{
				debug_log( LOG_ERR, 
							"refreshADLInfo(): failed to read temperature on gpu: %d, bus: %d, index: %d, id: %d, ADL rc: %d", 
							adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
							);
				adlInfo.adapters[i].temperature = -1;
			} else
			{
				adlInfo.adapters[i].temperature = temp.iTemperature/1000;

				if (adlInfo.adapters[i].temperature > maxTemp)
				{
					maxTemp = adlInfo.adapters[i].temperature;
				}
			}

			// -------------------
			// Read the fan speed.
			// -------------------
			if (adlInfo.adapters[i].fanRPM != -2)
			{
				ADLFanSpeedInfo speedInfo;

				memset(&speedInfo, 0, sizeof(speedInfo));
				speedInfo.iSize = sizeof(speedInfo);

				if (adlInfo.adapters[i].supportFanRPM == 1)
				{
					fan.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
				} else
				{
					if (adlInfo.adapters[i].supportFanPercentage == 1)
					{
						fan.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;
					}
					else
					{
						adlInfo.adapters[i].fanRPM = -2;
						goto read_current_activity;
					}
				}

				fan.iSize = sizeof(ADLFanSpeedValue);
				//fan.iFlags = ADL_DL_FANCTRL_FLAG_USER_DEFINED_SPEED;
				fan.iFanSpeed = 0;
				result = ADL_Overdrive5_FanSpeed_Get(adlInfo.adapters[i].adapterIndex,0,&fan);
				if (result != ADL_OK)
				{
					debug_log( LOG_ERR, 
								"refreshADLInfo(): failed to read fan speed on gpu: %d, bus: %d, index: %d, id: %d, ADL rc: %d", 
								adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
								);
					adlInfo.adapters[i].fanRPM = -1;
				} else
				{
					adlInfo.adapters[i].fanRPM = fan.iFanSpeed;
					if (fan.iSpeedType == ADL_DL_FANCTRL_SPEED_TYPE_RPM)
					{
						if (adlInfo.adapters[i].fanRPM <= 0)
							adlInfo.iZeroFan = 1;
					} else
					{
						if (adlInfo.adapters[i].fanRPM < 0)
							adlInfo.iZeroFan = 1;
					}
				}
			}

read_current_activity:

			result = ADL_Overdrive5_CurrentActivity_Get(adlInfo.adapters[i].adapterIndex, &adlInfo.adapters[i].current);
			if (result != ADL_OK)
			{
				debug_log( LOG_ERR, "refreshADLInfo(): failed to read utilization of gpu: %d, bus: %d, index: %d, id: %d, ADL rc: %d",
							adlInfo.adapters[i].gpu, adlInfo.adapters[i].bus, adlInfo.adapters[i].adapterIndex, adlInfo.adapters[i].adapterID, result
							);
				adlInfo.adapters[i].current.iActivityPercent = -1;
			} else
			{
				if (adlInfo.adapters[i].current.iActivityPercent < minUtil)
					minUtil = adlInfo.adapters[i].current.iActivityPercent;
			}
		}
	}

	adlInfo.iMaxTemp = maxTemp;
	adlInfo.iMinActivity = minUtil;

	updateADLStats(&adlInfo);

	adl_close();

	debug_log( LOG_INF, "refreshADLInfo(): min GPU utilization: %d%%, max GPU temperature: %dC", adlInfo.iMinActivity, adlInfo.iMaxTemp);
	
	free(adlInfo.adapters);

	return 1;
} // end of refreshADLInfo()

DWORD WINAPI adlPollingThread(LPVOID pvParam)
{
	CGMConfig * _cfg = (CGMConfig *) pvParam;
	
	Miner_Info   _mi;
	int adlCallCount = 0;

	int waitIntervalSecs = _cfg->wdogAdlRefreshInterval;

	DWORD waitInterval = waitIntervalSecs*1000; // miliseconds	

	debug_log(LOG_SVR, "adlPollingThread(): will check ADL every %d seconds", waitIntervalSecs);

	wait(15000);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		int exceptionOccurred = 0;

		memset(&_mi, 0, sizeof(_mi));
		getMinerStats(&_mi);

		__try
		{
			if (exceptionOccurred == 0 && _mi.config.gpuCount > 0)  // gpu count might go to 0 during smart metering schedule
			{
				debug_log(LOG_INF, "adlPollingThread(): woke up, time to check ADL library");

				refreshADLInfo();
				adlCallCount++;

				if (adlCallCount > 10)
				{
					ADLInfo adlInfo;

					getADLStats(&adlInfo);
					if(adlInfo.adapters != NULL)
					{
						displayADLStats(&adlInfo);
						free(adlInfo.adapters);
					}
					adlCallCount = 0;
				}
			}

		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
			exceptionOccurred = 1;
			debug_log(LOG_ERR, "adlPollingThread(): exception caught in refreshADLInfo()");
		}

		if (exceptionOccurred)
		{
			set_adl_status(ADL_STATUS_CALL_FAILED);  // re-boot of OS is necessary
		}

		if (_mi.config.gpuCount > 0)
			debug_log(LOG_DBG, "adlPollingThread(): ADL H/W reading done, status: %s", adl_status_str());

		wait(waitInterval);		
	} // end of while (waitForShutdown(1) == 0)

	debug_log(LOG_SVR, "adlPollingThread(): exiting ADL thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of adlPollingThread()
