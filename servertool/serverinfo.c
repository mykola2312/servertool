#include "serverinfo.h"
#include <CommCtrl.h>
#include <stdio.h>
#include <stdlib.h>

SERVERINFO_WINDOW_T g_ServerInfo;

static LRESULT CALLBACK WndCallback(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam);

static INT iWidth = 400;
static INT iHeight = 640;

VOID InitServerInfoWindow()
{
	WNDCLASSEX wndClass;

	g_ServerInfo.lpClass = SERVERINFO_CLASS;

	ZeroMemory(&wndClass,sizeof(WNDCLASSEX));

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpszClassName = g_ServerInfo.lpClass;
	wndClass.hInstance = g_App.hInst;
	wndClass.lpfnWndProc = WndCallback;
	wndClass.hCursor = LoadCursor(NULL,IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndClass.hIcon = g_App.hIcon;
	wndClass.hIconSm = g_App.hIconSm;
	
	RegisterClassEx(&wndClass);
}

VOID CreateServerInfoWindow()
{
	g_ServerInfo.hWnd = CreateWindow(SERVERINFO_CLASS,
		L"Server-Info",WINDOW_STYLE,
		CW_USEDEFAULT,CW_USEDEFAULT,
		iWidth,iHeight,NULL,NULL,
		g_App.hInst,NULL);

	ShowWindow(g_ServerInfo.hWnd,g_App.nCmdShow);
	UpdateWindow(g_ServerInfo.hWnd);
}

static VOID AddServer(SERVER_T* Server)
{
	static WCHAR szText[1024];

	swprintf(szText,1024,
		L"Name: %s\r\n"
		L"Map: %s\r\n"
		L"Game: %s\r\n"
		L"Players: %d\r\n"
		L"Slots: %d\r\n"
		L"Type: %C\r\n"
		L"System: %C\r\n"
		L"Password: %d\r\n"
		L"VAC: %d\r\n",
	Server->szName,
	Server->szMap,
	Server->szGame,
	Server->cPlayers,
	Server->cMaxPlayers,
	Server->cType,
	Server->cOS,
	Server->cVisibility,
	Server->cVAC);
	
	SetWindowText(g_ServerInfo.hInfoBox,szText);
}

static VOID AddPlayer(PCHAR pName,UINT Score,FLOAT Duration)
{
	WCHAR szText[64];
	LV_ITEM Item;
	INT Idx;
	ULONG Time;

	Idx = g_ServerInfo.PlayerNum;

	ZeroMemory(&Item,sizeof(Item));
	Item.mask = LVIF_TEXT;
	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.cchTextMax = 64;
	Item.iItem = Idx;
	Item.iSubItem = 0;
	ListView_InsertItem(g_ServerInfo.hPlayerList,&Item);

	MultiByteToWideChar(CP_UTF8,0,pName,-1,szText,64);
	ListView_SetItemText(g_ServerInfo.hPlayerList,Idx,0,szText);

	swprintf(szText,64,L"%u",Score);
	ListView_SetItemText(g_ServerInfo.hPlayerList,Idx,1,szText);

	Time = (ULONG)Duration;
	swprintf(szText,64,L"%02u:%02u:%02u",Time/3600,(Time/60)%60,Time%60);
	ListView_SetItemText(g_ServerInfo.hPlayerList,Idx,2,szText);

	g_ServerInfo.PlayerNum++;
}

static VOID AddRule(PCHAR pName,PCHAR pValue)
{
	WCHAR szText[MAX_PATH];
	LV_ITEM Item;
	INT Idx;

	Idx = g_ServerInfo.RuleNum;

	ZeroMemory(&Item,sizeof(Item));
	Item.mask = LVIF_TEXT;
	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.cchTextMax = 64;
	Item.iItem = Idx;
	Item.iSubItem = 0;
	ListView_InsertItem(g_ServerInfo.hRuleList,&Item);

	MultiByteToWideChar(CP_UTF8,0,pName,-1,szText,MAX_PATH);
	ListView_SetItemText(g_ServerInfo.hRuleList,Idx,0,szText);

	MultiByteToWideChar(CP_UTF8,0,pValue,-1,szText,MAX_PATH);
	ListView_SetItemText(g_ServerInfo.hRuleList,Idx,1,szText);

	g_ServerInfo.RuleNum++;
}

static DWORD WINAPI RequestThread(LPVOID lpArg)
{
	GetServerInfo((NETADR_T*)lpArg,AddServer,AddPlayer,AddRule);
	PostMessage(g_ServerInfo.hWnd,WM_ENDSERVERINFOREQUEST,0,0);
	return 0;
}

static VOID RequestServerInfo()
{
	WCHAR szText[64];
	char szAddr[32];
	PWCHAR pDel;
	NETADR_T Addr;

	g_ServerInfo.PlayerNum = 0;
	g_ServerInfo.RuleNum = 0;

	SetWindowText(g_ServerInfo.hInfoBox,L"");
	ListView_DeleteAllItems(g_ServerInfo.hPlayerList);
	ListView_DeleteAllItems(g_ServerInfo.hRuleList);

	if(g_ServerInfo.hRequestThread)
	{
		TerminateThread(g_ServerInfo.hRequestThread,0);
		CloseHandle(g_ServerInfo.hRequestThread);
	}

	GetWindowText(g_ServerInfo.hAddressEdit,szText,32);
	pDel = wcschr(szText,L':');
	if(!pDel) return;

	*pDel = L'\0';
	WideCharToMultiByte(CP_UTF8,0,szText,-1,szAddr,32,NULL,NULL);

	Addr.sin_addr.s_addr = inet_addr(szAddr);
	Addr.sin_port = htons(_wtoi(pDel+1));

	g_ServerInfo.hRequestThread = CreateThread(NULL,0,RequestThread,&Addr,0,NULL);

	SetWindowText(g_ServerInfo.hRequestBtn,L"Wait..");
}

VOID EndServerInfoRequest()
{
	SetWindowText(g_ServerInfo.hRequestBtn,L"Request");
	CloseHandle(g_ServerInfo.hRequestThread);
	g_ServerInfo.hRequestThread = NULL;
}

static HWND CreatePlayerList(LPCREATESTRUCT lpCreate,HWND hParent,INT x,INT y)
{
	HWND hList;
	LV_COLUMN Column;

	hList = CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,L"",
		WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_EDITLABELS,x,y,380,150,
		hParent,NULL,lpCreate->hInstance,NULL);

	ZeroMemory(&Column,sizeof(Column));

	Column.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;

	Column.iOrder = 0;
	Column.iSubItem = 0;
	Column.cx = 190;
	Column.pszText = L"Name";
	ListView_InsertColumn(hList,0,&Column);

	Column.iOrder = 1;
	Column.iSubItem = 1;
	Column.cx = 95;
	Column.pszText = L"Score";
	ListView_InsertColumn(hList,1,&Column);

	Column.iOrder = 2;
	Column.iSubItem = 2;
	Column.cx = 90;
	Column.pszText = L"Time";
	ListView_InsertColumn(hList,2,&Column);

	return hList;
}

static HWND CreateRulesList(LPCREATESTRUCT lpCreate,HWND hParent,INT x,INT y)
{
	HWND hList;
	LV_COLUMN Column;

	hList = CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,L"",
		WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_EDITLABELS,x,y,380,170,
		hParent,NULL,lpCreate->hInstance,NULL);

	ZeroMemory(&Column,sizeof(Column));

	Column.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;

	Column.iOrder = 0;
	Column.iSubItem = 0;
	Column.cx = 140;
	Column.pszText = L"Name";
	ListView_InsertColumn(hList,0,&Column);

	Column.iOrder = 1;
	Column.iSubItem = 1;
	Column.cx = 235;
	Column.pszText = L"Value";
	ListView_InsertColumn(hList,1,&Column);

	return hList;
}

static LRESULT CALLBACK WndCallback(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	LPCREATESTRUCT lpCreate;

	switch(Msg)
	{
	case WM_CREATE:
		lpCreate = (LPCREATESTRUCT)lParam;

		g_ServerInfo.hAddressEdit = CreateWindowEx(WS_EX_CLIENTEDGE,L"Edit",L"Address:Port",
			WS_CHILD|WS_VISIBLE,6,10,280,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_ServerInfo.hRequestBtn = CreateWindow(L"Button",L"Request",WS_CHILD|WS_VISIBLE,
			295,10,92,25,hWnd,(HMENU)IDC_SERVERINFO_REQUEST_BTN,lpCreate->hInstance,NULL);
		g_ServerInfo.hInfoBox = CreateWindowEx(WS_EX_CLIENTEDGE,L"Edit",L"",
			WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_LEFT,
			6,40,380,200,hWnd,NULL,lpCreate->hInstance,NULL);
		
		CreateWindow(L"STATIC",L"Players",WS_CHILD|WS_VISIBLE,
			6,245,100,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_ServerInfo.hPlayerList = CreatePlayerList(lpCreate,hWnd,6,275);

		CreateWindow(L"STATIC",L"Server's ConVar (FCVAR_NOTIFY)",
			WS_CHILD|WS_VISIBLE,6,405,380,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_ServerInfo.hRuleList = CreateRulesList(lpCreate,hWnd,6,435);

		g_ServerInfo.PlayerNum = 0;
		g_ServerInfo.RuleNum = 0;
		break;
	case WM_COMMAND:
		if(LOWORD(wParam) == IDC_SERVERINFO_REQUEST_BTN)
			RequestServerInfo();
		break;
	case WM_ENDSERVERINFOREQUEST:
		EndServerInfoRequest();
		break;
	}
	return DefWindowProc(hWnd,Msg,wParam,lParam);
}