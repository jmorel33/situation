// mock windows.h
#ifndef _WINDOWS_
#define _WINDOWS_
#include <stdint.h>
typedef void* HANDLE;
typedef void* HWND;
typedef void* HMIDIIN;
typedef void* HDEVNOTIFY;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned long DWORD_PTR;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef long LONG;
typedef void* LPVOID;
#define WINAPI
#define CALLBACK
#endif
