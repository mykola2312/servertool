#include "masterscan.h"
#include <CommCtrl.h>

MASTERSCAN_WINDOW_T g_MasterScan;
static INT iWidth = 400;
static INT iHeight = 350;

static LRESULT CALLBACK WndCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

VOID InitMasterScanWindow()
{
	WNDCLASSEX wndClass;

	g_MasterScan.lpClass = MASTERSCAN_CLASS;

	ZeroMemory(&wndClass,sizeof(WNDCLASSEX));

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpszClassName = g_MasterScan.lpClass;
	wndClass.hInstance = g_App.hInst;
	wndClass.lpfnWndProc = WndCallback;
	wndClass.hCursor = LoadCursor(NULL,IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndClass.hIcon = g_App.hIcon;
	wndClass.hIconSm = g_App.hIconSm;
	
	RegisterClassEx(&wndClass);
}

VOID CreateMasterScanWindow()
{
	g_MasterScan.IsPopup = FALSE;

	g_MasterScan.hWnd = CreateWindow(MASTERSCAN_CLASS,L"Master servers",
		WINDOW_STYLE,CW_USEDEFAULT,CW_USEDEFAULT,
		iWidth,iHeight,NULL,NULL,g_App.hInst,NULL);

	ShowWindow(g_MasterScan.hWnd,g_App.nCmdShow);
	UpdateWindow(g_MasterScan.hWnd);
}

VOID CreateMasterScanPopup()
{
	//CreateMasterScanWindow();
	//SetWindowPos(g_MasterScan.hWnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	g_MasterScan.IsPopup = TRUE;

	g_MasterScan.hWnd = CreateWindowEx(WS_EX_TOPMOST,
		MASTERSCAN_CLASS,
		L"Master servers [first scan]",
		WS_OVERLAPPED|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,
		iWidth,iHeight,g_MasterScan.hParent,NULL,g_App.hInst,NULL);

	ShowWindow(g_MasterScan.hWnd,g_App.nCmdShow);
	UpdateWindow(g_MasterScan.hWnd);
}

static DWORD WINAPI ScanThread(LPVOID lpArg)
{
	//Do some work

	//Send message to thread
	FindMasterServers((MASTERFINDPARAMETERS_T*)lpArg);

	PostMessage(g_MasterScan.hWnd,WM_ENDMASTERSCAN,0,0);
	return 0;
}

static VOID AddServer(struct sockaddr_in* addrs)
{
	LV_ITEM Item;
	WCHAR szText[32];

	ZeroMemory(&Item,sizeof(LV_ITEM));
	Item.iItem = g_MasterScan.MasterNum;

	Item.mask = LVIF_TEXT;
	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.cchTextMax = 64;

	ListView_InsertItem(g_MasterScan.hList,&Item);

	MultiByteToWideChar(CP_UTF8,0,inet_ntoa(addrs->sin_addr),-1,szText,32);
	ListView_SetItemText(g_MasterScan.hList,Item.iItem,0,szText);

	_itow(ntohs(addrs->sin_port),szText,10);
	ListView_SetItemText(g_MasterScan.hList,Item.iItem,1,szText);

	g_MasterScan.MasterNum++;
	SendMessage(g_MasterScan.hProgressBar,PBM_SETPOS,g_MasterScan.MasterNum,0);
}

VOID AddMasterServers()
{
	INT i;
	INT iSize;

	iSize = g_MasterScan.MasterNum;
	g_MasterScan.MasterNum = 0;
	for(i = 0; i < iSize; i++)
		AddServer(&g_MasterScan.addrs[i]);
}

VOID DoMasterScan(LPCSTR lpSearch,HWND hParent)
{
	g_MasterScan.hParent = hParent;
	CreateMasterScanPopup();
	
	SendMessage(g_MasterScan.hProgressBar,PBM_SETRANGE,0,MAKELPARAM(0,6));
	g_MasterScan.MasterNum = 0;
	g_MasterScan.ScanParams.addrs = g_MasterScan.addrs;
	g_MasterScan.ScanParams.AddServer = AddServer;
	//Start thread
	g_MasterScan.hScanThread = CreateThread(0,0,ScanThread,&g_MasterScan.ScanParams,0,NULL);

	SetWindowText(g_MasterScan.hLabel,L"Scanning..");
}

static VOID AddMaster()
{
	WCHAR szText[64];
	char szAddr[64];
	struct sockaddr_in addr;

	GetWindowText(g_MasterScan.hMasterIpEdit,szText,64);
	if(wcslen(szText) == 0) return;

	WideCharToMultiByte(CP_UTF8,0,szText,-1,szAddr,64,NULL,NULL);
	addr.sin_addr.s_addr = inet_addr(szAddr);
	
	GetWindowText(g_MasterScan.hMasterPortEdit,szText,64);
	if(wcslen(szText) == 0) return;
	addr.sin_port = htons(_wtoi(szText));

	SetWindowText(g_MasterScan.hMasterIpEdit,L"");
	SetWindowText(g_MasterScan.hMasterPortEdit,L"");

	if(g_MasterScan.MasterNum == 32)
	{
		MessageBox(g_MasterScan.hWnd,
			L"Reached limit of master servers!",
			L"Error",
		MB_ICONERROR);
		return;
	}

	CopyMemory(&g_MasterScan.addrs[g_MasterScan.MasterNum],
		&addr,sizeof(struct sockaddr_in));
	AddServer(&g_MasterScan.addrs[g_MasterScan.MasterNum]);
}

static HWND CreateListView(LPCREATESTRUCT lpCreate,HWND hParent,INT x,INT y,INT w,INT h)
{
	HWND hList;
	LV_COLUMN Column;

	hList = CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_EDITLABELS,
		x,y,w,h,hParent,NULL,lpCreate->hInstance,NULL);

	ZeroMemory(&Column,sizeof(LV_COLUMN));
	Column.mask = LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;

	Column.cx = w-(w/4);
	Column.iSubItem = 0;
	Column.iOrder = 0;
	Column.pszText = L"Address";
	ListView_InsertColumn(hList,0,&Column);

	Column.cx = w/4;
	Column.iSubItem = 1;
	Column.iOrder = 1;
	Column.pszText = L"Port";
	ListView_InsertColumn(hList,1,&Column);

	return hList;
}

static LRESULT CALLBACK WndCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LPCREATESTRUCT lpCreate;

	switch(Msg)
	{
	case WM_CREATE:
		//Create something
		lpCreate = (LPCREATESTRUCT)lParam;

		g_MasterScan.hProgressBar = CreateWindow(PROGRESS_CLASS,L"",WS_CHILD|WS_VISIBLE,
			6,10,380,20,hWnd,NULL,lpCreate->hInstance,NULL);
		g_MasterScan.hLabel = CreateWindow(L"STATIC",L"Status",WS_CHILD|WS_VISIBLE,
			6,35,100,20,hWnd,NULL,lpCreate->hInstance,NULL);
		g_MasterScan.hList = CreateListView(lpCreate,hWnd,6,50,380,200);

		g_MasterScan.hMasterIpEdit = CreateWindowEx(WS_EX_CLIENTEDGE,L"Edit",L"",WS_CHILD|WS_VISIBLE,
			6,255,300,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_MasterScan.hMasterPortEdit = CreateWindowEx(WS_EX_CLIENTEDGE,L"Edit",L"",WS_CHILD|WS_VISIBLE,
			310,255,76,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_MasterScan.hMasterAddBtn = CreateWindow(L"Button",L"Add master server",WS_CHILD|WS_VISIBLE,
			6,285,380,25,hWnd,(HMENU)IDC_MASTERSCAN_ADDMASTER_BTN,lpCreate->hInstance,NULL);

		if(!g_MasterScan.IsPopup)
			AddMasterServers();
		else
		{
			EnableWindow(g_MasterScan.hMasterIpEdit,FALSE);
			EnableWindow(g_MasterScan.hMasterPortEdit,FALSE);
			EnableWindow(g_MasterScan.hMasterAddBtn,FALSE);
		}
		break;
	case WM_COMMAND:
		//Command
		if(LOWORD(wParam) == IDC_MASTERSCAN_ADDMASTER_BTN)
			AddMaster();
		break;
	case WM_ENDMASTERSCAN:
		CloseHandle(g_MasterScan.hScanThread);
		SendMessage(g_MasterScan.hParent,WM_ENDMASTERSCAN,
			g_MasterScan.MasterNum,
			(LPARAM)g_MasterScan.addrs);
		SetWindowText(g_MasterScan.hLabel,L"Done");
		DestroyWindow(hWnd);
		break;
	case WM_CLOSE:
		if(g_MasterScan.IsPopup) return TRUE;
		break;
	case WM_DESTROY:
		break;
	}
	return DefWindowProc(hWnd,Msg,wParam,lParam);
}