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

#define DEVICE_SYSTEM	1
#define DEVICE_KEYPAD	2
#define DEVICE_RTC	3
#define DEVICE_TIMER	4
#define DEVICE_COM1	5
#define DEVICE_RADIO	6
#define DEVICE_HOLSTER	7
#define DEVICE_USER	128
#define LAST_DEVICE	DEVICE_HOLSTER

/* SYSTEM events */
#define SWITCH_FOREGROUND	0x0100
#define SWITCH_BACKGROUND	0x0101
#define BATTERY_LOW		0x0102
#define POWER_OFF		0x0103
#define POWER_UP		0x0104
#define SYSEVENT_UNKNOWN	0x0105 /* not defined */
#define TASK_LIST_CHANGED	0x0106 
#define BATTERY_GOOD		0x0107
#define MEMORY_LOW		0x0108
#define BATTERY_UPDATE		0x0109
#define EVENT_USER		0x8000

/* RTC events */
#define RTC_ALARM_EXPIRED	0x0300
#define RTC_CLOCK_UPDATE	0x0301

/* HOLSTER events */
#define OUT_OF_HOLSTER		0x0700
#define IN_HOLSTER		0x0701


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

/* mobitex.h */
typedef struct {
    long Sender;
    long Destination;
    int MpakType;
    int HPID;
    int Flags;
    TIME Timestamp;
    long lTime;
    int TrafficState;
} MPAK_HEADER;

typedef struct {
    int RadioOn;
    DWORD LocalMAN;
    DWORD ESN;
    int Base;
    int Area;
    int RSSI;
    WORD NetworkID;
    DWORD FaultBits;
    BOOL Active;
    BOOL PowerSaveMode;
    BOOL LiveState;
    BOOL TransmitterEnabled;
} RADIO_INFO;

typedef struct {
    BYTE SkipNum;
    BYTE ProtocolRevision;
    BYTE SkipTrans;
    BYTE Mode;
} SKIPNUM_INFO;

typedef struct {
    int DefaultNetworkIndex;
    int CurrentNetworkIndex;
    int NumValidNetworks;
    struct {
	WORD NetworkId;
	BYTE NetworkName[10];
    } Networks[10];
} NETWORKS_INFO;

#define RSSI_NO_COVERAGE -256

#define RADIO_OFF	0
#define RADIO_ON	1
#define RADIO_LOWBATT	2
#define RADIO_GET_ONOFF	3

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

/* radio */

#define MAX_QUEUED_MPAKS	7

#define MESSAGE_RECEIVED	0x0601
#define MESSAGE_SENT		0x0602
#define MESSAGE_NOT_SENT	0x0603
#define SIGNAL_LEVEL		0x0604
#define NETWORK_STARTED		0x0605
#define BASE_STATION_CHANGE	0x0606
#define RADIO_TURNED_OFF	0x0607
#define MESSAGE_STATUS		0x0608

#define RADIO_APP_NOT_REGISTERED	-1
#define RADIO_MPAK_NOT_FOUND		-2
#define RADIO_NO_FREE_BUFFERS		-3
#define RADIO_BAD_DATA			-4
#define RADIO_BAD_TAG			-5
#define RADIO_ERROR_GENERAL		-6
#define RADIO_ILLEGAL_SKIPNUM		-7
#define RADIO_ILLEGAL_WSM_PACKET	-8

#define TS_MESSAGE_OK			0
#define TS_MESSAGE_FROM_MAILBOX		1
#define TS_MESSAGE_IN_MAILBOX		2
#define TS_CANNOT_BE_REACHED		3
#define TS_ILLEGAL_MESSAGE		4
#define TS_NETWORK_CONGESTED		5
#define TS_TECHNICAL_ERROR		6
#define TS_DESTINATION_BUSY		7

/* Subscription flags */
#define FLAG_MAILBOX			1
#define FLAG_POSACK			2
#define FLAG_SENDLIST			4
#define FLAG_UNKNOWN_FLAG		8

#define MPAK_NOT_PSUBCOM		0 /* not actually defined */
#define MPAK_TEXT			1
#define MPAK_DATA			2
#define MPAK_STATUS			3
#define MPAK_HPDATA			4

void sim_RadioRegister(void);
void sim_RadioDeregister(void);
int sim_RadioOnOff(int mode);
void sim_RadioGetDetailedInfo(RADIO_INFO *info);
int sim_RadioGetSignalLevel(void);
int sim_RadioGetMpak(int mpakTag, MPAK_HEADER *header, BYTE *data);
int sim_RadioSendMpak(MPAK_HEADER *header, BYTE *data, int length);
int sim_RadioCancelSendMpak(int mpakTag);
void sim_RadioStopReception(int mpakTag);
void sim_RadioResumeReception(void);
int sim_RadioRequestSkipnum(SKIPNUM_INFO *SkipInfo, int Skipnum);
void sim_RadioAccelerateRetries(int mpakTag);
void sim_RadioGetAvailableNetworks(NETWORKS_INFO *info);
void sim_RadioChangeNetworks(DWORD NetworkId, BYTE *NetworkName);

/* keypad */
#define KEY_DOWN	0x0201
#define KEY_REPEAT	0x0202
#define KEY_UP		0x0203
#define THUMB_CLICK	0x0204
#define THUMB_UNCLICK	0x0205
#define THUMB_ROLL_UP	0x0206
#define THUMB_ROLL_DOWN	0x0207
#define KEY_STATUS	0x0208

#define ALT_STATUS	0x0001
#define SHIFT_STATUS	0x0002
#define CAPS_LOCK	0x0004
#define KEY_HELD_WHILE_ROLLING	0x0008
#define ALT_LOCK	0x0010
#define SHIFT_STATUS_L	0x0020
#define SHIFT_STATUS_R	0x0040

#define KEY_BACKSPACE	0x0008
#define KEY_ENTER	0x000d
#define KEY_SPACE	0x0020
#define KEY_DELETE	0x007f

#define KEY_SHIFT	0x0100
#define KEY_ALT		0x0101

#define KEY_ESCAPE	0x001b
#define KEY_SHIFT2	0x0102
#define KEY_BKLITE	0x0103

#if 0 /* does anything actuall use these? */
void sim_KeypadBeep(BOOL Enable);
void sim_KeypadRate(WORD Delay, WORD Rate);
BOOL sim_KeypadRegister(DWORD key);
#endif

#define TIMER_PERIODIC 1
#define TIMER_ONE_SHOT 2
#define TIMER_ABSOLUTE 3

#pragma pack(4) /* XXX */

#endif /* __SIMRIMOS_H__ */
