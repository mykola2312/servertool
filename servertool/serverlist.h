#ifndef __SERVERLIST_H
#define __SERVERLIST_H

#include "servertool.h"

#define SERVERLIST_CLASS L"ServerTool_ServerList"

#define IDC_REQUEST_BTN 128
#define IDC_FILE_SAVEAS 129
#define IDC_SERVERS_MASTERS 130
#define IDC_SERVERS_QUERY 131
#define IDC_HELP_HELP 132
#define IDC_HELP_ABOUT 133

typedef struct {
	LPCWSTR lpClass;
	HWND hWnd;
	HWND hProgressBar;
	HWND hRequestEdit;
	HWND hRequestBtn;
	HWND hLabel;
	HWND hServerList;
	
	INT ServerIdx;

	MASTER_T Masters[32];
	INT MasterNum;
	

	MASTERREQUESTPARAMETERS_T Params;
	HANDLE hScanThread;
} SERVERLIST_WINDOW_T;

VOID InitServerListWindow();
VOID CreateServerListWindow();

extern SERVERLIST_WINDOW_T g_ServerList;

#endif