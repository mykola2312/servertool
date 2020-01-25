#ifndef __SERVERTOOL_H
#define __SERVERTOOL_H

#include <WinSock2.h>

#define WINDOW_STYLE (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX)
#define POPUP_STYLE (WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX)

#define WM_ENDMASTERSCAN (WM_USER+1)
//WPARAM - MasterNum
//LPARAM - Addrs
#define WM_ENDMASTERREQUEST (WM_USER+2)
//LPARAM - Param ptr
#define WM_ENDQUERYSERVERS (WM_USER+3)
//LPARAM - Server num
#define WM_ENDSERVERINFOREQUEST (WM_USER+4)

#define SERVER_BLOCK 2048

#define MAX_MASTER_NUM 6

typedef struct {
	HINSTANCE hInst;
	INT nCmdShow;
	HICON hIcon;
	HICON hIconSm;
} APPLICATION_T;

typedef struct {
	SOCKET s;
	struct sockaddr_in addr;
} CONNECTION_T;

#pragma pack(push,1)
typedef struct {
	IN_ADDR sin_addr;
	SHORT sin_port;
} NETADR_T;
#pragma pack(pop)

typedef struct {
	CONNECTION_T Conn;
	NETADR_T* Servers;
	INT ServerNum;
	INT PacketNum;
} MASTER_T;

typedef struct {
	LPCSTR lpszSearch;
	struct sockaddr_in* addrs;
	VOID (*AddServer)(struct sockaddr_in*);
} MASTERFINDPARAMETERS_T;

typedef struct {
	WCHAR szName[MAX_PATH];
	WCHAR szMap[64];
	WCHAR szGame[64];
	BYTE cPlayers;
	BYTE cMaxPlayers;
	BYTE cOS;
	BYTE cType;
	BYTE cVisibility;
	BYTE cVAC;
	BOOL IsValid;
	NETADR_T Addr;
} SERVER_T;

typedef struct {
	IN char szSearch[MAX_PATH];
	IN MASTER_T* Masters;
	IN INT MasterNum;
	IN HWND hParent;
	OUT NETADR_T* Servers;
	OUT INT ServerNum;
	SERVER_T* ServerList;
} MASTERREQUESTPARAMETERS_T;

typedef struct {
	WCHAR szName[32];
	INT Score;
	FLOAT Duration;
} PLAYER_T;

typedef VOID (*AddServer_t)(SERVER_T*);
typedef VOID (*AddPlayer_t)(PCHAR,UINT,FLOAT);
typedef VOID (*AddRule_t)(PCHAR,PCHAR);

VOID FormatRequest(LPVOID lpBuf,NETADR_T* pAddr,LPSTR lpSearch,PSIZE_T pszLen);
VOID MakeNonblocking(SOCKET s);
BOOL FindMasterServers(MASTERFINDPARAMETERS_T* pParams);
INT DoMasterServers(MASTER_T* pMasters,INT MasterNum,LPCSTR lpSearch);
VOID MasterFree(MASTER_T* Master);
BOOL IsServerAlreadyInList(NETADR_T* Servers,INT ServerNum,NETADR_T* Target);
NETADR_T* GetUniqueServers(MASTER_T* pMasters,INT MasterNum,INT* pServerNum);
VOID ParsePacket(PBYTE pPacket,SERVER_T* Server);
VOID QueryServers(NETADR_T* Addrs,SERVER_T* Servers,INT ServerNum,
	AddServer_t AddServer,INT WaitTime);
BOOL GetServerInfo(NETADR_T* Addr,AddServer_t AddServer,
	AddPlayer_t AddPlayer,AddRule_t AddRule);

extern APPLICATION_T g_App;

#endif