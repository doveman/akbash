// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_BTC_H
#define CGM_BTC_H

typedef struct _BTCInfo
{
	double last;
	double bid;
	double ask;

} BTCInfo;

void getBTCStatsStr(char * buf);
void getLTCStatsStr(char * buf);

void getBTCStats(BTCInfo * btc);
void getLTCStats(BTCInfo * ltc);

DWORD WINAPI btcQuotesThread(LPVOID pvParam);
DWORD WINAPI ltcQuotesThread(LPVOID pvParam);

#endif