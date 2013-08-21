// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <time.h>
#include "msprocess.h"
#include "miner_api.h"
#include "log.h"
#include "listen.h"
#include "wdmain.h"
#include "adl.h"
#include "miner_monitor.h"
#include "network.h"

enum GPU_STATUS gPrevStatus = NOT_RUNNING;
enum GPU_STATUS gCurrStatus = NOT_STARTED;

cgmProcessInfo gMinerProcessInfo;

void formatReasons(char * buf, int bufSize, int reason, int device)
{
	char p[256];

	memset(p, 0, sizeof(p));
	memset(buf, 0, sizeof(bufSize));

	if (reason & WDOG_RESTART_TEMPERATURE_THRESHOLD)
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_TEMPERATURE_THRESHOLD (%d)<br>", cfg->wdogCutOffTemperatureThreshold);
		strcat(buf, p);
	}
	if (reason & WDOG_RESTART_ACTIVITY_THRESHOLD)
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_ACTIVITY_THRESHOLD (%d)<br>", cfg->wdogAdlGpuActivityThreshold);
		strcat(buf, p);
	}

	if (reason & WDOG_RESTART_FAN_DOWN)                 strcat(buf, "WDOG_RESTART_FAN_DOWN<br>");
	if (reason & WDOG_RESTART_MINER_CRASH)              strcat(buf, "WDOG_RESTART_MINER_CRASH<br>");

    if (reason & WDOG_RESTART_GPU_HASH_THRESHOLD)       
	{
		if (device > -1)
			sprintf_s(p, sizeof(p), "WDOG_RESTART_GPU_HASH_THRESHOLD (%.2f), GPU: %d<br>", cfg->minerGPUAvgRateThreshold, device);
		else
			sprintf_s(p, sizeof(p), "WDOG_RESTART_GPU_HASH_THRESHOLD (%.2f)<br>", cfg->minerGPUAvgRateThreshold);

		strcat(buf, p);
	}

	if (reason & WDOG_REBOOT_DRIVER_CRASH)              strcat(buf, "WDOG_REBOOT_DRIVER_CRASH<br>");
	if (reason & WDOG_RESTART_MINER_NOT_RUNNING)        strcat(buf, "WDOG_RESTART_MINER_NOT_RUNNING<br>");
	if (reason & WDOG_RESTART_MINER_NOT_RESPONDING)     strcat(buf, "WDOG_RESTART_MINER_NOT_RESPONDING<br>");

	if (reason & WDOG_RESTART_WORKING_SET_THRESHOLD) 
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_WORKING_SET_THRESHOLD (%u)<br>", cfg->minerWorkingSetThreshold);
		strcat(buf, p);
	}

	if (reason & WDOG_RESTART_HANDLE_COUNT_THRESHOLD) 
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_HANDLE_COUNT_THRESHOLD (%u)<br>", cfg->minerHandleCountThreshold);
		strcat(buf, p);
	}

	if (reason & WDOG_RESTART_VIA_HTTP)                 strcat(buf, "WDOG_RESTART_VIA_HTTP<br>");
	if (reason & WDOG_RESTART_NOT_ALIVE)                strcat(buf, "WDOG_RESTART_NOT_ALIVE<br>");
	
	if (reason & WDOG_RESTART_PGA_HASH_THRESHOLD) 
	{
		if (device > -1)
			sprintf_s(p, sizeof(p), "WDOG_RESTART_PGA_HASH_THRESHOLD (%.2f), PGA: %d<br>", cfg->minerPGAAvgRateThreshold, device);	
		else
			sprintf_s(p, sizeof(p), "WDOG_RESTART_PGA_HASH_THRESHOLD (%.2f)<br>", cfg->minerPGAAvgRateThreshold);	

		strcat(buf, p);
	}

	if (reason & WDOG_REBOOT_MAX_RESTARTS) 
	{
		sprintf_s(p, sizeof(p), "WDOG_REBOOT_MAX_RESTARTS (%d)<br>", cfg->wdogNumberOfRestarts);
		strcat(buf, p);
	}

	if (reason & WDOG_REBOOT_NOT_RUNNING_AFTER_RESTART) strcat(buf, "WDOG_REBOOT_NOT_RUNNING_AFTER_RESTART<br>");
	if (reason & WDOG_REBOOT_VIA_HTTP)                  strcat(buf, "WDOG_REBOOT_VIA_HTTP<br>");

	if (reason & WDOG_RESTART_GPU_ACTIVITY_THRESHOLD) 
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_GPU_ACTIVITY_THRESHOLD (%d)<br>", cfg->wdogAdlGpuActivityThreshold);
		strcat(buf, p);
	}

	if (reason & WDOG_RESTART_MINER_HW_ERRORS_THRESHOLD) 
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_MINER_HW_ERRORS_THRESHOLD (%u)<br>", cfg->minerHWErrorsThreshold);
		strcat(buf, p);
	}

	if (reason & WDOG_RESTART_PGA_HAS_BEEN_DISABLED) 
	{
		sprintf_s(p, sizeof(p), "WDOG_RESTART_PGA_HAS_BEEN_DISABLED<br>");
		strcat(buf, p);
	}
} // end of formatReasons()

void restartMiner(int cooldownPeriod, int reason, int device)
{
	incrementRestartCount();

	getProcessInfo(cfg->minerExeName, &gMinerProcessInfo);

	if (shouldWeReboot(cfg))
	{
		debug_log(LOG_SVR, "restartMiner(): too many restart attempts, rebootting OS");
		reboot(reason | WDOG_REBOOT_MAX_RESTARTS);
	}

	// kill the sick puppies
	if (gMinerProcessInfo.werFaultID != 0)
	{
		debug_log(LOG_SVR, "restartMiner(): killing wer fault: %d process", gMinerProcessInfo.werFaultID);
		//killProcessByID(gMinerProcessInfo.werFaultID);
		killAllWERProcesses();
	}

	if (gMinerProcessInfo.processID != 0)
	{
		debug_log(LOG_SVR, "restartMiner(): killing miner process: %d", gMinerProcessInfo.processID);
		killProcessByID(gMinerProcessInfo.processID);
	}

	gPrevStatus = NOT_RUNNING;
	gCurrStatus = KILLED;

	debug_log(LOG_SVR, "restartMiner(): sleeping for %d seconds before restarting a new miner...", cooldownPeriod);
	wait(cooldownPeriod*1000);

	// restart new instance
	debug_log(LOG_INF, "restartMiner(): ---------------------------------------------");
	if (gMinerProcessInfo.processID != 0)
	{
		debug_log(LOG_INF, "restartMiner(): old process ID: %d", gMinerProcessInfo.processID);
		debug_log(LOG_INF, "restartMiner(): process name: %s", gMinerProcessInfo.szProcessName);
	}
	debug_log(LOG_INF, "restartMiner(): starting new instance...");
	debug_log(LOG_INF, "restartMiner(): command line: %s", sysCmd);
	
	system(sysCmd);
	
	send_smtp_restarted_msg(reason, device);
	debug_log(LOG_INF, "restartMiner(): sleeping for %d seconds...", cfg->minerInitInterval);

	restart = 0;

	wait(cfg->minerInitInterval*1000);

	getProcessInfo(cfg->minerExeName, &gMinerProcessInfo);

	debug_log(LOG_INF, "restartMiner(): checking if miner is running...");
	if (gMinerProcessInfo.processID == 0)
	{
		debug_log(LOG_SVR, "restartMiner(): unable to find miner process after restart, attempting to reboot OS");
		reboot(reason | WDOG_REBOOT_NOT_RUNNING_AFTER_RESTART);
	} else
	{
		debug_log(LOG_INF, "restartMiner(): new minerprocess ID: %d", gMinerProcessInfo.processID);
	}
} // end of restartMiner()

DWORD WINAPI monitorThread( LPVOID lpParam )
{
	Miner_Info   _mi;
	CGMConfig * _cfg = (CGMConfig *) lpParam;
	
	ADLInfo _adlInfo;

	int resetHashRestartCount = 0;
	int restartHashCount = 0;
	int i = 0;
	time_t notConnectedTimer;
	time_t now;

	time_t aliveTimer;
	time_t gpuUtilTimer = 0;

	long long waitLeft = 0;

	double timePeriod = 0;
	int restartDelay = DEFAULT_RESTART_DELAY; 
	int restartReason = 0;
	long lastAccepted = 0;
	int faultyDevice = -1;
	double lastBestShare = 0;

	time(&notConnectedTimer); 

	while (waitForShutdown(1) == 0)	
	{
		waitLeft = 30000;
		faultyDevice = -1;

		getProcessInfo(cfg->minerExeName, &gMinerProcessInfo);

		// To do:
		// if wefault present, kill all werfault processes send "crashed" message then re-check miner process

		if (waitForShutdown(1)) break;

		if (gMinerProcessInfo.processID == 0)
		{
			gCurrStatus = NOT_RUNNING;

			debug_log(	LOG_SVR, "monitorThread(): miner process is not running, status: %s -> %s, restarting...", 
						gpuStatusStr(gPrevStatus),
						gpuStatusStr(gCurrStatus)
			         );

			restart = 1; // send miner not running message
			restartReason |= WDOG_RESTART_MINER_NOT_RUNNING;
			goto miner_restart_check;
		}

		// ----------------------------------------
		// process is running, check if it crashed.
		// ----------------------------------------
		if (gMinerProcessInfo.werFaultID != 0)
		{
			restart = 1;
			restartReason |= WDOG_RESTART_MINER_CRASH;
			goto miner_restart_check;
		} 

		getMinerStats(&_mi);

		gCurrStatus = _mi.status;

		// If in NOT_CONNECTED longer than X minutes, restart it anyway
		if (gCurrStatus == NOT_CONNECTED)
		{
			if (gPrevStatus != NOT_CONNECTED)
			{
				time(&notConnectedTimer); // entering NOT CONNECTED
				debug_log(LOG_INF, "monitorThread(): wdog is entering state: %s -> %s", gpuStatusStr(gPrevStatus), gpuStatusStr(gCurrStatus));
			} else
			{
				// check if notConnected timer has been set more than XX minutes ago
				time(&now);
				timePeriod = difftime(now, notConnectedTimer);

				if (timePeriod > cfg->wdogNotConnectedTimeout)
				{
					restart = 1; // send "not connected" msg
					debug_log(LOG_SVR, "monitorThread(): wdog has been in %s state for longer than %d minutes, restarting miner...timePeriod: %f", gpuStatusStr(gCurrStatus), cfg->wdogNotConnectedTimeout/60, timePeriod);
					restartReason |= WDOG_RESTART_MINER_NOT_RESPONDING;
					goto miner_restart_check;
				}
			}
		} 

		// ------------------------------
		// Checking general process info.
		// ------------------------------
		if (gCurrStatus != ALIVE && gCurrStatus != NOT_CONNECTED) 
		{
			debug_log(LOG_INF, "monitorThread(): current miner state: %s, will attempt to restart it.", gpuStatusStr(gCurrStatus));

			restart = 1; // not in alive state msg
			restartReason |= WDOG_RESTART_NOT_ALIVE;
			goto miner_restart_check;
		}
			
		if (gMinerProcessInfo.workingSetSize > cfg->minerWorkingSetThreshold)
		{
			debug_log( LOG_SVR, "monitorThread(): working set size (%ld) exceeded set threshold (%ld), will restart miner process...",
						gMinerProcessInfo.workingSetSize, cfg->minerWorkingSetThreshold
						);
			restartReason |= WDOG_RESTART_WORKING_SET_THRESHOLD;
			restart = 1;
			goto miner_restart_check;
		}

		if (gMinerProcessInfo.handleCount > cfg->minerHandleCountThreshold)
		{
			debug_log( LOG_SVR, "monitorThread(): handle count (%ld) exceeded set threshold (%ld), will restart miner process...",
						gMinerProcessInfo.handleCount,
						cfg->minerHandleCountThreshold
						);
			restartReason |= WDOG_RESTART_HANDLE_COUNT_THRESHOLD;
			restart = 1;
			goto miner_restart_check;
		}
			
		// --------------------------
		// Checking ADL H/W readings.
		// --------------------------
		if (cfg->smartMeterOnPeak == 1)
		{
			debug_log(LOG_DBG, "monitorThread(): smart metering on-peak schedule is ON, no need to check H/W GPU stats...");
		} else
		{
			if (cfg->wdogAdlDisabled == 0 && _mi.config.gpuCount > 0)  // smart metering might have turned off GPUs
			{
				getADLStats(&_adlInfo);
				//displayADLStats(&_adlInfo);

				if (_adlInfo.iMaxTemp >= cfg->wdogCutOffTemperatureThreshold)
				{
					restart = 1; // cutoff temp reached msg
					restartReason |= WDOG_RESTART_TEMPERATURE_THRESHOLD;
					restartDelay = cfg->wdogCutOffTemperatureCooldownPeriod*60;

					debug_log( LOG_SVR, "monitorThread(): max gpu temperature (%dC) has exceeded a defined threshold (%dC), will restart miner process after: %d seconds...", 
								_adlInfo.iMaxTemp, cfg->wdogCutOffTemperatureThreshold,
								restartDelay
								);
					goto miner_restart_check;
				} else
				{
					debug_log( LOG_DBG, "monitorThread(): max gpu temperature (%dC) stays below defined threshold (%dC)", 
								_adlInfo.iMaxTemp, cfg->wdogCutOffTemperatureThreshold
								);
				}

				if (_adlInfo.iZeroFan)
				{
					restart = 1; 
					restartReason |= WDOG_RESTART_FAN_DOWN;
					restartDelay = cfg->wdogCutOffTemperatureCooldownPeriod*60;

					debug_log( LOG_SVR, "monitorThread(): at least one fan appears to be down on one of the GPU cards, will restart miner process after: %d seconds...", 
								restartDelay
								);
					goto miner_restart_check;
				} else
				{
					debug_log(LOG_DBG, "monitorThread(): all GPU fans appear to be functional");
				}

				if (gCurrStatus == ALIVE || gPrevStatus == ALIVE)
				{
					time_t howLongAgo;

					time(&howLongAgo);

					howLongAgo -= gpuUtilTimer;

					if (_adlInfo.iMinActivity <= cfg->wdogAdlGpuActivityThreshold)
					{
						if (gpuUtilTimer > 0 && howLongAgo > cfg->wdogAdlGpuActivityTimeout)
						{
							gpuUtilTimer = 0;
							restart = 1; 
							restartReason |= WDOG_RESTART_GPU_ACTIVITY_THRESHOLD;
							restartDelay = 15;

							debug_log( LOG_SVR, "monitorThread(): GPU mining activity: %d%% is lower than threshold: %d%%, timeout of %d seconds has been reached, will restart miner process after: %d seconds...", 
										_adlInfo.iMinActivity,
										cfg->wdogAdlGpuActivityThreshold,
										cfg->wdogAdlGpuActivityTimeout,
										restartDelay
										);
							goto miner_restart_check;
						} else
						{
							debug_log( LOG_SVR, "monitorThread(): GPU (lowest) mining utilization: %d%% is lower than threshold: %d%%", 
										_adlInfo.iMinActivity,
										cfg->wdogAdlGpuActivityThreshold
										);

							if (gpuUtilTimer == 0) 
							{
								debug_log( LOG_SVR, "monitorThread(): starting gpuUtilTimer timer...");
								time(&gpuUtilTimer);
							}
						}
					} else
					{
						debug_log( LOG_DBG, "monitorThread(): GPU (lowest) mining utilization: %d%% is higher than threshold: %d%%", 
									_adlInfo.iMinActivity,
									cfg->wdogAdlGpuActivityThreshold
									);
						gpuUtilTimer = 0;
					}
				}

				if (_adlInfo.iADLStatus == ADL_STATUS_EXCEPTION)
				{
					restart = 1; 
					restartReason |= WDOG_REBOOT_DRIVER_CRASH;

					debug_log( LOG_SVR, "monitorThread(): AMD driver crashed, will attempt to reboot the system." );
					restartReason |= WDOG_REBOOT_DRIVER_CRASH;
					reboot(restartReason);
				}

			} // end of if (cfg->wdogAdlDisabled == 0)

		} // end of if/else (cfg->smartMeterOnPeak == 1)

		if (restart == 0 && gCurrStatus ==  ALIVE)
		{
			if (gPrevStatus != ALIVE)
			{
				time(&aliveTimer); // entering ALIVE state from other state
				debug_log(LOG_INF, "monitorThread(): wdog has entered ALIVE state");
				if (cfg->smartMeterOnPeak == 0)
				{
					debug_log(LOG_INF, "monitorThread(): monitoring GPU temperatures, fan rpms, utilization and mining performance");
				} else
				{
					debug_log(LOG_INF, "monitorThread(): on-peak smart metering schedule is ON, monitoring of GPU stats has been suspended");
				}
			} else
			{
				time(&now);
				timePeriod = difftime(now, aliveTimer);

				if (timePeriod > cfg->wdogAliveTimeout) // 10 minutes in ALIVE state 
				{
					int isAnyGpuHashBelowThreshold = 0;

					// ---------------------------------------------------------------------------------------
					// If miner is not solo mining, check if pool has accepting any shares in last 10 minutes.
					// ---------------------------------------------------------------------------------------
					if (cfg->minerSoloMining == 0)
					{
						// ---------------------
						// Check accepted count.
						// ---------------------
						if (_mi.summary.accepted > lastAccepted)
						{
							debug_log( LOG_INF, 
									   "monitorThread(): accepted shares: %d > last accepted shares: %d, miner looks OK", 
									   _mi.summary.accepted,
									   lastAccepted								   
									 );

							lastAccepted = _mi.summary.accepted;
						} else
						{
							restart = 1;
							restartReason |= WDOG_RESTART_ACCEPTED_RATE_IS_FLAT;
							debug_log( LOG_SVR, 
									   "monitorThread(): miner reported NO change in number of accepted shares (%d) in the last 10 minutes; considered SICK ...", 
									   _mi.summary.accepted
									 );
							lastAccepted = 0;

							goto miner_restart_check;
						}
					} else
					{
						// ---------------------------------------------------------------------------------------------------
						// Miner is solo mining, check if user wants to receive email notifications when a new block is found.
						// ---------------------------------------------------------------------------------------------------
						if (cfg->minerNotifyWhenBlockFound == 1 && _mi.summary.targetDifficulty > 0)
						{
							debug_log( LOG_DBG, "monitorThread(): target difficulty: %f, best share: %f", _mi.summary.targetDifficulty, _mi.summary.bestshare*1000);
							double bestShare  = _mi.summary.bestshare*1000;
							if (bestShare > _mi.summary.targetDifficulty)
							{
								debug_log( LOG_DBG, "monitorThread(): best share: %f > target difficulty: %f", _mi.summary.bestshare*1000, _mi.summary.targetDifficulty);
								if (lastBestShare != bestShare)
								{
									send_smtp_block_found_msg();
									lastBestShare = bestShare;
								}
							}
						}
					}

					// ---------------------
					// Check for H/W errors.
					// ---------------------
					if (_mi.summary.hw > cfg->minerHWErrorsThreshold)
					{
						restart = 1;
						restartReason |= WDOG_RESTART_MINER_HW_ERRORS_THRESHOLD;
						debug_log( LOG_SVR, 
						           "monitorThread(): miner reported total of %d hardware errors, hw error threshold is: %d, will restart miner process...", 
								   _mi.summary.hw,
								   cfg->minerHWErrorsThreshold
							     );
						goto miner_restart_check;
					}
					
					if (cfg->smartMeterOnPeak == 0)
					{
						// wdog has been in ALIVE state for longer than 15 minutes, checking hash rates.
						for (i=0; i < _mi.config.gpuCount; i++)
						{
							if (waitForShutdown(1)) break;

							if (_mi.gpu[i].disabled == 0 && _mi.gpu[i].avg < cfg->minerGPUAvgRateThreshold)
							{
								debug_log( LOG_SVR, "monitorThread(): hash rate for (%0.2f) gpu: %d fell below set threshold (%0.2f), will restart miner process...", 
											_mi.gpu[i].avg, i, cfg->minerGPUAvgRateThreshold);
								restartReason |= WDOG_RESTART_GPU_HASH_THRESHOLD;
								faultyDevice = i;
								isAnyGpuHashBelowThreshold = 1;
								break;
							}
						}
					} else
					{
						debug_log(LOG_DBG, "monitorThread(): smart metering on-peak schedule is ON, no need to check GPU hash rates...");
					}

					for (i=0; i < _mi.config.pgaCount; i++)
					{
						if (waitForShutdown(1)) break;

						if (_mi.pga[i].avg < cfg->minerPGAAvgRateThreshold)
						{
							debug_log( LOG_SVR, "monitorThread(): hash rate (%0.2f) for pga: %d fell below set threshold (%0.2f), will restart miner process...",
										_mi.pga[i].avg, i, cfg->minerPGAAvgRateThreshold);
							restartReason |= WDOG_RESTART_PGA_HASH_THRESHOLD;
							faultyDevice = i;
							isAnyGpuHashBelowThreshold = 1;
							break;
						}

						if (_mi.pga[i].disabled)
						{
							debug_log(LOG_SVR, "monitorThread(): pga: %d has been disabled, will restart miner process...",	i);
							restartReason |= WDOG_RESTART_PGA_HAS_BEEN_DISABLED;
							restart = 1;
							goto miner_restart_check;
						}
					}

					if (waitForShutdown(1)) break;


					if (isAnyGpuHashBelowThreshold == 1)
					{
						restart = 1;
						restartHashCount++;

						debug_log( LOG_SVR, "monitorThread(): restarting miner, restartHashCount: %d", restartHashCount );
						if (restartHashCount > 2)
						{
							restartReason |= WDOG_REBOOT_MAX_RESTARTS;
							reboot(restartReason);
						}
					}

					displayMinerInfoObject(&_mi);

					resetHashRestartCount = 1;
					for (i=0; i < _mi.config.gpuCount; i++)
					{
						if (_mi.gpu[i].avg < cfg->minerGPUAvgRateThreshold)
							resetHashRestartCount = 0;							
					}

					for (i=0; i < _mi.config.pgaCount; i++)
					{
						if (_mi.pga[i].avg < cfg->minerPGAAvgRateThreshold)
							resetHashRestartCount = 0;							
					}

					if (resetHashRestartCount)
						restartHashCount = 0;

					time(&aliveTimer); // reset alive timer...
				}
			}
		} // end of if (restart == 0 && gCurrStatus ==  ALIVE)
		
miner_restart_check:

		gPrevStatus = gCurrStatus;

		if (restart == 1) 
		{
			if (waitForShutdown(1)) break;
			
			restartMiner(restartDelay, restartReason, faultyDevice);

			restartDelay = DEFAULT_RESTART_DELAY;
		}

		if (waitForShutdown(1)) break;

		wait(waitLeft);
	}

	debug_log(LOG_SVR, "monitorThread(): exiting thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of monitorThread()