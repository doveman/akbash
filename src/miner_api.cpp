// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <winsock2.h>
#include <stdio.h>

#include "miner_api.h"
#include "log.h"
#include "listen.h"
#include "wdmain.h"
#include "network.h"

char _minerHost[32];
short int _minerPort = 4028;

Miner_Info gMinerInfo;

void resetMinerInfoObject(Miner_Info * mi);

void initMinerListeningParameters(char * hostIP, int port)
{
	strcpy_s(_minerHost, sizeof(_minerHost), hostIP);
	_minerPort = port;
}

void resetMinerInfo()
{
	if (getGpuMutex())
	{
		__try
		{
			resetMinerInfoObject(&gMinerInfo);
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		releaseGpuMutex();
	}
}

void updateMinerStats(Miner_Info * mi)
{
	if (mi != NULL)
	{
		if (getGpuMutex())
		{
			__try
			{
				memcpy(&gMinerInfo, mi, sizeof(Miner_Info));
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}
			
			releaseGpuMutex();
		}
	}
}

void getMinerStats(Miner_Info * mi)
{
	if (mi != NULL)
	{
		if (getGpuMutex())
		{
			__try
			{
				memcpy(mi, &gMinerInfo, sizeof(Miner_Info));
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}
				
			releaseGpuMutex();
		}
	}
}

void resetMinerInfoObject(Miner_Info * mi)
{
	int i = 0;

	memset(mi, 0, sizeof(Miner_Info));
	mi->status = NOT_CONNECTED;


	for (i=0; i < MAX_GPU_DEVICES; i++)	mi->gpu[i].status = NOT_CONNECTED;

	for (i=0; i < MAX_PGA_DEVICES; i++)	mi->pga[i].status = NOT_CONNECTED;

	sprintf(mi->summary.startedOn, "%02d/%02d/%04d %02d:%02d:%02d", 0, 0, 0, 0, 0,0);
} // end of resetMinerInfoObject()

void displayMinerInfoObject(Miner_Info * mi)
{
	int i = 0;

	debug_log( LOG_DBG, "displayMinerInfoObject(): status: %s, gpus: %d, pgas: %d, a: %d, gw: %d, avg: %0.2f, hw: %d, since: %s, u: %0.2f, ver: %s, days: %d, hrs: %d, min: %d, secs: %d",
		       gpuStatusStr(mi->status),
			   mi->config.gpuCount,
			   mi->config.pgaCount,
			   mi->summary.accepted,
			   mi->summary.getworks,
			   mi->summary.mhsAvg,
			   mi->summary.hw,
			   mi->summary.startedOn,
			   mi->summary.util,
			   mi->summary.version,
			   mi->summary.days,
			   mi->summary.hrs,
			   mi->summary.min,
			   mi->summary.secs
		     );

	for (i=0; i < mi->config.gpuCount; i++)	
		debug_log( LOG_DBG, "displayMinerInfoObject(): gpu %d, disabled: %s, status: %s, avg: %0.2f, %d/%d, %0.2fC@%d%% hw: %d, v: %0.2f, u: %0.2f",
		           mi->gpu[i].id,
				   mi->gpu[i].disabled ? "Y" : "N",
		           gpuStatusStr(mi->gpu[i].status),
				   mi->gpu[i].avg,
				   mi->gpu[i].engine,
				   mi->gpu[i].mem,
				   mi->gpu[i].temp,
				   mi->gpu[i].fan,
				   mi->gpu[i].hw,
				   mi->gpu[i].volt,
				   mi->gpu[i].util
		         );

	for (i=0; i < mi->config.pgaCount; i++)	
		debug_log( LOG_DBG, "displayMinerInfoObject(): pga %d, disabled: %s, status: %s, avg: %0.2f, %0.2fC hw: %d, u: %0.2f",
		           mi->pga[i].id,
				   mi->pga[i].disabled ? "Y" : "N",
		           gpuStatusStr(mi->pga[i].status),
				   mi->pga[i].avg,
				   mi->pga[i].temp,
				   mi->pga[i].hw,
				   mi->pga[i].util
		         );

	for (i=0; i < mi->config.poolCount; i++)	
		debug_log( LOG_DBG, "displayMinerInfoObject(): pool %d, url: %s status: %s",
		           mi->pools[i].id,
		           mi->pools[i].url,
		           mi->pools[i].status
		         );

} // end of displayMinerInfoObject()

char * gpuStatusStr(enum GPU_STATUS status)
{
	if (status == NOT_CONNECTED)
		return "NOT CONNECTED";

	if (status == ALIVE)
		return"ALIVE";

	if (status == NOT_RUNNING)
		return "NOT RUNNNING";

	if (status == RUNNING)
		return "RUNNING";

	if (status == DEAD)
		return "DEAD";

	if (status == SICK)
		return "SICK";

	if (status == NOT_STARTED)
		return "NOT STARTED";

	if (status == DISABLED)
		return "DISABLED";

	if (status == CRASHED)
		return "CRASHED";

	if (status == KILLED)
		return "KILLED";

	if (status == LOW_HASH)
		return "LOW_HASH";

	if (status == REJECTING)
		return "REJECTING";

	if (status == UNKNOWN)
		return "UNKNOWN";

	if (status == NOSTART)
		return "NOSTART";

	return "NOT CONNECTED";
}

int sendCommand(char * cmd, char * response, int responseSize)
{
	SOCKET sock = INVALID_SOCKET;
	int ret = 0;
	int rc = 0;
	int outLen = 0;

	rc = net_connect(_minerHost, _minerPort, &sock, 1);  // 1 - localhost
	if (rc != SOCK_NO_ERROR)
	{
		debug_log(LOG_ERR, "sendCommand(): failed to connect to: %s@%d, rc: %d", _minerHost, _minerPort, rc);
		return 0;
	}

	rc = net_sendBytes(sock, cmd, (int) strlen(cmd), &outLen, SOCK_TIMEOUT_INTERVAL);
	if ( rc != SOCK_NO_ERROR) 
	{
		debug_log(LOG_ERR, "sendCommand(): net_sendBytes() failed: %d", rc);
		closesocket(sock);
		return 0;
	}
	else 
	{
		rc = net_recvBytes(sock, response, responseSize, &outLen, SOCK_TIMEOUT_INTERVAL);
		if ( rc != SOCK_NO_ERROR) 
		{
			debug_log(LOG_ERR, "sendCommand(): net_recvBytes() failed: %d", rc);
			closesocket(sock);
			return 0;
		}
	}

	closesocket(sock);
	return 1; // ok
} // end of sendCommand()

void parseGPUStats(char * buf, GPU_Stats * stats)
{
	char temp[256] = {0};
	char * ptr = NULL;
	char * ptrEnd = NULL;

	memset(stats, 0, sizeof(GPU_Stats));
	stats->status = NOT_CONNECTED;

    //"{\"GPU\":%d,\"Enabled\":\"%s\",\"Status\":\"%s\",\"Temperature\":%.2f,\"Fan Speed\":%d,\"Fan Percent\":%d,\"GPU Clock\":%d,\"Memory Clock\":%d,\"GPU Voltage\":%.3f,\"GPU Activity\":%d,\"Powertune\":%d,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",

	ptr = strstr(buf, "Enabled");

	if (ptr != NULL && strlen(ptr) > 20)
	{
		ptr +=10;
		ptrEnd = strstr(ptr, "\",");

		if (ptrEnd != NULL)
		{
			memset(temp, 0, sizeof(temp));
			strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

			if (_stricmp(temp, "N") == 0)
				stats->disabled = 1;
		}

		ptr = strstr(buf, "Status");

		if (ptr != NULL)
		{
			ptr +=9;
			ptrEnd = strstr(ptr, "\",");			

			if (ptrEnd != NULL)
			{
				memset(temp, 0, sizeof(temp));
				strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);


	//static const char *NOSTART = "NoStart";
	//static const char *DISABLED = "Disabled";
	//static const char *ALIVE = "Alive";
	//static const char *REJECTING = "Rejecting";
	//static const char *UNKNOWN = "Unknown";

				if (_stricmp(temp, "Alive") == 0)
					stats->status = ALIVE;
				else
					if (_stricmp(temp, "Dead") == 0)	
						stats->status = DEAD;
					else
						if (_stricmp(temp, "NoStart") == 0)
							stats->status = NOT_STARTED;
						else
							if (_stricmp(temp, "Disabled") == 0)
								stats->status = DISABLED;
							else
								if (_stricmp(temp, "Sick") == 0)
									stats->status = SICK;				

				ptr = strstr(buf, "Temperature");
				if (ptr != NULL)
				{
		//"{\"GPU\":%d,\"Enabled\":\"%s\",\"Status\":\"%s\",\"Temperature\":%.2f,\"Fan Speed\":%d,\"Fan Percent\":%d,\"GPU Clock\":%d,\"Memory Clock\":%d,\"GPU Voltage\":%.3f,\"GPU Activity\":%d,\"Powertune\":%d,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
					ptr += 13;

					ptrEnd = strstr(ptr, ",");
					if (ptrEnd != NULL)
					{
						memset(temp, 0, sizeof(temp));
						strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

						stats->temp = atof(temp);

						ptr = strstr(buf, "Fan Percent");
						if (ptr != NULL)
						{
							ptr += 13;

							ptrEnd = strstr(ptr, ",");
							if (ptrEnd != NULL)
							{
								memset(temp, 0, sizeof(temp));
								strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

								stats->fan = atoi(temp);

								ptr = strstr(buf, "GPU Clock");
								if (ptr != NULL)
								{
									ptr += 11;
									//"GPU Clock\":%d,\"Memory Clock\":%d,\"GPU Voltage\":%.3f,\"GPU Activity\":%d,\"Powertune\":%d,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
									ptrEnd = strstr(ptr, ",");
									if (ptrEnd != NULL)
									{
										memset(temp, 0, sizeof(temp));
										strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
										stats->engine = atoi(temp);
										ptr = strstr(buf, "Memory Clock");
										if (ptr != NULL)
										{
											ptr += 14;
											//Memory Clock\":%d,\"GPU Voltage\":%.3f,\"GPU Activity\":%d,\"Powertune\":%d,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
											ptrEnd = strstr(ptr, ",");
											if (ptrEnd != NULL)
											{
												memset(temp, 0, sizeof(temp));
												strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
												stats->mem = atoi(temp);

												ptr = strstr(buf, "GPU Voltage");
												if (ptr != NULL)
												{
													ptr += 13;
													//GPU Voltage\":%.3f,\"GPU Activity\":%d,\"Powertune\":%d,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
													ptrEnd = strstr(ptr, ",");
													if (ptrEnd != NULL)
													{
														memset(temp, 0, sizeof(temp));
														strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
														stats->volt = atof(temp);

														ptr = strstr(buf, "MHS av");
														if (ptr != NULL)
														{
															ptr += 8;
															//MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
															ptrEnd = strstr(ptr, ",");
															if (ptrEnd != NULL)
															{
																memset(temp, 0, sizeof(temp));
																strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																stats->avg = atof(temp);

																ptr = strstr(buf, "Hardware Errors");
																if (ptr != NULL)
																{
																	ptr += 17;
																	//"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
																	ptrEnd = strstr(ptr, ",");
																	if (ptrEnd != NULL)
																	{
																		memset(temp, 0, sizeof(temp));
																		strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																		stats->hw = atoi(temp);

																		ptr = strstr(buf, "Utility");
																		if (ptr != NULL)
																		{
																			ptr += 9;
																			//"Hardware Errors\":%d,\"Utility\":%.2f,\"Intensity\":\"%s\",\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
																			ptrEnd = strstr(ptr, ",");
																			if (ptrEnd != NULL)
																			{
																				memset(temp, 0, sizeof(temp));
																				strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																				stats->util = atof(temp);

																				ptr = strstr(buf, "Accepted");
																				if (ptr != NULL)
																				{
																					ptr += 10;
																					// Accepted\":%d,\"Rejected
																					ptrEnd = strstr(ptr, ",");
																					if (ptrEnd != NULL)
																					{
																						memset(temp, 0, sizeof(temp));
																						strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																						stats->accepted = atol(temp);
																					}
																				}

																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
} // end of parseGPUStats()

void fetchGPUStats(int gpu, GPU_Stats * stats)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"gpu\" , \"parameter\" : \"%d\" }", gpu);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		parseGPUStats(buf, stats);
		stats->id = gpu;
	}
} // end of fetchGPUStats()

void parsePGAStats(char * buf, PGA_Stats * stats)
{
	char temp[256] = {0};

	char * ptr = NULL;
	char * ptrEnd = NULL;

	memset(stats, 0, sizeof(PGA_Stats));
	stats->status = NOT_CONNECTED;		

    //debug_log( LOG_DBG, "parsePGAStats(): buf: %s", buf);

	ptr = strstr(buf, "Enabled");

	if (ptr != NULL && strlen(ptr) > 20)
	{
		ptr +=9;
		ptrEnd = strstr(ptr, "\",");			
		if (ptrEnd != NULL)
		{
			memset(temp, 0, sizeof(temp));
			strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

			if (_stricmp(temp, "N") == 0)
				stats->disabled = 1;
		}

		ptr = strstr(buf, "Status");

		if (ptr != NULL)
		{
			ptr +=9;
			ptrEnd = strstr(ptr, "\",");			

			if (ptrEnd != NULL)
			{
				memset(temp, 0, sizeof(temp));
				strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

				if (_stricmp(temp, "alive") == 0)
					stats->status = ALIVE;
				else
					if (_stricmp(temp, "dead") == 0)	
						stats->status = DEAD;
					else
						if (_stricmp(temp, "NoStart") == 0)
							stats->status = NOT_STARTED;
						else
							if (_stricmp(temp, "Disabled") == 0)
								stats->status = DISABLED;

				ptr = strstr(buf, "Temperature");
				if (ptr != NULL)
				{
					//{\"PGA\":%d,\"Name\":\"%s\",\"ID\":%d,\"Enabled\":\"%s\",\"Status\":\"%s\",\"Temperature\":%.2f,\"MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
					ptr += 13;

					ptrEnd = strstr(ptr, ",");
					if (ptrEnd != NULL)
					{
						memset(temp, 0, sizeof(temp));
						strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

						stats->temp = atof(temp);
					}
				}
						ptr = strstr(buf, "MHS av");
						if (ptr != NULL)
						{
							ptr += 8;
									//MHS av\":%.2f,\"MHS %ds\":%.2f,\"Accepted\":%d,\"Rejected\":%d,\"Hardware Errors\":%d,\"Utility\":%.2f,\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
							ptrEnd = strstr(ptr, ",");
							if (ptrEnd != NULL)
							{
								memset(temp, 0, sizeof(temp));
								strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
								stats->avg = atof(temp);

								ptr = strstr(buf, "Hardware Errors");
								if (ptr != NULL)
								{
									ptr += 17;
									//Hardware Errors\":%d,\"Utility\":%.2f,\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
									ptrEnd = strstr(ptr, ",");
									if (ptrEnd != NULL)
									{
										memset(temp, 0, sizeof(temp));
										strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
										stats->hw = atoi(temp);

										ptr = strstr(buf, "Utility");
										if (ptr != NULL)
										{
											ptr += 9;
											//Hardware Errors\":%d,\"Utility\":%.2f,\"Last Share Pool\":%d,\"Last Share Time\":%lu,\"Total MH\":%.4f}",
											ptrEnd = strstr(ptr, ",");
											if (ptrEnd != NULL)
											{
												memset(temp, 0, sizeof(temp));
												strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
												stats->util = atof(temp);

												ptr = strstr(buf, "Accepted");
												if (ptr != NULL)
												{
													ptr += 10;
													// Accepted\":%d,\"Rejected
													ptrEnd = strstr(ptr, ",");
													if (ptrEnd != NULL)
													{
														memset(temp, 0, sizeof(temp));
														strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
														stats->accepted = atol(temp);
													}
												}

											}
										}
									}
								}
							}
						}
					
				
			}
		}
	}
}

void parsePoolStats(char * buf, Miner_Info * mi)
{
	char temp[256] = {0};

	char * ptr = NULL;
	char * ptrEnd = NULL;
	int i = 0;
	int len = 0;
	int j = 0;
	char * start = buf;

	for (i=0; i < mi->config.poolCount; i++)
	{
		ptr = strstr(start, "\"POOL\":");

		if (ptr != NULL)
		{
			ptr +=7;
			start = ptr;
			ptrEnd = strstr(ptr, ",");			

			if (ptrEnd != NULL)
			{
				memset(temp, 0, sizeof(temp));
				strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
				mi->pools[i].id = atoi(temp);

				ptr = strstr(start, "\"URL\":");

				if (ptr != NULL)
				{
					ptr +=7;
					ptrEnd = strstr(ptr, "\",");			

					if (ptrEnd != NULL)
					{
						strncpy_s(mi->pools[i].url, sizeof(mi->pools[i].url), ptr, ptrEnd-ptr);

						ptr = strstr(start, "\"Status\":");
						if (ptr != NULL)
						{				
							ptr += 10;							

							ptrEnd = strstr(ptr, "\",");
							if (ptrEnd != NULL)
							{
								start = ptrEnd;

                                memset(temp, 0, sizeof(temp));
                                strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);

								//strncpy_s(mi->pools[i].status, sizeof(mi->pools[i].status), ptr, ptrEnd-ptr);

								len = strlen(temp);
								for (j=0; j < len; j++)
								{
									mi->pools[i].status[j] = toupper(temp[j]);
								}
								mi->pools[i].status[len] = 0;

								ptr = strstr(start, "\"Priority\":");
								if (ptr != NULL)
								{				
									ptr += 11;							

									ptrEnd = strstr(ptr, ",");
									if (ptrEnd != NULL)
									{
										start = ptrEnd;
										memset(temp, 0, sizeof(temp));
										strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
										mi->pools[i].priority = atoi(temp);										
									}
								}
							}
						}
					}
				}
			}
		}

		//debug_log(LOG_SVR, "parsePoolStats(): pool: %d, url: %s, status: %s", mi->pools[i].id, mi->pools[i].url, mi->pools[i].status);

	}
}

int disableGPU(int gpu)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"gpudisable\" , \"parameter\" : \"%d\" }", gpu);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		debug_log(LOG_SVR, "disableGPU(): disable gpu: %d command has been sent successfully", gpu);
		return 1;
	}

	return 0;
} // end of disableGPU()

int enableGPU(int gpu)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"gpuenable\" , \"parameter\" : \"%d\" }", gpu);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		debug_log(LOG_SVR, "enableGPU(): enable gpu: %d command has been sent successfully", gpu);
		return 1;
	}

	return 0;
} // end of enableGPU()

int disableGPUs(void)
{
	Miner_Info _mi;
	int i = 0;

	memset(&_mi, 0, sizeof(_mi));

	getMinerStats(&_mi);

	for (i=0; i < _mi.config.gpuCount; i++)
	{
		debug_log(LOG_SVR, "disableGPUs(): disabling GPU: %d",i);
		if (disableGPU(i) == 0)
			return 0;
	}	

	return 1;
} // end of disableGPUs()

int enableGPUs(void)
{
	Miner_Info _mi;
	int i = 0;

	memset(&_mi, 0, sizeof(_mi));

	getMinerStats(&_mi);

	for (i=0; i < _mi.config.gpuCount; i++)
	{
		debug_log(LOG_SVR, "enableGPUs(): enabling GPU: %d",i);
		if (enableGPU(i) == 0)
			return 0;
	}

	return 1;
} // end of enableGPUs()


void switchPool(int pool)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"switchpool\" , \"parameter\" : \"%d\" }", pool);

	debug_log(LOG_DBG, "switchPool(): pool: %d, command: %s", pool, command);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
//		debug_log(LOG_SVR, "switchPool(): buf: %s", buf);
	}
} // end of switchPool()

void fetchPoolStats(Miner_Info * mi)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"pools\" , \"parameter\" : \"%d\" }", 0);

	//debug_log(LOG_SVR, "fetchPoolStats(): pool: %d, command: %s", 0, command);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		//debug_log(LOG_SVR, "fetchPoolStats(): buf: %s", buf);
		parsePoolStats(buf, mi);
	}
} // end of fetchPoolStats()

void fetchPGAStats(int gpu, PGA_Stats * stats)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	sprintf_s(command, sizeof(command), "{ \"command\" : \"pga\" , \"parameter\" : \"%d\" }", gpu);

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		parsePGAStats(buf, stats);
		stats->id = gpu;
	}
} // end of fetchPGAStats()

void parseMinerConfig(char * buf, Miner_Config * minerCfg)
{
	char temp[256] = {0};
	char * ptr = NULL;
	char * ptrEnd = NULL;

	memset(minerCfg, 0, sizeof(Miner_Config));

	ptr = strstr(buf,"\"GPU Count\":");
	if (ptr != NULL)
	{
		ptr += 12;

		ptrEnd = strstr(ptr, ",");

		if (ptrEnd != NULL)
		{
			memset(temp, 0, sizeof(temp));
			strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
			minerCfg->gpuCount = atoi(temp);

			if (minerCfg->gpuCount > MAX_GPU_DEVICES)
			{
				debug_log( LOG_SVR, 
							"parseMinerConfig(): number of gpus defined in cgminer (%d) is greater than %d gpus supported by akbash, only the first %d gpus will be monitored.",
							minerCfg->gpuCount,
							MAX_GPU_DEVICES,
							MAX_GPU_DEVICES
							);
				minerCfg->gpuCount = MAX_GPU_DEVICES;
			}

			ptr = strstr(buf,"\"PGA Count\":");
			if (ptr != NULL)
			{
				ptr += 12;

				ptrEnd = strstr(ptr, ",");

				if (ptrEnd != NULL)
				{
					memset(temp, 0, sizeof(temp));
					strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
					minerCfg->pgaCount = atoi(temp);

					if (minerCfg->pgaCount > MAX_PGA_DEVICES)
					{
						debug_log( LOG_SVR, 
									"parseMinerConfig(): number of PGAs defined in cgminer (%d) is greater than %d PGAs supported by akbash, only the first %d PGAs will be monitored.",
									minerCfg->pgaCount,
									MAX_PGA_DEVICES,
									MAX_PGA_DEVICES
									);
						minerCfg->pgaCount = MAX_GPU_DEVICES;
					}


					ptr = strstr(buf,"\"Pool Count\":");
					if (ptr != NULL)
					{
						ptr += 13;

						ptrEnd = strstr(ptr, ",");

						if (ptrEnd != NULL)
						{
							memset(temp, 0, sizeof(temp));
							strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
							minerCfg->poolCount = atoi(temp);							
							if (minerCfg->poolCount > MAX_POOLS)
							{
								debug_log( LOG_SVR, 
									       "parseMinerConfig(): number of pools defined in cgminer (%d) is greater than %d pools supported by akbash, only the first %d pools will be used.",
									       minerCfg->poolCount,
										   MAX_POOLS,
										   MAX_POOLS
										 );
								minerCfg->poolCount = MAX_POOLS;
							}
						}
					}
				}
			}
		}
	}

}

void fetchMinerConfig(Miner_Config * minerCfg)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	char * ptr = NULL;
	char * ptrEnd = NULL;

	strcpy_s(command, sizeof(command), "{ \"command\" : \"config\" }");

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		parseMinerConfig(buf, minerCfg);
	}	
} // end of fetchMinerConfig()


void parseGPUSummary(char * buf, GPU_Summary * sum)
{
	char temp[256] = {0};
	char * ptr = NULL;
	char * ptrEnd = NULL;
	long value = 0;
	time_t now;
	struct tm *nowTm;

	memset(sum, 0, sizeof(GPU_Summary));

//{"STATUS":[{"STATUS":"S","Code":11,"Msg":"Summary","Description":"cgminer 2.3.1"}],
//"SUMMARY":[{"Elapsed":326,"MHS av":1992.95,"Found Blocks":0,"Getworks":170,"Accepted":162,"Rejected":0,"Hardware Errors":0,"Utility":29.79,"Discarded":3,"Stale":0,"Get Failures":0,"Local Work":0,"Remote Failures":0,"Network Blocks":1,"Total MH
//":650217.7833}]

    debug_log( LOG_DBG, "parseGPUSummary(): buf: %s", buf);

	ptr = strstr(buf, "miner");

	if (ptr != NULL)
	{
		ptr += 6;

		ptrEnd = strstr(ptr, "\"}");
		if (ptrEnd != NULL)
		{
			memset(temp, 0, sizeof(temp));
			strncpy_s(sum->version, sizeof(sum->version), ptr, ptrEnd-ptr);
				
			ptr = strstr(buf,"Elapsed");
			if (ptr != NULL)
			{
				ptr += 9;

				ptrEnd = strstr(ptr, ",");
				if (ptrEnd != NULL)
				{
					memset(temp, 0, sizeof(temp));
					strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
					value = atol(temp);

					sum->days = value/86400;
					sum->hrs = (value -sum->days*86400)/3600;
					sum->min = (value -sum->days*86400 - sum->hrs*3600)/60;
					sum->secs = value -sum->days*86400 - sum->hrs*3600 - sum->min*60;

					time(&now);
					now -= value;
					nowTm = localtime(&now);

					sprintf( sum->startedOn, 
								"%02d/%02d/%04d %02d:%02d:%02d",
								nowTm->tm_mon+1,
								nowTm->tm_mday,
								nowTm->tm_year+1900,
								nowTm->tm_hour,
								nowTm->tm_min,
								nowTm->tm_sec 
							);

//{"STATUS":[{"STATUS":"S","Code":11,"Msg":"Summary","Description":"cgminer 2.3.1"}],
//"SUMMARY":[{"Elapsed":326,"MHS av":1992.95,"Found Blocks":0,"Getworks":170,"Accepted":162,"Rejected":0,"Hardware Errors":0,"Utility":29.79,"Discarded":3,"Stale":0,"Get Failures":0,"Local Work":0,"Remote Failures":0,"Network Blocks":1,"Total MH
//":650217.7833}]

                    ptr = strstr(buf, "Description");

					if (ptr != NULL)
					{
						ptr += 14;

						ptrEnd = strstr(ptr, "\"");
						if (ptrEnd != NULL)
						{
							memset(sum->description, 0, sizeof(sum->description));
							strncpy_s(sum->description, sizeof(sum->description), ptr, ptrEnd-ptr);							
						}
					}

					ptr = strstr(buf, "MHS av");
					if (ptr != NULL)
					{
						ptr += 8;

						ptrEnd = strstr(ptr, ",");
						if (ptrEnd != NULL)
						{
							memset(temp, 0, sizeof(temp));
							strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
							sum->mhsAvg = atof(temp);

							ptr = strstr(buf, "Hardware");
								
							if (ptr != NULL)
							{
								ptr += 17;
									
								ptrEnd = strstr(ptr, ",");
									
								if (ptrEnd != NULL)
								{
									memset(temp, 0, sizeof(temp));
									strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
									sum->hw = atol(temp);

									ptr = strstr(buf, "Utility");

									if (ptr != NULL)
									{
										ptr += 9;

										ptrEnd = strstr(ptr, ",");
										if (ptrEnd != NULL)
										{
											memset(temp, 0, sizeof(temp));
											strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
											sum->util = atof(temp);

											ptr = strstr(buf, "Accepted");

											if (ptr != NULL)
											{
												ptr += 10;

												ptrEnd = strstr(ptr, ",");
												if (ptrEnd != NULL)
												{
													memset(temp, 0, sizeof(temp));
													strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
													sum->accepted = atol(temp);

													ptr = strstr(buf, "Getworks");

													if (ptr != NULL)
													{
														ptr += 10;

														ptrEnd = strstr(ptr, ",");
														if (ptrEnd != NULL)
														{
															memset(temp, 0, sizeof(temp));
															strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
															sum->getworks = atol(temp);

															ptr = strstr(buf, "Rejected");

															if (ptr != NULL)
															{
																ptr += 10;

																ptrEnd = strstr(ptr, ",");
																if (ptrEnd != NULL)
																{
																	memset(temp, 0, sizeof(temp));
																	strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																	sum->rejected = atol(temp);
																}

															}

															ptr = strstr(buf, "Best Share");

															if (ptr != NULL)
															{
																ptr += 12;

																ptrEnd = strstr(ptr, ",");
																if (ptrEnd != NULL)
																{
																	memset(temp, 0, sizeof(temp));
																	strncpy_s(temp, sizeof(temp), ptr, ptrEnd-ptr);
																	sum->bestshare = atof(temp)/1000; // in K
																}
															}
														}
													}
												}
											}												
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
} // end of parseGPUSummary()

void fetchGPUSummary(GPU_Summary * sum)
{
	char command[256] = {0};
	char buf[RECVSIZE+1] = {0};

	strcpy_s(command, sizeof(command), "{ \"command\" : \"summary\" }");

	if (sendCommand(command, buf, sizeof(buf)) == 1)
	{
		parseGPUSummary(buf, sum);
	}

} // end of fetchGPUSummary()

void fetchMinerInfo(Miner_Info * mi)
{
	int i = 0;
	double maxTemp = 0;
	char * url = "https://blockchain.info/q/getdifficulty";
	char diffStr[128];
	double difficulty = 0;

	resetMinerInfoObject(mi);	

	// ------------------------
	// Get miner configuration.
	// ------------------------
	fetchMinerConfig(&(mi->config));

	// ------------------
	// Get miner summary.
	// ------------------
	fetchGPUSummary(&(mi->summary));

	if (mi->config.gpuCount > 0 || mi->config.pgaCount > 0)
		mi->status = ALIVE;


	// --------------
	// Get GPU Stats.
	// --------------
	for(i=0; i < mi->config.gpuCount; i++)
	{
		// -------------
		// Get GPU stat.
		// -------------
		fetchGPUStats(i, &(mi->gpu[i]));
		if (mi->gpu[i].temp > maxTemp)
			maxTemp = mi->gpu[i].temp;

		if (mi->gpu[i].disabled)
		{
			mi->summary.mhsAvg -= mi->gpu[i].avg;
			mi->summary.util -= mi->gpu[i].util;
			mi->gpu[i].avg = 0.0;
			mi->gpu[i].util = 0.0;
		}

		if (mi->gpu[i].status != ALIVE)
		{
			mi->status = mi->gpu[i].status;
			debug_log( LOG_INF, "fetchMinerInfo(): gpu %d: status: %s, setting miner status to: %s", 
				        i,
					    gpuStatusStr(mi->gpu[i].status), 
						gpuStatusStr(mi->status)       
					 );
		}
	}

	// --------------
	// Get PGA Stats.
	// --------------
	for(i=0; i < mi->config.pgaCount; i++)
	{
		// -------------
		// Get PGA stat.
		// -------------
		fetchPGAStats(i, &(mi->pga[i]));

		if (mi->pga[i].temp > maxTemp)
			maxTemp = mi->pga[i].temp;

		if (mi->pga[i].status != ALIVE)
		{
			mi->status = mi->gpu[i].status;
			debug_log( LOG_INF, "fetchMinerInfo(): pga %d: status: %s, setting miner status to: %s", 
				       i,
					   gpuStatusStr(mi->pga[i].status), 
					   gpuStatusStr(mi->status)       
					 );
		}
	}

	mi->summary.maxTemp = maxTemp;

	// --------------
	// Get POOL Stats.
	// --------------
	fetchPoolStats(mi);


	// get target difficulty
	memset(diffStr, 0, sizeof(diffStr));

	net_get_url(url, diffStr, sizeof(diffStr));
			
	difficulty = atof(diffStr);

	debug_log(	LOG_DBG, "fetchMinerInfo(): target difficulty: %f (%0.2fM)", difficulty, difficulty/1000000); 
					
	if (difficulty > 0.0)
	{
		mi->summary.targetDifficulty = difficulty;
	}

	// --------------------------------
	// Update global miner info object.
	// --------------------------------
	updateMinerStats(mi);

} // end of fetchMinerInfo()

DWORD WINAPI minerApiThread( LPVOID lpParam )
{
	CGMConfig * _cfg = (CGMConfig *) lpParam;

	Miner_Info _minerInfo;

	DWORD waitInterval = _cfg->minerPollInterval*1000; // miliseconds	

	debug_log(LOG_SVR, "minerApiThread(): will poll miner (%s@%d) status every %d seconds", _minerHost, _minerPort, waitInterval/1000);

	resetMinerInfoObject(&_minerInfo);

	wait(waitInterval);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		debug_log(LOG_DBG, "minerApiThread(): woke up; checking miner status...");
		fetchMinerInfo(&_minerInfo);

		if (_minerInfo.status != ALIVE)
		{
			displayMinerInfoObject(&_minerInfo);
		} else
			debug_log(LOG_DBG, "minerApiThread(): miner status: ALIVE");

		wait(waitInterval);
	}  

	debug_log(LOG_SVR, "minerApiThread(): exiting thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of minerApiThread()