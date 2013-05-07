// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_tree.h>

#include "config.h"
#include "wdmain.h"

#define MIN_INIT_INTERVAL          30      // seconds
#define MIN_ALIVE_INTERVAL         120     // seconds
#define MIN_NOT_CONNECTED_INTERVAL 60      // seconds
#define MIN_LOG_FILE_SIZE          20000  // bytes
#define MAX_LOG_FILE_SIZE          2097152 // 2 MB bytes 1024*1024*2
#define MIN_EMAIL_STATUS_INTERVAL  15      // minutes
#define MIN_POOL_REFRESH_INTERVAL  5       // minutes
#define MIN_BTC_REFRESH_INTERVAL   5       // minutes
#define MIN_LTC_REFRESH_INTERVAL   5       // minutes

#define MIN_CUTOFF_TEMP			   86      // deg. C
#define MIN_CUTOFF_TEMP_COOLDOWN   5       // minutes

#define MIN_HW_POLLING_INTERVAL    60     // seconds
#define MIN_HW_ACTIVITY_THRESHOLD  10
#define MAX_HW_ACTIVITY_THRESHOLD  50
#define MIN_HW_ACTIVITY_TIMEOUT    80

#define MIN_HW_ERRORS_THRESHOLD    0   // H/W errors as reported by miner API

static unsigned char fileData[4096];
static CGMConfig cgmConfig;

CGMConfig * getCGMConfig()
{
	return &cgmConfig;
}

int parsePoolURL(CGMConfig * cfg)
{
	char * start = NULL;
	char * end = NULL;
	char portStr[32];
	int offset = 7;

	start = strstr(cfg->poolApiUrl, "http://");

	if (start == NULL)
	{
		start = strstr(cfg->poolApiUrl, "https://");
		offset = 8;
	}

	if (start != NULL)
	{
		start += offset;

		end = strstr(start, ":");
		if (end != NULL)
		{
			strncpy_s(cfg->poolHost, sizeof(cfg->poolHost), start, end-start);
			start = end + 1;

			end = strstr(start, "/");

			if (end != NULL)
			{
				strncpy_s(portStr, sizeof(portStr), start, end-start);
				cfg->poolPort = atoi(portStr);

				strcpy_s(cfg->poolUri, sizeof(cfg->poolUri), end);
			} else
			{
				// error
				return 1;
			}
		} else
		{
			end = strstr(start, "/");

			if (end != NULL)
			{
				cfg->poolPort = 80;
				strncpy_s(cfg->poolHost, sizeof(cfg->poolHost), start, end-start);
				strcpy_s(cfg->poolUri, sizeof(cfg->poolUri), end);
			} else
			{
				// error
				return 1;
			}
		}

	}
	return 0;
} // end of parsePoolURL()

CGMConfig * parseConfigFile(const char * fileName)
{
	size_t rd;
    yajl_val node;
    char errbuf[1024];

	FILE * inFile = fopen(fileName, "r");

	memset(fileData, 0, sizeof(fileData));
	memset(errbuf, 0, sizeof(errbuf));
	
	memset(&cgmConfig, 0, sizeof(cgmConfig));

	if (inFile == NULL)
	{
		fprintf(stderr, "\n%s\n", title);
		fprintf(stderr, "\nparseConfigFile(): Failed to open: \'%s\' file.\n", fileName);
		exit(-1);
	}

    // read the entire config file 
    rd = fread((void *) fileData, 1, sizeof(fileData) - 1, inFile);
	fclose(inFile);

    // file read error handling 
    if (rd == 0 && !feof(stdin)) 
	{
		fprintf(stderr, "\n%s\n", title);
        fprintf(stderr, "\nparseConfigFile(): Error reading file: \'%s\'\n", fileName);
        exit(-2);
    } else 
		if (rd >= sizeof(fileData) - 1) 
		{
			fprintf(stderr, "\n%s\n", title);
			fprintf(stderr, "parseConfigFile(): config file :\'%s\' is too big\n", fileName);
			exit(-3);
		}

    // we have the whole config file in memory.  let's parse it ... 
    node = yajl_tree_parse((const char *) fileData, errbuf, sizeof(errbuf));

    // parse error handling 
    if (node == NULL) 
	{
		fprintf(stderr, "\n%s\n", title);
        fprintf(stderr, "\nparseConfigFile(): Error parsing config file, parse_error: ");
        if (strlen(errbuf)) 
			fprintf(stderr, " %s", errbuf);
        else 
			fprintf(stderr, "unknown error");
        fprintf(stderr, "\n");
        exit(-4);
    }

    // ... and extract a nested value from the config file 
	{
		const char * cgminer_exe[] = { CGMINER_EXE_LABEL, (const char *) 0 };
		const char * logFile[] = { LOG_FILE_LABEL, (const char *) 0 };
		const char * logLevel[] = { WDOG_LOG_LEVEL, (const char *) 0 };

		const char * cgminer_name[] = { CGMINER_NAME_LABEL, (const char *) 0 };
		const char * miner_init_interval[] = { MINER_INIT_INTERVAL, (const char *) 0 };
		const char * total_avg_rate[] = { TOTAL_AVG_RATE, (const char *) 0 };
		const char * pga_avg_rate_threshold[] = { PGA_AVG_RATE, (const char *) 0 };
		const char * miner_api_poll_interval[] = { MINER_API_POLL_INTERVAL, (const char *) 0 };
		const char * listen_ip[] = { LISTEN_IP, (const char *) 0 };
		const char * listen_port[] = { LISTEN_PORT, (const char *) 0 };

		const char * miner_listen_ip[] = { MINER_LISTEN_IP, (const char *) 0 };
		const char * miner_listen_port[] = { MINER_LISTEN_PORT, (const char *) 0 };
		const char * miner_solo_mining[] = { MINER_SOLO_MINING, (const char *) 0 };

		const char * number_of_restarts[] = { NUMBER_OF_RESTARTS, (const char *) 0 };
		const char * alive_timeout[] = { ALIVE_TIMEOUT, (const char *) 0 };
		const char * not_connected_timeout[] = { NOT_CONNECTED_TIMEOUT, (const char *) 0 };
		const char * log_file_size[] = { LOG_FILE_SIZE, (const char *) 0 };
		const char * disable_remote_api[] = { DISABLE_REMOTE_API, (const char *) 0 };
		const char * disable_remote_help[] = { DISABLE_REMOTE_HELP, (const char *) 0 };
		const char * disable_remote_restart[] = { DISABLE_REMOTE_RESTART, (const char *) 0 };
		const char * disable_remote_reboot[] = { DISABLE_REMOTE_REBOOT, (const char *) 0 };
		const char * disable_remote_getlog[] = { DISABLE_REMOTE_GETLOG, (const char *) 0 };
		const char * disable_remote_status[] = { DISABLE_REMOTE_STATUS, (const char *) 0 };
		const char * wdog_rig_name[] = { WDOG_RIG_NAME, (const char *) 0 };		
		
		const char * disable_notify_email[] = { DISABLE_NOTIFICATION_EMAILS, (const char *) 0 };
		const char * disable_status_notify_email[] = { DISABLE_STATUS_NOTIFICATION_EMAILS, (const char *) 0 };
		
		const char * notify_email_address[] = { NOTIFICATIONS_EMAIL, (const char *) 0 };
		const char * status_notification_freq[] = { STATUS_NOTIFICATION_FREQ, (const char *) 0 };
		const char * disable_pool_info[] = { DISABLE_POOL_INFO, (const char *) 0 };
		const char * pool_refresh_interval[] = { POOL_REFRESH_INTERVAL, (const char *) 0 };
		const char * pool_api_url[] = { POOL_API_URL, (const char *) 0 };
		const char * pool_balance_label[] = { POOL_BALANCE_LABEL, (const char *) 0 };
		const char * pool_hash_rate_label[] = { POOL_HASH_RATE_LABEL, (const char *) 0 };
		const char * pool_valids_label[] = { POOL_VALIDS_LABEL, (const char *) 0 };
		const char * pool_stales_label[] = { POOL_STALES_LABEL, (const char *) 0 };
		const char * pool_invalids_label[] = { POOL_INVALIDS_LABEL, (const char *) 0 };
		
		const char * disable_btc_quotes[] = { DISABLE_BTC_QUOTES, (const char *) 0 };
		const char * btc_quote_refresh_interval[] = { QUOTE_REFRESH_INTERVAL, (const char *) 0 };
		const char * btc_quote_url[] = { QUOTE_URL, (const char *) 0 };
		const char * btc_last_label[] = { QUOTE_LAST_LABEL, (const char *) 0 };
		const char * btc_bid_label[] = { QUOTE_BID_LABEL, (const char *) 0 };
		const char * btc_ask_label[] = { QUOTE_ASK_LABEL, (const char *) 0 };

		const char * disable_ltc_quotes[] = { DISABLE_LTC_QUOTES, (const char *) 0 };
		const char * ltc_quote_refresh_interval[] = { LTC_QUOTE_REFRESH_INTERVAL, (const char *) 0 };
		const char * ltc_quote_url[] = { LTC_QUOTE_URL, (const char *) 0 };
		const char * ltc_last_label[] = { LTC_QUOTE_LAST_LABEL, (const char *) 0 };
		const char * ltc_bid_label[] = { LTC_QUOTE_BID_LABEL, (const char *) 0 };
		const char * ltc_ask_label[] = { LTC_QUOTE_ASK_LABEL, (const char *) 0 };

		const char * miner_working_set_threshold[] = { MINER_WORKING_SET_THRESHOLD, (const char *) 0 };
		const char * miner_handle_count_threshold[] = { MINER_HANDLE_COUNT_THRESHOLD, (const char *) 0 };

		const char * wdog_cutoff_temp_threshold[] = { CUT_OFF_TEMPERATURE, (const char *) 0 };
		const char * wdog_cutoff_temp_cooldown[] = { CUT_OFF_TEMPERATURE_COOLDOWN, (const char *) 0 };

		const char * wdog_disable_hw_monitoring[] = { DISABLE_ADL_READINGS, (const char *) 0 };
		const char * wdog_hw_monitoring_interval[] = { ADL_HW_POLLING, (const char *) 0 };
		const char * wdog_gpu_utilization_threshold[] = { ADL_HW_ACTIVITY_THRESHOLD, (const char *) 0 };
		const char * wdog_gpu_utilization_timeout[] = { ADL_HW_ACTIVITY_TIMEOUT, (const char *) 0 };

		const char * miner_hw_errors_threshold[] = { MINER_HW_ERRORS_THRESHOLD, (const char *) 0 };

		const char * smart_metering_disable[] = { SMART_METERING_DISABLE, (const char *) 0 };
		const char * smart_metering_polling_interval[] = { SMART_METERING_POLLING_INTERVAL, (const char *) 0 };
		const char * smart_metering_on_peak_start_time[] = { SMART_METERING_ON_PEAK_START_TIME, (const char *) 0 };
		const char * smart_metering_off_peak_start_time[] = { SMART_METERING_OFF_PEAK_START_TIME, (const char *) 0 };
		const char * smart_metering_on_peak_shutdown[] = { SMART_METERING_ON_PEAK_SHUTDOWN, (const char *) 0 };
		const char * smart_metering_on_peak_disable_gpus[] = { SMART_METERING_ON_PEAK_DISABLE_GPUS, (const char *) 0 };

		
		yajl_val v = yajl_tree_get(node, cgminer_exe, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\n%s\n", title);
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'\n", cgminer_exe[0], fileName);
			exit(-5);
		}

		strcpy_s(cgmConfig.minerExeFullPath, sizeof(cgmConfig.minerExeFullPath), YAJL_GET_STRING(v));
		inFile = fopen(cgmConfig.minerExeFullPath, "r");
		if (inFile == NULL)
		{
			fprintf(stderr, "\n%s\n", title);
			fprintf(stderr, "\nparseConfigFile(): Unable to open: \'%s\' file, rc: %d,\nAre you sure your config \'%s\' file has been updated correctly?\n", cgmConfig.minerExeFullPath, errno, fileName);
			fprintf(stderr, "\nYou should at least customize the following fields:\n");
			fprintf(stderr, "\n\t\"%s\" - shortcut to start your miner\n", CGMINER_EXE_LABEL);
			fprintf(stderr, "\n\t\"%s\" - full path to akbash log file\n", LOG_FILE_LABEL);
			fprintf(stderr, "\n\t\"%s\" - miner name, eq. cgminer.exe or bfgminer.exe\n", CGMINER_NAME_LABEL);
			fprintf(stderr, "\n\t\"%s\" - local IP address\n", LISTEN_IP);
			fprintf(stderr, "\n\t\"%s\" - your email address\n", NOTIFICATIONS_EMAIL);
			fprintf(stderr, "\n\n");
			exit(-6);
		}

		v = yajl_tree_get(node, logFile, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to akbash.log\n", logFile[0], fileName);
			strcpy_s(cgmConfig.wdogLogFileName, sizeof(cgmConfig.wdogLogFileName), "akbash.log");
		} else
		{
			strcpy_s(cgmConfig.wdogLogFileName, sizeof(cgmConfig.wdogLogFileName), YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, cgminer_name, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to cgminer.exe\n", cgminer_name[0], fileName);
			strcpy_s(cgmConfig.minerExeName, sizeof(cgmConfig.minerExeName), "cgminer.exe");
		} else
		{
			strcpy_s(cgmConfig.minerExeName, sizeof(cgmConfig.minerExeName), YAJL_GET_STRING(v));
		}

   	    v = yajl_tree_get(node, wdog_rig_name, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to Rig #1\n", wdog_rig_name[0], fileName);
			strcpy_s(cgmConfig.wdogRigName, sizeof(cgmConfig.wdogRigName), "Rig #1");
		} else
		{
			strcpy_s(cgmConfig.wdogRigName, sizeof(cgmConfig.wdogRigName),  YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, miner_init_interval, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.minerInitInterval = MIN_INIT_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d seconds\n", miner_init_interval[0], fileName, cgmConfig.minerInitInterval);			
		} else
		{
			cgmConfig.minerInitInterval = atoi(YAJL_GET_STRING(v));
		}

		if ( cgmConfig.minerInitInterval < MIN_INIT_INTERVAL)
		{
			cgmConfig.minerInitInterval = MIN_INIT_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too small. Recommended init interval is %d seconds\n", miner_init_interval[0], fileName, cgmConfig.minerInitInterval);			
		}

		v = yajl_tree_get(node, total_avg_rate, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 300 (Mh/s)\n", total_avg_rate[0], fileName);
			cgmConfig.minerGPUAvgRateThreshold = 300.00;
		} else
		{
			cgmConfig.minerGPUAvgRateThreshold = atof(YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, pga_avg_rate_threshold, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 200 (Mh/s)\n", pga_avg_rate_threshold[0], fileName);
			cgmConfig.minerPGAAvgRateThreshold = 200.00;
		} else
		{
			cgmConfig.minerPGAAvgRateThreshold = atof(YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, miner_working_set_threshold, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 400 MB\n", miner_working_set_threshold[0], fileName);
			cgmConfig.minerWorkingSetThreshold = 400*1024*1024;
		} else
		{
			cgmConfig.minerWorkingSetThreshold = atoi(YAJL_GET_STRING(v))*1024*1024;
		}

		v = yajl_tree_get(node, miner_handle_count_threshold, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 10000\n", miner_handle_count_threshold[0], fileName);
			cgmConfig.minerHandleCountThreshold = 10000;
		} else
		{
			cgmConfig.minerHandleCountThreshold = atoi(YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, miner_solo_mining, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 0\n", miner_solo_mining[0], fileName);
			cgmConfig.minerSoloMining = 1;
		} else
		{
			cgmConfig.minerSoloMining = atoi(YAJL_GET_STRING(v));
		}		

		// --------------------------
		// Miner API related entries.
		// --------------------------
		v = yajl_tree_get(node, listen_ip, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 127.0.0.1\n", listen_ip[0], fileName);
			strcpy_s(cgmConfig.wdogListenIP, sizeof(cgmConfig.wdogListenIP), "127.0.0.1");
		} else
		{
			strcpy_s(cgmConfig.wdogListenIP, sizeof(cgmConfig.wdogListenIP), YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, listen_port, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 80\n", listen_port[0], fileName);
			cgmConfig.wdogListenPort = 80;
		} else
		{
			cgmConfig.wdogListenPort = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogListenPort == 0) cgmConfig.wdogListenPort = 80;

		v = yajl_tree_get(node, miner_listen_ip, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 127.0.0.1\n", miner_listen_ip[0], fileName);
			strcpy_s(cgmConfig.minerListenIP, sizeof(cgmConfig.minerListenIP), "127.0.0.1");
		} else
		{
			strcpy_s(cgmConfig.minerListenIP, sizeof(cgmConfig.minerListenIP), YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, miner_listen_port, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 80\n", miner_listen_port[0], fileName);
			cgmConfig.minerListenPort = 4023;
		} else
		{
			cgmConfig.minerListenPort = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.minerListenPort == 0) cgmConfig.minerListenPort = 4023;




		v = yajl_tree_get(node, miner_api_poll_interval, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 30 seconds\n", miner_api_poll_interval[0], fileName);
			cgmConfig.minerPollInterval = 20;
		} else
		{
			cgmConfig.minerPollInterval = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.minerPollInterval > 120) cgmConfig.minerPollInterval = 120;
		if (cgmConfig.minerPollInterval < 20) cgmConfig.minerPollInterval = 20;

		// -------------------------
		// Miner monitoring entries.
		// -------------------------
		v = yajl_tree_get(node, number_of_restarts, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 10\n", number_of_restarts[0], fileName);
			cgmConfig.wdogNumberOfRestarts = 10;
		} else
		{
			cgmConfig.wdogNumberOfRestarts = atoi(YAJL_GET_STRING(v));
		}
		
		if (cgmConfig.wdogNumberOfRestarts == 0) 
			cgmConfig.wdogNumberOfRestarts = 10;

		v = yajl_tree_get(node, alive_timeout, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.wdogAliveTimeout = MIN_ALIVE_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d seconds\n", alive_timeout[0], fileName, cgmConfig.wdogAliveTimeout);			
		} else
		{
			cgmConfig.wdogAliveTimeout = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogAliveTimeout < MIN_ALIVE_INTERVAL) 
		{
			cgmConfig.wdogAliveTimeout = MIN_ALIVE_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too small. Defaulting to %d seconds\n", alive_timeout[0], fileName, cgmConfig.wdogAliveTimeout);			
		}

		v = yajl_tree_get(node, not_connected_timeout, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.wdogNotConnectedTimeout = MIN_NOT_CONNECTED_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d seconds\n", not_connected_timeout[0], fileName, cgmConfig.wdogNotConnectedTimeout);
		} else
		{
			cgmConfig.wdogNotConnectedTimeout = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogNotConnectedTimeout < MIN_NOT_CONNECTED_INTERVAL)	
		{
			cgmConfig.wdogNotConnectedTimeout = MIN_NOT_CONNECTED_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too small. Defaulting to %d seconds\n", not_connected_timeout[0], fileName, cgmConfig.wdogNotConnectedTimeout);
		}


		v = yajl_tree_get(node, miner_hw_errors_threshold, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.minerHWErrorsThreshold = MIN_HW_ERRORS_THRESHOLD;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d hardware errors\n", miner_hw_errors_threshold[0], fileName, cgmConfig.minerHWErrorsThreshold);
		} else
		{
			cgmConfig.minerHWErrorsThreshold = atol(YAJL_GET_STRING(v));

			if (cgmConfig.minerHWErrorsThreshold < MIN_HW_ERRORS_THRESHOLD)
				cgmConfig.minerHWErrorsThreshold = MIN_HW_ERRORS_THRESHOLD;
		}
		
		// -----------------
		// Log file entries.
		// -----------------
		v = yajl_tree_get(node, log_file_size, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.wdogLogFileSize = MIN_LOG_FILE_SIZE;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d bytes\n", log_file_size[0], fileName, cgmConfig.wdogLogFileSize);			
		} else
		{
			cgmConfig.wdogLogFileSize = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogLogFileSize < MIN_LOG_FILE_SIZE) 
		{
			cgmConfig.wdogLogFileSize = MIN_LOG_FILE_SIZE;
			fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too small. Recommended minimum size is %d bytes\n", log_file_size[0], fileName, cgmConfig.wdogLogFileSize);			
		}

		if (cgmConfig.wdogLogFileSize > MAX_LOG_FILE_SIZE)  // 2MB Max
		{
			cgmConfig.wdogLogFileSize = MAX_LOG_FILE_SIZE;
			fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too big. Recommended maximum size is %d bytes\n", log_file_size[0], fileName, cgmConfig.wdogLogFileSize);			
		}


		v = yajl_tree_get(node, logLevel, yajl_t_string);
		if (v == NULL) 
		{ 
			cgmConfig.wdogLogLevel = 3;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d\n", logLevel[0], fileName, cgmConfig.wdogLogLevel);			
		} else
		{
			cgmConfig.wdogLogLevel = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogLogLevel < 0)
			cgmConfig.wdogLogLevel = 0;

		if (cgmConfig.wdogLogLevel > 3)
			cgmConfig.wdogLogLevel = 3;

		// ------------
		// Remote APIs.
		// ------------
		v = yajl_tree_get(node, disable_remote_api, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Remote control via HTTP will be disabled.\n", disable_remote_api[0], fileName);
			cgmConfig.wdogDisableRemoteApi = 1;
			cgmConfig.wdogDisableRemoteHelp = 1;
			cgmConfig.wdogDisableRemoteRestart = 1;
			cgmConfig.wdogDisableRemoteReboot = 1;
			cgmConfig.wdogDisableRemoteStatus = 1;
			cgmConfig.wdogDisableRemoteGetLog = 1;
		} else
		{
			cgmConfig.wdogDisableRemoteApi = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogDisableRemoteApi == 0)
		{
			v = yajl_tree_get(node, disable_remote_help, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 1 (disable)\n", disable_remote_help[0], fileName);
				cgmConfig.wdogDisableRemoteHelp = 1;
			} else
			{
				cgmConfig.wdogDisableRemoteHelp = atoi(YAJL_GET_STRING(v));
			}

			v = yajl_tree_get(node, disable_remote_restart, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 1 (disable)\n", disable_remote_restart[0], fileName);
				cgmConfig.wdogDisableRemoteRestart = 1;
			} else
			{
				cgmConfig.wdogDisableRemoteRestart = atoi(YAJL_GET_STRING(v));
			}

			v = yajl_tree_get(node, disable_remote_reboot, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 1 (disable)\n", disable_remote_reboot[0], fileName);
				cgmConfig.wdogDisableRemoteReboot = 1;
			} else
			{
				cgmConfig.wdogDisableRemoteReboot = atoi(YAJL_GET_STRING(v));
			}

			v = yajl_tree_get(node, disable_remote_getlog, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 1 (disable)\n", disable_remote_getlog[0], fileName);
				cgmConfig.wdogDisableRemoteGetLog = 1;
			} else
			{
				cgmConfig.wdogDisableRemoteGetLog = atoi(YAJL_GET_STRING(v));
			}

			v = yajl_tree_get(node, disable_remote_status, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to 1 (disable)\n", disable_remote_status[0], fileName);
				cgmConfig.wdogDisableRemoteStatus = 1;
			} else
			{
				cgmConfig.wdogDisableRemoteStatus = atoi(YAJL_GET_STRING(v));
			}
		}

		// --------------------
		// Notification Emails.
		// --------------------
		v = yajl_tree_get(node, disable_notify_email, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to \'none\'.  Email notifications will be disabled.\n", disable_notify_email[0], fileName);
			cgmConfig.wdogDisableNotifications = 1;
		} else
		{
			cgmConfig.wdogDisableNotifications = atoi(YAJL_GET_STRING(v));
		}

			if (cgmConfig.wdogDisableNotifications == 0)  // Emails enabled
			{

			v = yajl_tree_get(node, disable_status_notify_email, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to \'none\'.  Email status notifications will be disabled.\n", disable_status_notify_email[0], fileName);
				cgmConfig.wdogDisableStatusNotifications = 1;
			} else
			{
				cgmConfig.wdogDisableStatusNotifications = atoi(YAJL_GET_STRING(v));
			}

			v = yajl_tree_get(node, notify_email_address, yajl_t_string);
			if (v == NULL) 
			{ 
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to \'none\'.  Email notifications will be disabled.\n", notify_email_address[0], fileName);
				strcpy_s(cgmConfig.wdogEmailAddress, sizeof(cgmConfig.wdogEmailAddress), "none");
			} else
			{
				strcpy_s(cgmConfig.wdogEmailAddress, sizeof(cgmConfig.wdogEmailAddress), YAJL_GET_STRING(v));
			}
			
			if (strlen(cgmConfig.wdogEmailAddress) > 0)
			{
				if (_stricmp(cgmConfig.wdogEmailAddress, "none") == 0)
				{
					cgmConfig.wdogDisableNotifications = 1;
				} else
				{
					char * p = strstr(cgmConfig.wdogEmailAddress, "@");
					if (p == NULL)
					{
						fprintf(stderr, "\nparseConfigFile(): please check your email address: %s, it does not appear to be correct.  Email Notifications will be disabled.\n", cgmConfig.wdogEmailAddress);
						cgmConfig.wdogDisableNotifications = 1;
					} else
						cgmConfig.wdogDisableNotifications = 0;
				}
			} else
			{
				cgmConfig.wdogDisableNotifications = 1;
			}

			v = yajl_tree_get(node, status_notification_freq, yajl_t_string);
			if (v == NULL) 
			{ 
				cgmConfig.wdogStatusNotificationFrequency = MIN_EMAIL_STATUS_INTERVAL;
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d minutes\n", status_notification_freq[0], fileName, cgmConfig.wdogStatusNotificationFrequency);
			} else
			{
				cgmConfig.wdogStatusNotificationFrequency = atoi(YAJL_GET_STRING(v));

				if (cgmConfig.wdogStatusNotificationFrequency < MIN_EMAIL_STATUS_INTERVAL)
				{
					cgmConfig.wdogStatusNotificationFrequency = MIN_EMAIL_STATUS_INTERVAL;
					fprintf(stderr, "\nparseConfigFile(): \'%s\' field entered in the config file: \'%s\' is too small. Defaulting to %d minutes\n", status_notification_freq[0], fileName, cgmConfig.wdogStatusNotificationFrequency);
				}
			}
		}

		// ------------------
		// Pool Info section.
		// ------------------
	    v = yajl_tree_get(node, disable_pool_info, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Pool polling will be disabled\n", disable_pool_info[0], fileName);
			cgmConfig.poolInfoDisabled = 1;
		} else
		{
			cgmConfig.poolInfoDisabled = atoi(YAJL_GET_STRING(v));
		}
			
		if (cgmConfig.poolInfoDisabled == 0)  // Pool Info is enabled
		{
			v = yajl_tree_get(node, pool_refresh_interval, yajl_t_string);
			if (v == NULL) 
			{ 
				cgmConfig.poolRefreshInterval = MIN_POOL_REFRESH_INTERVAL;
				fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Pool refresh interval will be set to %d [minutes].\n", pool_refresh_interval[0], fileName, cgmConfig.poolRefreshInterval);				
			} else
			{
				cgmConfig.poolRefreshInterval = atoi(YAJL_GET_STRING(v));

				if (cgmConfig.poolRefreshInterval < MIN_POOL_REFRESH_INTERVAL)
				{
					cgmConfig.poolRefreshInterval = MIN_POOL_REFRESH_INTERVAL;
					fprintf(stderr, "\nparseConfigFile(): \'%s\' field in the config file: \'%s\' is too small. Pool refresh interval will be set to %d [minutes].\n", pool_refresh_interval[0], fileName, cgmConfig.poolRefreshInterval);									
				}

				v = yajl_tree_get(node, pool_api_url, yajl_t_string);
				if (v == NULL) 
				{ 
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Pool info will be excluded from the watchdog status report.\n", pool_api_url[0], fileName);
					cgmConfig.poolInfoDisabled = 1;
				} else
				{
					strcpy_s(cgmConfig.poolApiUrl, sizeof(cgmConfig.poolApiUrl), YAJL_GET_STRING(v));

					if (parsePoolURL(&cgmConfig) == 1)
					{
						fprintf(stderr, "\nparseConfigFile(): unable to parse pool url: \'%s\' field in the config file: \'%s\'. Pool info will be excluded from the watchdog status report.\n", pool_api_url[0], fileName);
						cgmConfig.poolInfoDisabled = 1;
						goto the_end;
					}

					v = yajl_tree_get(node, pool_balance_label, yajl_t_string);
					if (v == NULL) 
					{
						fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Pool info will be excluded from the watchdog status report.\n", pool_balance_label[0], fileName);
						cgmConfig.poolInfoDisabled = 1;
					} else
					{
						strcpy_s(cgmConfig.poolBalanceLabel, sizeof(cgmConfig.poolBalanceLabel), YAJL_GET_STRING(v));

						v = yajl_tree_get(node, pool_hash_rate_label, yajl_t_string);
						if (v == NULL) 
						{
							fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Pool info will be excluded from the watchdog status report.\n", pool_hash_rate_label[0], fileName);
							cgmConfig.poolInfoDisabled = 1;
						} else
						{
							strcpy_s(cgmConfig.poolHashRateLabel, sizeof(cgmConfig.poolHashRateLabel), YAJL_GET_STRING(v));

							v = yajl_tree_get(node, pool_valids_label, yajl_t_string);
							if (v == NULL) 
							{
								fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Check your pool API to see if it provides count of valid shares.\n", pool_valids_label[0], fileName);
								cgmConfig.poolInfoDisabled = 1;
							} else
							{
								strcpy_s(cgmConfig.poolValidsLabel, sizeof(cgmConfig.poolValidsLabel), YAJL_GET_STRING(v));

								v = yajl_tree_get(node, pool_stales_label, yajl_t_string);
								if (v == NULL) 
								{
									fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Check your pool API to see if it provides count of stale shares.\n", pool_stales_label[0], fileName);							
								} else
								{
									strcpy_s(cgmConfig.poolStalesLabel, sizeof(cgmConfig.poolStalesLabel), YAJL_GET_STRING(v));
								}

								v = yajl_tree_get(node, pool_invalids_label, yajl_t_string);
								if (v == NULL) 
								{
									fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Check your pool API to see if it provides count of invalid shares.\n", pool_invalids_label[0], fileName);							
								} else
								{
									strcpy_s(cgmConfig.poolInvalidsLabel, sizeof(cgmConfig.poolInvalidsLabel), YAJL_GET_STRING(v));
								}
							}
						}
					}
				}
			}
		}

		v = yajl_tree_get(node, disable_btc_quotes, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time BTC quotes are disabled.\n", disable_btc_quotes[0], fileName);
			cgmConfig.btcQuotesDisabled = 1;
		} else
		{
			cgmConfig.btcQuotesDisabled = atoi(YAJL_GET_STRING(v));

			if (cgmConfig.btcQuotesDisabled == 0)
			{
				v = yajl_tree_get(node, btc_quote_url, yajl_t_string);
				if (v == NULL) 
				{ 
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time BTC quotes will be disabled.\n", btc_quote_url[0], fileName);
					cgmConfig.btcQuotesDisabled = 1;
				} else
				{
					strcpy_s(cgmConfig.btcUrl, sizeof(cgmConfig.btcUrl), YAJL_GET_STRING(v));

					v = yajl_tree_get(node, btc_last_label, yajl_t_string);
					if (v == NULL) 
					{
						fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time BTC quotes will be disabled.\n", btc_last_label[0], fileName);
						cgmConfig.btcQuotesDisabled = 1;
					} else
					{
						strcpy_s(cgmConfig.btcLastLabel, sizeof(cgmConfig.btcLastLabel), YAJL_GET_STRING(v));

						v = yajl_tree_get(node, btc_bid_label, yajl_t_string);
						if (v == NULL) 
						{
							fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time BTC quotes will be disabled.\n", btc_bid_label[0], fileName);
							cgmConfig.btcQuotesDisabled = 1;
						} else
						{
							strcpy_s(cgmConfig.btcBidLabel, sizeof(cgmConfig.btcBidLabel), YAJL_GET_STRING(v));

							v = yajl_tree_get(node, btc_ask_label, yajl_t_string);
							if (v == NULL) 
							{
								fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time BTC quotes will be disabled.\n", btc_ask_label[0], fileName);
								cgmConfig.btcQuotesDisabled = 1;
							} else
							{
								strcpy_s(cgmConfig.btcAskLabel, sizeof(cgmConfig.btcAskLabel), YAJL_GET_STRING(v));

								v = yajl_tree_get(node, btc_quote_refresh_interval, yajl_t_string);
								if (v == NULL) 
								{
									cgmConfig.btcQuotesRefreshInterval = MIN_BTC_REFRESH_INTERVAL;
									fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d minutes.\n", btc_quote_refresh_interval[0], fileName, cgmConfig.btcQuotesRefreshInterval);
								} else
								{
									cgmConfig.btcQuotesRefreshInterval = atoi(YAJL_GET_STRING(v));

									if (cgmConfig.btcQuotesRefreshInterval < MIN_BTC_REFRESH_INTERVAL)
										cgmConfig.btcQuotesRefreshInterval = MIN_BTC_REFRESH_INTERVAL;
								}								
							}
						}
					}
				}
			}			
		}

		v = yajl_tree_get(node, disable_ltc_quotes, yajl_t_string);
		if (v == NULL) 
		{ 
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time LTC quotes are disabled.\n", disable_ltc_quotes[0], fileName);
			cgmConfig.ltcQuotesDisabled = 1;
		} else
		{
			cgmConfig.ltcQuotesDisabled = atoi(YAJL_GET_STRING(v));

			if (cgmConfig.ltcQuotesDisabled == 0)
			{
				v = yajl_tree_get(node, ltc_quote_url, yajl_t_string);
				if (v == NULL) 
				{ 
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time LTC quotes will be disabled.\n", ltc_quote_url[0], fileName);
					cgmConfig.ltcQuotesDisabled = 1;
				} else
				{
					strcpy_s(cgmConfig.ltcUrl, sizeof(cgmConfig.ltcUrl), YAJL_GET_STRING(v));

					v = yajl_tree_get(node, ltc_last_label, yajl_t_string);
					if (v == NULL) 
					{
						fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time LTC quotes will be disabled.\n", ltc_last_label[0], fileName);
						cgmConfig.ltcQuotesDisabled = 1;
					} else
					{
						strcpy_s(cgmConfig.ltcLastLabel, sizeof(cgmConfig.ltcLastLabel), YAJL_GET_STRING(v));

						v = yajl_tree_get(node, ltc_bid_label, yajl_t_string);
						if (v == NULL) 
						{
							fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time LTC quotes will be disabled.\n", ltc_bid_label[0], fileName);
							cgmConfig.ltcQuotesDisabled = 1;
						} else
						{
							strcpy_s(cgmConfig.ltcBidLabel, sizeof(cgmConfig.ltcBidLabel), YAJL_GET_STRING(v));

							v = yajl_tree_get(node, ltc_ask_label, yajl_t_string);
							if (v == NULL) 
							{
								fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Real-time LTC quotes will be disabled.\n", ltc_ask_label[0], fileName);
								cgmConfig.ltcQuotesDisabled = 1;
							} else
							{
								strcpy_s(cgmConfig.ltcAskLabel, sizeof(cgmConfig.ltcAskLabel), YAJL_GET_STRING(v));

								v = yajl_tree_get(node, ltc_quote_refresh_interval, yajl_t_string);
								if (v == NULL) 
								{
									cgmConfig.ltcQuotesRefreshInterval = MIN_LTC_REFRESH_INTERVAL;
									fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d minutes.\n", ltc_quote_refresh_interval[0], fileName, cgmConfig.btcQuotesRefreshInterval);
								} else
								{
									cgmConfig.ltcQuotesRefreshInterval = atoi(YAJL_GET_STRING(v));

									if (cgmConfig.ltcQuotesRefreshInterval < MIN_LTC_REFRESH_INTERVAL)
										cgmConfig.ltcQuotesRefreshInterval = MIN_LTC_REFRESH_INTERVAL;
								}								
							}
						}
					}
				}
			}			
		}

		v = yajl_tree_get(node, wdog_cutoff_temp_threshold, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogCutOffTemperatureThreshold = MIN_CUTOFF_TEMP;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d degrees Celcius.\n", wdog_cutoff_temp_threshold[0], fileName, cgmConfig.wdogCutOffTemperatureThreshold);
		} else
		{
			cgmConfig.wdogCutOffTemperatureThreshold = atoi(YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, wdog_cutoff_temp_cooldown, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogCutOffTemperatureCooldownPeriod = MIN_CUTOFF_TEMP_COOLDOWN;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Defaulting to %d [minutes].\n", wdog_cutoff_temp_cooldown[0], fileName, cgmConfig.wdogCutOffTemperatureCooldownPeriod);
		} else
		{
			cgmConfig.wdogCutOffTemperatureCooldownPeriod = atoi(YAJL_GET_STRING(v));
		}

		// -----------------------
		// H/W monitoring entries.
		// -----------------------
		v = yajl_tree_get(node, wdog_disable_hw_monitoring, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogAdlDisabled = 0;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. H/W monitoring will be enabled.\n", wdog_disable_hw_monitoring[0], fileName);
		} else
		{
			cgmConfig.wdogAdlDisabled = atoi(YAJL_GET_STRING(v));
		}

		v = yajl_tree_get(node, wdog_hw_monitoring_interval, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogAdlRefreshInterval = MIN_HW_POLLING_INTERVAL;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Mining H/W will be read every %d seconds\n", wdog_hw_monitoring_interval[0], fileName, cgmConfig.wdogAdlRefreshInterval);
		} else
		{
			cgmConfig.wdogAdlRefreshInterval = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogAdlRefreshInterval < MIN_HW_POLLING_INTERVAL)
			cgmConfig.wdogAdlRefreshInterval = MIN_HW_POLLING_INTERVAL;

		v = yajl_tree_get(node, wdog_gpu_utilization_threshold, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogAdlGpuActivityThreshold = MIN_HW_ACTIVITY_THRESHOLD;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Mining H/W activity percentage will be set to %d %%\n", wdog_gpu_utilization_threshold[0], fileName, cgmConfig.wdogAdlGpuActivityThreshold);
		} else
		{
			cgmConfig.wdogAdlGpuActivityThreshold = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogAdlGpuActivityThreshold < MIN_HW_ACTIVITY_THRESHOLD)
			cgmConfig.wdogAdlGpuActivityThreshold = MIN_HW_ACTIVITY_THRESHOLD;

		if (cgmConfig.wdogAdlGpuActivityThreshold > MAX_HW_ACTIVITY_THRESHOLD)
			cgmConfig.wdogAdlGpuActivityThreshold = MAX_HW_ACTIVITY_THRESHOLD;

		v = yajl_tree_get(node, wdog_gpu_utilization_timeout, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.wdogAdlGpuActivityTimeout = MIN_HW_ACTIVITY_TIMEOUT;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Mining H/W activity timeout % [seconds]\n", wdog_gpu_utilization_timeout[0], fileName, cgmConfig.wdogAdlGpuActivityTimeout);
		} else
		{
			cgmConfig.wdogAdlGpuActivityTimeout = atoi(YAJL_GET_STRING(v));
		}

		if (cgmConfig.wdogAdlGpuActivityTimeout < MIN_HW_ACTIVITY_TIMEOUT)
			cgmConfig.wdogAdlGpuActivityTimeout = MIN_HW_ACTIVITY_TIMEOUT;

		if ( cgmConfig.wdogAdlRefreshInterval+20 > cgmConfig.wdogAdlGpuActivityTimeout)
		{
			cgmConfig.wdogAdlGpuActivityTimeout = cgmConfig.wdogAdlRefreshInterval+20;
		}

		// -----------------------
		// Smart metering entries.
		// -----------------------
		cgmConfig.smartMeterOnPeak = 0;
		v = yajl_tree_get(node, smart_metering_disable, yajl_t_string);
		if (v == NULL) 
		{
			cgmConfig.smartMeterDisabled = 1;
			fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Smart metering will be disabled.\n", smart_metering_disable[0], fileName);
		} else
		{
			cgmConfig.smartMeterDisabled = atoi(YAJL_GET_STRING(v));
			if (cgmConfig.smartMeterDisabled != 0 && cgmConfig.smartMeterDisabled != 1)
			{
				cgmConfig.smartMeterDisabled = 1;
				fprintf(stderr, "\nparseConfigFile(): \'%s\' field can only be 0 or 1. Smart metering will be disabled.\n", smart_metering_disable[0]);
				goto the_end;
			}

			if (cgmConfig.smartMeterDisabled == 0)
			{
				v = yajl_tree_get(node, smart_metering_on_peak_start_time, yajl_t_string);
				if (v == NULL) 
				{
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Smart metering will be disabled.\n", smart_metering_on_peak_start_time[0], fileName);
					goto the_end;
				} else
				{
					char * p = YAJL_GET_STRING(v);
					int len = strlen(p);

					if (len > 5)
					{
						cgmConfig.smartMeterDisabled = 1;
						fprintf(stderr, "\nparseConfigFile(): \'%s\' field has to be in the following format: HH:MM , smart metering will be disabled.\n", smart_metering_on_peak_start_time[0]);				
						goto the_end;
					} else
					{
						strcpy_s(cgmConfig.smartMeterOnPeakStartTime, sizeof(cgmConfig.smartMeterOnPeakStartTime), p);
					}			
				}

				v = yajl_tree_get(node, smart_metering_polling_interval, yajl_t_string);
				if (v == NULL) 
				{
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. smart metering polling interval will be set to 1\n", smart_metering_polling_interval[0], fileName);
					cgmConfig.smartMeterPollingInterval = 1;
				} else
				{
					cgmConfig.smartMeterPollingInterval = atoi(YAJL_GET_STRING(v));

					if (cgmConfig.smartMeterPollingInterval < 1)
					{
						cgmConfig.smartMeterPollingInterval = 1;
						fprintf(stderr, "\nparseConfigFile(): \'%s\' field has to be greater than 1; smart metering polling interval will be set to 1\n", smart_metering_polling_interval[0]);				
					}
				}

				v = yajl_tree_get(node, smart_metering_off_peak_start_time, yajl_t_string);
				if (v == NULL) 
				{
					cgmConfig.smartMeterDisabled = 1;
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. Smart metering will be disabled.\n", smart_metering_off_peak_start_time[0], fileName);
				} else
				{
					char * p = YAJL_GET_STRING(v);
					int len = strlen(p);

					if (len > 5)
					{
						cgmConfig.smartMeterDisabled = 1;
						fprintf(stderr, "\nparseConfigFile(): \'%s\' field has to be in the following format: HH:MM , smart metering will be disabled.\n", smart_metering_off_peak_start_time[0]);				
						goto the_end;
					} else
					{
						strcpy_s(cgmConfig.smartMeterOffPeakStartTime, sizeof(cgmConfig.smartMeterOffPeakStartTime), p);
					}			
				}

				v = yajl_tree_get(node, smart_metering_on_peak_shutdown, yajl_t_string);
				if (v == NULL) 
				{
					cgmConfig.smartMeterOnPeakShutdown = 0;
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. System will not be shutdown during on-peak period.\n", smart_metering_on_peak_shutdown[0], fileName);
				} else
				{
					cgmConfig.smartMeterOnPeakShutdown = atoi(YAJL_GET_STRING(v));
					if (cgmConfig.smartMeterOnPeakShutdown != 0 && cgmConfig.smartMeterOnPeakShutdown != 1)
					{
						cgmConfig.smartMeterOnPeakShutdown = 0;
						fprintf(stderr, "\nparseConfigFile(): \'%s\' field can only be 0 or 1. System will not be shutdown during on-peak period.\n", smart_metering_on_peak_shutdown[0]);
					}
				}


				v = yajl_tree_get(node, smart_metering_on_peak_disable_gpus, yajl_t_string);
				if (v == NULL) 
				{
					cgmConfig.smartMeterOnPeakDisableGPUs = 1;
					fprintf(stderr, "\nparseConfigFile(): unable to find: \'%s\' field in the config file: \'%s\'. GPUs will be disabled during on-peak period.\n", smart_metering_on_peak_disable_gpus[0], fileName);
				} else
				{
					cgmConfig.smartMeterOnPeakDisableGPUs = atoi(YAJL_GET_STRING(v));
					if (cgmConfig.smartMeterOnPeakDisableGPUs != 0 && cgmConfig.smartMeterOnPeakDisableGPUs != 1)
					{
						cgmConfig.smartMeterOnPeakDisableGPUs = 1;
						fprintf(stderr, "\nparseConfigFile(): \'%s\' field can only be 0 or 1. GPUs will not be disabled during on-peak period.\n", smart_metering_on_peak_disable_gpus[0]);
					}
				}
			}
		}

the_end:

		if (cgmConfig.smartMeterDisabled == 1)
		{
			strcpy(cgmConfig.smartMeterOffPeakStartTime, "00:00");
			strcpy(cgmConfig.smartMeterOnPeakStartTime, "00:00");
			cgmConfig.smartMeterOnPeakShutdown = 0;
			cgmConfig.smartMeterOnPeakDisableGPUs = 0;
		}

		yajl_tree_free(node);
	}

	return &cgmConfig;
} // end of parseConfigFile()
