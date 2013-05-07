// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <conio.h>
#include "msprocess.h"
#include "log.h"
#include "miner_api.h"
#include "config.h"
#include "listen.h"
#include "miner_monitor.h"
#include "smtp.h"
#include "pool.h"
#include "btc.h"
#include "network.h"
#include "adl.h"
#include "smart_meter.h"

#define WAIT_ONE_BLOCK             5000 // 5 seconds Sleeps(), interruptable by shutdown semaphore

CGMConfig * cfg = NULL;

char * sysCmd = 0;
int restart = 0;

time_t started;
char startedOn[32];

char title[256];

HANDLE hCgmShutdown = NULL;

static HANDLE hMinerApiThread = NULL;
static DWORD dwMinerApiThreadID = 0;

static HANDLE hListenThread = NULL;
static DWORD dwListenThreadID = 0;

static HANDLE hMonitorThread = NULL;
static DWORD dwMonitorThreadID = 0;

static HANDLE hEmailStatusThread = NULL;
static DWORD dwEmailStatusThreadID = 0;

static HANDLE hPoolInfoThread = NULL;
static DWORD dwPoolInfoThreadID = 0;

static HANDLE hBTCQuotesThread = NULL;
static DWORD dwBTCQuotesThreadID = 0;

static HANDLE hLTCQuotesThread = NULL;
static DWORD dwLTCQuotesThreadID = 0;

static HANDLE hADLThread = NULL;
static DWORD dwADLThreadID = 0;

static HANDLE hSmartMeteringThread = NULL;
static DWORD dwSmartMeteringThreadID = 0;

HANDLE btcMtx = NULL;
HANDLE ltcMtx = NULL;

HANDLE poolMtx = NULL;
HANDLE gpuMtx = NULL;
HANDLE adlMtx = NULL;

void age(char * buf, int bufSize)
{
	time_t _now        = 0;

	long _value   = 0;
	long _days    = 0;
	long _hours   = 0;
	long _minutes = 0;
	long _seconds = 0; 

	time(&_now);

	_value = (long) (_now - started);

	_days    = _value/86400;
	_hours   = (_value - _days*86400)/3600;
	_minutes = (_value - _days*86400 - _hours*3600)/60;
	_seconds = _value -_days*86400 - _hours*3600 - _minutes*60;

	memset(buf, 0, bufSize);

	sprintf_s( buf, 
		       bufSize, 
		      "%03ld:%02ld:%02ld:%02ld",
	           _days,
	           _hours,
	           _minutes,
	           _seconds
		    );
}

int waitForShutdown(long timeout)
{
	return WaitForSingleObject(hCgmShutdown, timeout) == WAIT_OBJECT_0; // true if hCgmShutdown is set
}

void wait(long long howLong)
{
	DWORD oneBlock = WAIT_ONE_BLOCK;
	while (howLong > 0)
	{
		SleepEx(oneBlock, TRUE);
		howLong -= oneBlock;

		if (waitForShutdown(1) == 1) break;  // shutdown is set
	}
}

int getBTCMutex()
{
	return WaitForSingleObject(btcMtx, INFINITE) == WAIT_OBJECT_0;
}

void releaseBTCMutex()
{
	ReleaseMutex(btcMtx);
}

int getLTCMutex()
{
	return WaitForSingleObject(ltcMtx, INFINITE) == WAIT_OBJECT_0;
}

void releaseLTCMutex()
{
	ReleaseMutex(ltcMtx);
}

int getADLMutex()
{
	return WaitForSingleObject(adlMtx, INFINITE) == WAIT_OBJECT_0;
}

void releaseADLMutex()
{
	ReleaseMutex(adlMtx);
}

int getPoolMutex()
{
	return WaitForSingleObject(poolMtx, INFINITE) == WAIT_OBJECT_0;
}

void releasePoolMutex()
{
	ReleaseMutex(poolMtx);
}

int getGpuMutex()
{
	return WaitForSingleObject(gpuMtx, INFINITE) == WAIT_OBJECT_0;
}

void releaseGpuMutex()
{
	ReleaseMutex(gpuMtx);
}

void signalShutdown()
{
	SetEvent(hCgmShutdown);
	debug_log(LOG_SVR, "signalShutdown(): set shutdown event semaphore...");

	debug_log(LOG_SVR, "signalShutdown(): waiting for miner api thread to finish...");
	WaitForSingleObject(hMinerApiThread, INFINITE);
	CloseHandle(hMinerApiThread);
	debug_log(LOG_SVR, "signalShutdown(): monitor api thread is no longer running.");


	debug_log(LOG_SVR, "signalShutdown(): waiting for miner monitor thread to finish...");
	WaitForSingleObject(hMonitorThread, INFINITE);
	CloseHandle(hMonitorThread);
	debug_log(LOG_SVR, "signalShutdown(): monitor thread is no longer running.");

	if (cfg->wdogDisableRemoteApi == 0)  // Enabled
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for API listen thread to finish...");
		WaitForSingleObject(hListenThread, INFINITE);
		CloseHandle(hListenThread);
		debug_log(LOG_SVR, "signalShutdown(): api listen thread is no longer running.");
	}

	if (cfg->wdogDisableNotifications == 0) // Enabled
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for email status thread to finish...");
		WaitForSingleObject(hEmailStatusThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): email status thread is no longer running.");
		CloseHandle(hEmailStatusThread);
	}

	if (cfg->poolInfoDisabled == 0)
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for pool polling thread to finish...");
		WaitForSingleObject(hPoolInfoThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): pool polling thread is no longer running.");
		CloseHandle(hPoolInfoThread);		
	}

	if (cfg->btcQuotesDisabled == 0)
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for btc quotes thread to finish...");
		WaitForSingleObject(hBTCQuotesThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): btc quotes thread is no longer running.");
		CloseHandle(hBTCQuotesThread);		
	}

	if (cfg->ltcQuotesDisabled == 0)
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for ltc quotes thread to finish...");
		WaitForSingleObject(hLTCQuotesThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): ltc quotes thread is no longer running.");
		CloseHandle(hLTCQuotesThread);		
	}

	if (cfg->wdogAdlDisabled == 0)
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for ADL thread to finish...");
		WaitForSingleObject(hADLThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): ADL thread is no longer running.");
		CloseHandle(hADLThread);		
	}

	if (cfg->smartMeterDisabled == 0)
	{
		debug_log(LOG_SVR, "signalShutdown(): waiting for smart metering thread to finish...");
		WaitForSingleObject(hSmartMeteringThread, INFINITE);
		debug_log(LOG_SVR, "signalShutdown(): smart metering thread is no longer running.");
		CloseHandle(hSmartMeteringThread);		
	}
	
	debug_log(LOG_SVR, "signalShutdown(): closing hCgmShutdown handle...");
	CloseHandle(hCgmShutdown);

    debug_log(LOG_SVR, "signalShutdown(): closing btcMtx handle...");
	CloseHandle(btcMtx);

    debug_log(LOG_SVR, "signalShutdown(): closing ltcMtx handle...");
	CloseHandle(ltcMtx);

    debug_log(LOG_SVR, "signalShutdown(): closing poolMtx handle...");
	CloseHandle(poolMtx);

    debug_log(LOG_SVR, "signalShutdown(): closing gpuMtx handle...");
	CloseHandle(gpuMtx);

	debug_log(LOG_SVR, "signalShutdown(): closing adlMtx handle...");
	CloseHandle(adlMtx);
}

static void sighandler(int sig)
{
	debug_log(LOG_SVR, "received %d signal, exiting...",sig);
	signalShutdown();

	debug_log(LOG_SVR, "main(): cleaning up Winsockets...");
	WSACleanup();
	debug_log(LOG_SVR, "main(): ----------- Exiting -------------");
	debug_logClose();

	exit(0);
}

int main(int argc, char *argv[])
{		
	Miner_Info   _mi;
	
	int smtpInitCount = 0;
	int rc = 0;
	int c = 0;
	int i = 0;
	int wsa = 0; 
	ADLInfo adlInfo;

    struct tm * stTimeTm;

	WSADATA WSA_Data;
	cgmProcessInfo _minerProcessInfo;

	char * startStr = NULL;
	char * endStr = NULL;
	char progName[MAX_PATH+1];
	
	int numOfAkbash = numberOfAkbashProcesses(argv[0]);

	if (numOfAkbash > 1)
	{
		printf("main(): another %s process is already running.  No need to run more than one instance. \n", argv[0]);
		return -1;
	}

	if (argc != 2)
	{
		fprintf(stderr, "\n%s\n", title);
		fprintf(stderr, "\n\tUsage: %s akbash.cfg\n", argv[0]);
		exit(-1);
	}

	sprintf(title, "akbash watchdog ver. %s (%s) - copyright (c) 2012 by Peter Moss", WDOG_VERSION, argv[1]);

	_tzset(); // set timezone info in C-lib

	time(&started);

	stTimeTm = localtime(&started);

	memset(startedOn, 0, sizeof(startedOn));

	sprintf( startedOn, 
		     "%02d/%02d/%04d %02d:%02d:%02d",
			 stTimeTm->tm_mon+1,
			 stTimeTm->tm_mday,
			 stTimeTm->tm_year+1900,
			 stTimeTm->tm_hour,
			 stTimeTm->tm_min,
			 stTimeTm->tm_sec 
		   );


	cfg = parseConfigFile(argv[1]);

	if (cfg == NULL)
	{
		fprintf(stderr, "\n%s\n", title);
		fprintf(stderr, "\n\tUsage: %s akbash.cfg\n", argv[0]);
		exit(-1);
	}

	signal(SIGTERM, sighandler);
	signal(SIGINT,  sighandler);

	debug_logOpen(cfg->wdogLogFileName, cfg->wdogLogFileSize, cfg->wdogLogLevel);

	SetConsoleTitleA(title);

	memset(progName, 0, sizeof(progName));

	strcpy_s(progName, sizeof(progName), argv[0]);

	if (strstr(progName, ".exe") == NULL)
	{
		strcat_s(progName, sizeof(progName), ".exe");
	}

	endStr = progName;

	while ((startStr = strstr(endStr, "\\")) != NULL)
	{
		endStr = startStr+1;
	}

	strcpy_s(cfg->wdogExeName, sizeof(cfg->wdogExeName), endStr);

	debug_log(LOG_SVR, "main(): ----------- Starting -------------");
	debug_log(LOG_SVR, "main(): %s", title);
	debug_log(LOG_SVR, "main(): akbash started at: %s",startedOn);
	debug_log(LOG_SVR, "main(): akbash exe name: %s", cfg->wdogExeName);
	
	debug_log(LOG_DBG, "main(): ---------------------------------");
	debug_log(LOG_DBG, "main(): watchdog specific config entries:");
	debug_log(LOG_DBG, "main(): ---------------------------------");
	debug_log(LOG_DBG, "main(): %s: %s", LOG_FILE_LABEL, cfg->wdogLogFileName);
	debug_log(LOG_DBG, "main(): %s: %d [bytes]", LOG_FILE_SIZE, cfg->wdogLogFileSize);	
	debug_log(LOG_DBG, "main(): %s: %d", WDOG_LOG_LEVEL, cfg->wdogLogLevel);	
	debug_log(LOG_DBG, "main(): %s: %d", NUMBER_OF_RESTARTS, cfg->wdogNumberOfRestarts);
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", ALIVE_TIMEOUT, cfg->wdogAliveTimeout);
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", NOT_CONNECTED_TIMEOUT, cfg->wdogNotConnectedTimeout);
	debug_log(LOG_DBG, "main(): %s: %s", LISTEN_IP, cfg->wdogListenIP);
	debug_log(LOG_DBG, "main(): %s: %s", WDOG_RIG_NAME, cfg->wdogRigName);
	debug_log(LOG_DBG, "main(): %s: %d", LISTEN_PORT, cfg->wdogListenPort);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_API, cfg->wdogDisableRemoteApi);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_HELP, cfg->wdogDisableRemoteHelp);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_RESTART, cfg->wdogDisableRemoteRestart);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_REBOOT, cfg->wdogDisableRemoteReboot);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_GETLOG, cfg->wdogDisableRemoteGetLog);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_REMOTE_STATUS, cfg->wdogDisableRemoteStatus);
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_NOTIFICATION_EMAILS, cfg->wdogDisableNotifications);
    debug_log(LOG_DBG, "main(): %s: %d", DISABLE_STATUS_NOTIFICATION_EMAILS, cfg->wdogDisableStatusNotifications);
	debug_log(LOG_DBG, "main(): %s: %s", NOTIFICATIONS_EMAIL, cfg->wdogEmailAddress);
	debug_log(LOG_DBG, "main(): %s: %d [minutes]", STATUS_NOTIFICATION_FREQ, cfg->wdogStatusNotificationFrequency);
	debug_log(LOG_DBG, "main(): %s: %d [deg. C]", CUT_OFF_TEMPERATURE, cfg->wdogCutOffTemperatureThreshold);	
	debug_log(LOG_DBG, "main(): %s: %d [minutes]", CUT_OFF_TEMPERATURE_COOLDOWN, cfg->wdogCutOffTemperatureCooldownPeriod);	
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_ADL_READINGS, cfg->wdogAdlDisabled);
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", ADL_HW_POLLING, cfg->wdogAdlRefreshInterval);	
	debug_log(LOG_DBG, "main(): %s: %d [%%]", ADL_HW_ACTIVITY_THRESHOLD, cfg->wdogAdlGpuActivityThreshold);	
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", ADL_HW_ACTIVITY_TIMEOUT, cfg->wdogAdlGpuActivityTimeout);	

	debug_log(LOG_DBG, "main(): ------------------------------");
    debug_log(LOG_DBG, "main(): miner specific config entries:");
    debug_log(LOG_DBG, "main(): ------------------------------");
    debug_log(LOG_DBG, "main(): %s: %s", CGMINER_EXE_LABEL, cfg->minerExeFullPath);	
	debug_log(LOG_DBG, "main(): %s: %s", CGMINER_NAME_LABEL, cfg->minerExeName);
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", MINER_INIT_INTERVAL, cfg->minerInitInterval);
	debug_log(LOG_DBG, "main(): %s: %.2f [Mh/s]", TOTAL_AVG_RATE, cfg->minerGPUAvgRateThreshold);
	debug_log(LOG_DBG, "main(): %s: %.2f [Mh/s]", PGA_AVG_RATE, cfg->minerPGAAvgRateThreshold);
	debug_log(LOG_DBG, "main(): %s: %ld [MB]", MINER_WORKING_SET_THRESHOLD, cfg->minerWorkingSetThreshold/1024/1024);
	debug_log(LOG_DBG, "main(): %s: %ld [open handles]", MINER_HANDLE_COUNT_THRESHOLD, cfg->minerHandleCountThreshold);
	debug_log(LOG_DBG, "main(): %s: %d [seconds]", MINER_API_POLL_INTERVAL, cfg->minerPollInterval);
	debug_log(LOG_DBG, "main(): %s: %d [h/w errors]", MINER_HW_ERRORS_THRESHOLD, cfg->minerHWErrorsThreshold);
	debug_log(LOG_DBG, "main(): %s: %s", MINER_LISTEN_IP, cfg->minerListenIP);
	debug_log(LOG_DBG, "main(): %s: %d", MINER_LISTEN_PORT, cfg->minerListenPort);
	debug_log(LOG_DBG, "main(): %s: %d", MINER_SOLO_MINING, cfg->minerSoloMining);	

	debug_log(LOG_DBG, "main(): --------------------");
	debug_log(LOG_DBG, "main(): pool config entries:");
    debug_log(LOG_DBG, "main(): --------------------");
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_POOL_INFO, cfg->poolInfoDisabled);
	debug_log(LOG_DBG, "main(): %s: %d [minutes]", POOL_REFRESH_INTERVAL, cfg->poolRefreshInterval);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_API_URL, cfg->poolApiUrl);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_BALANCE_LABEL, cfg->poolBalanceLabel);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_HASH_RATE_LABEL, cfg->poolHashRateLabel);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_VALIDS_LABEL, cfg->poolValidsLabel);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_STALES_LABEL, cfg->poolStalesLabel);
	debug_log(LOG_DBG, "main(): %s: %s", POOL_INVALIDS_LABEL, cfg->poolInvalidsLabel);
	debug_log(LOG_DBG, "main(): %s: %s", "parsed pool host", cfg->poolHost);
	debug_log(LOG_DBG, "main(): %s: %d", "parsed pool port", cfg->poolPort);
	debug_log(LOG_DBG, "main(): %s: %s", "parsed pool uri", cfg->poolUri);

	debug_log(LOG_DBG, "main(): ------------------");
	debug_log(LOG_DBG, "main(): BTC quote entries:");
    debug_log(LOG_DBG, "main(): ------------------");
	debug_log(LOG_DBG, "main(): %s: %d", DISABLE_BTC_QUOTES, cfg->btcQuotesDisabled);
	debug_log(LOG_DBG, "main(): %s: %d [minutes]", QUOTE_REFRESH_INTERVAL, cfg->btcQuotesRefreshInterval);
	debug_log(LOG_DBG, "main(): %s: %s", QUOTE_URL, cfg->btcUrl);
	debug_log(LOG_DBG, "main(): %s: %s", QUOTE_LAST_LABEL, cfg->btcLastLabel);
	debug_log(LOG_DBG, "main(): %s: %s", QUOTE_ASK_LABEL, cfg->btcAskLabel);
	debug_log(LOG_DBG, "main(): %s: %s", QUOTE_BID_LABEL, cfg->btcBidLabel);

	debug_log(LOG_DBG, "main(): -----------------------");
	debug_log(LOG_DBG, "main(): Smart Metering entries:");
    debug_log(LOG_DBG, "main(): -----------------------");
	debug_log(LOG_DBG, "main(): %s: %d", SMART_METERING_DISABLE, cfg->smartMeterDisabled);
	debug_log(LOG_DBG, "main(): %s: %d", SMART_METERING_POLLING_INTERVAL, cfg->smartMeterPollingInterval);
	debug_log(LOG_DBG, "main(): %s: %s", SMART_METERING_ON_PEAK_START_TIME, cfg->smartMeterOnPeakStartTime);
	debug_log(LOG_DBG, "main(): %s: %s", SMART_METERING_OFF_PEAK_START_TIME, cfg->smartMeterOffPeakStartTime);
	debug_log(LOG_DBG, "main(): %s: %d", SMART_METERING_ON_PEAK_SHUTDOWN, cfg->smartMeterOnPeakShutdown);
	debug_log(LOG_DBG, "main(): %s: %d", SMART_METERING_ON_PEAK_DISABLE_GPUS, cfg->smartMeterOnPeakDisableGPUs);

	debug_log(LOG_DBG, "main(): ---------------------------------");

	sysCmd = (char *) calloc(strlen(cfg->minerExeFullPath)+7, 1);
	if (sysCmd == NULL)
	{
		debug_log(LOG_ERR, "main(): failed to allocate memory (len: %d) for sysCmd.\n", strlen(cfg->minerExeFullPath)+7);
		debug_logClose();
		exit(0);
	}

	sprintf(sysCmd, "start %s", cfg->minerExeFullPath);

	if (wsa = WSAStartup(0x0202, &WSA_Data)) 
	{ 
		debug_log(LOG_ERR, "main(): WSAStartup failed: %d", wsa); 
		debug_logClose();
		exit(0);
	}

	hCgmShutdown = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (hCgmShutdown == NULL)
	{
		debug_log(LOG_ERR, "Failed to create hCgmShutdown event");
		debug_logClose();
		exit(0);
	}

	ResetEvent(hCgmShutdown);

	btcMtx = CreateMutex(NULL, FALSE, NULL);
	if (btcMtx == NULL)
	{
		debug_log(LOG_ERR, "Failed to create btcMtx mutex");
		debug_logClose();
		CloseHandle(hCgmShutdown);
		exit(0);
	}

    ltcMtx = CreateMutex(NULL, FALSE, NULL);
	if (ltcMtx == NULL)
	{
		debug_log(LOG_ERR, "Failed to create ltcMtx mutex");
		debug_logClose();
		CloseHandle(btcMtx);
		CloseHandle(hCgmShutdown);
		exit(0);
	}

	poolMtx = CreateMutex(NULL, FALSE, NULL);
	if (poolMtx == NULL)
	{
		debug_log(LOG_ERR, "Failed to create poolMtx mutex");
		debug_logClose();
		CloseHandle(hCgmShutdown);
		CloseHandle(btcMtx);
		CloseHandle(ltcMtx);
		CloseHandle(ltcMtx);
		exit(0);
	}

	gpuMtx = CreateMutex(NULL, FALSE, NULL);
	if (gpuMtx == NULL)
	{
		debug_log(LOG_ERR, "Failed to create gpuMtx mutex");
		debug_logClose();
		CloseHandle(hCgmShutdown);
		CloseHandle(btcMtx);
		CloseHandle(ltcMtx);
		CloseHandle(poolMtx);
		exit(0);
	}

	adlMtx = CreateMutex(NULL, FALSE, NULL);
	if (adlMtx == NULL)
	{
		debug_log(LOG_ERR, "Failed to create adlMtx mutex");
		debug_logClose();
		CloseHandle(hCgmShutdown);
		CloseHandle(btcMtx);
		CloseHandle(ltcMtx);
		CloseHandle(poolMtx);
		CloseHandle(gpuMtx);
		exit(0);
	}

init_smtp:	
	
	rc = initSmtp(cfg->wdogEmailAddress, cfg->wdogDisableNotifications);

	if (rc == 1) // failed due to some network/DNS failure
	{
		if (cfg->wdogDisableNotifications == 0)
		{
			smtpInitCount++;
			if (smtpInitCount < 5)
			{
				debug_log( LOG_SVR, 
						   "main(): initSmtp(\'%s\', %d) failed, sleeping 15 seconds before retrying...retry: %d/5", 
						   cfg->wdogEmailAddress, cfg->wdogDisableNotifications, smtpInitCount
						 ); 

				Sleep(15000);
				goto init_smtp;			
			} else
			{
				debug_log( LOG_SVR, 
						   "main(): initSmtp(\'%s\', %d) failed, retry: %d/3; email notifications are disabled", 
						   cfg->wdogEmailAddress, cfg->wdogDisableNotifications, smtpInitCount
						 ); 
				cfg->wdogDisableNotifications = 1;
			}

		}
	}

	reset_adl_stats();

	if (cfg->wdogAdlDisabled == 0)
	{
		if (initADLInfo() == 0)
		{
			debug_log(LOG_SVR, "main(): refreshADLInfo() failed, temperature monitoring will be disabled."); 
			cfg->wdogAdlDisabled = 1;
		} else
		{
			refreshADLInfo();

			debug_log(LOG_INF, "main(): refreshADLInfo() ADL H/W reading was successful, library status: %s, H/W monitoring will be enabled.", adl_status_str()); 
			cfg->wdogAdlDisabled = 0;
			getADLStats(&adlInfo);
			displayADLStats(&adlInfo);
			free(adlInfo.adapters);
		}
	}

	debug_log(LOG_DBG, "main(): re-setting miner status...");
	resetMinerInfo();

	debug_log(LOG_INF, "main(): checking if miner is running...");
	getProcessInfo(cfg->minerExeName, &_minerProcessInfo);

	while (_minerProcessInfo.processID == 0)
	{
		debug_log(LOG_SVR, "main(): miner is not running...sleeping for 5 seconds and retrying...");
		Sleep(5000);

		if (_minerProcessInfo.processID == 0)
			getProcessInfo(cfg->minerExeName, &_minerProcessInfo);

		if (_minerProcessInfo.processID == 0)
		{
			debug_log(LOG_SVR, "main(): miner is not running...restarting");
			restartMiner(DEFAULT_RESTART_DELAY, WDOG_RESTART_MINER_NOT_RUNNING);
		
			debug_log(LOG_SVR, "main(): waiting %d seconds for miner to settle down...", cfg->minerInitInterval);
			Sleep(cfg->minerInitInterval*1000);
		}

		getProcessInfo(cfg->minerExeName, &_minerProcessInfo);
	}

	// -----------------------------------------
	// Checking cgminer/bfgminer status.
	// -----------------------------------------
	initMinerListeningParameters(cfg->minerListenIP, cfg->minerListenPort);
	debug_log(LOG_INF, "main(): checking miner status...");
	fetchMinerInfo(&_mi);

	if (_mi.config.gpuCount < 1 && _mi.config.pgaCount < 1)
	{
		debug_log(LOG_SVR, "main(): unable to retrieve number of devices (GPU/PGA), waiting %d seconds for miner to initialize...", cfg->minerInitInterval);
		Sleep(cfg->minerInitInterval*1000);

		fetchMinerInfo(&_mi);

		if (_mi.config.gpuCount < 1 && _mi.config.pgaCount < 1)
		{
			debug_log(LOG_SVR, "main(): unable to retrieve number of devices (GPU/PGA), attempting to restart miner...");
			restartMiner(DEFAULT_RESTART_DELAY, WDOG_RESTART_MINER_NOT_RUNNING);
		
			debug_log(LOG_SVR, "main(): waiting %d seconds for miner to settle down...", cfg->minerInitInterval);
			Sleep(cfg->minerInitInterval*1000);
		}

		fetchMinerInfo(&_mi);
	}

	if (_mi.config.gpuCount < 1)
	{
		cfg->wdogAdlDisabled = 1;
		debug_log(LOG_SVR, "main(): no GPUs used by the miner, GPU H/W monitoring will be disabled...");
	}

	displayMinerInfoObject(&_mi);

	hMonitorThread = CreateThread(NULL, 0, monitorThread, (void *) cfg, 0, &dwMonitorThreadID);  
	debug_log(LOG_SVR, "main(): monitor thread created, handle: %d, threadId: 0x%04x", hMonitorThread, dwMonitorThreadID);

	hMinerApiThread = CreateThread(NULL, 0, minerApiThread, (void *) cfg, 0, &dwMinerApiThreadID);  
	debug_log(LOG_SVR, "main(): miner api thread created, handle: %d, threadId: 0x%04x", hMinerApiThread, dwMinerApiThreadID);

	if (cfg->wdogDisableRemoteApi == 0)  // Enabled
	{
		hListenThread = CreateThread(NULL, 0, listenForCommands, NULL, 0, &dwListenThreadID);  
		debug_log(LOG_SVR, "main(): listen thread created, handle: %d, threadId: 0x%04x", hListenThread, dwListenThreadID);
	}

	if (cfg->wdogDisableNotifications == 0 && cfg->wdogDisableStatusNotifications == 0) // Enabled
	{
		hEmailStatusThread = CreateThread(NULL, 0, statusEmailThread, (void *) cfg->wdogStatusNotificationFrequency, 0, &dwEmailStatusThreadID);  
		debug_log(LOG_SVR, "main(): email notifications thread created, handle: %d, threadId: 0x%04x", hEmailStatusThread, dwEmailStatusThreadID);
	}

	if (cfg->poolInfoDisabled == 0)
	{
		hPoolInfoThread = CreateThread(NULL, 0, poolPollingThread, (void *) cfg, 0, &dwPoolInfoThreadID);  
		debug_log(LOG_SVR, "main(): pool polling thread created, handle: %d, threadId: 0x%04x", hPoolInfoThread, dwPoolInfoThreadID);
	}

	if (cfg->btcQuotesDisabled == 0)
	{
		hBTCQuotesThread = CreateThread(NULL, 0, btcQuotesThread, (void *) cfg, 0, &dwBTCQuotesThreadID);  
		debug_log(LOG_SVR, "main(): BTC quotes thread created, handle: %d, threadId: 0x%04x", hBTCQuotesThread, dwBTCQuotesThreadID);
	}

	if (cfg->ltcQuotesDisabled == 0)
	{
		hLTCQuotesThread = CreateThread(NULL, 0, ltcQuotesThread, (void *) cfg, 0, &dwLTCQuotesThreadID);  
		debug_log(LOG_SVR, "main(): LTC quotes thread created, handle: %d, threadId: 0x%04x", hLTCQuotesThread, dwLTCQuotesThreadID);
	}

	if (cfg->wdogAdlDisabled == 0)
	{
		hADLThread = CreateThread(NULL, 0, adlPollingThread, (void *) cfg, 0, &dwADLThreadID);  
		debug_log(LOG_SVR, "main(): ADL thread created, handle: %d, threadId: 0x%04x", hADLThread, dwADLThreadID);
	}

	if (cfg->smartMeterDisabled == 0)
	{
		hSmartMeteringThread = CreateThread(NULL, 0, smartMeteringThread, (void *) cfg, 0, &dwSmartMeteringThreadID);  
		debug_log(LOG_SVR, "main(): smart metering thread created, handle: %d, threadId: 0x%04x", hSmartMeteringThread, dwSmartMeteringThreadID);
	}

	if (cfg->wdogDisableNotifications == 0) // Enabled
		send_smtp_wdog_restarted_msg();

	for (;;)
	{
		c = _getch_nolock();
		switch (c)
		{
			case 'q':
			case 'Q':
				debug_log(LOG_SVR, "main(): user pressed: \'%c\', terminating...", c);
				sighandler(c);
				break;
			case 3:
				debug_log(LOG_SVR, "main(): user pressed: CTRL+C terminating...");
				sighandler(c);
				break;
			case '0':
				debug_logLevel(0);
				break;
			case '1':
				debug_logLevel(1);
				break;
			case '2':
				debug_logLevel(2);
				break;
			case '3':
				debug_logLevel(3);
				break;

		}
	}
}
