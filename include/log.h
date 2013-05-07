// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_LOG_H
#define CGM_LOG_H

#include <stdio.h>

typedef enum _logLevel
{
	LOG_ERR = 0,
	LOG_SVR = 1,
	LOG_INF = 2, 
	LOG_DBG = 3

} LOG_LEVEL;

void debug_logLevel(int level);
void debug_logOpen(char * fileName, int size, int level);
void debug_logClose(void);
void debug_log(int level, const char *format, ...);

#endif
