#include "serverlist.h"
#include "masterscan.h"
#include "serverinfo.h"
#include <CommCtrl.h>
#include <stdio.h>
#include <stdlib.h>

SERVERLIST_WINDOW_T g_ServerList;
static INT iWidth = 1072;
static INT iHeight = 600;

static LRESULT CALLBACK WndCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

VOID InitServerListWindow()
{
	WNDCLASSEX wndClass;

	g_ServerList.lpClass = SERVERLIST_CLASS;

	ZeroMemory(&wndClass,sizeof(WNDCLASSEX));

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpszClassName = g_ServerList.lpClass;
	wndClass.hInstance = g_App.hInst;
	wndClass.lpfnWndProc = WndCallback;
	wndClass.hCursor = LoadCursor(NULL,IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndClass.hIcon = g_App.hIcon;
	wndClass.hIconSm = g_App.hIconSm;
	
	RegisterClassEx(&wndClass);
}

VOID CreateServerListWindow()
{
	g_ServerList.hWnd = CreateWindow(SERVERLIST_CLASS,L"ServerList",
		WINDOW_STYLE,CW_USEDEFAULT,CW_USEDEFAULT,
		iWidth,iHeight,NULL,NULL,g_App.hInst,NULL);

	ShowWindow(g_ServerList.hWnd,g_App.nCmdShow);
	UpdateWindow(g_ServerList.hWnd);
}

static VOID StartMasterScan(HWND hParent)
{
	DoMasterScan("\\appid\\320",hParent);
}

static VOID LoadMasterServers(HWND hParent)
{
	HANDLE hFile;
	LARGE_INTEGER size;
	NETADR_T Addr;
	DWORD dwRead;
	INT i;

	hFile = CreateFile(L"master.dat",GENERIC_READ,FILE_SHARE_READ,
		NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		StartMasterScan(hParent);
		return;
	}

	GetFileSizeEx(hFile,&size);
	g_ServerList.MasterNum = (INT)(size.QuadPart/sizeof(NETADR_T));
	if(g_ServerList.MasterNum > 32)
		g_ServerList.MasterNum = 32;
	g_MasterScan.MasterNum = g_ServerList.MasterNum;
	for(i = 0; i < g_ServerList.MasterNum; i++)
	{
		ReadFile(hFile,&Addr,sizeof(NETADR_T),&dwRead,NULL);
		g_ServerList.Masters[i].Conn.addr.sin_family = AF_INET;
		g_ServerList.Masters[i].Conn.addr.sin_addr = Addr.sin_addr;
		g_ServerList.Masters[i].Conn.addr.sin_port = Addr.sin_port;
		g_MasterScan.addrs[i].sin_family = AF_INET;
		g_MasterScan.addrs[i].sin_addr = Addr.sin_addr;
		g_MasterScan.addrs[i].sin_port = Addr.sin_port;
	}
	CloseHandle(hFile);

	g_ServerList.Params.Masters = g_ServerList.Masters;
	g_ServerList.Params.MasterNum = g_ServerList.MasterNum;
	g_MasterScan.ScanParams.addrs = g_MasterScan.addrs;
}

static VOID SaveMasters()
{
	HANDLE hFile;
	NETADR_T Addr;
	DWORD dwWrote;
	INT i;

	hFile = CreateFile(L"master.dat",GENERIC_ALL,0,NULL,
		CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	for(i = 0; i < g_ServerList.MasterNum; i++)
	{
		Addr.sin_addr = g_ServerList.Masters[i].Conn.addr.sin_addr;
		Addr.sin_port = g_ServerList.Masters[i].Conn.addr.sin_port;
		WriteFile(hFile,&Addr,sizeof(NETADR_T),&dwWrote,NULL);
	}
	CloseHandle(hFile);
}

static VOID EndMasterScan(WPARAM wParam,LPARAM lParam)
{
	INT i;
	struct sockaddr_in* addrs;

	if(wParam < MAX_MASTER_NUM) //We need 3 master servers at least
	{
		Sleep(5000);
		StartMasterScan(g_ServerList.hWnd);
	}
	else
	{
		g_ServerList.MasterNum = (INT)wParam;
		addrs = (struct sockaddr_in*)lParam;

		for(i = 0; i < g_ServerList.MasterNum; i++)
		{
			CopyMemory(&g_ServerList.Masters[i].Conn.addr,
				&addrs[i],sizeof(struct sockaddr_in));
		}

		//Save masters to file
		SaveMasters();
	}
}

static DWORD WINAPI RequestThread(LPVOID lpArg)
{
	MASTERREQUESTPARAMETERS_T* Params;

	Params = (MASTERREQUESTPARAMETERS_T*)lpArg;

	DoMasterServers(Params->Masters,Params->MasterNum,Params->szSearch);
	Params->Servers = GetUniqueServers(Params->Masters,Params->MasterNum,&Params->ServerNum);

	SendMessage(Params->hParent,WM_ENDMASTERREQUEST,0,(LPARAM)Params);
	return 0;
}

VOID MakeMasterServersRequest()
{
	WCHAR szRequest[MAX_PATH];

	if(g_ServerList.Params.ServerList)
	{
		free(g_ServerList.Params.ServerList);
		g_ServerList.Params.ServerList = NULL;
	}

	if(g_ServerList.hScanThread)
	{
		TerminateThread(g_ServerList.hScanThread,0);
		CloseHandle(g_ServerList.hScanThread);
		g_ServerList.hScanThread = NULL;
	}

	ListView_DeleteAllItems(g_ServerList.hServerList);
	SendMessage(g_ServerList.hProgressBar,PBM_SETPOS,0,0);

	ZeroMemory(&g_ServerList.Params,sizeof(g_ServerList.Params));

	GetWindowText(g_ServerList.hRequestEdit,szRequest,MAX_PATH);
	WideCharToMultiByte(CP_UTF8,0,szRequest,-1,
		g_ServerList.Params.szSearch,MAX_PATH,NULL,NULL);

	g_ServerList.Params.hParent = g_ServerList.hWnd;
	g_ServerList.Params.Masters = g_ServerList.Masters;
	g_ServerList.Params.MasterNum = g_ServerList.MasterNum;

	g_ServerList.hScanThread = CreateThread(0,0,RequestThread,
		&g_ServerList.Params,0,NULL);

	SetWindowText(g_ServerList.hLabel,L"Requests to masters..");
	SetWindowText(g_ServerList.hRequestBtn,L"Stop");
}

static VOID AddServer(SERVER_T* Server)
{
	LV_ITEM Item;
	WCHAR szText[64];
	char szAddr[64];
	LPCWSTR pText;
	INT Idx;

	ZeroMemory(&Item,sizeof(LV_ITEM));
	Idx = g_ServerList.ServerIdx;

	Item.mask = LVIF_TEXT;
	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.iItem = Idx;

	ListView_InsertItem(g_ServerList.hServerList,&Item);

	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,0,Server->szName);
	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,1,Server->szMap);
	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,2,Server->szGame);

	_itow(Server->cPlayers,szText,10);
	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,3,szText);

	_itow(Server->cMaxPlayers,szText,10);
	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,4,szText);

	switch(Server->cType)
	{
		case 'd': pText = L"Dedicated"; break;
		case 'l': pText = L"Non-Dedic."; break;
		case 'p': pText = L"SourceTV"; break;
		default: 
			swprintf(szText,64,L"<%02X>",Server->cType&0xFF);
			pText = szText;
	}

	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,5,(LPWSTR)pText);

	switch(Server->cOS)
	{
		case 'l': pText = L"Linux"; break;
		case 'w': pText = L"Windows"; break;
		case 'm': pText = L"Mac OS"; break;
		default: 
			swprintf(szText,64,L"<%02X>",Server->cOS&0xFF);
			pText = szText;
	}

	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,6,(LPWSTR)pText);

	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,7,
		Server->cVisibility ? L"Yes" : L"No");

	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,8,
		Server->cVAC ? L"Yes" : L"No");

	sprintf(szAddr,"%s:%d",inet_ntoa(Server->Addr.sin_addr),
		ntohs(Server->Addr.sin_port));
	MultiByteToWideChar(CP_UTF8,0,szAddr,-1,szText,64);
	
	ListView_SetItemText(g_ServerList.hServerList,Item.iItem,9,szText);

	SendMessage(g_ServerList.hProgressBar,PBM_SETPOS,Idx,0);
	g_ServerList.ServerIdx++;
}

typedef struct {
	UINT Num;
	UINT Size;
	UINT WaitTime;
} BLOCKPARAMS_T;

static BLOCKPARAMS_T s_BlockSize[] = {
	{300,32,2},
	{512,128,3},
	{1024,256,4},
	{2048,512,5},
	{4096,1024,7},
	{-1,2048,10},
};

static BLOCKPARAMS_T* GetBlockParams(UINT Num)
{
	INT i;

	i = 0;
	while(Num > s_BlockSize[i].Num){i++;}
	return &s_BlockSize[i];
}

static DWORD WINAPI QueryThread(LPVOID lpArg)
{
	MASTERREQUESTPARAMETERS_T* Params;
	BLOCKPARAMS_T* Block;
	NETADR_T* AddrPtr;
	SERVER_T* ServerPtr;
	INT iNum,iRemaind;
	INT i;
	
	Params = (MASTERREQUESTPARAMETERS_T*)lpArg;
	Block = GetBlockParams(Params->ServerNum);
	Params->ServerList = (SERVER_T*)calloc(Params->ServerNum,sizeof(SERVER_T));
	ServerPtr = Params->ServerList;

	g_ServerList.ServerIdx = 0;
	iNum = Params->ServerNum/Block->Size;
	iRemaind = Params->ServerNum%Block->Size;

	AddrPtr = Params->Servers;
	for(i = 0; i < iNum; i++)
	{
		QueryServers(AddrPtr,ServerPtr,Block->Size,AddServer,Block->WaitTime);
		AddrPtr += Block->Size;
		ServerPtr += Block->Size;
	}

	if(iRemaind)
		QueryServers(AddrPtr,ServerPtr,iRemaind,AddServer,Block->WaitTime);
	
	PostMessage(g_ServerList.hWnd,WM_ENDQUERYSERVERS,0,0);
	return 0;
}

VOID EndMasterRequest()
{
	WCHAR szText[64];

	CloseHandle(g_ServerList.hScanThread);

	swprintf(szText,64,L"Found %d servers. Request..",g_ServerList.Params.ServerNum);
	SetWindowText(g_ServerList.hLabel,szText);

	SendMessage(g_ServerList.hProgressBar,PBM_SETRANGE,0,MAKELPARAM(0,g_ServerList.Params.ServerNum));

	//Query all servers
	g_ServerList.hScanThread = CreateThread(0,0,QueryThread,&g_ServerList.Params,0,NULL);
}

VOID EndQueryServers()
{
	SetWindowText(g_ServerList.hLabel,L"Done");
	
	free(g_ServerList.Params.Servers);
	g_ServerList.hScanThread = NULL;

	//ZeroMemory(&g_ServerList.Params,sizeof(g_ServerList.Params));
	SendMessage(g_ServerList.hProgressBar,PBM_SETPOS,0,0);
	SetWindowText(g_ServerList.hRequestBtn,L"Execute");
}

VOID StopAnyRequests()
{
	TerminateThread(g_ServerList.hScanThread,0);
	EndQueryServers();
}

typedef struct {
	INT iWidth;
	LPWSTR lpName;
} COLUMNHDR_T;

static COLUMNHDR_T Columns[] = {
	{300,L"Name"},
	{100,L"Map"},
	{100,L"Game"},
	{65,L"Players"},
	{65,L"Slots"},
	{95,L"Type"},
	{80,L"System"},
	{60,L"Password"},
	{40,L"VAC"},
	{150,L"Address"},
};

VOID CreateBar(LPCREATESTRUCT lpCreate,HWND hParent)
{
	HMENU hMenuBar,hMenu;

	hMenuBar = CreateMenu();
	
	hMenu = CreateMenu();
	AppendMenu(hMenu,MF_STRING,IDC_FILE_SAVEAS,L"Save ss..");

	AppendMenu(hMenuBar,MF_POPUP,(UINT_PTR)hMenu,L"File");

	hMenu = CreateMenu();
	AppendMenu(hMenu,MF_STRING,IDC_SERVERS_MASTERS,L"Master servers");
	AppendMenu(hMenu,MF_STRING,IDC_SERVERS_QUERY,L"Server-Info");

	AppendMenu(hMenuBar,MF_POPUP,(UINT_PTR)hMenu,L"Servers");

	hMenu = CreateMenu();
	AppendMenu(hMenu,MF_STRING,IDC_HELP_HELP,L"Help");
	AppendMenu(hMenu,MF_SEPARATOR,0,NULL);
	AppendMenu(hMenu,MF_STRING,IDC_HELP_ABOUT,L"About program");

	AppendMenu(hMenuBar,MF_POPUP,(UINT_PTR)hMenu,L"Help");

	SetMenu(hParent,hMenuBar);
}

VOID DialogHelp()
{
	MessageBox(g_ServerList.hWnd,
		L"You must make request like it described here:"
		L">>> https://developer.valvesoftware.com/wiki/Master_Server_Query_Protocol#Filter\n"
		L"\n"
		L"Example for Garry's Mod:\n\\appid\\4000\n\n"
		L"\\appid\\%SteamAppId ÑÅÐÂÅÐÀ èãðû!!!%\\\n\n",

		L"Help",
	MB_ICONINFORMATION);
}

VOID DialogAbout()
{
	MessageBox(g_ServerList.hWnd,
		L"https://github.com/nikolaytihonov/servertool\n"
		L"\thttps://steamcommunity.com/id/dominqnta\n"
		L"\tDiscord: ISurface030#1470"
		L"\n\nAuthor: nikolaytihonov",

		L"About",
	MB_ICONINFORMATION);
}

#define FILE_1STLINE L"Name\tMap\tGame\tPlayers\tSlots\tType\tSystem\tPassword\tVAC\tAddress\r\n"

VOID SaveServers()
{
	HANDLE hFile;
	static WCHAR szLine[512];
	static WCHAR szPath[MAX_PATH];
	static WCHAR szText[64];
	static CHAR sz8Line[1024];
	char szAddr[32];
	PWCHAR pText;
	OPENFILENAME ofn;
	DWORD dwWrote;
	INT i;

	ZeroMemory(&ofn,sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = g_ServerList.hWnd;
	ofn.lpstrFile = szPath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L".TXT";

	if(!GetSaveFileName(&ofn))
	{
		MessageBox(g_ServerList.hWnd,L"Can't save file!",L"Error",MB_ICONERROR);
		return;
	}

	hFile = CreateFile(szPath,GENERIC_ALL,0,NULL,
		CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		MessageBox(g_ServerList.hWnd,L"Can't save file!",L"Error",MB_ICONERROR);
		return;
	}
	
	WideCharToMultiByte(CP_UTF8,0,FILE_1STLINE,-1,sz8Line,sizeof(sz8Line),NULL,NULL);
	WriteFile(hFile,sz8Line,(DWORD)strlen(sz8Line)*sizeof(CHAR),&dwWrote,NULL);

	for(i = 0; i < g_ServerList.Params.ServerNum; i++)
	{
		SERVER_T* Server;

		//swprintf(szLine,L"%s\t%s\t%s%d\t%d\t%s\t%s\t%s
		Server = &g_ServerList.Params.ServerList[i];
		if(!Server->IsValid) continue;
		
		wcsncpy(szLine,Server->szName,512);
		wcsncat(szLine,L" ",512);

		wcsncat(szLine,Server->szMap,512);
		wcsncat(szLine,L" ",512);

		wcsncat(szLine,Server->szGame,512);
		wcsncat(szLine,L" ",512);

		_itow(Server->cPlayers,szText,10);

		wcsncat(szLine,szText,512);
		wcsncat(szLine,L" ",512);

		_itow(Server->cMaxPlayers,szText,10);

		wcsncat(szLine,szText,512);
		wcsncat(szLine,L" ",512);

		switch(Server->cType)
		{
			case 'd': pText = L"Dedicated"; break;
			case 'l': pText = L"Non-Dedic."; break;
			case 'p': pText = L"SourceTV"; break;
			default: 
				swprintf(szText,64,L"<%02X>",Server->cType&0xFF);
				pText = szText;
		}

		wcsncat(szLine,pText,512);
		wcsncat(szLine,L" ",512);

		switch(Server->cOS)
		{
			case 'l': pText = L"Linux"; break;
			case 'w': pText = L"Windows"; break;
			case 'm': pText = L"Mac OS"; break;
			default: 
				swprintf(szText,64,L"<%02X>",Server->cOS&0xFF);
				pText = szText;
		}

		wcsncat(szLine,pText,512);
		wcsncat(szLine,L" ",512);

		wcsncat(szLine,Server->cVisibility ? L"Yes" : L"No",512);
		wcsncat(szLine,L" ",512);

		wcsncat(szLine,Server->cVAC ? L"Yes" : L"No",512);
		wcsncat(szLine,L" ",512);

		sprintf(szAddr,"%s:%d",inet_ntoa(Server->Addr.sin_addr),
			ntohs(Server->Addr.sin_port));
		MultiByteToWideChar(CP_UTF8,0,szAddr,-1,szText,64);
	
		wcsncat(szLine,szText,512);
		wcsncat(szLine,L"\r\n",512);

		WideCharToMultiByte(CP_UTF8,0,szLine,-1,sz8Line,sizeof(sz8Line),NULL,NULL);
		WriteFile(hFile,sz8Line,(DWORD)strlen(sz8Line)*sizeof(CHAR),&dwWrote,NULL);
	}
	CloseHandle(hFile);
}

static HWND CreateListView(LPCREATESTRUCT lpCreate,HWND hParent,INT x,INT y,INT w,INT h)
{
	HWND hList;
	LV_COLUMN Column;
	INT i;

	hList = CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_EDITLABELS,
		x,y,w,h,hParent,NULL,lpCreate->hInstance,NULL);

	ZeroMemory(&Column,sizeof(LV_COLUMN));
	Column.mask = LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;

	for(i = 0; i < sizeof(Columns)/sizeof(COLUMNHDR_T); i++)
	{
		Column.cx = Columns[i].iWidth;
		Column.pszText = Columns[i].lpName;
		Column.iSubItem = i;
		Column.iOrder = i;
		ListView_InsertColumn(hList,i,&Column);
	}

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

		g_ServerList.hProgressBar = CreateWindow(PROGRESS_CLASS,L"",WS_CHILD|WS_VISIBLE,
			6,10,1055,20,hWnd,NULL,lpCreate->hInstance,NULL);
		g_ServerList.hRequestEdit = CreateWindowEx(WS_EX_CLIENTEDGE,L"Edit",L"\\appid\\4000",
			WS_CHILD|WS_VISIBLE,6,35,400,25,hWnd,NULL,lpCreate->hInstance,NULL);
		g_ServerList.hRequestBtn = CreateWindow(L"Button",L"Request",WS_CHILD|WS_VISIBLE,
			415,35,95,25,hWnd,(HMENU)IDC_REQUEST_BTN,lpCreate->hInstance,NULL);
		g_ServerList.hLabel = CreateWindow(L"STATIC",L"Status",WS_CHILD|WS_VISIBLE,
			525,35,200,30,hWnd,NULL,lpCreate->hInstance,NULL);

		CreateBar(lpCreate,hWnd);

		g_ServerList.hServerList = CreateListView(lpCreate,hWnd,6,70,1055,480);
		g_ServerList.hScanThread = NULL;
		g_ServerList.Params.Servers = NULL;
		ZeroMemory(&g_ServerList.Params,sizeof(g_ServerList.Params));

		//Find master servers
		//StartMasterScan(hWnd);
		LoadMasterServers(hWnd);
		break;
	case WM_ENDMASTERSCAN:
		EndMasterScan(wParam,lParam);
		break;
	case WM_ENDMASTERREQUEST:
		EndMasterRequest();
		break;
	case WM_ENDQUERYSERVERS:
		EndQueryServers();
		break;
	case WM_COMMAND:
		if(LOWORD(wParam) == IDC_REQUEST_BTN)
		{
			if(g_ServerList.hScanThread != 0)
				StopAnyRequests();
			else MakeMasterServersRequest();
		}
		else
		{
			switch(LOWORD(wParam))
			{
				case IDC_FILE_SAVEAS: SaveServers(); break;
				case IDC_SERVERS_MASTERS:
					CreateMasterScanWindow();
					break;
				case IDC_SERVERS_QUERY: 
					CreateServerInfoWindow();
					break;
				case IDC_HELP_HELP: DialogHelp(); break;
				case IDC_HELP_ABOUT: DialogAbout(); break;
			}
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd,Msg,wParam,lParam);
}