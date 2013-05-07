// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_POOL_H
#define CGM_POOL_H

#include <winsock2.h>
#include <stdio.h>

#include "miner_api.h"
#include "log.h"
#include "listen.h"
#include "config.h"

typedef struct _poolInfo
{
	double balance;
    double hashrate;
	long   valids;
	long   stales;
	long   invalids;

} PoolInfo;

void getPoolStats(PoolInfo * pi);
void getPoolStatsStr(char * buf);

DWORD WINAPI poolPollingThread(LPVOID pvParam);

#endif