/* 
	Copyright (C) 2015 Suguru Kawamoto
	This software is released under the MIT License.
*/

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

typedef PVOID FT_HANDLE;
typedef ULONG FT_STATUS;
#define FT_BAUD_9600 9600
#define FT_PURGE_RX 1
#define FT_PURGE_TX 2
#define FT_BITMODE_RESET 0x00
#define FT_BITMODE_SYNC_BITBANG 0x04

typedef FT_STATUS (__stdcall* _FT_Open)(int, FT_HANDLE*);
typedef FT_STATUS (__stdcall* _FT_Close)(FT_HANDLE);
typedef FT_STATUS (__stdcall* _FT_Write)(FT_HANDLE, LPVOID, DWORD, LPDWORD);
typedef FT_STATUS (__stdcall* _FT_SetBaudRate)(FT_HANDLE, ULONG);
typedef FT_STATUS (__stdcall* _FT_ResetDevice)(FT_HANDLE);
typedef FT_STATUS (__stdcall* _FT_Purge)(FT_HANDLE, ULONG);
typedef FT_STATUS (__stdcall* _FT_SetTimeouts)(FT_HANDLE, ULONG, ULONG);
typedef FT_STATUS (__stdcall* _FT_SetBitMode)(FT_HANDLE, UCHAR, UCHAR);
typedef FT_STATUS (__stdcall* _FT_GetBitMode)(FT_HANDLE, PUCHAR);

HMODULE g_hFTD2XX;
_FT_Open FT_Open;
_FT_Close FT_Close;
_FT_Write FT_Write;
_FT_SetBaudRate FT_SetBaudRate;
_FT_ResetDevice FT_ResetDevice;
_FT_Purge FT_Purge;
_FT_SetTimeouts FT_SetTimeouts;
_FT_SetBitMode FT_SetBitMode;
_FT_GetBitMode FT_GetBitMode;

BOOL LoadFTD2XX(LPCTSTR FileName)
{
	BOOL bResult;
	bResult = FALSE;
	g_hFTD2XX = LoadLibrary(FileName);
	if(g_hFTD2XX)
	{
		FT_Open = (_FT_Open)GetProcAddress(g_hFTD2XX, "FT_Open");
		FT_Close = (_FT_Close)GetProcAddress(g_hFTD2XX, "FT_Close");
		FT_Write = (_FT_Write)GetProcAddress(g_hFTD2XX, "FT_Write");
		FT_SetBaudRate = (_FT_SetBaudRate)GetProcAddress(g_hFTD2XX, "FT_SetBaudRate");
		FT_ResetDevice = (_FT_ResetDevice)GetProcAddress(g_hFTD2XX, "FT_ResetDevice");
		FT_Purge = (_FT_Purge)GetProcAddress(g_hFTD2XX, "FT_Purge");
		FT_SetTimeouts = (_FT_SetTimeouts)GetProcAddress(g_hFTD2XX, "FT_SetTimeouts");
		FT_SetBitMode = (_FT_SetBitMode)GetProcAddress(g_hFTD2XX, "FT_SetBitMode");
		FT_GetBitMode = (_FT_GetBitMode)GetProcAddress(g_hFTD2XX, "FT_GetBitMode");
		bResult = TRUE;
	}
	return bResult;
}

void UnloadFTD2XX()
{
	if(g_hFTD2XX)
		FreeLibrary(g_hFTD2XX);
}

#define I2C_HIGH 1
#define I2C_LOW 0

FT_HANDLE g_Handle;
BYTE g_Output;
BYTE g_Input;

void Init()
{
	DWORD i;
	LoadFTD2XX(_T("ftd2xx.dll"));
	FT_Open(0, &g_Handle);
	FT_ResetDevice(g_Handle);
	FT_Purge(g_Handle, FT_PURGE_RX | FT_PURGE_TX);
	FT_SetBitMode(g_Handle, 0xfa, FT_BITMODE_RESET);
	FT_SetTimeouts(g_Handle, 10, 10);
	FT_SetBaudRate(g_Handle, FT_BAUD_9600);
	FT_SetBitMode(g_Handle, 0xfa, FT_BITMODE_SYNC_BITBANG);
	g_Output = 0x0a;
	FT_Write(g_Handle, &g_Output, 1, &i);
	FT_GetBitMode(g_Handle, &g_Input);
}

void Uninit()
{
	FT_Close(g_Handle);
	g_Handle = NULL;
	UnloadFTD2XX();
}

unsigned char GetSCL()
{
	return (g_Input & (1 << 0)) ? I2C_HIGH : I2C_LOW;
}

void SetSCL(unsigned char Status)
{
	g_Output = (g_Output & ~(1 << 1)) | (Status == I2C_LOW ? 0 : (1 << 1));
}

unsigned char GetSDA()
{
	return (g_Input & (1 << 2)) ? I2C_HIGH : I2C_LOW;
}

void SetSDA(unsigned char Status)
{
	g_Output = (g_Output & ~(1 << 3)) | (Status == I2C_LOW ? 0 : (1 << 3));
}

void WaitForQuarterClock()
{
	DWORD i;
	FT_Write(g_Handle, &g_Output, 1, &i);
	if(i < 1)
	{
		FT_Purge(g_Handle, FT_PURGE_RX | FT_PURGE_TX);
		FT_Write(g_Handle, &g_Output, 1, &i);
	}
	FT_GetBitMode(g_Handle, &g_Input);
}

void WaitForHalfClock()
{
	WaitForQuarterClock();
	WaitForQuarterClock();
}

void i2c_init()
{
	Init();
}

void i2c_uninit()
{
	Uninit();
}

void i2c_stop()
{
	WaitForQuarterClock();
	if(GetSCL() != I2C_HIGH || GetSDA() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
		SetSDA(I2C_LOW);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
	}
	SetSDA(I2C_HIGH);
	WaitForQuarterClock();
}

void i2c_start()
{
	WaitForQuarterClock();
	if(GetSCL() != I2C_HIGH || GetSDA() != I2C_HIGH)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
		SetSDA(I2C_HIGH);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
	}
	SetSDA(I2C_LOW);
	WaitForQuarterClock();
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
}

void i2c_rep_start()
{
	i2c_start();
}

unsigned char i2c_write(unsigned char data)
{
	unsigned char r;
	unsigned char i;
	if(GetSCL() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	for(i = 0; i < 8; i++)
	{
		SetSDA((data & (0x80 >> i)) ? I2C_HIGH : I2C_LOW);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA(I2C_HIGH);
	WaitForQuarterClock();
	SetSCL(I2C_HIGH);
	while(GetSCL() == I2C_LOW)
	{
		WaitForQuarterClock();
	}
	WaitForHalfClock();
	r = (GetSDA() == I2C_LOW) ? 0 : 1;
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
	return r;
}

unsigned char i2c_read(unsigned char ack)
{
	unsigned char r;
	unsigned char i;
	if(GetSCL() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA(I2C_HIGH);
	r = 0;
	for(i = 0; i < 8; i++)
	{
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
		r |= (GetSDA() == I2C_HIGH) ? (0x80 >> i) : 0;
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA((ack == 0) ? I2C_HIGH : I2C_LOW);
	WaitForQuarterClock();
	SetSCL(I2C_HIGH);
	while(GetSCL() == I2C_LOW)
	{
		WaitForQuarterClock();
	}
	WaitForHalfClock();
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
	return r;
}

int _tmain(int argc, _TCHAR* argv[])
{
	unsigned int t;
	float rf;
	float gf;
	float bf;
	float irf;
	unsigned short r;
	unsigned short g;
	unsigned short b;
	unsigned short ir;
	printf("S11059-02DT color sensor over FTDI Chip FT232H\n");
	i2c_init();
	t = 3120 / 10;
	rf = (3120 / (float)t) / 117.0f;
	gf = (3120 / (float)t) / 85.0f;
	bf = (3120 / (float)t) / 44.8f;
	irf = (3120 / (float)t) / 30.0f;
	i2c_start();
	i2c_write((0x2a << 1) | 0x00);
	i2c_write(0x00);
	i2c_write(0x8c);
	i2c_write((unsigned char)((t >> 8) & 0xff));
	i2c_write((unsigned char)(t & 0xff));
	i2c_stop();
	while(!(GetKeyState(VK_ESCAPE) & 0x8000))
	{
		i2c_start();
		i2c_write((0x2a << 1) | 0x00);
		i2c_write(0x00);
		i2c_write(0x8c);
		i2c_stop();
		i2c_start();
		i2c_write((0x2a << 1) | 0x00);
		i2c_write(0x00);
		i2c_write(0x0c);
		i2c_stop();
		Sleep((175 * t * (2 * 4 + 1) / 2 + 1000 - 1) / 1000);
		i2c_start();
		i2c_write((0x2a << 1) | 0x00);
		i2c_write(0x03);
		i2c_rep_start();
		i2c_write((0x2a << 1) | 0x01);
		r = i2c_read(1) & 0xff;
		r = (r << 8) | (i2c_read(1) & 0xff);
		g = i2c_read(1) & 0xff;
		g = (g << 8) | (i2c_read(1) & 0xff);
		b = i2c_read(1) & 0xff;
		b = (b << 8) | (i2c_read(1) & 0xff);
		ir = i2c_read(1) & 0xff;
		ir = (ir << 8) | (i2c_read(0) & 0xff);
		i2c_stop();
		printf("%.2f\t%.2f\t%.2f\t%.2f\n", r * rf, g * gf, b * bf, ir * irf);
	}
	i2c_uninit();
	return 0;
}

