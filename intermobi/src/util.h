/* -*- Mode: ab-c -*- */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <LCD_API.h> /* for LcdPutString in dprintf */

#define DPRINTF_PREFIX APPNAME ": "

extern LcdConfig lcdconfig;

char *index(const char *s, int c);
int strncmp(const char *s1, const char *s2, int n);
int strcmp(const char *s1, const char *s2);


/* All this is disgusting */
extern char dprintf_buffer[512];

#define dprintf(x, y...) do { \
    RimDebugPrintf(DPRINTF_PREFIX x, ##y); \
    RimSprintf(dprintf_buffer, sizeof(dprintf_buffer), x, ##y); \
	LcdPutStringXY(0, lcdconfig.height-8, dprintf_buffer, -1, 0); \
	LcdScroll(8); \
} while (0)

#define dinlineprintf(x, y...) do { \
    RimDebugPrintf(x, ##y); \
    RimSprintf(dprintf_buffer, sizeof(dprintf_buffer), x, ##y); \
	LcdPutStringXY(0, lcdconfig.height-8, dprintf_buffer, -1, 0); \
	LcdScroll(8); \
} while (0)

void dumpbox(const unsigned char *buf, int buflen);

#define ntohs(x) ( \
    (((x) & 0x00ff) << 8) | \
    (((x) & 0xff00) >> 8)   \
)

#define ntohl(x) ( \
    (((x) & 0x000000ff) << 24) | \
    (((x) & 0x0000ff00) <<  8) | \
    (((x) & 0x00ff0000) >>  8) | \
    (((x) & 0xff000000) >> 24)   \
)

#define htons(x) ntohs(x)
#define htonl(x) ntohl(x)


#endif /* __UTIL_H__ */
