// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#ifndef CGM_CONFIG_H
#define CGM_CONFIG_H
#include <Windows.h>
#include "smtp.h"

#define MAX_URL_LEN				 1024

// -------------------------
// Watchdog related entries.
// -------------------------
#define LOG_FILE_LABEL                     "wdog-logFile"
#define LOG_FILE_SIZE                      "wdog-log-file-size"
#define WDOG_LOG_LEVEL                     "wdog-log-level"
#define LISTEN_IP                          "wdog-listen-ip"
#define LISTEN_PORT                        "wdog-listen-port"
#define DISABLE_REMOTE_API                 "wdog-disable-remote-api" 
#define DISABLE_REMOTE_HELP                "wdog-disable-remote-help"
#define DISABLE_REMOTE_RESTART             "wdog-disable-remote-restart"
#define DISABLE_REMOTE_REBOOT              "wdog-disable-remote-reboot"
#define DISABLE_REMOTE_GETLOG              "wdog-disable-remote-getlog"
#define DISABLE_REMOTE_STATUS              "wdog-disable-remote-status"
#define DISABLE_NOTIFICATION_EMAILS        "wdog-disable-notification-emails"
#define DISABLE_STATUS_NOTIFICATION_EMAILS "wdog-disable-status-notifications"
#define NOTIFICATIONS_EMAIL                "wdog-notifications-email-address"
#define STATUS_NOTIFICATION_FREQ           "wdog-status-notification-frequency"
#define CUT_OFF_TEMPERATURE                "wdog-cutoff-temperature"
#define CUT_OFF_TEMPERATURE_COOLDOWN       "wdog-cutoff-temperature-cooldown-period"
#define DISABLE_ADL_READINGS               "wdog-disable-hw-monitoring"
#define ADL_HW_POLLING		               "wdog-hw-monitoring-interval"
#define ADL_HW_ACTIVITY_THRESHOLD          "wdog-gpu-utilization-threshold"
#define ADL_HW_ACTIVITY_TIMEOUT            "wdog-gpu-utilization-timeout"
#define WDOG_RIG_NAME                      "wdog-rig-name"

// ----------------------
// Miner related entries.
// ----------------------
#define CGMINER_EXE_LABEL              "miner-exe-full-path"
#define CGMINER_NAME_LABEL             "miner-exe-name"
#define MINER_INIT_INTERVAL            "miner-init-interval"
#define TOTAL_AVG_RATE                 "miner-gpu-avg-rate-threshold"
#define PGA_AVG_RATE                   "miner-pga-avg-rate-threshold"
#define MINER_WORKING_SET_THRESHOLD    "miner-working-set-threshold" 
#define MINER_HANDLE_COUNT_THRESHOLD   "miner-handle-count-threshold" 
#define MINER_API_POLL_INTERVAL        "miner-api-poll-interval" 
#define NUMBER_OF_RESTARTS             "miner-number-of-restarts"
#define ALIVE_TIMEOUT                  "miner-alive-timeout"
#define NOT_CONNECTED_TIMEOUT          "miner-not-connected-timeout"
#define MINER_HW_ERRORS_THRESHOLD      "miner-hw-errors-threshold"
#define MINER_LISTEN_IP                "miner-listen-ip"
#define MINER_LISTEN_PORT              "miner-listen-port"
#define MINER_SOLO_MINING              "miner-solo-mining"
#define MINER_BLOCK_FOUND_NOTIFICATION "miner-notify-when-block-found"

// ---------------------
// Pool related entries.
// ---------------------
#define DISABLE_POOL_INFO        "pool-disable-info"
#define POOL_REFRESH_INTERVAL    "pool-refresh-interval"
#define POOL_API_URL             "pool-api-url"
#define POOL_BALANCE_LABEL       "pool-balance-label"
#define POOL_HASH_RATE_LABEL     "pool-hash-rate-label"
#define POOL_VALIDS_LABEL        "pool-valids-label"
#define POOL_STALES_LABEL        "pool-stales-label"
#define POOL_INVALIDS_LABEL      "pool-invalids-label"

// --------------------
// BTC related entries.
// --------------------
#define DISABLE_BTC_QUOTES       "btc-disable-quotes"
#define QUOTE_REFRESH_INTERVAL   "btc-refresh-interval"
#define QUOTE_URL                "btc-quote-url"
#define QUOTE_LAST_LABEL         "btc-quote-last-label"
#define QUOTE_BID_LABEL          "btc-quote-bid-label"
#define QUOTE_ASK_LABEL          "btc-quote-ask-label"

// --------------------
// LTC related entries.
// --------------------
#define DISABLE_LTC_QUOTES          "ltc-disable-quotes"
#define LTC_QUOTE_REFRESH_INTERVAL  "ltc-refresh-interval"
#define LTC_QUOTE_URL               "ltc-quote-url"
#define LTC_QUOTE_LAST_LABEL        "ltc-quote-last-label"
#define LTC_QUOTE_BID_LABEL         "ltc-quote-bid-label"
#define LTC_QUOTE_ASK_LABEL         "ltc-quote-ask-label"


// -----------------------
// Smart Metering entries.
// -----------------------
#define SMART_METERING_DISABLE              "smart-metering-disable"
#define SMART_METERING_POLLING_INTERVAL     "smart-metering-polling-interval" 
#define SMART_METERING_ON_PEAK_START_TIME   "smart-metering-on-peak-start-time"
#define SMART_METERING_OFF_PEAK_START_TIME  "smart-metering-off-peak-start-time"
#define SMART_METERING_ON_PEAK_SHUTDOWN     "smart-metering-on-peak-shutdown"
#define SMART_METERING_ON_PEAK_DISABLE_GPUS "smart-metering-on-peak-disable-gpus"

typedef struct _cgmConfig
{
	char   wdogExeName[MAX_PATH+1];
	char   wdogLogFileName[MAX_PATH+1];
	long   wdogLogFileSize;
	int    wdogLogLevel;
	char   wdogListenIP[32];
    int    wdogListenPort;
	char   wdogRigName[MAX_PATH+1];
	int    wdogNumberOfRestarts;
	int    wdogAliveTimeout;
	int    wdogNotConnectedTimeout;
	int    wdogDisableRemoteApi;
	int    wdogDisableRemoteHelp;
	int    wdogDisableRemoteRestart;
	int    wdogDisableRemoteReboot;
	int    wdogDisableRemoteGetLog;
	int    wdogDisableRemoteStatus;
	char   wdogEmailAddress[MAX_PATH+1];
	int    wdogDisableNotifications;
	int    wdogDisableStatusNotifications;
	int    wdogStatusNotificationFrequency;
	int    wdogCutOffTemperatureThreshold;
	int    wdogCutOffTemperatureCooldownPeriod;
    int    wdogAdlDisabled;
	int    wdogAdlRefreshInterval;
	int    wdogAdlGpuActivityThreshold;
	int    wdogAdlGpuActivityTimeout;

	char   minerExeFullPath[MAX_PATH+1];
    char   minerExeName[MAX_PATH+1];
	int    minerInitInterval;
    double minerGPUAvgRateThreshold;
	double minerPGAAvgRateThreshold;
	unsigned long long minerWorkingSetThreshold;
	unsigned long long minerHandleCountThreshold;
    int    minerPollInterval;
	unsigned long long minerHWErrorsThreshold;
	int    minerSoloMining;
	int    minerNotifyWhenBlockFound;

	char   minerListenIP[32];
    int    minerListenPort;

	int    poolInfoDisabled;
	int    poolRefreshInterval;
	char   poolApiUrl[MAX_URL_LEN+1];
	char   poolBalanceLabel[MAX_PATH+1];
	char   poolHashRateLabel[MAX_PATH+1];
	char   poolValidsLabel[MAX_PATH+1];
	char   poolStalesLabel[MAX_PATH+1];
	char   poolInvalidsLabel[MAX_PATH+1];
	char   poolHost[SMTP_MAX_HOST_NAME_LEN+1];
	char   poolUri[SMTP_MAX_TEXT_LINE_LEN+1];
	int    poolPort;

	int    btcQuotesDisabled;
	int    btcQuotesRefreshInterval;
	char   btcUrl[MAX_URL_LEN+1];
	char   btcLastLabel[MAX_PATH+1];
	char   btcBidLabel[MAX_PATH+1];
	char   btcAskLabel[MAX_PATH+1];

	int    ltcQuotesDisabled;
	int    ltcQuotesRefreshInterval;
	char   ltcUrl[MAX_URL_LEN+1];
	char   ltcLastLabel[MAX_PATH+1];
	char   ltcBidLabel[MAX_PATH+1];
	char   ltcAskLabel[MAX_PATH+1];

	int    smartMeterDisabled;
	int    smartMeterPollingInterval;
	char   smartMeterOnPeakStartTime[6];
	char   smartMeterOffPeakStartTime[6];
	int    smartMeterOnPeakShutdown;
	int    smartMeterOnPeakDisableGPUs;
	int    smartMeterOnPeak;
} CGMConfig;

CGMConfig * parseConfigFile(const char * fileName);
CGMConfig * getCGMConfig();
#endif
