// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include "pool.h"
#include "wdmain.h"
#include "network.h"

// uri, host
#define POOL_HTTP_REQUEST "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:11.0) Gecko/20100101 Firefox/11.0\r\nAccept: text/html\r\nAccept-Language: en-us,en;q=0.5\r\nConnection: close\r\n\r\n"

PoolInfo poolStats;
/*
int sendPoolRequest(char * host, int port, char * uri, char * response, int responseMaxSize)
{
	SOCKET sock;
	int ret = 0;
	int rc = 0;

	int index = 0;
	int err = 0;
	char * cmd = NULL;
	int len = 0;
	int outLen = 0;

	if (net_connect(host, port, &sock, 0) != SOCK_NO_ERROR)
	{
		debug_log(LOG_ERR, "sendPoolRequest(): failed to connect to: %s@%d", host, port);
		return 0;
	}

	cmd = (char *) calloc(1,strlen(uri)+1024);
	if (cmd == NULL)
	{
		debug_log(LOG_ERR, "sendPoolRequest(): failed to allocate memory for url request");
		return 0;
	}

	sprintf(cmd, POOL_HTTP_REQUEST, cfg->poolUri, cfg->poolHost);
	len = strlen(cmd);
	rc = net_sendBytes(sock, cmd, len, &outLen, SOCK_TIMEOUT_INTERVAL);
	if (rc != SOCK_NO_ERROR) 
	{
		debug_log(LOG_ERR, "sendPoolRequest(): net_sendBytes() failed: %d, len: %d outLen: %d", rc, len, outLen);
		closesocket(sock);
		free(cmd);
		return 0;
	}
	else 
	{
		debug_log(LOG_DBG, "sendPoolRequest(): request sent ok, reading response...");
		rc = net_recvBytes(sock, response, responseMaxSize, &outLen, SOCK_TIMEOUT_INTERVAL);
		if (rc != SOCK_NO_ERROR)
		{
			debug_log(LOG_ERR, "sendPoolRequest(): net_recvBytes() failed: %d, len: %d outLen: %d", rc, responseMaxSize, outLen);
			closesocket(sock);
			free(cmd);
			return 0;
		}
	}
	debug_log(LOG_DBG, "sendPoolRequest(): response received ok");

	free(cmd);	
	closesocket(sock);
	return 1; // ok
} // end of sendPoolRequest()
*/

void updatePoolStats(PoolInfo * pi)
{
	if (pi != NULL)
	{
		if (getPoolMutex())
		{
			__try
			{
				poolStats.balance  = pi->balance;
				poolStats.hashrate = pi->hashrate;
				poolStats.valids   = pi->valids;
				poolStats.stales   = pi->stales;
				poolStats.invalids = pi->invalids;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}

			releasePoolMutex();
		}
	}
}

void getPoolStats(PoolInfo * pi)
{
	if (pi != NULL)
	{
		if (getPoolMutex())
		{
			__try
			{
				pi->balance  = poolStats.balance;
				pi->hashrate = poolStats.hashrate;
				pi->valids   = poolStats.valids;
				pi->stales   = poolStats.stales;
				pi->invalids = poolStats.invalids;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
			}
			
			releasePoolMutex();
		}
	}
}

void getPoolStatsStr(char * buf)
{
	if (buf == NULL) return;

	if (getPoolMutex())
	{
		__try
		{
			long total = poolStats.valids+poolStats.stales+poolStats.invalids;
			
			sprintf( buf, "balance: %0.5f, avg: %0.2f Mh/s, e: %0.2f%%", 
				     poolStats.balance, poolStats.hashrate, total == 0 ? 0.00 :
					 poolStats.valids*100.00/total
				   );

		} __except(EXCEPTION_EXECUTE_HANDLER)
		{
		}

		releasePoolMutex();
	}
} // end of getPoolStatsStr()


void cleanUpNumberField(char *field, int sizeofField, char * outField, int sizeofOutField)
{
	int i = 0;
	int j = 0;

	memset(outField, 0, sizeofOutField);

	if (sizeofOutField < sizeofField )
		return;

	for (i = 0; i < sizeofField; i++)
	{
		if ( field[i] == '0' ||
			 field[i] == '1' ||
			 field[i] == '2' ||
			 field[i] == '3' ||
			 field[i] == '4' ||
			 field[i] == '5' ||
			 field[i] == '6' ||
			 field[i] == '7' ||
			 field[i] == '8' ||
			 field[i] == '9' ||
			 field[i] == '.' ||
			 field[i] == '-' ||
			 field[i] == '+'
		   )
		{
			outField[j] = field[i];
			j++;
		}
	}
}


// BTCGuild
//{"user":{"user_id":47275,"total_rewards":23.09003062,"paid_rewards":22.93525781,"unpaid_rewards":0.15477281,"past_24h_rewards":0.00000000,"total_rewards_nmc":60.59038773,"paid_rewards_nmc":0.00000000,"unpaid_rewards_nmc":60.59038772,"past_24h_rewards_nmc":0.00000000},"workers":{"1":{"worker_name":"petervii_01","hash_rate":0.00,"valid_shares":715241,"stale_shares":908,"dupe_shares":84,"unknown_shares":10,"valid_shares_since_reset":607232,"stale_shares_since_reset":776,"dupe_shares_since_reset":77,"unknown_shares_since_reset":9,"valid_shares_nmc":707378,"stale_shares_nmc":8767,"dupe_shares_nmc":84,"unknown_shares_nmc":10,"valid_shares_nmc_since_reset":707378,"stale_shares_nmc_since_reset":8767,"dupe_shares_nmc_since_reset":84,"unknown_shares_nmc_since_reset":10,"last_share":109899},"2":{"worker_name":"petervii_02","hash_rate":0.00,"valid_shares":3708,"stale_shares":5,"dupe_shares":0,"unknown_shares":16,"valid_shares_since_reset":2710,"stale_shares_since_reset":4,"dupe_shares_since_reset":0,"unknown_shares_since_reset":0,"valid_shares_nmc":3657,"stale_shares_nmc":56,"dupe_shares_nmc":0,"unknown_shares_nmc":16,"valid_shares_nmc_since_reset":3657,"stale_shares_nmc_since_reset":56,"dupe_shares_nmc_since_reset":0,"unknown_shares_nmc_since_reset":16,"last_share":2957499}},"pool":{"pool_speed":1267.27,"pps_rate":0.00003010305384390648,"difficulty":1577913,"pps_rate_nmc":0.00007429269444744222,"difficulty_nmc":639363}}

// ABCPool
//{"confirmed_rewards":0.30878135149,"hashrate":3555,"payout_history":46.65603534528,"workers":{"petervii.btc-01":{"alive":true,"hashrate":3555}},"running_valids":73498,"running_stales":143,"running_invalids":52,"lifetime_valids":1509423,"lifetime_stales":2658,"lifetime_invalids":1315}

void fetchPoolInfo(CGMConfig * cfg)
{
	PoolInfo _poolStats;
	char buf[RECVSIZE+1] = {0};
	char bufTemp[256] = {0};
	char bufTemp2[256] = {0};
	char * start = NULL;
	char * end = NULL;

	getPoolStats(&_poolStats);

	// Get stats from the pool
	debug_log(LOG_INF, "fetchPoolInfo(): getting stats from pool (current balance: %0.5f BTC)", _poolStats.balance);

	//sendPoolRequest(cfg->poolHost, cfg->poolPort, cfg->poolUri, buf, sizeof(buf));

	net_get_url(cfg->poolApiUrl, buf, sizeof(buf));

	//{"confirmed_rewards":2.81445785776,"hashrate":3665,"payout_history":46.65603534528,"workers":{"petervii.btc-01":{"alive":true,"hashrate":3665}},"running_valids":73752,"running_stales":153,"running_invalids":30,"lifetime_valids":1589527,"lifetime_stales":2833,"lifetime_invalids":1345}

	start =strstr(buf, cfg->poolBalanceLabel);

	if (start != NULL)
	{
		start += strlen(cfg->poolBalanceLabel)+2;

		end = strstr(start, ",");

		if (end != NULL)
		{
			memset(bufTemp, 0, sizeof(bufTemp));
			strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
			cleanUpNumberField(bufTemp, sizeof(bufTemp), bufTemp2, sizeof(bufTemp2));
			_poolStats.balance = atof(bufTemp2);

			start = strstr(buf, cfg->poolHashRateLabel);

			if (start != NULL)
			{
				start += strlen(cfg->poolHashRateLabel)+2;
				end = strstr(start, ",");

				if (end != NULL)
				{
					memset(bufTemp, 0, sizeof(bufTemp));
					strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
					cleanUpNumberField(bufTemp, sizeof(bufTemp), bufTemp2, sizeof(bufTemp2));
					_poolStats.hashrate = atof(bufTemp2);

					start = strstr(buf, cfg->poolValidsLabel);

					if (start != NULL)
					{
						start += strlen(cfg->poolValidsLabel)+2;
						end = strstr(start, ",");

						if (end != NULL)
						{
							memset(bufTemp, 0, sizeof(bufTemp));
							strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
							cleanUpNumberField(bufTemp, sizeof(bufTemp), bufTemp2, sizeof(bufTemp2));
							_poolStats.valids = atol(bufTemp2);

							start = strstr(buf, cfg->poolStalesLabel);
							if (start != NULL)
							{
								start += strlen(cfg->poolStalesLabel)+2;
								end = strstr(start, ",");

								if (end != NULL)
								{
									memset(bufTemp, 0, sizeof(bufTemp));
									strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
									cleanUpNumberField(bufTemp, sizeof(bufTemp), bufTemp2, sizeof(bufTemp2));
									_poolStats.stales = atol(bufTemp2);

									start = strstr(buf, cfg->poolInvalidsLabel);
									if (start != NULL)
									{
										start += strlen(cfg->poolInvalidsLabel)+2;
										end = strstr(start, ",");

										if (end != NULL)
										{
											memset(bufTemp, 0, sizeof(bufTemp));
											strncpy_s(bufTemp, sizeof(bufTemp), start, end-start);
											cleanUpNumberField(bufTemp, sizeof(bufTemp), bufTemp2, sizeof(bufTemp2));
											_poolStats.invalids = atol(bufTemp2);
										}
									}
								}
							}
						}
					}
				}
			} else
			{
				debug_log(LOG_DBG, "fetchPoolInfo(): failed to find label: \'%s\' in pool response:\'%s\'", cfg->poolHashRateLabel, buf);
			}
		}
	} else
	{
		debug_log(LOG_DBG, "fetchPoolInfo(): failed to find label: \'%s\' in pool response:\'%s\'", cfg->poolBalanceLabel, buf);
	}

	debug_log(LOG_INF, "fetchPoolInfo(): updating pool cache (balance: %0.5f BTC)", _poolStats.balance);

	// Update global variable
	updatePoolStats(&_poolStats);
	debug_log(LOG_INF, "fetchPoolInfo(): updated local cache, ok");
} // end of fetchPoolInfo()

DWORD WINAPI poolPollingThread(LPVOID pvParam)
{
	CGMConfig * _cfg = (CGMConfig *) pvParam;

	int waitIntervalMin = _cfg->poolRefreshInterval;

	long long waitInterval = waitIntervalMin*60*1000; // miliseconds	

	debug_log(LOG_SVR, "poolPollingThread(): will poll pool: %s every %d minutes", _cfg->poolHost, waitIntervalMin);

	wait(15000);

	while (waitForShutdown(1) == 0)  // while shutdown event is not set
	{
		fetchPoolInfo(_cfg);
		wait(waitInterval);
		debug_log(LOG_INF, "poolPollingThread(): woke up, time to poll pool: %s", _cfg->poolHost);
	}  

	debug_log(LOG_SVR, "poolPollingThread(): exiting pool polling thread: 0x%04x", GetCurrentThreadId());
	ExitThread(0);
	return 0;
} // end of poolInfoThread()
