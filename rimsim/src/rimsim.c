/* -*- Mode: ab-c -*- */
/*
 *
 * rimsim: What will someday resemble a simulation environment
 * for the RIM handheld.
 *
 * I'm taking the same approach that the windows simulator from
 * RIM does: run the code natively and just link in your own
 * version of the OS API.  Of course, their version has a slightly
 * easier task, since RIM applications are win32 DLLs, and relinking
 * is just a matter of a few win32 KERNEL calls. 
 *
 * This code has to handle parsing, linking, relocation, etc, of the
 * win32 DLL format itself.  Which is not a major problem.  Its well
 * documented and all; its just a bit tedious (and so far, this is
 * what 90% of the code below is dedicated to). 
 *
 * The entire task is straightforward.  There are no obvious fundamental
 * problems.  All calls to external linkage in win32 just pass through
 * a thunk variable. You just need to fill that in with your own
 * routine and you're on your way.
 *
 * Of course, theres some minor things.  Like cdecl versus stdcall (anyone
 * have any hard documentation on what the differences are between the
 * various calling syntaxes MSVC allows?  I know what the asm looks like
 * for each, I just don't know _why_).
 *
 * Oh well.  I've never written a dynamic loader before.  This is fun.
 *
 *
 *
 * All this code assumes its reading little-endian PE on a 
 * little-endian machine.  This is just fine, though, since
 * we're going to execute raw RIM code on your x86, and you'll
 * have problems more important than endianness if the above
 * assumptions aren't true.
 * 
 *
 * XXX The Major Thing That Needs To Be Done For Completeness is
 * processing of the fixups (ie, actually doing relocation).  (Make
 * sure I remember to fixup the pointers in the .version section, too. 
 * But I shouldn't have to remember to do that because somehow those
 * addresses are in the big fixup tables anyway.)
 *
 *
 * Thunking fields are now getting filled in properly.  External
 * calls are now working, without any issue.  I can't believe it.  I 
 * didn't even have to write any asm to rearrange the calling order.
 *
 *
 * The good news is that after all that fun stuff is done, a complete
 * loader/simulator is just a long, tedious task away: implement
 * all the RimFoo() API calls, as well as emulating their dumb
 * OS semantics.  There are probably some interesting things to do 
 * there, but for now, I'll discount the possibility to avoid the 
 * hassle of anticipation...
 *
 * XXX This code should be cleaned up _a lot_.
 *
 */

#include <rimsim.h>

#define PE_SIG_LEN 4
#define PE_OFF_STARTOFFSET 0x003c
#define PE_SECNAME_LEN 8

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/*
 * These two structs are nearly verbatim from RIMVersion.h in the 
 * BlackBerry 2.0 SDK.  I hope no one minds.
 *
 */

/*
 * These come first in the .rimdep section.
 *
 * RIMVersion's are normally used to define an API version that
 * this module is exporting.  Although they can be used on applications
 * not exporting an API, they are most useful in that case (hence
 * the minCompat(Major,Minor) fields).
 *
 * And since a DLL can export any number of APIs, more than one
 * RIMVersion can exist in any DLL.
 *
 */
struct RIMVersion {
	const struct RIMVersion *thisRec;
	unsigned char size;
	unsigned char flags;
	unsigned short verMajor;
	unsigned short verMinor;
	unsigned short verPatch;
	unsigned short verBuild;
	unsigned short minCompatMajor;
	unsigned short minCompatMinor;
	char *name;
	struct RIMVersion *next;
};

/*
 * These come after that.
 *
 * Notice that the first three fields of RIMDependency and RIMVersion
 * are the same.  This is because they are put in the similar locations
 * in the .rimdep section and the flags value must be used to determine
 * if a record is a Dependency (0x01) or a Version (0x00).
 *
 */
struct RIMDependency {
	const struct RIMDependency *thisRec;
	unsigned char size;
	unsigned char flags;
	unsigned short verReqMajor;
	unsigned short verReqMinor;
	char *name;
	struct RIMDependency *next;
};

/*
 * Required "exports".  These values must be provided by
 * every RIM application (or resource) DLL.  However, they
 * are not exported in the way things are normally exported
 * (I assume because that would cause massive symbol conflict
 * at load time).
 *
 * Instead, the .version section provides pointers for these. Note
 * that these are virtual addresses, and that they do have fixup
 * entries in the .reloc section.
 *
 * I have yet to determine which entry point is the one that 
 * gets used. Theres two that the OS could conceivably know about:
 * the one here, and the one in the COFF Optional header.  They 
 * start at slightly different places, but end up in the same
 * code (_DllMainCRTStartup in Osentry.obj).  
 *
 * Its also worth noting that PagerMain is never directly exported
 * from the application, either in symbol form or in address form.
 * Only the entry points in Osentry.obj are given to the OS (at least
 * from what I can tell).
 *
 */
#define RIM_VERSEC_SIG_LEN 4
typedef void (*func_PagerMain)(void);
struct RIMVersionSection {
	const struct RIMVersionSection *thisRec; /* sure, why not... */
	char signature[RIM_VERSEC_SIG_LEN]; /* "RIM" */
	void *EntryPtr; /* 0x016c10a9 -- deep in OsEntry.obj majick */
	unsigned long version; /* 0x00020000 -- guess */
	char *VersionPtr;
	int *AppStackSize;
};


/*
 * This type will build our table of simulated calls below...
 */
typedef void (*simulfunc_t)() __attribute__((__cdecl__));
typedef struct simulcall_s {
	char libname[128];
	char name[256]; /* length could be an issue with C++ mangled names */
	unsigned long ordinal;
	simulfunc_t func;
	int returnsize;
	int argcount;
} simulcall_t;


struct import_directory {
	struct import_lookup_table *ilt;
	struct import_lookup_table *iltrva; /* just the address, not the list */
	time_t timestamp;
	unsigned long forwarder_chain; /* These are unsupported */
	char *name;
	void *iat;
	struct import_directory *next;
};

/* This struct is really a single 32bit int (yes, thats right) */
struct import_lookup_table {
	unsigned char ordinal_flag;
	unsigned long ordinalnum; /* only present if ordinal_flag is 0x01 */
	struct {
		unsigned short hint;
		char *name;
	} ihnt; /* only present if ordinal_flag is zero */
	void *thunk; /* address of thunk variable */
	simulcall_t *resolved; /* what we matched it to */
	struct import_lookup_table *next;
};

/*
 * These were derived nearly directly from the MS PE spec.
 *
 * http://msdn.microsoft.com/library/specs/msdn_pecoff.htm
 *
 */
typedef struct dll_section_s {
	char Name[PE_SECNAME_LEN+1];
	u32 VirtualSize;
	u32 VirtualAddress;
	u32 SizeOfRawData;
	u32 PointerToRawData;
	u32 PointerToRelocations;
	u32 PointerToLineNumbers;
	u16 NumberOfRelocations;
	u16 NumberOfLineNumbers;
	u32 Characteristics;
	unsigned char *data;
	int datalen;
} dll_section_t;

typedef struct rimdll_s {
	char Signature[PE_SIG_LEN];
	struct {
		u16 Machine;
		u16 NumberOfSections;
		u32 TimeDateStamp;
		u32 PointerToSymbolTable;
		u32 NumberOfSymbols;
		u16 SizeOfOptionalHeader;
		u16 Characteristics;
		struct {
			u16 Magic;
			u8 MajorLinkerVersion;
			u8 MinorLinkerVersion;
			u32 SizeOfCode;
			u32 SizeOfInitializedData;
			u32 SizeOfUninitializedData;
			u32 AddressOfEntryPoint;
			u32 BaseOfCode;
			u32 BaseOfData;
			u32 ImageBase;
			u32 SectionAlignment;
			u32 FileAlignment;
			u16 MajorOperatingSystemVersion;
			u16 MinorOperatingSystemVersion;
			u16 MajorImageVersion;
			u16 MinorImageVersion;
			u16 MajorSubsystemVersion;
			u16 MinorSubsystemVersion;
			u32 Reserved;
			u32 SizeOfImage;
			u32 SizeOfHeaders;
			u32 CheckSum;
			u16 Subsystem;
			u16 DLLCharacteristics;
			u32 SizeOfStackReserve;
			u32 SizeOfStackCommit;
			u32 SizeOfHeapReserve;
			u32 SizeOfHeapCommit;
			u32 LoaderFlags;
			u32 NumberOfRvaAndSizes;
			struct {
				u32 rva;
				u32 size;
			} directories[16];
		} opthdr;
	} coff;
	dll_section_t *sections;
	unsigned char *vma;
	int vmalen;
	struct RIMVersionSection rimversion;
	func_PagerMain PagerMain __attribute__((__cdecl__));
	struct RIMDependency *rimdeps;
	struct RIMVersion *rimvers;
	struct import_directory *importdir;
} rimdll_t;


/* ---------    libc Emulation Functions    --------- */

static long sim_strtol(const char *nptr, char **endptr, int base)
{
	printf("sim: strtol(%s, %p, %d)\n", nptr, endptr, base);

	return 0;
}

static void *sim_memmove(void *dest, const void *src, size_t len)
{
	printf("sim: memmove(%p, %p, %d)\n", dest, src, len);

	return NULL;
}

static void sim__purecall(void)
{
	printf("sim: _purecall()\n");

	return;
}

static void sim_YAXPAX(void)
{
	printf("sim: YAXPAX\n");

	return;
}

static void sim_YAPAXI(void)
{
	printf("sim: YAXPAXI\n");

	return;
}

/* ---------    Core RIM OS Functions    --------- */

static void sim_RimGetDeviceInfo(DEVICE_INFO *Info)
{
	printf("sim: RimGetDeviceInfo(%p)\n", Info);

	return;
}

static TASK sim_RimCreateThread(void (*entry)(void), DWORD stacksize)
{
	printf("sim: RimCreateThread(%p, %ld)\n", entry, stacksize);

	return 0;
}

static BOOL sim_RimDisableAppSwitch(void)
{
	printf("sim: RimDisableAppSwitch()\n");

	return FALSE;
}

static BOOL sim_RimEnableAppSwitch(void)
{
	printf("sim: RimEnableAppSwitch()\n");

	return FALSE;
}

static TASK sim_RimFindTask(char *prefix)
{
	printf("sim: RimFindTask(%s)\n", prefix);

	return 0;
}

static TASK sim_RimGetCurrentTaskID(void)
{
	printf("sim: RimGetCurrentTaskID()\n");

	return 0;
}

static TASK sim_RimGetMessage(MESSAGE *msg)
{
	printf("sim: RimGetMessage(%p)\n", msg);

	fflush(stdout);
	for(;;)
		;

	return 0;
}

static BOOL sim_RimGetPID(TASK hTask, PID *Pid, const char **Subtitle)
{
	printf("sim: RimGetPID(%ld, %p, %p)\n", hTask, Pid, Subtitle);

	return FALSE;
}

static TASK sim_RimGetForegroundApp(void)
{
	printf("sim: RimGetForegroundApp()\n");

	return 0;
}

static void sim_RimInvokeTaskSwitcher(void)
{
	printf("sim: RimInvokeTaskSwitcher()\n");

	return;
}

static BOOL sim_RimPeekMessage(void)
{
	printf("sim: RimPeekMessage()\n");

	return FALSE;
}

static void sim_RimPostMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimPostMessage(%ld, %p)\n", hTask, msg);

	return;
}

static BOOL sim_RimRegisterMessageCallback(DWORD MessageBits, DWORD MaskBits, CALLBACK_FUNC HandlerFunc)
{
	printf("sim: RimRegisterMessageCallback(0x%08lx, 0x%08lx, %p)\n", MessageBits, MaskBits, HandlerFunc);

	return FALSE;
}

static BOOL sim_RimReplyMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimReplyMessage(%ld, %p)\n", hTask, msg);

	return FALSE;
}

static BOOL sim_RimRequestForeground(TASK hTask)
{
	printf("sim: RimRequestForeground(%ld)\n", hTask);

	return TRUE;
}

static BOOL sim_RimSendMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimSendMessage(%ld, %p)\n", hTask, msg);

	return FALSE;
}

static BOOL sim_RimSendSyncMessage(TASK hTask, MESSAGE *msg, MESSAGE *replyMsg)
{
	printf("sim: RimSendSyncMessage(%ld, %p, %p)\n", hTask, msg, replyMsg);

	return FALSE;
}

static void sim_RimSetPID(PID *pid)
{
	printf("sim: RimSetPID({%s, %d, %p})\n", pid->Name, pid->EnableForeground, pid->Icon);

	return;
}

static void sim_RimSetReceiveFromDevice(DWORD device, BOOL recieveFrom)
{
	printf("sim: RimSetReceiveFromDevice(%ld, %d)\n", device, recieveFrom);

	return;
}

static void sim_RimTaskYield(void)
{
	printf("sim: RimTaskYield()\n");

	return;
}

static void sim_RimTerminateThread(void)
{
	printf("sim: RimTerminateThread()\n");

	return;
}

static void sim_RimToggleMessageReceiving(BOOL recieveMessages)
{
	printf("sim: RimToggleMessageReceiving(%d)\n", recieveMessages);

	return;
}

static BOOL sim_RimWaitForSpecificMessage(MESSAGE *msg, MESSAGE *compare, DWORD mask)
{
	printf("sim: RimWaitForSpecificMessage(%p, %p, 0x%08lx)\n", msg, compare, mask);

	return FALSE;
}

static void sim_RimFree(void *block)
{
	printf("sim: RimFree(%p)\n", block);

	return;
}

static void *sim_RimMalloc(DWORD size)
{
	printf("sim: RimMalloc(%ld)\n", size);

	return NULL;
}

static DWORD sim_RimMemoryRemaining(void)
{
	printf("sim: RimMemoryRemaining()\n");

	return 0;
}

static void *sim_RimRealloc(void *Ptr, DWORD size)
{
	printf("sim: RimRealloc(%p, %ld)\n", Ptr, size);

	return NULL;
}

static TASK sim_RimGetAlarm(TIME *time)
{
	printf("sim: RimGetAlarm(%p)\n", time);

	return 0;
}

static BOOL sim_RimSetAlarmClock(TIME *time, BOOL enable)
{
	printf("sim: RimSetAlarmClock(%p, %d)\n", time, enable);

	return FALSE;
}

static void sim_RimGetDateTime(TIME *time)
{
	printf("sim: RimGetDateTime(%p)\n", time);

	return;
}

static long sim_RimGetIdleTime(void)
{
	printf("sim: RimGetIdleTime()\n");

	return 0;
}

static long sim_RimGetTicks(void)
{
	printf("sim: RimGetTicks()\n");

	return 0;
}

static void sim_RimKillTimer(DWORD timerID)
{
	printf("sim: RimKillTimer(%ld)\n", timerID);

	return;
}

static BOOL sim_RimSetDate(TIME *time)
{
	printf("sim: RimSetDate(%p)\n", time);

	return FALSE;
}

static BOOL sim_RimSetTime(TIME *time)
{
	printf("sim: RimSetTime(%p)\n", time);

	return FALSE;
}

static BOOL sim_RimSetTimer(DWORD timerID, DWORD time, DWORD type)
{
	printf("sim: RimSetTimer(%ld, %ld, %ld)\n", timerID, time, type);

	return FALSE;
}

static void sim_RimSleep(DWORD ticks)
{
	printf("sim: RimSleep(%ld)\n", ticks);

	return;
}

static void sim_RimAlertNotify(int notify, int maxRepeats)
{
	printf("sim: RimAlertNotify(%d, %d)\n", notify, maxRepeats);

	return;
}

static void sim_RimGetAlertConfiguration(AlertConfiguration *alertConfig)
{
	printf("sim: RimGetAlertConfiguration(%p)\n", alertConfig);

	return;
}

static void sim_RimSetAlertConfiguration(AlertConfiguration *alertConfig)
{
	printf("sim: RimSetAlertConfiguration(%p)\n", alertConfig);

	return;
}

static void sim_RimSpeakerBeep(int frequency, int duration, int duty, int volume)
{
	printf("sim: RimSpeakerBeep(%d, %d, %d, %d)\n", frequency, duration, duty, volume);

	return;
}

static void sim_RimTestAlert(int notify, AlertConfiguration *trialConfig)
{
	printf("sim: RimTestAlert(%d, %p)\n", notify, trialConfig);

	return;
}

static unsigned int sim_RimGetNumberOfTunes(void)
{
	printf("sim: RimGetNumberOfTunes()\n");

	return 0;
}

static int sim_RimGetTuneName(unsigned int TuneIndex, const char **TuneName)
{
	printf("sim: RimGetTuneName(%u, %p)\n", TuneIndex, TuneName);

	return 0;
}

static void sim_RimCatastrophicFailure(char *FailureMessage)
{
	printf("sim: RimCatastrophicFailure(%s)\n", FailureMessage);

	abort();

	return;
}

static int sim_RimSprintf(char *Buffer, int Maxlen, char *Fmt, ...)
{
	va_list ap;
	int ret;

	printf("sim: RimSprintf(%p, %d, %s, ...)\n", Buffer, Maxlen, Fmt);
	va_start(ap, Fmt);
	ret = vsnprintf(Buffer, Maxlen, Fmt, ap);
	va_end(ap);

	return ret;
}

static void sim_RimSetLed(int LedNumber, int LedState)
{
	printf("sim: RimSetLed(%d, %d)\n", LedNumber, LedState);

	return;
}

/*
 * These, of course, should not be simulated, but run from the
 * actual RIM DLL.  But I don't want to deal with that right now,
 * but my sample app needs these two anyway.
 */
static void sim_ribbon_RibbonRegisterApplication(const char *applicationName, const BitMap *bitmap, int applicationData, int position)
{
	printf("sim: ribbon: RibbonRegisterApplication(%s, %p, %d, %d)\n", applicationName, bitmap, applicationData, position);

	return;
}

static void sim_ribbon_RibbonShowRibbon(void)
{
	printf("sim: ribbon: RibbonShowRibbon()\n");

	return;
}

simulcall_t simulatedcalls[] = {
	{"RIMOS.EXE", "RimSprintf", -1,
	 (simulfunc_t)sim_RimSprintf, 0, 0},
	{"RIMOS.EXE", "RimTaskYield", -1,
	 (simulfunc_t)sim_RimTaskYield, 0, 0},
	{"RIMOS.EXE", "strtol", -1,
	 (simulfunc_t)sim_strtol, 0, 0},
	{"RIMOS.EXE", "LcdClearToEndOfLine", -1,
	 (simulfunc_t)sim_LcdClearToEndOfLine, 0, 0},
	{"RIMOS.EXE", "RimRequestForeground", -1,
	 (simulfunc_t)sim_RimRequestForeground, 0, 0},
	{"RIMOS.EXE", "RimGetCurrentTaskID", -1,
	 (simulfunc_t)sim_RimGetCurrentTaskID, 0, 0},
	{"RIMOS.EXE", "RimGetMessage", -1,
	 (simulfunc_t)sim_RimGetMessage, 0, 0},
	{"RIMOS.EXE", "LcdPutStringXY", -1, 
	 (simulfunc_t)sim_LcdPutStringXY, 0, 0},
	{"RIMOS.EXE", "RimTerminateThread", -1, 
	 (simulfunc_t)sim_RimTerminateThread, 0, 0},
	{"RIMOS.EXE", "RimMalloc", -1, 
	 (simulfunc_t)sim_RimMalloc, 0, 0},
	{"RIMOS.EXE", "LcdClearDisplay", -1, 
	 (simulfunc_t)sim_LcdClearDisplay, 0, 0},
	{"RIMOS.EXE", "RimSetLed", -1, 
	 (simulfunc_t)sim_RimSetLed, 0, 0},
	{"RIMOS.EXE", "RimSetPID", -1,
	 (simulfunc_t)sim_RimSetPID, 0, 0},
	{"RIMOS.EXE", "RimDebugPrintf", -1,
	 (simulfunc_t)sim_RimDebugPrintf, 0, 0},
	{"RIMOS.EXE", "RimPostMessage", -1,
	 (simulfunc_t)sim_RimPostMessage, 0, 0},
	{"RIMOS.EXE", "LcdCopyBitmapToDisplay", -1,
	 (simulfunc_t)sim_LcdCopyBitmapToDisplay, 0, 0},
	{"RIMOS.EXE", "LcdRasterOp", -1,
	 (simulfunc_t)sim_LcdRasterOp, 0, 0},
	{"RIMOS.EXE", "LcdDrawLine", -1,
	 (simulfunc_t)sim_LcdDrawLine, 0, 0},
	{"RIMOS.EXE", "LcdDrawBox", -1,
	 (simulfunc_t)sim_LcdDrawBox, 0, 0},
	{"RIMOS.EXE", "_purecall", -1,
	 (simulfunc_t)sim__purecall, 0, 0},
	{"RIMOS.EXE", "RimFree", -1,
	 (simulfunc_t)sim_RimFree, 0, 0},
	{"RIMOS.EXE", "memmove", -1,
	 (simulfunc_t)sim_memmove, 0, 0},
	{"RIMOS.EXE", "RimRealloc", -1,
	 (simulfunc_t)sim_RimRealloc, 0, 0},
	{"RIMOS.EXE", "??3@YAXPAX@Z", -1,
	 (simulfunc_t)sim_YAXPAX, 0, 0},
	{"RIMOS.EXE", "??2@YAPAXI@Z", -1,
	 (simulfunc_t)sim_YAPAXI, 0, 0},
	{"RIMOS.EXE", "LcdGetConfig", -1,
	 (simulfunc_t)sim_LcdGetConfig, 0, 0},
	{"RIMOS.EXE", "RimGetTicks", -1,
	 (simulfunc_t)sim_RimGetTicks, 0, 0},
	{"RIMOS.EXE", "RimGetForegroundApp", -1,
	 (simulfunc_t)sim_RimGetForegroundApp, 0, 0},
	{"RIMOS.EXE", "RimGetDateTime", -1,
	 (simulfunc_t)sim_RimGetDateTime, 0, 0},
	{"RIMOS.EXE", "LcdSetPixel", -1,
	 (simulfunc_t)sim_LcdSetPixel, 0, 0},
	{"RIMOS.EXE", "LcdScroll", -1,
	 (simulfunc_t)sim_LcdScroll, 0, 0},
	{"RIMOS.EXE", "LcdSetTextWindow", -1,
	 (simulfunc_t)sim_LcdSetTextWindow, 0, 0},
	{"RIMOS.EXE", "LcdReplaceFont", -1,
	 (simulfunc_t)sim_LcdReplaceFont, 0, 0},
	{"RIMOS.EXE", "LcdSetCurrentFont", -1,
	 (simulfunc_t)sim_LcdSetCurrentFont, 0, 0},
	{"RIMOS.EXE", "LcdGetCharacterWidth", -1,
	 (simulfunc_t)sim_LcdGetCharacterWidth, 0, 0},
	{"RIMOS.EXE", "LcdGetCurrentFont", -1,
	 (simulfunc_t)sim_LcdGetCurrentFont, 0, 0},
	{"RIMOS.EXE", "RimSendMessage", -1,
	 (simulfunc_t)sim_RimSendMessage, 0, 0},
	{"RIMOS.EXE", "RimFindTask", -1,
	 (simulfunc_t)sim_RimFindTask, 0, 0},
	{"RIMOS.EXE", "RimSetTimer", -1,
	 (simulfunc_t)sim_RimSetTimer, 0, 0},
	{"RIMOS.EXE", "LcdGetDisplayContext", -1,
	 (simulfunc_t)sim_LcdGetDisplayContext, 0, 0},
	{"RIMOS.EXE", "LcdSetDisplayContext", -1,
	 (simulfunc_t)sim_LcdSetDisplayContext, 0, 0},
	{"RIMOS.EXE", "LcdIconsEnable", -1,
	 (simulfunc_t)sim_LcdIconsEnable, 0, 0},
	{"RIMOS.EXE", "LcdCreateDisplayContext", -1,
	 (simulfunc_t)sim_LcdCreateDisplayContext, 0, 0},
	{"RIMOS.EXE", "RimGetDeviceInfo", -1,
	 (simulfunc_t)sim_RimGetDeviceInfo, 0, 0},
	{"RIMOS.EXE", "RimCreateThread", -1,
	 (simulfunc_t)sim_RimCreateThread, 0, 0},
	{"RIMOS.EXE", "RimDisableAppSwitch", -1,
	 (simulfunc_t)sim_RimDisableAppSwitch, 0, 0},
	{"RIMOS.EXE", "RimEnableAppSwitch", -1,
	 (simulfunc_t)sim_RimEnableAppSwitch, 0, 0},
	{"RIMOS.EXE", "RimGetPID", -1,
	 (simulfunc_t)sim_RimGetPID, 0, 0},
	{"RIMOS.EXE", "RimInvokeTaskSwitcher", -1,
	 (simulfunc_t)sim_RimInvokeTaskSwitcher, 0, 0},
	{"RIMOS.EXE", "RimPeekMessage", -1,
	 (simulfunc_t)sim_RimPeekMessage, 0, 0},
	{"RIMOS.EXE", "RimRegisterMessageCallback", -1,
	 (simulfunc_t)sim_RimRegisterMessageCallback, 0, 0},
	{"RIMOS.EXE", "RimReplyMessage", -1,
	 (simulfunc_t)sim_RimReplyMessage, 0, 0},
	{"RIMOS.EXE", "RimSendSyncMessage", -1,
	 (simulfunc_t)sim_RimSendSyncMessage, 0, 0},
	{"RIMOS.EXE", "RimSetReceiveFromDevice", -1,
	 (simulfunc_t)sim_RimSetReceiveFromDevice, 0, 0},
	{"RIMOS.EXE", "RimToggleMessageReceiving", -1,
	 (simulfunc_t)sim_RimToggleMessageReceiving, 0, 0},
	{"RIMOS.EXE", "RimWaitForSpecificMessage", -1,
	 (simulfunc_t)sim_RimWaitForSpecificMessage, 0, 0},
	{"RIMOS.EXE", "RimMemoryRemaining", -1,
	 (simulfunc_t)sim_RimMemoryRemaining, 0, 0},
	{"RIMOS.EXE", "RimGetAlarm", -1,
	 (simulfunc_t)sim_RimGetAlarm, 0, 0},
	{"RIMOS.EXE", "RimSetAlarmClock", -1,
	 (simulfunc_t)sim_RimSetAlarmClock, 0, 0},
	{"RIMOS.EXE", "RimGetIdleTime", -1,
	 (simulfunc_t)sim_RimGetIdleTime, 0, 0},
	{"RIMOS.EXE", "RimKillTimer", -1,
	 (simulfunc_t)sim_RimKillTimer, 0, 0},
	{"RIMOS.EXE", "RimSetDate", -1,
	 (simulfunc_t)sim_RimSetDate, 0, 0},
	{"RIMOS.EXE", "RimSetTime", -1,
	 (simulfunc_t)sim_RimSetTime, 0, 0},
	{"RIMOS.EXE", "RimSleep", -1,
	 (simulfunc_t)sim_RimSleep, 0, 0},
	{"RIMOS.EXE", "RimAlertNotify", -1,
	 (simulfunc_t)sim_RimAlertNotify, 0, 0},
	{"RIMOS.EXE", "RimGetAlertConfiguration", -1,
	 (simulfunc_t)sim_RimGetAlertConfiguration, 0, 0},
	{"RIMOS.EXE", "RimSetAlertConfiguration", -1,
	 (simulfunc_t)sim_RimSetAlertConfiguration, 0, 0},
	{"RIMOS.EXE", "RimSpeakerBeep", -1,
	 (simulfunc_t)sim_RimSpeakerBeep, 0, 0},
	{"RIMOS.EXE", "RimTestAlert", -1,
	 (simulfunc_t)sim_RimTestAlert, 0, 0},
	{"RIMOS.EXE", "RimGetNumberOfTunes", -1,
	 (simulfunc_t)sim_RimGetNumberOfTunes, 0, 0},
	{"RIMOS.EXE", "RimGetTuneName", -1,
	 (simulfunc_t)sim_RimGetTuneName, 0, 0},
	{"RIMOS.EXE", "RimCatastrophicFailure", -1,
	 (simulfunc_t)sim_RimCatastrophicFailure, 0, 0},

	{"RIMOS.EXE", "DbPointTable", -1,
	 (simulfunc_t)sim_DbPointTable, 0, 0},
	{"RIMOS.EXE", "DbAddRec", -1,
	 (simulfunc_t)sim_DbAddRec, 0, 0},
	{"RIMOS.EXE", "DbAndRec", -1,
	 (simulfunc_t)sim_DbAndRec, 0, 0},


	{"ribbon.dll", "RibbonRegisterApplication", 23,
	 (simulfunc_t)sim_ribbon_RibbonRegisterApplication, 0, 0},
	{"ribbon.dll", "RibbonShowRibbon", 26,
	 (simulfunc_t)sim_ribbon_RibbonShowRibbon, 0, 0},
	{"", "", -1, NULL, 0, 0}
};

static void dumpdll(rimdll_t *dll)
{

	printf("COFF Header:\n");

	printf("\tMachine: 0x%04x\n", dll->coff.Machine);
	printf("\tNumberOfSections: 0x%04x\n", dll->coff.NumberOfSections);
	printf("\tDateTimeStamp: 0x%08lx = %s", dll->coff.TimeDateStamp, ctime((time_t *)&dll->coff.TimeDateStamp));
	printf("\tPointerToSymbolTable: 0x%08lx\n", dll->coff.PointerToSymbolTable);
	printf("\tNumberOfSymbols: 0x%08lx\n", dll->coff.NumberOfSymbols);
	printf("\tSizeOfOptionalHeader: 0x%04x\n", dll->coff.SizeOfOptionalHeader);
	printf("\tCharacteristics: 0x%04x\n", dll->coff.Characteristics);

	if (dll->coff.SizeOfOptionalHeader) {

		printf("\tOptional header:\n");

		printf("\t\tMagic: 0x%04x\n", dll->coff.opthdr.Magic);
		printf("\t\tMajorLinkerVersion: 0x%02x\n", dll->coff.opthdr.MajorLinkerVersion);
		printf("\t\tMinorLinkerVersion: 0x%02x\n", dll->coff.opthdr.MinorLinkerVersion);
		printf("\t\tSizeOfCode: 0x%08lx\n", dll->coff.opthdr.SizeOfCode);
		printf("\t\tSizeOfInitializedData: 0x%08lx\n", dll->coff.opthdr.SizeOfInitializedData);
		printf("\t\tSizeOfUninitializedData: 0x%08lx\n", dll->coff.opthdr.SizeOfUninitializedData);
		printf("\t\tAddressOfEntryPoint: 0x%08lx\n", dll->coff.opthdr.AddressOfEntryPoint);
		printf("\t\tBaseOfCode: 0x%08lx\n", dll->coff.opthdr.BaseOfCode);
		printf("\t\tBaseOfData: 0x%08lx\n", dll->coff.opthdr.BaseOfData);

		printf("\t\tImageBase: 0x%08lx\n", dll->coff.opthdr.ImageBase);
		printf("\t\tSectionAlignment: 0x%08lx\n", dll->coff.opthdr.SectionAlignment);
		printf("\t\tFileAlignment: 0x%08lx\n", dll->coff.opthdr.FileAlignment);
		printf("\t\tMajorOperatingSystemVersion: 0x%04x\n", dll->coff.opthdr.MajorOperatingSystemVersion);
		printf("\t\tMinorOperatingSystemVersion: 0x%04x\n", dll->coff.opthdr.MinorOperatingSystemVersion);
		printf("\t\tMajorImageVersion: 0x%04x\n", dll->coff.opthdr.MajorImageVersion);
		printf("\t\tMinorImageVersion: 0x%04x\n", dll->coff.opthdr.MinorImageVersion);
		printf("\t\tMajorSubsystemVersion: 0x%04x\n", dll->coff.opthdr.MajorSubsystemVersion);
		printf("\t\tMinorSubsystemVersion: 0x%04x\n", dll->coff.opthdr.MinorSubsystemVersion);
		printf("\t\tReserved: 0x%08lx\n", dll->coff.opthdr.Reserved);
		printf("\t\tSizeOfImage: 0x%08lx\n", dll->coff.opthdr.SizeOfImage);
		printf("\t\tSizeOfHeaders: 0x%08lx\n", dll->coff.opthdr.SizeOfHeaders);
		printf("\t\tCheckSum: 0x%08lx\n", dll->coff.opthdr.CheckSum);
		printf("\t\tSubsystem: 0x%04x\n", dll->coff.opthdr.Subsystem);
		printf("\t\tDLL Characteristics: 0x%04x\n", dll->coff.opthdr.DLLCharacteristics);
		printf("\t\tSizeOfStackReserve: 0x%08lx\n", dll->coff.opthdr.SizeOfStackReserve);
		printf("\t\tSizeOfStackCommit: 0x%08lx\n", dll->coff.opthdr.SizeOfStackCommit);
		printf("\t\tSizeOfHeapReserve: 0x%08lx\n", dll->coff.opthdr.SizeOfHeapReserve);
		printf("\t\tSizeOfHeapCommit: 0x%08lx\n", dll->coff.opthdr.SizeOfHeapCommit);
		printf("\t\tLoaderFlags: 0x%08lx\n", dll->coff.opthdr.LoaderFlags);
		printf("\t\tNumberOfRvaAndSizes: 0x%08lx\n", dll->coff.opthdr.NumberOfRvaAndSizes);


		printf("\t\tData Directories:\n");

		printf("\t\t\tExport Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[0].rva, 
			   dll->coff.opthdr.directories[0].size);
		printf("\t\t\tImport Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[1].rva, 
			   dll->coff.opthdr.directories[1].size);
		printf("\t\t\tResource Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[2].rva, 
			   dll->coff.opthdr.directories[2].size);
		printf("\t\t\tException Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[3].rva, 
			   dll->coff.opthdr.directories[3].size);
		printf("\t\t\tCertificate Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[4].rva, 
			   dll->coff.opthdr.directories[4].size);
		printf("\t\t\tBase Reloc Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[5].rva, 
			   dll->coff.opthdr.directories[5].size);
		printf("\t\t\tDebug: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[6].rva, 
			   dll->coff.opthdr.directories[6].size);
		printf("\t\t\tArchitecture: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[7].rva, 
			   dll->coff.opthdr.directories[7].size);
		printf("\t\t\tGlobal Pointer: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[8].rva, 
			   dll->coff.opthdr.directories[8].size);
		printf("\t\t\tTLS Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[9].rva, 
			   dll->coff.opthdr.directories[9].size);
		printf("\t\t\tLoad Config Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[10].rva, 
			   dll->coff.opthdr.directories[10].size);
		printf("\t\t\tBound Import: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[11].rva, 
			   dll->coff.opthdr.directories[11].size);
		printf("\t\t\tImport Address Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[12].rva, 
			   dll->coff.opthdr.directories[12].size);
		printf("\t\t\tDelay Import Table: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[13].rva, 
			   dll->coff.opthdr.directories[13].size);
		printf("\t\t\tCOM+ Runtime Header: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[14].rva, 
			   dll->coff.opthdr.directories[14].size);
		printf("\t\t\tReserved: 0x%08lx [0x%08lx]\n", 
			   dll->coff.opthdr.directories[15].rva, 
			   dll->coff.opthdr.directories[15].size);
	}

	if (dll->coff.NumberOfSections) {
		int j;

		printf("\tSections: (%d)\n", dll->coff.NumberOfSections);

		for (j = 0; j < dll->coff.NumberOfSections; j++) {

			printf("\t\t%s:\n", dll->sections[j].Name);
			printf("\t\t\tVirtualSize: 0x%08lx\n", dll->sections[j].VirtualSize);
			printf("\t\t\tVirtualAddress: 0x%08lx\n", dll->sections[j].VirtualAddress);
			printf("\t\t\tSizeOfRawData: 0x%08lx\n", dll->sections[j].SizeOfRawData);
			printf("\t\t\tPointerToRawData: 0x%08lx\n", dll->sections[j].PointerToRawData);
			printf("\t\t\tPointerToRelocations: 0x%08lx\n", dll->sections[j].PointerToRelocations);
			printf("\t\t\tPointerToLineNumbers: 0x%08lx\n", dll->sections[j].PointerToLineNumbers);
			printf("\t\t\tNumberOfRelocations: 0x%04x\n", dll->sections[j].NumberOfRelocations);
			printf("\t\t\tNumberOfLineNumbers: 0x%04x\n", dll->sections[j].NumberOfLineNumbers);
			printf("\t\t\tCharacteristics: 0x%08lx\n", dll->sections[j].Characteristics);

		}
	}

	printf(".version Section:\n");
	printf("\tPointer: %p\n", dll->rimversion.thisRec);
	printf("\tSignature: %s\n", dll->rimversion.signature);
	printf("\tEntry point: %p\n", dll->rimversion.EntryPtr);
	printf("\tRequired OS Version: %ld.%ld\n", dll->rimversion.version >> 8, dll->rimversion.version & 0xff);
	printf("\tVersionPtr: %s (%p)\n", dll->rimversion.VersionPtr, dll->rimversion.VersionPtr);
	printf("\tAppStackSize: %d (%p)\n", *dll->rimversion.AppStackSize, dll->rimversion.AppStackSize);

	if (dll->rimvers) {
		struct RIMVersion *cur;

		printf("Exported API Versions:\n");
		for (cur = dll->rimvers; cur; cur = cur->next) {
			printf("\t%s v%d.%d.%d.%d, minimum v%d.%d\n",
				   cur->name,
				   cur->verMajor, cur->verMinor,
				   cur->verPatch, cur->verBuild,
				   cur->minCompatMajor, cur->minCompatMinor);
		}
	}

	if (dll->rimdeps) {
		struct RIMDependency *cur;

		printf("Declared Dependencies:\n");
		for (cur = dll->rimdeps; cur; cur = cur->next) {
			printf("\t%s v%d.%d\n",
				   cur->name,
				   cur->verReqMajor, cur->verReqMinor);
		}
	}

	if (dll->importdir) {
		struct import_directory *cur;

		printf("Imported Functions:\n");

		for (cur = dll->importdir; cur; cur = cur->next) {
			struct import_lookup_table *ilt;

			printf("\tFrom %s: (IAT at %p)\n", cur->name, cur->iat);

			for (ilt = cur->ilt; ilt; ilt = ilt->next) {
				if (ilt->ordinal_flag == 0x00)
					printf("\t\t%s/%d (thunk at %p, resolved to %p)\n", ilt->ihnt.name, ilt->ihnt.hint, ilt->thunk, ilt->resolved?ilt->resolved->func:0);
				else
					printf("\t\t%ld (thunk at %p, resolved to %p)\n", ilt->ordinalnum, ilt->thunk, ilt->resolved?ilt->resolved->func:0);
			}
		}
	}

	return;
}

static void dumpbox(unsigned long addr, unsigned char *buf, int len)
{
	char z[256];
	int i, y, x;

	for (i = 0, x = 0; i < len; ) {
		x = snprintf(z, sizeof(z), "0x%08lx:  ", addr+i);
		for (y = 0; y < 8; y++) {
			if (i < len) {
				x += snprintf(z+x, sizeof(z)-x, "%02x", *(buf+i));
				i++;
			} else
				break;
		}
		printf("%s\n", z);
		fflush(stdout);
	}

	return;
}


#define loadval8(x, y, z) { \
    (x) = *(unsigned char *)((y)+(z)); \
    (z)++;                  \
}

#define loadval16(x, y, z) { \
    (x) = *(unsigned short *)((y)+(z)); \
    (z) += 2;                \
}

#define loadval32(x, y, z) { \
    (x) = *(unsigned long *)((y)+(z));  \
    (z) += 4;                \
}

void freedll(rimdll_t *dll)
{
	struct RIMDependency *cur;
	struct RIMVersion *curv;
	struct import_directory *idt;

	if (!dll)
		return;

	if (dll->vma && (dll->vma != MAP_FAILED))
		munmap(dll->vma, dll->vmalen);

	for (cur = dll->rimdeps; cur; ) {
		struct RIMDependency *tmp;

		tmp = cur;
		cur = cur->next;

		free(tmp->name);
		free(tmp);
	}

	for (curv = dll->rimvers; curv; ) {
		struct RIMVersion *tmp;

		tmp = curv;
		curv = curv->next;

		free(tmp->name);
		free(tmp);
	}

	for (idt = dll->importdir; idt; ) {
		struct import_directory *tmp;
		struct import_lookup_table *ilt;

		tmp = idt;
		idt = idt->next;

		for (ilt = tmp->ilt; ilt; ) {
			struct import_lookup_table *ilttmp;

			ilttmp = ilt;
			ilt = ilt->next;

			free(ilttmp->ihnt.name);
			free(ilttmp);
		}

		free(tmp);
	}

	free(dll->sections);
	free(dll);

	return;
}

static dll_section_t *getdllsection(rimdll_t *dll, const char *name)
{
	int i;

	if (!dll)
		return NULL;

	for (i = 0; i < dll->coff.NumberOfSections; i++) {
		if (strcmp(dll->sections[i].Name, name) == 0)
			return dll->sections+i;
	}

	return NULL;
}

static void loaddll_processversions(rimdll_t *dll)
{
	dll_section_t *sec;
	int i = 0;

	if (!dll || !(sec = getdllsection(dll, ".version")))
		return;

	loadval32((unsigned long)dll->rimversion.thisRec, sec->data, i);
	memcpy(dll->rimversion.signature, sec->data+i, RIM_VERSEC_SIG_LEN);
	i += RIM_VERSEC_SIG_LEN;
	loadval32((unsigned long)dll->rimversion.EntryPtr, sec->data, i);
	loadval32((unsigned long)dll->rimversion.version, sec->data, i);
	loadval32((unsigned long)dll->rimversion.VersionPtr, sec->data, i);
	loadval32((unsigned long)dll->rimversion.AppStackSize, sec->data, i);

	fprintf(stderr, "processversions: %p, %s, %p, 0x%08lx, %p, %p\n",
			dll->rimversion.thisRec,
			dll->rimversion.signature,
			dll->rimversion.EntryPtr,
			dll->rimversion.version,
			dll->rimversion.VersionPtr,
			dll->rimversion.AppStackSize);

	return;
}

static void loaddll_processdeps(rimdll_t *dll)
{
	dll_section_t *sec;
	int i;

	if (!dll || !(sec = getdllsection(dll, ".rimdep")))
		return;

	for (i = 0; i < sec->datalen; ) {
		unsigned long start;
		unsigned char size, flags;
		int namelen;

		loadval32(start, sec->data, i);

		/*
		 * I'm not real sure how you're supposed to determine
		 * if its a valid record.  This works well.
		 *
		 * There is lots of zero-padding between records, but
		 * they always appear to be in 4 byte blocks.
		 *
		 */
		if (start == 0x00000000)
			continue;

		loadval8(size, sec->data, i);
		loadval8(flags, sec->data, i);

		if (flags == 0x00) { /* its a Version */
			struct RIMVersion *cur;

			if (!(cur = malloc(sizeof(struct RIMVersion))))
				break;
			memset(cur, 0, sizeof(struct RIMVersion));

			cur->thisRec = (struct RIMVersion *)start;
			cur->size = size;
			cur->flags = flags;
			loadval16(cur->verMajor, sec->data, i);
			loadval16(cur->verMinor, sec->data, i);
			loadval16(cur->verPatch, sec->data, i);
			loadval16(cur->verBuild, sec->data, i);
			loadval16(cur->minCompatMajor, sec->data, i);
			loadval16(cur->minCompatMinor, sec->data, i);

			namelen = cur->size-4-1-1-2-2-2-2-2-2;
			if (!(cur->name = malloc(namelen))) {
				free(cur);
				break;
			}
			strcpy(cur->name, sec->data+i);
			i += namelen;

			cur->next = dll->rimvers;
			dll->rimvers = cur;

			fprintf(stderr, "processdeps: version: %s v%d.%d.%d.%d, min %d.%d\n", 
					cur->name,
					cur->verMajor, cur->verMinor, cur->verPatch, cur->verBuild,
					cur->minCompatMajor, cur->minCompatMinor);

		} else if (flags == 0x01) { /* its a Dependency */
			struct RIMDependency *cur;

			if (!(cur = malloc(sizeof(struct RIMDependency))))
				break;
			memset(cur, 0, sizeof(struct RIMDependency));

			cur->thisRec = (struct RIMDependency *)start;
			cur->size = size;
			cur->flags = flags;
			loadval16(cur->verReqMajor, sec->data, i);
			loadval16(cur->verReqMinor, sec->data, i);

			namelen = cur->size-4-1-1-2-2;
			if (!(cur->name = malloc(namelen))) {
				free(cur);
				break;
			}
			strcpy(cur->name, sec->data+i);
			i += namelen;

			cur->next = dll->rimdeps;
			dll->rimdeps = cur;

			fprintf(stderr, "processdeps: dependency: %s v%d.%d\n", 
					cur->name, cur->verReqMajor, cur->verReqMinor);

		} else {
			fprintf(stderr, "processdeps: unknown record flag 0x%02x\n", flags);
			i += size-4-1-1;
		}
	}

	return;
}

static void appendilt(struct import_directory *dir, struct import_lookup_table *ilt)
{

	ilt->next = NULL;

	if (!dir->ilt)

		dir->ilt = ilt;

	else {
		struct import_lookup_table *cur;

		for (cur = dir->ilt; cur->next; cur = cur->next)
			;
		cur->next = ilt;
	}

	return;
}

/*
 * The .idata section contains the import tables.  The Import Table
 * Data Directory Entry in the COFF Optional Header should contain a 
 * pointer that directly matches the start of the .idata section.  This
 * code assumes thats true.
 * 
 * Note that RVA's in .idata do not get fixups from .reloc.  The
 * ImageBase address must get added to these manually.
 *
 */
static void loaddll_processidata(rimdll_t *dll)
{
	int i = 0;
	unsigned long iltrva = 0;
	unsigned char *idata;

	if (!dll || !dll->coff.opthdr.directories[1].rva)
		return;

	idata = dll->vma + dll->coff.opthdr.directories[1].rva;

	for ( ; ; ) {
		struct import_directory *cur;

		loadval32(iltrva, idata, i);
		if (iltrva == 0x00000000)
			break;

		if (!(cur = malloc(sizeof(struct import_directory))))
			break;
		memset(cur, 0, sizeof(struct import_directory));

		cur->iltrva = (struct import_lookup_table *)iltrva;
		loadval32((unsigned long)cur->timestamp, idata, i);
		loadval32(cur->forwarder_chain, idata, i);
		loadval32((unsigned long)cur->name, idata, i);
		loadval32((unsigned long)cur->iat, idata, i);

		cur->iltrva = (struct import_lookup_table *)((unsigned char *)cur->iltrva + (unsigned long)dll->vma);
		cur->name = (char *)((unsigned char *)cur->name + (unsigned long)dll->vma);
		cur->iat = (void *)((unsigned char *)cur->iat + (unsigned long)dll->vma);

		fprintf(stderr, "processidata: idt: %p, %ld, %ld, %s, %p\n",
				cur->iltrva, cur->timestamp, cur->forwarder_chain,
				cur->name, cur->iat);

		/*
		 * Load the name/ordinal of each function into the list.
		 *
		 * Note that the order of the list must be in the same
		 * order that the imports appear in the file, since that
		 * order is the only thing we have to go on when we build
		 * the thunking table.
		 *
		 */
		if (cur->iltrva) {
			unsigned long curilt;
			int j = 0;
			struct import_lookup_table *ilt;
			unsigned char *curthunk;

			curthunk = (unsigned char *)cur->iat;

			for ( ; ; ) {

				loadval32(curilt, (unsigned char *)cur->iltrva, j);
				if (curilt == 0x00000000)
					break;

				if (!(ilt = malloc(sizeof(struct import_lookup_table))))
					break;
				memset(ilt, 0, sizeof(struct import_lookup_table));

				ilt->ordinal_flag = (curilt >> 31) & 0x01;

				if (ilt->ordinal_flag == 0x00) { /* import by name */
					int k = 0;
					unsigned char *hintentry;

					hintentry = dll->vma + (unsigned long)(curilt & 0x7fffffff);

					loadval16(ilt->ihnt.hint, hintentry, k);
					ilt->ihnt.name = strdup(hintentry+k);
					fprintf(stderr, "processidata: idt: ilt: hint %d/%s\n", ilt->ihnt.hint, ilt->ihnt.name);

				} else if (ilt->ordinal_flag == 0x01) { /* import by ordinal */

					ilt->ordinalnum = curilt & 0x7fffffff;
					fprintf(stderr, "processidata: idt: ilt: ordinal %ld\n", ilt->ordinalnum);

				}

				ilt->thunk = (void *)curthunk;
				curthunk += 4;

				appendilt(cur, ilt);
			}
		}

		cur->next = dll->importdir;
		dll->importdir = cur;

	}

	return;
}

/*
 * The entry point listed in the .version section is probably
 * okay to use (and works so far for me).  It is not the address
 * of PagerMain, but the address of some routine that gets linked
 * in with OsEntry.obj.  I don't want to touch this stuff.  But
 * it seems to work, so I'll go with it.
 *
 */
static void loaddll_setpagermain(rimdll_t *dll)
{

	dll->PagerMain = (func_PagerMain)dll->rimversion.EntryPtr;

	return;
}

static rimdll_t *loaddll(unsigned char *buf)
{
	static const unsigned char pesig[PE_SIG_LEN] = {'P', 'E', '\0', '\0'};
	rimdll_t *dll;
	int i = 0;
	unsigned long pestart;

	if (!(dll = malloc(sizeof(rimdll_t))))
		return NULL;
	memset(dll, 0, sizeof(rimdll_t));

	/*
	 * All DLL's will start with that damned MS-DOS stub header. The
	 * offset of the start of the PE image is stored at 0x3c into the
	 * file (its a points to an offset within the file).
	 */
	pestart = *((unsigned long *)(buf+PE_OFF_STARTOFFSET));

	printf("start of PE image: 0x%08lx\n", pestart);

	i += pestart;

	/*
	 * Validate PE signature.
	 */
	memcpy(dll->Signature, buf+i, PE_SIG_LEN);
	if (memcmp(dll->Signature, pesig, PE_SIG_LEN) != 0) {
		fprintf(stderr, "loaddll: invalid PE signature\n");
		freedll(dll);
		return NULL;
	}
	i += PE_SIG_LEN;

	/*
	 * COFF-like header.
	 */
	loadval16(dll->coff.Machine, buf, i);
	loadval16(dll->coff.NumberOfSections, buf, i);
	loadval32(dll->coff.TimeDateStamp, buf, i);
	loadval32(dll->coff.PointerToSymbolTable, buf, i);
	loadval32(dll->coff.NumberOfSymbols, buf, i);
	loadval16(dll->coff.SizeOfOptionalHeader, buf, i);
	loadval16(dll->coff.Characteristics, buf, i);

	/*
	 * "Optional" Header (always present on RIM DLLs)
	 */
	if (dll->coff.SizeOfOptionalHeader) {
		int n;

		loadval16(dll->coff.opthdr.Magic, buf, i);
		if (dll->coff.opthdr.Magic != 0x010b) {
			fprintf(stderr, "loaddll: invalid Magic\n");
			freedll(dll);
			return NULL;
		}

		loadval8(dll->coff.opthdr.MajorLinkerVersion, buf, i);
		loadval8(dll->coff.opthdr.MinorLinkerVersion, buf, i);
		loadval32(dll->coff.opthdr.SizeOfCode, buf, i);
		loadval32(dll->coff.opthdr.SizeOfInitializedData, buf, i);
		loadval32(dll->coff.opthdr.SizeOfUninitializedData, buf, i);
		loadval32(dll->coff.opthdr.AddressOfEntryPoint, buf, i);
		loadval32(dll->coff.opthdr.BaseOfCode, buf, i);
		loadval32(dll->coff.opthdr.BaseOfData, buf, i);
		loadval32(dll->coff.opthdr.ImageBase, buf, i);
		loadval32(dll->coff.opthdr.SectionAlignment, buf, i);
		loadval32(dll->coff.opthdr.FileAlignment, buf, i);
		loadval16(dll->coff.opthdr.MajorOperatingSystemVersion, buf, i);
		loadval16(dll->coff.opthdr.MinorOperatingSystemVersion, buf, i);
		loadval16(dll->coff.opthdr.MajorImageVersion, buf, i);
		loadval16(dll->coff.opthdr.MinorImageVersion, buf, i);
		loadval16(dll->coff.opthdr.MajorSubsystemVersion, buf, i);
		loadval16(dll->coff.opthdr.MinorSubsystemVersion, buf, i);
		loadval32(dll->coff.opthdr.Reserved, buf, i);
		loadval32(dll->coff.opthdr.SizeOfImage, buf, i);
		loadval32(dll->coff.opthdr.SizeOfHeaders, buf, i);
		loadval32(dll->coff.opthdr.CheckSum, buf, i);
		loadval16(dll->coff.opthdr.Subsystem, buf, i);
		loadval16(dll->coff.opthdr.DLLCharacteristics, buf, i);
		loadval32(dll->coff.opthdr.SizeOfStackReserve, buf, i);
		loadval32(dll->coff.opthdr.SizeOfStackCommit, buf, i);
		loadval32(dll->coff.opthdr.SizeOfHeapReserve, buf, i);
		loadval32(dll->coff.opthdr.SizeOfHeapCommit, buf, i);
		loadval32(dll->coff.opthdr.LoaderFlags, buf, i);
		loadval32(dll->coff.opthdr.NumberOfRvaAndSizes, buf, i);

		if (dll->coff.opthdr.NumberOfRvaAndSizes != 16) {
			fprintf(stderr, "loaddll: invalid number of data directory entries\n");
			freedll(dll);
			return NULL;
		}

		for (n = 0; n < dll->coff.opthdr.NumberOfRvaAndSizes; n++) {
			loadval32(dll->coff.opthdr.directories[n].rva, buf, i);
			loadval32(dll->coff.opthdr.directories[n].size, buf, i);
		}
	}

	/*
	 * Before the sections are loaded, the memory
	 * range that will become the loaded process must
	 * be allocated.
	 *
	 * First attempt to get the region requested.  The
	 * default for DLLs is 0x10000000.  Which means that
	 * at most one DLL will get the range it wants, if
	 * that (since systems may or may not allow that 
	 * address to be mapped by applications).
	 *
	 * XXX For now, bail out when we can't get what we want.
	 * Thats because I'm really not in the mood to implement
	 * fixups right now.
	 *
	 */
	if ((dll->vma = mmap((void *)dll->coff.opthdr.ImageBase, dll->coff.opthdr.SizeOfImage, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
		fprintf(stderr, "loaddll: mmap failed: %s\n", strerror(errno));
		freedll(dll);
		return NULL;
	}
	dll->vmalen = dll->coff.opthdr.SizeOfImage;

	/*
	 * Finally come the section headers.
	 *
	 * As the headers are being processed, the data is also copied
	 * into the newly allocated VMA at their requested offsets.
	 *
	 */
	if (dll->coff.NumberOfSections) {
		int n;

		if (!(dll->sections = malloc(sizeof(dll_section_t)*dll->coff.NumberOfSections))) {
			freedll(dll);
			return NULL;
		}
		memset(dll->sections, 0, sizeof(dll_section_t)*dll->coff.NumberOfSections);

		for (n = 0; n < dll->coff.NumberOfSections; n++) {

			memcpy(dll->sections[n].Name, (char *)(buf+i), PE_SECNAME_LEN);
			i += PE_SECNAME_LEN;

			fprintf(stderr, "loaddll: loading section %s...\n", dll->sections[n].Name);

			loadval32(dll->sections[n].VirtualSize, buf, i);
			loadval32(dll->sections[n].VirtualAddress, buf, i);
			loadval32(dll->sections[n].SizeOfRawData, buf, i);
			loadval32(dll->sections[n].PointerToRawData, buf, i);
			loadval32(dll->sections[n].PointerToRelocations, buf, i);
			loadval32(dll->sections[n].PointerToLineNumbers, buf, i);
			loadval16(dll->sections[n].NumberOfRelocations, buf, i);
			loadval16(dll->sections[n].NumberOfLineNumbers, buf, i);
			loadval32(dll->sections[n].Characteristics, buf, i);

			dll->sections[n].data = dll->vma+dll->sections[n].VirtualAddress;
			dll->sections[n].datalen = dll->sections[n].VirtualSize;

			fprintf(stderr, "loaddll: going to put %d bytes of %s at %p from 0x%08lx\n", dll->sections[n].datalen, dll->sections[n].Name, dll->sections[n].data, dll->sections[n].PointerToRawData);

			/*
			 * The PE spec says it should be "zero filled".
			 *
			 * This doesn't make a lot of sense, but we'll do it anyway.
			 */
			memset(dll->sections[n].data, 0, dll->sections[n].datalen);

			/* Copy in the data... */
			memcpy(dll->sections[n].data, buf+dll->sections[n].PointerToRawData, dll->sections[n].datalen);

#if 0
			if (strcmp(dll->sections[n].Name, ".rdata") == 0)
				dumpbox(dll->sections[n].VirtualAddress, dll->sections[n].data, dll->sections[n].datalen);
#endif

		}
	}

	loaddll_processversions(dll);
	//loaddll_processdeps(dll);
	loaddll_processidata(dll);
	loaddll_setpagermain(dll);

	return dll;
}

static simulcall_t *dll_findsym(struct import_directory *dir, struct import_lookup_table *ilt)
{
	int i;

	for (i = 0; strlen(simulatedcalls[i].libname); i++) {

		if (strcmp(simulatedcalls[i].libname, dir->name) != 0)
			continue;

		if (ilt->ordinal_flag == 0x00) {
			if (strcmp(simulatedcalls[i].name, ilt->ihnt.name) == 0)
				return simulatedcalls+i;
		} else if (ilt->ordinal_flag == 0x01) {
			if (ilt->ordinalnum == simulatedcalls[i].ordinal)
				return simulatedcalls+i;
		}
	}

	return NULL;
}

/*
 * Run through the ILT's and match requested symbols with symbols
 * listed in simulatedcalls[].
 *
 * Returns the number of unresolved symbols.
 *
 */
static int dll_resolvesymbols(rimdll_t *dll)
{
	struct import_directory *dir;
	int symcount = 0, symresolved = 0, symunresolved = 0;

	for (dir = dll->importdir; dir; dir = dir->next) {
		struct import_lookup_table *cur;

		for (cur = dir->ilt; cur; cur = cur->next) {

			symcount++;

			if ((cur->resolved = dll_findsym(dir, cur)))
				symresolved++;
			else
				symunresolved++;
		}
	}

	fprintf(stderr, "dll_resolvesymbols: %d symbols, %d resolved, %d unresolved\n", symcount, symresolved, symunresolved);

	return symunresolved;
}

/*
 * Fill in the thunking fields with the address that have been
 * resolved by dll_resolvesymbols().
 */
static void dll_bindthunks(rimdll_t *dll)
{
	struct import_directory *dir;

	for (dir = dll->importdir; dir; dir = dir->next) {
		struct import_lookup_table *cur;

		for (cur = dir->ilt; cur; cur = cur->next) {
			if (cur->thunk && cur->resolved)
				*((unsigned long *)cur->thunk) = (unsigned long)cur->resolved->func;
		}
	}

	return;
}

int main(int argc, char **argv)
{
	int c;
	char *fn = NULL;
	unsigned char *fdata = NULL;
	struct stat st;
	int fd;
	rimdll_t *dll = NULL;
	int loadit = 0;

	while ((c = getopt(argc, argv, "h")) != EOF) {
		if (c == 'h') {
		showhelp:
			printf("hah.\n");
			return -1;
		}
	}

	if (optind >= argc)
		goto showhelp;
	fn = argv[optind];

	printf("opening %s...\n", fn);

	if (stat(fn, &st) == -1) {
		printf("could not stat file: %s\n", strerror(errno));
		return -1;
	}

	if (st.st_size < (PE_OFF_STARTOFFSET+4)) {
		printf("invalid PE image: not big enough to find start offset\n");
		return -1;
	}

	if ((fd = open(fn, O_RDONLY)) == -1) {
		printf("could not open file: %s\n", strerror(errno));
		return -1;
	}

	/*
	 * mmap() the entire file.  We won't actually be using this to
	 * execute code, nor will the executed code use it for anything.
	 * This mapping is for convience only and could be replaced with
	 * read()'s.
	 *
	 * That is not true for any other mmap()ing in this code, unless
	 * noted.
	 *
	 */
	if ((fdata = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		printf("mmap failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (!(dll = loaddll(fdata))) {
		printf("loaddll failed\n");
		goto out;
	}

	loadit = dll_resolvesymbols(dll);
	dll_bindthunks(dll);

	dumpdll(dll);

	printf("Loaded application for platform %s v%ld.%ld: %s, requires %d bytes stack, entry at %p\n", 
		   dll->rimversion.signature, 
		   dll->rimversion.version >> 8,
		   dll->rimversion.version & 0xff,
		   dll->rimversion.VersionPtr,
		   *(dll->rimversion.AppStackSize),
		   dll->rimversion.EntryPtr);

	if (loadit) {
		printf("found %d unresolved symbols, skipping execution\n", loadit);
		goto out;
	}

#if 1
	sleep(1);
	fprintf(stderr, "jumping to main entry point (%p)...\n", dll->PagerMain);

	/*
	 * XXX Of course, there'll have to be something better here, since
	 * PagerMain functions are designed to never terminate (yes, really).
	 *
	 * Indeed, there needs to be a whole collection of PagerMain's running
	 * for something more than a one-app simulation. 
	 *
	 * So I have a theory question for you. Is it possible to fully
	 * emulate a threaded cooperative "multitasking" OS without using threads
	 * yourself?  To me the answer is obviously yes, but perhaps I'm
	 * missing something.
	 *
	 * Is it worth the hassle of not just giving up and using threads?
	 * Probably not.  You'd have to keep a lot of state that would be
	 * implied if you were running threads yourself.
	 *
	 * In any case, implementing RimCreateThread, RimTerminateThread,
	 * RimTaskYield, etc, is a ways off yet.
	 *
	 */
	dll->PagerMain();
#endif

 out:
	freedll(dll);

	printf("closing %s\n", fn);

	munmap(fdata, st.st_size);
	close(fd);

	return 0;
}
