#ifndef __SERVERINFO_H
#define __SERVERINFO_H

#include "servertool.h"

#define SERVERINFO_CLASS L"ServerTool_ServerInfo"

#define IDC_SERVERINFO_REQUEST_BTN 135

typedef struct {
	LPCWSTR lpClass;
	HWND hWnd;

	HWND hAddressEdit;
	HWND hRequestBtn;
	HWND hInfoBox;
	HWND hPlayerList;
	HWND hRuleList;
	
	INT PlayerNum;
	INT RuleNum;

	HANDLE hRequestThread;
} SERVERINFO_WINDOW_T;

VOID InitServerInfoWindow();
VOID CreateServerInfoWindow();

extern SERVERINFO_WINDOW_T g_ServerInfo;

#endif