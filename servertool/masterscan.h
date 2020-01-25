#ifndef __MASTERSCAN_H
#define __MASTERSCAN_H

#include "servertool.h"

#define MASTERSCAN_CLASS L"ServerTool_MasterScan"

#define IDC_MASTERSCAN_ADDMASTER_BTN 134

typedef struct {
	LPCWSTR lpClass;
	HWND hParent;
	HWND hWnd;
	HWND hProgressBar;
	HWND hLabel;
	HWND hList;
	HWND hMasterIpEdit;
	HWND hMasterPortEdit;
	HWND hMasterAddBtn;

	struct sockaddr_in addrs[32];
	INT MasterNum;

	BOOL IsPopup;
	HANDLE hScanThread;
	MASTERFINDPARAMETERS_T ScanParams;
} MASTERSCAN_WINDOW_T;

VOID InitMasterScanWindow();
VOID CreateMasterScanWindow();
VOID CreateMasterScanPopup();

VOID DoMasterScan(LPCSTR lpSearch,HWND hParent);

extern MASTERSCAN_WINDOW_T g_MasterScan;

#endif