// --------------------------------------------------------------------------
// Copyright © 2012 Peter Moss (peter@petermoss.com)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.  See GNULicenseV3.txt for more details.
// --------------------------------------------------------------------------
#include "msprocess.h"
#include "log.h"

// ntdll.dll call to read PEB block of cgminer.exe
typedef NTSTATUS (NTAPI *pfnNtQueryInformationProcess)(
    IN  HANDLE ProcessHandle,
    IN  PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN  ULONG ProcessInformationLength,
    OUT PULONG ReturnLength    OPTIONAL
    );

int isThisProcessID( DWORD processID, const char * name )
{
	CHAR szProcessName[MAX_PATH];
	BOOL rc = 0;
	DWORD len = MAX_PATH;
	char * p = 0;

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );
	
	memset(szProcessName, 0, sizeof(szProcessName));

	if (hProcess != NULL)
	{
		// Get the process name.
		rc = GetProcessImageFileNameA(hProcess, szProcessName, len);

		if (rc > 0)
		{
			//printf( "%s  (PID: %u)\n", szProcessName, processID );

			p = strstr(szProcessName, name);

			if (p != NULL)
			{
				//printf("p: %s\n", p);

				if( _stricmp(p, name) == 0)
				{
					CloseHandle( hProcess );
					return 1;
				}
			}
		}
	}

    // Release the handle to the process.
    CloseHandle( hProcess );

	return 0;
}

int numberOfAkbashProcesses(char * name)
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;
	int akbashCount = 0;

	memset(&aProcesses, 0, sizeof(aProcesses));

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return 0;
    }

    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.

    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            if (isThisProcessID( aProcesses[i], name))
			{
				akbashCount++;
			}
        }
    }

	return akbashCount;
}

int getProcessID(const char * name)
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

	memset(&aProcesses, 0, sizeof(aProcesses));

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return 0;
    }

    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.

    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            if (isThisProcessID( aProcesses[i], name ))
				return aProcesses[i];
        }
    }

	return 0;
}



void getProcessInfo(const char * name, cgmProcessInfo * procInfo)
{
	NTSTATUS rc = 0;
	DWORD len = MAX_PATH;
	ULONG retLen = 0;

	char * p= 0;
	char * start=0;

//	PROCESS_BASIC_INFORMATION pbInfo;
//	PEB pebInfo;
//	pfnNtQueryInformationProcess gNtQueryInformationProcess;
//	HMODULE hNtDll;
//	RTL_USER_PROCESS_PARAMETERS rtl;
	//UNICODE_STRING commandLine;
	//UNICODE_STRING imagePath;

	//WCHAR *commandLineContents;
	//WCHAR *imagePathContents;

	PROCESS_MEMORY_COUNTERS memoryCounters;
	HANDLE hProcess = NULL;

	DWORD werFault = 0;

	DWORD processID = 0;

	if (procInfo == NULL)
	{
		debug_log(LOG_ERR, "getProcessInfo(): Invalid procInfo handle.");
		return;
	}

	memset(procInfo, 0, sizeof(cgmProcessInfo));

	werFault = getProcessID("WerFault.exe");
	processID = getProcessID(name);

	procInfo->processID = processID;
	procInfo->werFaultID = werFault;

	if (procInfo->processID == 0)
	{
		debug_log(LOG_SVR, "getProcessInfo(): unable to find process: %s, processId: %d, werFault: %d", name, procInfo->processID, procInfo->werFaultID);
		return;
	}

    //hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );
	hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, processID );

	
	if (hProcess != NULL)
	{
		strcpy_s(procInfo->szProcessName, sizeof(procInfo->szProcessName), name);

		// -----------------
		// Get Handle Count.
		// -----------------
		procInfo->handleCount = 0;
		rc = GetProcessHandleCount(hProcess, &procInfo->handleCount);
		if (rc == 0)
		{
			debug_log(LOG_ERR, "getProcessInfo(): unable to retrieve handle count for process: \'%s\', rc: %d", name, GetLastError());
			procInfo->handleCount = 0;
			CloseHandle(hProcess);
			return;
		}

		// --------------------------------
		// Get the size of the Working Set.
		// --------------------------------
		procInfo->workingSetSize = 0;
		memset(&memoryCounters, 0, sizeof(memoryCounters));
		rc = GetProcessMemoryInfo(hProcess, &memoryCounters, sizeof(memoryCounters));
		if (rc == 0)
		{
			debug_log(LOG_ERR, "getProcessInfo(): unable to retrieve the size of the working set for process: \'%s\', rc: %d", name, GetLastError());
			procInfo->workingSetSize = 0;
			CloseHandle(hProcess);
			return;
		}

		procInfo->workingSetSize = memoryCounters.WorkingSetSize;
/*
		hNtDll = LoadLibrary(_T("ntdll.dll"));
		if (hNtDll == NULL) 
		{
			debug_log("getProcessInfo(): Unable to load ntdll.dll");
			CloseHandle(hProcess);
			return;
		}

		gNtQueryInformationProcess = (pfnNtQueryInformationProcess) GetProcAddress(hNtDll, "NtQueryInformationProcess");

		if(gNtQueryInformationProcess == NULL) 
		{
			debug_log("getProcessInfo(): NtQueryInformationProcess() call failed.");
			FreeLibrary(hNtDll);
			CloseHandle(hProcess);
			return ;
		}

		rc = gNtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbInfo, sizeof(pbInfo), &retLen);
		if(pbInfo.PebBaseAddress == NULL)
		{
			debug_log("getProcessInfo(): PEB address is null.");
			FreeLibrary(hNtDll);
			CloseHandle(hProcess);
			return;
		}

		// ------------------------------------------------------------------------------
		// We need to read PEB of the process to find out the command line and full path.
		// ------------------------------------------------------------------------------
		if(pbInfo.PebBaseAddress)
		{
			if(ReadProcessMemory(hProcess, pbInfo.PebBaseAddress, &pebInfo, sizeof(pebInfo) , NULL))
			{
				if(ReadProcessMemory(hProcess, pebInfo.ProcessParameters, &rtl, sizeof(rtl) , NULL))
				{
					rc = ReadProcessMemory(hProcess, &rtl.CommandLine, &commandLine, sizeof(commandLine), NULL);
				
					if(rc != 0 || (rc == 0 && GetLastError() == ERROR_PARTIAL_COPY))
					{
						// allocate memory to hold the command line 
						commandLineContents = (WCHAR *)malloc(rtl.CommandLine.Length);

						// read the command line 
						if (!ReadProcessMemory(hProcess, rtl.CommandLine.Buffer,
							commandLineContents, rtl.CommandLine.Length, NULL))
						{
							debug_log("getProcessInfo(): Could not read the command line string!");
							free(commandLineContents);
							FreeLibrary(hNtDll);
							CloseHandle(hProcess);
							return ;
						}

						if (rtl.CommandLine.Length < sizeof(procInfo->szCmd))
							wcstombs(procInfo->szCmd, commandLineContents, rtl.CommandLine.Length);

						free(commandLineContents);
					} else
					{
						debug_log("getProcessInfo(): Failed to read PEBs command line, rc: %d", GetLastError());
					}

					rc = ReadProcessMemory(hProcess, &rtl.ImagePathName, &imagePath, sizeof(imagePath), NULL);
					if(rc != 0 || (rc == 0 && GetLastError() == ERROR_PARTIAL_COPY))
					{
						// allocate memory to hold the image path 
						imagePathContents = (WCHAR *)malloc(rtl.ImagePathName.Length);

						// read the path 
						if (!ReadProcessMemory(hProcess, rtl.ImagePathName.Buffer,
							imagePathContents, rtl.ImagePathName.Length, NULL))
						{
							debug_log("getProcessInfo(): Could not read the image path line string!");
							free(imagePathContents);
							FreeLibrary(hNtDll);
							CloseHandle(hProcess);
							return ;
						}

						if (rtl.ImagePathName.Length < sizeof(procInfo->szImage))
							wcstombs(procInfo->szImage, imagePathContents, rtl.ImagePathName.Length);

						free(imagePathContents);

						start = procInfo->szImage;
						p = strstr(procInfo->szImage, name);
						if (p != NULL)
							strncpy(procInfo->szCWD, start, p-start);
					} else
					{
						debug_log("getProcessInfo(): Failed to read PEBs image path line, rc: %d", GetLastError());
					}
				}
			}
		}

		FreeLibrary(hNtDll);		
	*/

		CloseHandle(hProcess);
	} else
	{
		debug_log(LOG_ERR, "getProcessInfo(): unable to open process: \'%s\', rc: %d", name, GetLastError());
		return;
	}
}


void killProcessByID(DWORD processID)
{
	int rc = -89;

	if (processID != 0)
	{		
		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, processID );

		if (hProcess != NULL)
		{
			rc = TerminateProcess(hProcess, 0);
			debug_log(LOG_SVR, "killProcessByID(%d): process handle: %d, TeminateProcess() returned rc: %d", processID, hProcess, rc);
			CloseHandle( hProcess );
		}
	}
} // end of killProcessByID()

void killAllWERProcesses(void)
{
	DWORD werFault = getProcessID("WerFault.exe");

	while (werFault != 0)
	{		
		killProcessByID(werFault);
		Sleep(1000);
		werFault = getProcessID("WerFault.exe");
	}
} // end of killAllWERProcesses()

/*
void restartMinerProcess(cgmProcessInfo * procInfo)
{
	PROCESS_INFORMATION processInformation;
    STARTUPINFOA startupInfo;
    
	memset(&processInformation, 0, sizeof(processInformation));
    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

	if (procInfo == NULL)
	{
		debug_log("restartMiner(): procInfo is NULL.");
		return;
	}

	if (procInfo->processID < 1)
	{
		debug_log("restartMiner(): proccessID: %d does not seem to be valid.", procInfo->processID);
		return;
	}

	//killProcessByID(procInfo->processID);

	if (CreateProcessA(NULL, procInfo->szCmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, procInfo->szCWD, &startupInfo, &processInformation))	
	{
		procInfo->processID = processInformation.dwProcessId;

	} else
	{
		debug_log("restartMiner(): failed to create: \'%s\' process, rc: %d", procInfo->szProcessName, GetLastError() );
	}
	return;
}
*/