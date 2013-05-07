// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys\stat.h>

#include <windows.h>
#include "log.h"

#define LOG_ERR_STR "ERROR"
#define LOG_SVR_STR "SEVERE"
#define LOG_INF_STR "INFO"
#define LOG_DBG_STR "DEBUG"

static FILE * _outFile = NULL;
static int   _fd = 0;
static HANDLE _mtxLog = NULL;
static long _maxFileSize = 0;
static char  _logFileName[MAX_PATH+1];

static int _logLevel = 3;

char * logLevelStr(int level)
{
	if (level == LOG_ERR) return "ERR";
	if (level == LOG_SVR) return "SVR";
	if (level == LOG_INF) return "INF";
	if (level == LOG_DBG) return "DBG";

	return "DBX";
}

void debug_logLevel(int level)
{
	if (_logLevel != level)
	{
		debug_log(-1, "debug_logLevel(): log level changing %s -> %s", logLevelStr(_logLevel), logLevelStr(level));
		_logLevel = level;
	}
}

void debug_logOpen(char * fileName, int size, int level)
{
	_logLevel = level;
	_maxFileSize = size;

	_mtxLog = CreateMutex(NULL, FALSE, NULL);
  
	if (_mtxLog == NULL)
	{
		fprintf(stderr, "\nUnable to create mtxLog mutex.  Exiting...");
		exit(0);
	}

	 // Open a file.
    if( _sopen_s( &_fd, fileName, _O_RDWR | _O_APPEND | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE) )
	{
		fprintf(stderr, "\nUnable to open \'%s\' file.  _sopen_s() failed: \'%s\' Exiting...", fileName, strerror(errno));
        exit(0);
	}

    // Get stream from file descriptor.
    _outFile = _fdopen( _fd, "a+" );
	if (_outFile == NULL)
	{
		fprintf(stderr, "\nUnable to open \'%s\' file.  Exiting...", fileName);
		exit(0);
	}

	strcpy_s(_logFileName, sizeof(_logFileName), fileName);
}

void debug_logClose(void)
{
	if (WaitForSingleObject(_mtxLog, INFINITE) ==  WAIT_OBJECT_0)
	{
		__try
		{
			fclose(_outFile);
			_outFile = NULL;
		} __finally 
		{ 
			ReleaseMutex(_mtxLog);
		}
	}
				
	CloseHandle(_mtxLog);
	_mtxLog = NULL;
}

void debug_log(int level, const char *format, ...)
{
	va_list args;
	struct tm *newtime;
	__int64 ltime;
	char buff[85];

	fpos_t pos;

	//printf("\nlevel: %d, _logLevel: %d\n", level, _logLevel);

	if (level > _logLevel) return;

	if (_outFile == NULL)
		return;

	if (WaitForSingleObject(_mtxLog, INFINITE) ==  WAIT_OBJECT_0)
	{
		__try
		{
			_time64( &ltime );

			// Obtain coordinated universal time:
			newtime = _localtime64( &ltime ); // C4996
  
			pos = _lseek( _fd, 0L, SEEK_CUR );

			if (pos > _maxFileSize)
			{
				char buf1[MAX_PATH+1];
				char buf2[MAX_PATH+1];
				char buf3[MAX_PATH+1];

				fclose(_outFile);

				sprintf(buf1, "%s.001.txt", _logFileName);
				sprintf(buf2, "%s.002.txt", _logFileName);
				sprintf(buf3, "%s.003.txt", _logFileName);

				_unlink(buf3);
				rename(buf2, buf3);
				rename(buf1, buf2);
				rename(_logFileName, buf1);

				// Open a file.
				if( _sopen_s( &_fd, _logFileName, _O_RDWR | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE) )
				{
					fprintf(stderr, "\nUnable to open \'%s\' file.  _sopen_s() failed: \'%s\' Exiting...", _logFileName, strerror(errno));
					exit(0);
				}

				// Get stream from file descriptor.
				_outFile = _fdopen( _fd, "a+" );
				if (_outFile == NULL)
				{
					fprintf(stderr, "\nUnable to open \'%s\' file.  Exiting...", _logFileName);
					exit(0);
				}
			}

			sprintf( buff, "%02d/%02d/%02d %02d:%02d:%02d 0x%04x %s ",
					 newtime->tm_mon+1,
					 newtime->tm_mday,
					 newtime->tm_year-100, // 12 for 2012
					 newtime->tm_hour,
					 newtime->tm_min,
					 newtime->tm_sec,
					 GetCurrentThreadId(),
					 logLevelStr(level)
				   );
	
			va_start(args, format);

			// print to screen
			printf( "%s", buff );
			vprintf(format, args);
			printf( "\n");

			// print to log
			fprintf( _outFile, "%s", buff );
			vfprintf( _outFile, format, args);
			fprintf( _outFile, "\r\n");

			va_end(args);
			fflush(stdout);
			fflush(_outFile);
		} __finally 
		{ 
			ReleaseMutex(_mtxLog);
		}
	}
}
