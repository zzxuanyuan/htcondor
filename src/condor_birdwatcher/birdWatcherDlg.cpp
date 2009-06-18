/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


// BirdWatcherDlg.cpp : implementation file
//

#include "stdafx.h"
#include "birdwatcher.h"
#include "BirdWatcherDlg.h"
#include "systrayminimize.h"

#ifdef _DEBUG
//#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
RECT rc, rcDlg, rcOwner;
/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg dialog

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		OutputDebugString(L"Message received");
/*		parentHwnd = GetDesktopWindow();
		GetWindowRect(parentHwnd, &rcOwner);
		GetWindowRect(hwndDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
*/
		//SetWindowPos(hwndDlg, HWND_TOP, rcOwner.left + (rc.right/2), rcOwner.top + (rc.bottom/2), 0, 0, SWP_NOSIZE);
		//ShowWindow(hwndDlg, SW_SHOW);
		SetTimer(birdwatcherDLG, 1000, 1000, NULL);
		
		return true;

	case WM_TIMER:
		OnTimer(WM_TIMER);

		return true;
		break;
	case WM_CLOSE:
		MinimizeWndToTray(hwndDlg);

		return true;
		break;
	case WM_COMMAND:
		MinimizeWndToTray(hwndDlg);

		return true;
		break;
	default:
		return false;
	}
}

/*
BEGIN_MESSAGE_MAP(CBirdWatcherDlg, CDialog)
	//{{AFX_MSG_MAP(CBirdWatcherDlg)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
*/
/////////////////////////////////////////////////////////////////////////////
// CBirdWatcherDlg message handlers
/*
void CBirdWatcherDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	
}
*/
void OnTimer(UINT nIDEvent) 
{
	if (!IsWindowVisible(birdwatcherDLG))
		return;

	LARGE_INTEGER fileSize;
	DWORD dwBytesRead;

	

	HANDLE hBirdq = CreateFile(*zCondorDir + L"birdq.tmp", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(!(hBirdq == INVALID_HANDLE_VALUE))
	{	
		GetFileSizeEx(hBirdq, &fileSize);
		if(fileSize.HighPart == 0)
		{
			char *psBuf = new char[fileSize.LowPart+1];
			ReadFile(hBirdq, psBuf, fileSize.LowPart-1, &dwBytesRead, NULL);
			psBuf[fileSize.LowPart] = 0;
			
			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_TOP_PANE, psBuf);
			delete [] psBuf;
			CloseHandle(hBirdq);
		}
	}

	HANDLE hBirdst = CreateFile(*zCondorDir + L"birdstatus.tmp", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(!(hBirdst == INVALID_HANDLE_VALUE))
	{		
		GetFileSizeEx(hBirdst, &fileSize);
		if(fileSize.HighPart == 0)
		{
			char *psBuf = new char[fileSize.LowPart+1];
			ReadFile(hBirdst, psBuf, fileSize.LowPart-1, &dwBytesRead, NULL);
			psBuf[fileSize.LowPart] = 0;
			
			SetDlgItemTextA(birdwatcherDLG, IDC_EDIT_TOP_PANE, psBuf);
			delete [] psBuf;
			CloseHandle(hBirdst);
		}
	}

	STARTUPINFO si[2];
	PROCESS_INFORMATION pi[2];

	ZeroMemory(&si[0], sizeof(si[0]));
	ZeroMemory(&si[1], sizeof(si[1]));

	ZeroMemory(&pi[0], sizeof(pi[0]));
	ZeroMemory(&pi[1], sizeof(pi[1]));

	hBirdq = CreateFile(*zCondorDir + L"birdq.tmp", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	hBirdst = CreateFile(*zCondorDir + L"birdstatus.tmp", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	
	
	si[0].dwFlags = STARTF_USESTDHANDLES;
	si[0].hStdOutput = hBirdq;
	si[0].cb = sizeof(si[0]);

	si[1].dwFlags = STARTF_USESTDHANDLES;
	si[1].hStdOutput = hBirdst;
	si[1].cb = sizeof(si[1]);
	
	WCHAR condorq[] = L"bin\\condor_q.exe";
	WCHAR condorst[] = L"bin\\condor_status.exe";

	if(!(hBirdq == INVALID_HANDLE_VALUE))
	{
		if(CreateProcess(condorq, NULL, NULL, NULL, true, CREATE_NO_WINDOW, NULL, zCondorDir, &si[0], &pi[0]))
			if(WaitForSingleObject(pi[0].hProcess, 2000) == WAIT_TIMEOUT)
				TerminateProcess(pi[0].hProcess, 1);
			
		CloseHandle(pi[0].hProcess);
		CloseHandle(pi[0].hThread);
		CloseHandle(hBirdq);
	}

	

	if(!(hBirdst == INVALID_HANDLE_VALUE))
	{
		if(CreateProcess(condorst, NULL, NULL, NULL, true, CREATE_NO_WINDOW, NULL, zCondorDir, &si[1], &pi[1]))
			if(WaitForSingleObject(pi[0].hProcess, 2000) == WAIT_TIMEOUT)
				TerminateProcess(pi[1].hProcess, 1);

		CloseHandle(pi[1].hProcess);
		CloseHandle(pi[1].hThread);
		CloseHandle(hBirdst);
	}
}
/*
BOOL CBirdWatcherDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	SetIcon(LoadIcon(NULL, MAKEINTRESOURCE(IDR_MAINFRAME)), TRUE);
	
	char cLast = zCondorDir[zCondorDir.GetLength()-1];
	if (cLast != '\\' && cLast != '/')
		zCondorDir += '\\';
		
	SetTimer(1000, 1000, NULL);	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
*/