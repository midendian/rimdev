
CHAINPREFIX=i386-mingw32msvc
CC=$(CHAINPREFIX)-gcc
CXX=$(CHAINPREFIX)-g++
LD=$(CHAINPREFIX)-ld
DLLTOOL=$(CHAINPREFIX)-dlltool
STRIP=$(CHAINPREFIX)-strip
OBJDUMP=$(CHAINPREFIX)-objdump

SDKPREFIX=/home/mid/rimsdk/

CFLAGS=-Wall -g -ffreestanding -nostdinc -nostdinc++ -I$(SDKPREFIX)/include -I$(SDKPREFIX)/include/Internal

DLLNAME=intermobi
OBJECTS=intermobi.o
LIBDIR=$(SDKPREFIX)/lib
LIBS=$(LIBDIR)/OSEntry.obj $(LIBDIR)/rimos.lib
ENTRYPOINT=__DllMainCRTStartup@12
LDFLAGS=--subsystem windows --stack 0x00100000 --image-base 0x10000000 --section-alignment 0x00001000 --file-alignment 0x00001000 --major-image-version 0

all: $(DLLNAME).dll

$(DLLNAME).dll: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(DLLNAME).dll.tmp $(OBJECTS) $(LIBS) --dll -e $(ENTRYPOINT) --base-file $(DLLNAME).base
	$(DLLTOOL) -D $(DLLNAME).dll --def $(DLLNAME).def --output-lib $(DLLNAME).lib --output-exp $(DLLNAME).exp --base-file $(DLLNAME).base
	$(LD) $(LDFLAGS) -o $(DLLNAME).dll $(OBJECTS) $(DLLNAME).exp $(LIBS) --dll -e $(ENTRYPOINT)

strip: $(DLLNAME).dll
	$(STRIP) $(DLLNAME).dll

tmpclean:
	rm -rf $(DLLNAME).dll.tmp $(DLLNAME).base $(DLLNAME).exp
clean: tmpclean
	rm -rf *~ core $(OBJECTS) $(DLLNAME).dll $(DLLNAME).lib
odx:
	$(OBJDUMP) -x $(DLLNAME).dll |less



