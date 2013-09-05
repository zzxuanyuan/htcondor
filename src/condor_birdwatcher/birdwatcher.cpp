// condor_birdwatcher.cpp : Defines the entry point for the application.
//
#pragma once
#include "stdafx.h"
#include "birdwatcher.h"
#define MAX_LOADSTRING 100
HINSTANCE hInst;
// Global Variables:
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
WCHAR sharedName[MAX_PATH] = { 0 };
static bool sharedMenNamePresent = false;

#define SHARED_SIZE sizeof(LONG)

static HANDLE hMapFile = NULL;
static volatile PLONG updateTime = NULL;

// Forward declarations of functions included in this code module:
VOID CALLBACK KeyboardMouseCallback(
    _In_  HWND hwnd,
    _In_  UINT uMsg,
    _In_  UINT_PTR idEvent,
    _In_  DWORD dwTime);

bool GetSharedMemName()
{
    LSTATUS errCode;
    DWORD keyValueSize = MAX_PATH;
    HKEY condorKey;

    errCode = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\condor", 0, KEY_READ, &condorKey);
    if(errCode) return false;

    errCode = RegQueryValueExW(condorKey, L"birdwatcher_shared", NULL, NULL, (LPBYTE)sharedName, &keyValueSize);
    RegCloseKey(condorKey);

    if(errCode) return false;

    return true;
}

int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR    lpCmdLine,
                     int       nCmdShow)
{
    UINT_PTR timerID;
	MSG msg;
    
	hInst = hInstance;

	SystrayManager sysman;
	HICON hFlying2 = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_FLYING2));
	HICON hClaimed = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_CLAIMED));

	sysman.init(LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_OFF)),
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_IDLE)),
				hClaimed,
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONDOR_FLYING1)),
				hFlying2,
				hFlying2,
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PREEMPTING))
				);

    sharedMenNamePresent = GetSharedMemName();
    timerID = SetTimer(
        NULL,
        0,
        5000,
        KeyboardMouseCallback);
	
	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(birdwatcherDLG == 0 || !IsDialogMessage(birdwatcherDLG, &msg))
		{
            switch(msg.message)
            {
                case WM_DESTROY:
                    KillTimer(NULL, timerID);
                    if(updateTime) UnmapViewOfFile(updateTime);
                    if(hMapFile) CloseHandle(hMapFile);
                    break;
            }

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}

VOID CALLBACK KeyboardMouseCallback(
    _In_  HWND hwnd,
    _In_  UINT uMsg,
    _In_  UINT_PTR idEvent,
    _In_  DWORD dwTime)
{
    LASTINPUTINFO lii;
    CURSORINFO cursorInf;
    static DWORD previousInputTick = 0;
    static POINT previousPos = { 0, 0 };

    lii.cbSize = sizeof(LASTINPUTINFO);
	lii.dwTime = 0;

    if(!sharedMenNamePresent) return;

    if(!hMapFile)
    {
        hMapFile = OpenFileMappingW(FILE_MAP_WRITE, FALSE, sharedName);
        if(!hMapFile) return;

        updateTime = (PLONG)MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, SHARED_SIZE);
        if(!updateTime) return;
    }

    if(GetLastInputInfo(&lii))
    {
        if(lii.dwTime > previousInputTick)
        {
            previousInputTick = lii.dwTime;
            InterlockedExchange(updateTime, previousInputTick);
            return;
        }
    }

    cursorInf.cbSize = sizeof(CURSORINFO);

    if(GetCursorInfo(&cursorInf))
    {
        if ((cursorInf.ptScreenPos.x != previousPos.x) || 
            (cursorInf.ptScreenPos.y != previousPos.y))
        {
            // the mouse has moved!
            // stash new position
            previousPos.x = cursorInf.ptScreenPos.x; 
            previousPos.y = cursorInf.ptScreenPos.y;
            previousInputTick = GetTickCount();
            InterlockedExchange(updateTime, previousInputTick);
        }
    }
}