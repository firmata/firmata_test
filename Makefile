OS ?= LINUX
#OS ?= MACOSX
#OS ?= WINDOWS

PROG = firmata_test

ifeq ($(OS), LINUX)
WXCONFIG ?= ~/wxwidgets/2.8.10.gtk2.teensy/bin/wx-config
else ifeq ($(OS), MACOSX)
FINAL_TARGET = $(PROG).dmg
SDK = /Developer/SDKs/MacOSX10.5.sdk
CXX = g++
STRIP = strip
WXCONFIG ?= ~/wxwidgets/2.8.10.mac.teensy/bin/wx-config
PLATFORM_CPPFLAGS = -isysroot $(SDK) -arch ppc -arch i386
PLATFORM_LIBS = -Xlinker -syslibroot -Xlinker $(SDK)
else ifeq ($(OS), WINDOWS)
TARGET = $(PROG).exe
CROSS ?= i586-mingw32msvc-
WINDRES = $(CROSS)windres
KEY_SPC = ~/bin/cert/mykey.spc
KEY_PVK = ~/bin/cert/mykey.pvk
KEY_TS = http://timestamp.comodoca.com/authenticode
WXCONFIG ?= ~/wxwidgets/2.8.10.mingw.teensy/bin/wx-config
endif

WXCONFIG := $(shell if [ -x $(WXCONFIG) ]; then echo $(WXCONFIG); else echo `which wx-config`; fi)

TARGET ?= $(PROG)
FINAL_TARGET ?= $(TARGET)
CXX ?= $(CROSS)g++
STRIP ?= $(CROSS)strip
LIBS = $(PLATFORM_LIBS) `$(WXCONFIG) --libs`
CPPFLAGS = $(PLATFORM_CPPFLAGS) -O2 -Wall -Wno-strict-aliasing `$(WXCONFIG) --cppflags` -D$(OS)

MAKEFLAGS = --jobs=4

OBJS = firmata_test.o serial.o

all: $(FINAL_TARGET)

$(PROG): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

$(PROG).exe: $(PROG)
	cp $(PROG) $@ 
	-upx -q $@
	-signcode -spc $(KEY_SPC) -v $(KEY_PVK) -t $(KEY_TS) $@
	-cp_win32.sh $@

$(PROG).app: $(PROG) Info.plist
	mkdir -p $(PROG).app/Contents/MacOS
	mkdir -p $(PROG).app/Contents/Resources/English.lproj
	cp Info.plist $(PROG).app/Contents/Info.plist
	/bin/echo -n 'APPL????' > $(PROG).app/Contents/PkgInfo
	cp $(PROG) $(PROG).app/Contents/MacOS/
	touch $(PROG).app

$(PROG).dmg: $(PROG).app
	mkdir dmg_tmpdir
	cp -r $(PROG).app dmg_tmpdir
	hdiutil create -ov -srcfolder dmg_tmpdir -format UDBZ -volname "Firmata Test" $(PROG).dmg

clean:
	rm -f $(PROG) $(PROG).exe $(PROG).exe.bak $(PROG).dmg *.o
	rm -rf $(PROG).app dmg_tmpdir

