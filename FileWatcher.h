#pragma once

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <filesystem>
#include <chrono>
#include <atlstr.h>

namespace fs = std::filesystem;

HANDLE hFileChangeNotify;
HANDLE hWaitCallback;

bool bIsFileWatcher = false;
bool bIsFileChanged = false;

fs::path m_fwfileName;
fs::file_time_type m_fwfileLastWrite;

void FileWatcherStart();
void FileWatcherStop();
void FWGetFileLastWrite();
bool FWCheckFileUpdated();
void FWWaitForFileChanges();

template <typename TP>
std::time_t to_time_t(TP tp)
{
	using namespace std::chrono;
	auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
														+ system_clock::now());
	return system_clock::to_time_t(sctp);
}

void CALLBACK WaitOrTimerCallback(
	_In_  PVOID lpParameter,
	_In_  BOOLEAN TimerOrWaitFired)
{
	static bool bIsNotifying;
	if (!bIsNotifying) {
		bIsNotifying = true;
		if (FWCheckFileUpdated()) 
		{
			FindNextChangeNotification(hFileChangeNotify);
		}
		else
		{
			//File deleted, stop watcher
			FileWatcherStop();
		}
		bIsNotifying = false;
	}
}

void FileWatcherStart(std::wstring inFileName)
{
	std::wstring inFilePath = inFileName.substr(0, inFileName.find_last_of(L"\\"));
	hFileChangeNotify = FindFirstChangeNotification(
		inFilePath.c_str(),
		FALSE, 
		FILE_NOTIFY_CHANGE_LAST_WRITE);

	if (hFileChangeNotify == INVALID_HANDLE_VALUE || hFileChangeNotify == NULL)
	{
		ExitProcess(GetLastError());
	}

	m_fwfileName = inFileName;
	bIsFileWatcher = true;
	FWGetFileLastWrite();
	FWWaitForFileChanges();
}

void FWGetFileLastWrite()
{
	m_fwfileLastWrite = fs::last_write_time(m_fwfileName);
}

bool FWCheckFileUpdated()
{
	if (fs::exists(m_fwfileName)) 
	{
		if (m_fwfileLastWrite != fs::last_write_time(m_fwfileName))
		{
			m_fwfileLastWrite = fs::last_write_time(m_fwfileName);
			bIsFileChanged = true;
		}
		return true;
	}
	return false;
}

void FWWaitForFileChanges()
{
	RegisterWaitForSingleObject(&hWaitCallback, hFileChangeNotify, &WaitOrTimerCallback, NULL, INFINITE, WT_EXECUTELONGFUNCTION);
}

void FileWatcherStop()
{
	UnregisterWait(hWaitCallback);
	FindCloseChangeNotification(hFileChangeNotify);
	bIsFileWatcher = false;
}
