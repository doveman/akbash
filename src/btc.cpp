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
#include "config.h"

#include "btc.h"
#include "wdmain.h"
#include "network.h"

BTCInfo gBTCStats;
BTCInfo gLTCStats;

void updateBTCStats(BTCInfo * btc)
{
	if (btc != NULL)
	{
		if (getBTCMutex())
		{
			__try
			{
				gBTCStats.last     = btc->last;
				gBTCStats.bid      = btc->bid;
				gBTCStats.ask      = btc->ask;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}

			releaseBTCMutex();
		}
	}
}

void updateLTCStats(BTCInfo * ltc)
{
	if (ltc != NULL)
	{
		if (getLTCMutex())
		{
			__try
			{
				gLTCStats.last     = ltc->last;
				gLTCStats.bid      = ltc->bid;
				gLTCStats.ask      = ltc->ask;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}

			releaseLTCMutex();
		}
	}
}

void getBTCStats(BTCInfo * btc)
{
	if (btc != NULL)
	{
		if (getBTCMutex())
		{
			__try
			{
				btc->last     = gBTCStats.last;
				btc->bid      = gBTCStats.bid;
				btc->ask      = gBTCStats.ask;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}
			
			releaseBTCMutex();
		}
	}
}

void getLTCStats(BTCInfo * ltc)
{
	if (ltc != NULL)
	{
		if (getLTCMutex())
		{
			__try
			{
				ltc->last     = gLTCStats.last;
				ltc->bid      = gLTCStats.bid;
				ltc->ask      = gLTCStats.ask;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}
			
			releaseLTCMutex();
		}
	}
}

void getBTCStatsStr(char * buf)
{
	if (buf == NULL) return;

	if (getBTCMutex())
	{
		__try
		{
			sprintf( buf, "BTC: bid/ask: $%0.2f/$%0.2f, last: $%0.2f", 
				     gBTCStats.bid, gBTCStats.ask, gBTCStats.last
				   );
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
		
		releaseBTCMutex();
	}
} // end of getBTCStatsStr()

void getLTCStatsStr(char * buf)
{
	if (buf == NULL) return;

	if (getLTCMutex())
	{
		__try
		{
			sprintf( buf, "LTC: bid/ask: $%0.2f/$%0.2f, last: $%0.2f", 
				     gLTCStats.bid, gLTCStats.ask, gLTCStats.last
				   );
		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
		
		releaseLTCMutex();
	}
} // end of getLTCStatsStr()

void fetchBTCQuotes(CGMConfig * cfg)
{
	BTCInfo _btcStats;
	char buf[RECVSIZE+1] = {0};
	char bufTemp[256] = {0};
	char * start = NULL;
	char * end = NULL;

	getBTCStats(&_btcStats);

	debug_log(LOG_INF, "fetchBTCQuotes(): getting BTC quotes...");
	
	net_get_url(cfg->btcUrl, buf, sizeof(buf));

	start =strstr(buf, cfg->btcLastLabel);

	if (start != NULL)
	{
		start += strlen(cfg->btcLastLabel)+2;

		end = strstr(start, ",");

		if (end != NULL)
		{
			memset(bufTemp, 0, sizeof(bufTemp));
			strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
			_btcStats.last = atof(bufTemp);

			start = strstr(buf, cfg->btcAskLabel);
			if (start != NULL)
			{
				start += strlen(cfg->btcAskLabel)+2;
				end = strpbrk(start, ",}");

				if (end != NULL)
				{
					memset(bufTemp, 0, sizeof(bufTemp));
					strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
					_btcStats.ask = atof(bufTemp);

					start = strstr(buf, cfg->btcBidLabel);
					if (start != NULL)
					{
						start += strlen(cfg->btcBidLabel)+2;
						end = strpbrk(start, ",}");

						if (end != NULL)
						{
							memset(bufTemp, 0, sizeof(bufTemp));
							strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
							_btcStats.bid = atof(bufTemp);
						}
					}
				}
			}
		}
	}

	// Update global variable
	updateBTCStats(&_btcStats);

	debug_log(LOG_INF, "fetchBTCQuotes(): updated local cache, ok");
} // end of fetchBTCQuotes()

void fetchLTCQuotes(CGMConfig * cfg)
{
	BTCInfo _ltcStats;
	char buf[RECVSIZE+1] = {0};
	char bufTemp[256] = {0};
	char * start = NULL;
	char * end = NULL;

	getLTCStats(&_ltcStats);

	debug_log(LOG_INF, "fetchLTCQuotes(): getting LTC quotes...");
	
	net_get_url(cfg->ltcUrl, buf, sizeof(buf));

	start =strstr(buf, cfg->ltcLastLabel);

	if (start != NULL)
	{
		start += strlen(cfg->ltcLastLabel)+2;

		end = strstr(start, ",");

		if (end != NULL)
		{
			memset(bufTemp, 0, sizeof(bufTemp));
			strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
			_ltcStats.last = atof(bufTemp);

			start = strstr(buf, cfg->ltcAskLabel);
			if (start != NULL)
			{
				start += strlen(cfg->ltcAskLabel)+2;
				end = strpbrk(start, ",}");

				if (end != NULL)
				{
					memset(bufTemp, 0, sizeof(bufTemp));
					strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
					_ltcStats.ask = atof(bufTemp);

					start = strstr(buf, cfg->ltcBidLabel);
					if (start != NULL)
					{
						start += strlen(cfg->ltcBidLabel)+2;
						end = strpbrk(start, ",}");

						if (end != NULL)
						{
							memset(bufTemp, 0, sizeof(bufTemp));
							strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
							_ltcStats.bid = atof(bufTemp);
						}
					}
				}
			}
		}
	}

	// Update global variable
	updateLTCStats(&_ltcStats);

	debug_log(LOG_INF, "fetchLTCQuotes(): updated local cache, ok");
} // end of fetchLTCQuotes()


DWORD WINAPI btcQuotesThread(LPVOID pvParam)
{
	CGMConfig * _cfg = (CGMConfig *) pvParam;

	int waitIntervalMin = _cfg->btcQuotesRefreshInterval;

	DWORD waitInterval = waitIntervalMin*60*1000; // miliseconds	

	debug_log(LOG_INF, "btcQuotesThread(): will query BTC quotes every %d minutes", waitIntervalMin);

	wait(15000);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		fetchBTCQuotes(_cfg);

		wait(waitInterval);

		debug_log(LOG_INF, "btcQuotesThread(): woke up, time to query BTC prices");
	}  

	debug_log(LOG_SVR, "btcQuotesThread(): exiting pool polling thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of btcQuotesThread()

DWORD WINAPI ltcQuotesThread(LPVOID pvParam)
{
	CGMConfig * _cfg = (CGMConfig *) pvParam;

	int waitIntervalMin = _cfg->ltcQuotesRefreshInterval;

	DWORD waitInterval = waitIntervalMin*60*1000; // miliseconds	

	debug_log(LOG_INF, "ltcQuotesThread(): will query LTC quotes every %d minutes", waitIntervalMin);

	wait(15000);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		fetchLTCQuotes(_cfg);

		wait(waitInterval);

		debug_log(LOG_INF, "ltcQuotesThread(): woke up, time to query LTC prices");
	}  

	debug_log(LOG_SVR, "ltcQuotesThread(): exiting pool polling thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of ltcQuotesThread()
