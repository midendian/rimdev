#ifndef __SIMRIMOS_H__
#define __SIMRIMOS_H__

/* Start of RIM compatibility definitions */
#pragma pack(2) /* code assumes 16bit alignment */

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

/* libc */
void *sim_memmove(void *dest, const void *src, size_t len);
long sim_strtol(const char *nptr, char **endptr, int base);
void sim__purecall(void);

/* core */
void sim_RimAlertNotify(int notify, int maxRepeats);
void sim_RimCatastrophicFailure(char *FailureMessage);
TASK sim_RimCreateThread(void (*entry)(void), DWORD stacksize);
void sim_RimDebugPrintf(const char *Format, ...);
BOOL sim_RimDisableAppSwitch(void);
BOOL sim_RimEnableAppSwitch(void);
TASK sim_RimFindTask(char *prefix);
void sim_RimFree(void *block);
TASK sim_RimGetAlarm(TIME *time);
void sim_RimGetAlertConfiguration(AlertConfiguration *alertConfig);
TASK sim_RimGetCurrentTaskID(void);
void sim_RimGetDateTime(TIME *time);
void sim_RimGetDeviceInfo(DEVICE_INFO *Info);
TASK sim_RimGetForegroundApp(void);
long sim_RimGetIdleTime(void);
TASK sim_RimGetMessage(MESSAGE *msg);
unsigned int sim_RimGetNumberOfTunes(void);
BOOL sim_RimGetPID(TASK hTask, PID *Pid, const char **Subtitle);
long sim_RimGetTicks(void);
int sim_RimGetTuneName(unsigned int TuneIndex, const char **TuneName);
void sim_RimInvokeTaskSwitcher(void);
void sim_RimKillTimer(DWORD timerID);
void *sim_RimMalloc(DWORD size);
DWORD sim_RimMemoryRemaining(void);
BOOL sim_RimPeekMessage(void);
void sim_RimPostMessage(TASK hTask, MESSAGE *msg);
void *sim_RimRealloc(void *Ptr, DWORD size);
BOOL sim_RimRegisterMessageCallback(DWORD MessageBits, DWORD MaskBits, CALLBACK_FUNC HandlerFunc);
BOOL sim_RimReplyMessage(TASK hTask, MESSAGE *msg);
BOOL sim_RimRequestForeground(TASK hTask);
BOOL sim_RimSendMessage(TASK hTask, MESSAGE *msg);
BOOL sim_RimSendSyncMessage(TASK hTask, MESSAGE *msg, MESSAGE *replyMsg);
BOOL sim_RimSetAlarmClock(TIME *time, BOOL enable);
void sim_RimSetAlertConfiguration(AlertConfiguration *alertConfig);
BOOL sim_RimSetDate(TIME *time);
void sim_RimSetLed(int LedNumber, int LedState);
void sim_RimSetPID(PID *pid);
void sim_RimSetReceiveFromDevice(DWORD device, BOOL recieveFrom);
BOOL sim_RimSetTime(TIME *time);
BOOL sim_RimSetTimer(DWORD timerID, DWORD time, DWORD type);
void sim_RimSleep(DWORD ticks);
void sim_RimSpeakerBeep(int frequency, int duration, int duty, int volume);
int sim_RimSprintf(char *Buffer, int Maxlen, char *Fmt, ...);
void sim_RimTaskYield(void);
void sim_RimTerminateThread(void);
void sim_RimTestAlert(int notify, AlertConfiguration *trialConfig);
void sim_RimToggleMessageReceiving(BOOL recieveMessages);
BOOL sim_RimWaitForSpecificMessage(MESSAGE *msg, MESSAGE *compare, DWORD mask);

/* lcd */
int sim_LcdPutStringXY(int x, int y, const char *s, int length, int flags);
int sim_LcdCreateDisplayContext(int displayToCopy);
void sim_LcdIconsEnable(BOOL Enable);
int sim_LcdSetDisplayContext(int iDC);
int sim_LcdGetDisplayContext(void);
int sim_LcdGetCharacterWidth(char c, int fontIndex);
int sim_LcdGetCurrentFont(void);
int sim_LcdSetCurrentFont(int iFontIndex);
int sim_LcdGetFontHeight(int fontIndex);
int sim_LcdReplaceFont(int iFontIndex, const FontDefinition *pNewFont);
void sim_LcdSetTextWindow(int x, int y, int wide, int high);
void sim_LcdScroll(int pixels);
void sim_LcdSetPixel(int x, int y, BOOL value);
void sim_LcdDrawBox(int iDrawingMode, int x1, int y1, int x2, int y2);
void sim_LcdDrawLine(int iDrawingMode, int x1, int y1, int x2, int y2);
void sim_LcdRasterOp(DWORD wOp, DWORD wWide, DWORD wHigh, const BitMap *src, int SrcX, int SrcY, BitMap *dest, int DestX, int DestY);
void sim_LcdCopyBitmapToDisplay(const BitMap *pbmSource, int iDisplayX, int iDisplayY);
void sim_LcdClearDisplay(void);
void sim_LcdClearToEndOfLine(void);
void sim_LcdGetConfig(LcdConfig *Config);

/* db */
HandleType sim_DbAddRec(HandleType db, unsigned size, const void *data);
void const * const *sim_DbPointTable(void);
STATUS sim_DbAndRec(HandleType rec, void *mask, unsigned size, unsigned offset);


#endif /* __SIMRIMOS_H__ */
