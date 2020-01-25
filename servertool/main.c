#include "servertool.h"
#include "serverlist.h"
#include "masterscan.h"
#include "serverinfo.h"
#include "resource.h"
#include <CommCtrl.h>

APPLICATION_T g_App;

INT APIENTRY wWinMain(HINSTANCE hInst,HINSTANCE hPrevInst,
	LPWSTR lpCmdLine,INT nCmdShow)
{
	WSADATA WSA;
	MSG Msg;


	g_App.hInst = hInst;
	g_App.nCmdShow = nCmdShow;

	//Load stuff

	InitCommonControls();
	
	g_App.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON1));
	g_App.hIconSm = LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON2));

	//Init WSA

	WSAStartup(MAKEWORD(2,2),&WSA);

	//Register windows

	InitServerListWindow();
	InitMasterScanWindow();
	InitServerInfoWindow();

	//Do something

	CreateServerListWindow();
	//CreateMasterScanPopup();

	//Main loop
	while(GetMessage(&Msg,NULL,0,0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	WSACleanup();
	return (INT)Msg.wParam;
}