#ifndef __SIMRIMOS_H__
#define __SIMRIMOS_H__

/* core */
void sim_RimDebugPrintf(const char *Format, ...);

/* lcd */
int sim_LcdPutStringXY(int x, int y, const char *s, int length, int flags);
int sim_LcdCreateDisplayContext(int displayToCopy);
void sim_LcdIconsEnable(BOOL Enable);
int sim_LcdSetDisplayContext(int iDC);
int sim_LcdGetDisplayContext(void);
int sim_LcdGetCharacterWidth(char c, int fontIndex);
int sim_LcdGetCurrentFont(void);
int sim_LcdSetCurrentFont(int iFontIndex);
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
