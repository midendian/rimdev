/* -*- Mode: ab-c -*- */

#ifndef __RIMSIM_H__
#define __RIMSIM_H__

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdarg.h>


/* basetype.h */
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD; 
typedef unsigned long DWORD;
typedef DWORD TASK;
#define TRUE 1
#define FALSE 0

typedef struct {
	BYTE bType;
	WORD wide;
	WORD high;
	BYTE *data;
} BitMap;


/* Rim.h */
typedef struct {
	DWORD Device;
	DWORD Event;
	DWORD SubMsg;
	DWORD Length;
	char *DataPtr;
	DWORD Data[2];
} MESSAGE;

typedef struct {
	const char *Name;
	BOOL EnableForeground;
	const BitMap *Icon;
} PID;


typedef struct {
	BYTE second;
	BYTE minute;
	BYTE hour;
	BYTE date;
	BYTE month;
	BYTE day;
	WORD year;
	BYTE TimeZone;
} TIME;

typedef struct {
	BYTE deviceType;
	BYTE networkType;
	WORD reserved1;
	union {
		DWORD MAN;
		DWORD LLI;
		BYTE reserved2[16];
	} deviceId;
	DWORD ESN;
	BYTE HSN[16];
	BYTE reserved3[24];
} DEVICE_INFO;

typedef int (__attribute__((__cdecl__)) *CALLBACK_FUNC)(MESSAGE *);

typedef struct {
	BYTE duty;
	BYTE volume;
	BYTE maxVolume;
	BYTE repetitions;
	BYTE vibrateTime;
	BYTE vibrateType;
	BYTE inHolsterNotify;
	BYTE outOfHolsterNotify;
	BYTE pause;
} AlertConfiguration;

typedef struct {
	WORD LcdType;
	WORD contrastRange;
	WORD width;
	WORD height;
} LcdConfig;

typedef struct {
	BYTE bFontType;
	BYTE bFirstChar;
	BYTE bLastChar;
	BYTE bCharHeight;
	BYTE bCharUnderlineRow;
	BYTE bCharWidth;
	BYTE *bCharDefinitions;
	WORD *wCharOffsets;
} FontDefinition;

typedef short HandleType;
typedef int STATUS;

#include <simrimos.h>

#endif /* __RIMSIM_H__ */
