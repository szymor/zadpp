// program do zadajnika (odczytnika ;p ) stanow logicznych
// wersja z kazdym bitem w oddzielnym okienku

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

struct SPort
{
	HWND hwnd[8];
	int posX[8];
	int posY[8];
	BYTE state;
	BYTE wanted_state;
};

BYTE numberOfPorts = 0;
HANDLE hComm = NULL;
SPort* zadport = NULL;
int wndwh = 128;
COLORREF onec, zeroc;
BOOL fullcircle;
char iniPath[1024];
UINT_PTR timer_id = 0;
int demoMode = 0;

char scriptPath[1024];
char* scriptSamples = NULL;
int scriptPorts = 0;
int scriptInterval = 0;
int scriptNumber = 0;
int scriptPosition = 0;
int scriptLoop = 0;
UINT_PTR scriptTimerID = 0;
int scriptPhase = 0;
// 0 - unloaded
// 1 - pause
// 2 - play

int PrepareCommPort(void);
BOOL GetPortBit(HWND hwnd, BYTE* port, BYTE* bit);
LRESULT CALLBACK ZadWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
VOID CALLBACK TimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime);
void SaveSettings(void);

int LoadScript( char* filename );
VOID CALLBACK ScriptTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if( !PrepareCommPort() )
	{
		MessageBox(NULL,"Nie znaleziono sprzêtu","B³¹d",MB_OK | MB_APPLMODAL | MB_ICONERROR | MB_RTLREADING);
		demoMode = 1;
		numberOfPorts = 2;
	} else
	{
		char aux[256];
		sprintf(aux, "%d", numberOfPorts);
		switch(numberOfPorts)
		{
		case 1:
			strcat(aux," port");
			break;
		case 2:
		case 3:
		case 4:
			strcat(aux," porty");
			break;
		default:
			strcat(aux," portów");
			break;
		}
		MessageBox(NULL,aux,"Znaleziono!",MB_OK | MB_APPLMODAL | MB_ICONINFORMATION);
	}

	if(!strcmp(lpCmdLine,""))
	{
		GetCurrentDirectory(1000,iniPath);
		strcat(iniPath,"\\settings.ini");
	}
	else
		strcpy(iniPath,lpCmdLine);

	char aux[256];
	wndwh = GetPrivateProfileInt("zadplusplus","wndwh",128,iniPath);
	GetPrivateProfileString("zadplusplus","zeroc","200",aux,256,iniPath);
	zeroc = atoi(aux);
	GetPrivateProfileString("zadplusplus","onec","51200",aux,256,iniPath);
	onec = atoi(aux);
	fullcircle = GetPrivateProfileInt("zadplusplus","fullcircle",0,iniPath);

	WNDCLASSEX wce;
	ZeroMemory(&wce,sizeof(wce));
	wce.cbSize = sizeof(wce);
	wce.lpszClassName = "ZadSmallWndClass";
	wce.lpfnWndProc = ZadWndProc;
	wce.hInstance = hInstance;
	wce.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wce.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	RegisterClassEx(&wce);

	zadport = (SPort*)GlobalAlloc(GMEM_ZEROINIT,numberOfPorts*sizeof(SPort));
	for(BYTE i = 0; i < numberOfPorts; ++i)
	{
		for(BYTE j = 0; j < 8; ++j)
		{
			char tmp1[256];
			char tmp2[256];
			HRGN hrgn;
			hrgn = CreateEllipticRgn(0,0,wndwh,wndwh);
			zadport[i].state = 0xff;
			zadport[i].wanted_state = 0xff;

			strcpy(tmp1,"zadplusplus");
			itoa(i,tmp2,10);
			strcat(tmp1,tmp2);
			strcat(tmp1,"_");
			itoa(j,tmp2,10);
			strcat(tmp1,tmp2);

			zadport[i].posX[j] = GetPrivateProfileInt(tmp1,"posX",CW_USEDEFAULT,iniPath);
			zadport[i].posY[j] = GetPrivateProfileInt(tmp1,"posY",CW_USEDEFAULT,iniPath);
			if( (i == 0) && (j == 0) )
				zadport[i].hwnd[j] = CreateWindowEx(WS_EX_TOPMOST,"ZadSmallWndClass","ZadPlusPlus",WS_POPUP | WS_SYSMENU,zadport[i].posX[j],zadport[i].posY[j],wndwh,wndwh,NULL,NULL,hInstance,NULL);
			else
				zadport[i].hwnd[j] = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,"ZadSmallWndClass","ZadPlusPlus",WS_POPUP | WS_SYSMENU,zadport[i].posX[j],zadport[i].posY[j],wndwh,wndwh,NULL,NULL,hInstance,NULL);
			SetWindowRgn(zadport[i].hwnd[j],hrgn,FALSE);
			ShowWindow(zadport[i].hwnd[j],nCmdShow);
		}
	}

	timer_id = SetTimer(NULL,0,100,(TIMERPROC)TimerProc);

	GetPrivateProfileString( "zadplusplus", "scriptfile", "", scriptPath, 1024, iniPath );
	if( strcmp( scriptPath, "" ) )
	{
		if( !LoadScript( scriptPath ) )
			scriptPhase = 1;
		else
			scriptPhase = 0;
	}

	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	KillTimer(NULL,timer_id);
	if( scriptTimerID )
		KillTimer( NULL, scriptTimerID );
	SaveSettings();
	GlobalFree(zadport);
	if( scriptSamples )
		GlobalFree( scriptSamples );
	return msg.wParam;
}

int PrepareCommPort(void)
{
	char buffer[5];
	DWORD nob;
	for(int i = 1; i < 10; ++i)
	{
		strncpy(buffer,"COM",3);
		sprintf(buffer+3, "%d", i);
		hComm = CreateFile(buffer, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
		if( hComm == INVALID_HANDLE_VALUE )
			continue;

		DCB dcb;
		COMMTIMEOUTS cto;
		cto.ReadIntervalTimeout = 0;	// maximum time between 2 bytes
		cto.ReadTotalTimeoutConstant = 0;
		cto.ReadTotalTimeoutMultiplier = 20;
		cto.WriteTotalTimeoutConstant = 0;
		cto.WriteTotalTimeoutMultiplier = 20;
		SetCommTimeouts(hComm, &cto);

		ZeroMemory(&dcb, sizeof(DCB));
		dcb.DCBlength = sizeof(DCB);
		if( !GetCommState(hComm, &dcb) )
		{
			CloseHandle(hComm);
			continue;
		}
		dcb.BaudRate = CBR_9600;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;
		if( !SetCommState(hComm, &dcb) )
		{
			CloseHandle(hComm);
			continue;
		}

		PurgeComm(hComm,0x0f);
		buffer[0] = '?';
		WriteFile(hComm,buffer,1,&nob,NULL);
		if(nob < 1)
		{
			CloseHandle(hComm);
			continue;
		}
		ReadFile(hComm,buffer,3,&nob,NULL);
		buffer[3] = 0;		// null-terminated string
		if((nob < 3) || strcmp(buffer,"ZAD"))
		{
			CloseHandle(hComm);
			continue;
		}
		buffer[0] = 'i';
		WriteFile(hComm,buffer,1,&nob,NULL);
		if(nob < 1)
		{
			CloseHandle(hComm);
			continue;
		}
		ReadFile(hComm,&numberOfPorts,1,&nob,NULL);
		return 1;
	}
	return 0;
}

BOOL GetPortBit(HWND hwnd, BYTE* port, BYTE* bit)
{
	for(BYTE i = 0; i < numberOfPorts; ++i)
		for(BYTE j = 0; j < 8; ++j)
			if( zadport[i].hwnd[j] == hwnd )
			{
				*port = i;
				*bit = j;
				return TRUE;
			}
	return FALSE;
}

LRESULT CALLBACK ZadWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	BYTE whichPort;
	BYTE whichBit;
	switch(msg)
	{
	case WM_NCHITTEST:
		LRESULT temp;
		temp = DefWindowProc(hWnd,msg,wParam,lParam);
		if( temp == HTCLIENT )
			return HTCAPTION;
		else
			return temp;
	case WM_KEYDOWN:
		if( wParam == 'T' )
			fullcircle = !fullcircle;
		if( wParam == 'S' )
		{
			if( scriptPhase == 1 )
			{
				scriptTimerID = SetTimer(NULL, 0, scriptInterval, (TIMERPROC) ScriptTimerProc);
				scriptPhase = 2;
			}
			else
				if( scriptPhase == 2 )
				{
					KillTimer( NULL, scriptTimerID );
					scriptPhase = 1;
				}
		}
		InvalidateRect(hWnd,NULL,FALSE);
		UpdateWindow(hWnd);
		return 0;
	case WM_NCLBUTTONDBLCLK:
		WORD buff;
		DWORD nob;
		GetPortBit(hWnd,&whichPort,&whichBit);
		zadport[whichPort].wanted_state ^= 0x01 << whichBit;
		buff = (zadport[whichPort].wanted_state << 8) + (whichPort + 'A');
		WriteFile(hComm,&buff,2,&nob,NULL);
		InvalidateRect(hWnd,NULL,FALSE);
		UpdateWindow(hWnd);
		return 0;
	case WM_PAINT:		// double-buffered painting
		PAINTSTRUCT ps;
		HDC hdc, hdcbuff;
		HBITMAP hbm, prevhbm;

		hdc = BeginPaint(hWnd,&ps);

		hdcbuff = CreateCompatibleDC(hdc);
		hbm = CreateCompatibleBitmap(hdc,GetDeviceCaps(hdcbuff,HORZRES),GetDeviceCaps(hdcbuff,VERTRES));
		prevhbm = (HBITMAP)SelectObject(hdcbuff,hbm);
		rect.left = 0;
		rect.top = 0;
		rect.bottom = GetDeviceCaps(hdc,VERTRES);
		rect.right = GetDeviceCaps(hdc,HORZRES);
		FillRect(hdcbuff,&rect,(HBRUSH)( COLOR_WINDOW+1 ));

		GetClientRect(hWnd,&rect);
		SetMapMode(hdc,MM_ANISOTROPIC);
		SetWindowOrgEx(hdc,0,0,NULL);
		SetWindowExtEx(hdc,128,128,NULL);
		SetViewportOrgEx(hdc,0,0,NULL);
		SetViewportExtEx(hdc,rect.right-rect.left,rect.bottom-rect.top,NULL);

		SetMapMode(hdcbuff,MM_ANISOTROPIC);
		SetWindowOrgEx(hdcbuff,0,0,NULL);
		SetWindowExtEx(hdcbuff,128,128,NULL);
		SetViewportOrgEx(hdcbuff,0,0,NULL);
		SetViewportExtEx(hdcbuff,rect.right-rect.left,rect.bottom-rect.top,NULL);

//---------BEGINNING OF PAINTING-----------

		char aux[256];
		char tmp[256];
		SIZE size;

		HBRUSH zero_brush, one_brush, old_brush;
		HPEN new_pen, old_pen;
		HFONT new_font, old_font;

		GetPortBit(hWnd,&whichPort,&whichBit);
		zero_brush = CreateSolidBrush(zeroc);
		one_brush = CreateSolidBrush(onec);
		new_pen = CreatePen(PS_SOLID,5,0);
		new_font = CreateFont(32,0,0,0,0,0,0,0,0,0,0,0,0,"Arial CE");
		old_pen = (HPEN)SelectObject(hdcbuff,new_pen);
		old_brush = (HBRUSH)SelectObject(hdcbuff,one_brush);
		old_font = (HFONT)SelectObject(hdcbuff,new_font);

		rect.left = 0;
		if(fullcircle)
			rect.right = 128;
		else
			rect.right = 64;
		rect.top = 0;
		rect.bottom = 128;
		if(!((zadport[whichPort].state >> whichBit) & 0x01))
		{
			FillRect(hdcbuff,&rect,zero_brush);
		}
		else
		{
			FillRect(hdcbuff,&rect,one_brush);
		}

		if(!fullcircle)
		{
			rect.left = 64;
			rect.right = 128;
			if(!((zadport[whichPort].wanted_state >> whichBit) & 0x01))
			{
				FillRect(hdcbuff,&rect,zero_brush);
			}
			else
			{
				FillRect(hdcbuff,&rect,one_brush);
			}
			MoveToEx(hdcbuff,64,0,NULL);
			LineTo(hdcbuff,64,128);
		}
		Arc(hdcbuff,1,1,126,126,0,0,0,0);

		sprintf(aux, "P%d.%d", whichPort, whichBit);
		GetTextExtentPoint32(hdcbuff,aux,strlen(aux),&size);
		SetTextColor(hdcbuff,RGB(255,255,255));
		SetBkColor(hdcbuff,0);
		TextOut(hdcbuff,64-size.cx/2,64-size.cy/2,aux,strlen(aux));

		SelectObject(hdcbuff,old_brush);
		SelectObject(hdcbuff,old_pen);
		SelectObject(hdcbuff,old_font);
		DeleteObject(new_pen);
		DeleteObject(zero_brush);
		DeleteObject(one_brush);
		DeleteObject(new_font);

//------------END OF PAINTING--------------

		BitBlt(hdc,0,0,800,600,hdcbuff,0,0,SRCCOPY);
		SelectObject(hdcbuff,prevhbm);
		DeleteObject(hbm);
		DeleteDC(hdcbuff);
		EndPaint(hWnd,&ps);

		return 0;
	case WM_DESTROY:
		for(BYTE i = 0; i < numberOfPorts; ++i)
			for(BYTE j = 0; j < 8; ++j)
			{
				GetWindowRect(zadport[i].hwnd[j],&rect);
				zadport[i].posX[j] = rect.left;
				zadport[i].posY[j] = rect.top;
			}
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd,msg,wParam,lParam);
}

VOID CALLBACK TimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime)
{
	DWORD nob;
	BYTE buff;
	for(BYTE i = 0; i < numberOfPorts; ++i)
	{
		buff = i + 0x30;
		if( !demoMode )
		{
			PurgeComm(hComm,0x0f);
			WriteFile(hComm,&buff,1,&nob,NULL);
			ReadFile(hComm,&buff,1,&nob,NULL);
			zadport[i].state = buff;
		}
		else
		{
			zadport[i].state = 0;
		}
		for(BYTE j = 0; j < 8; ++j)
		{
			InvalidateRect(zadport[i].hwnd[j],NULL,FALSE);
			UpdateWindow(zadport[i].hwnd[j]);
		}
	}
}

void SaveSettings(void)
{
	char aux[256];
	char tmp1[256];
	char tmp2[256];

	for(BYTE i = 0; i < numberOfPorts; ++i)
		for(BYTE j = 0; j < 8; ++j)
		{
			strcpy(tmp1,"zadplusplus");
			itoa(i,tmp2,10);
			strcat(tmp1,tmp2);
			strcat(tmp1,"_");
			itoa(j,tmp2,10);
			strcat(tmp1,tmp2);
			_itoa(zadport[i].posX[j],aux,10);
			WritePrivateProfileString(tmp1,"posX",aux,iniPath);
			_itoa(zadport[i].posY[j],aux,10);
			WritePrivateProfileString(tmp1,"posY",aux,iniPath);
		}

	_itoa(wndwh,aux,10);
	WritePrivateProfileString("zadplusplus","wndwh",aux,iniPath);
	_itoa(zeroc,aux,10);
	WritePrivateProfileString("zadplusplus","zeroc",aux,iniPath);
	_itoa(onec,aux,10);
	WritePrivateProfileString("zadplusplus","onec",aux,iniPath);
	_itoa(fullcircle,aux,10);
	WritePrivateProfileString("zadplusplus","fullcircle",aux,iniPath);
}

int LoadScript( char* filename )
{
	FILE* file;
	int temp;
	char aux[256];

	file = fopen( filename, "r" );
	if( !file )
		return 1;
	fscanf( file, "porty %d przerwa %d petla %d", &scriptPorts, &scriptInterval, &scriptLoop );
	scriptNumber = 0;

	/*
	while( !feof( file ) )
	{
		fscanf( file, "%x", &temp );
		++scriptNumber;
	}
	*/

	while( !feof( file ) )
	{
		fscanf( file, "%s", &aux );
		while( !strcmp( aux, "/*" ) )
		{
			do
			{
				fscanf( file, "%s", &aux );
			} while ( strcmp( aux, "*/" ) );
			fscanf( file, "%s", &aux );
		}
		sscanf( aux, "%x", &temp );
		++scriptNumber;
	}

	scriptNumber /= scriptPorts;
	fclose( file );

	file = fopen( filename, "r" );
	fscanf( file, "porty %d przerwa %d petla %d", &scriptPorts, &scriptInterval, &scriptLoop );
	scriptSamples = (char*) GlobalAlloc( GMEM_ZEROINIT, scriptPorts * scriptNumber );

	/*
	for( int i = 0; i < (scriptNumber * scriptPorts); ++i )
	{
		fscanf( file, "%x", &temp );
		scriptSamples[i] = (char) temp;
	}
	*/

	for( int i = 0; i < (scriptNumber * scriptPorts); ++i )
	{
		fscanf( file, "%s", &aux );
		while( !strcmp( aux, "/*" ) )
		{
			do
			{
				fscanf( file, "%s", &aux );
			} while ( strcmp( aux, "*/" ) );
			fscanf( file, "%s", &aux );
		}

		sscanf( aux, "%x", &temp );
		scriptSamples[i] = (char) temp;
	}

	scriptPosition = scriptNumber - 1;
	fclose( file );

	return 0;
}

VOID CALLBACK ScriptTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime)
{
	WORD buff;
	DWORD nob;

	scriptPosition = (scriptPosition + 1) % scriptNumber;
	if( (scriptPosition == ( scriptNumber - 1 )) && !scriptLoop )
	{
		KillTimer( NULL, scriptTimerID );
		scriptPhase = 1;
	}
	for( int i = 0; i < scriptPorts; ++i )
	{
		zadport[i].wanted_state = scriptSamples[ scriptPosition * scriptPorts + i ];
		buff = (zadport[i].wanted_state << 8) + (i + 'A');
		WriteFile(hComm,&buff,2,&nob,NULL);
	}
}
