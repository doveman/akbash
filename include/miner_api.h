// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_GPU_H
#define CGM_GPU_H

#define MAX_GPU_DEVICES 16
#define MAX_PGA_DEVICES 128
#define MAX_POOLS       10

#define GPU_STATS_STR_LEN 256
//#define GPU_STATS_FORMAT "gpu%d: %s, %.1f Mh/s, %.0fC@%d%%, %d/%d, hw: %d, u: %.2f/m"

#define PGA_STATS_STR_LEN 256
//#define PGA_STATS_FORMAT "pga%d: %s, %.1f Mh/s, %.0fC, hw: %d, u: %.2f/m"

typedef struct
{
	int gpuCount;
	int pgaCount;
	int poolCount;
} Miner_Config;

typedef enum GPU_STATUS
{
	NOT_RUNNING   = 0,
	RUNNING       = 1,
	NOT_CONNECTED = 2,
	ALIVE         = 3,
	DEAD          = 4,
	SICK          = 5,
	NOT_STARTED   = 6,
	DISABLED      = 7,
	CRASHED       = 8,
	KILLED        = 9,
	LOW_HASH      = 10,
	REJECTING     = 11,
	UNKNOWN       = 12,
	NOSTART       = 13
} GPU_STATUS;

typedef struct _gpu_Summary
{
	char   version[32];
	char   startedOn[20];
	int    days;
	int    hrs;
	int    min;
	int    secs;
	double mhsAvg;
	long   accepted;
	long   rejected;
	double bestshare;
	long   getworks;
	long   hw;
	double util;
} GPU_Summary;

typedef struct _gpu_Stats
{	
	int        id;
	GPU_STATUS status;
	int        disabled;
	double     avg;
	double     temp;
	int        fan;
	int        engine;
	int        mem;
	double     volt;
	long       accepted;
	long       hw;
	double     util;
} GPU_Stats;

typedef struct _pga_Stats
{	
	int        id;
	int        pgaId;
	GPU_STATUS status;
	int        disabled;
	double     avg;
	double     temp;
	long       hw;
	long       accepted;
	double     util;
} PGA_Stats;

typedef struct _pool_Stats
{	
	int        id;
	char       url[256];
	char       status[32];
	int        priority;
} POOL_Stats;

typedef struct _minerInfo
{
	GPU_STATUS   status;
	Miner_Config config;
	GPU_Summary  summary;
	GPU_Stats    gpu[MAX_GPU_DEVICES];
	PGA_Stats    pga[MAX_PGA_DEVICES];
	POOL_Stats   pools[MAX_POOLS];
} Miner_Info;

char * gpuStatusStr(enum GPU_STATUS status);

void initMinerListeningParameters(char * hostIP, int port);
void resetMinerInfo();
void getMinerStats(Miner_Info * mi);

int disableGPU(int gpu);
int enableGPU(int gpu);
int disableGPUs(void);
int enableGPUs(void);

void switchPool(int pool);

void displayMinerInfoObject(Miner_Info * mi);
void fetchMinerInfo(Miner_Info * mi);

DWORD WINAPI minerApiThread( LPVOID lpParam );
#endif
