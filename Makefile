OS = LINUX
#OS = MACOSX
#OS = WINDOWS

PROG = firmata_test

ifeq ($(OS), LINUX)
TARGET = $(PROG)
FINAL_TARGET = $(TARGET)
CXX = g++
STRIP = strip
WXCONFIG = ~/wxwidgets/2.8.10.gtk2.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall -Wno-strict-aliasing `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs`
else ifeq ($(OS), MACOSX)
TARGET = $(PROG)
FINAL_TARGET = $(PROG).dmg
SDK = /Developer/SDKs/MacOSX10.5.sdk
CXX = g++
STRIP = strip
WXCONFIG = ~/wxwidgets/2.8.10.mac.teensy/bin/wx-config
CPPFLAGS =  -O2 -Wall -Wno-strict-aliasing -isysroot $(SDK) `$(WXCONFIG) --cppflags` -D$(OS) -arch ppc -arch i386
LIBS = -Xlinker -syslibroot -Xlinker $(SDK) `$(WXCONFIG) --libs`
else ifeq ($(OS), WINDOWS)
TARGET = $(PROG).exe
FINAL_TARGET = $(TARGET)
CXX = i586-mingw32msvc-g++
STRIP = i586-mingw32msvc-strip
WINDRES = i586-mingw32msvc-windres
KEY_SPC = ~/bin/cert/mykey.spc
KEY_PVK = ~/bin/cert/mykey.pvk
KEY_TS = http://timestamp.comodoca.com/authenticode
WXCONFIG = ~/wxwidgets/2.8.10.mingw.teensy/bin/wx-config
CPPFLAGS =  -O2 -Wall -Wno-strict-aliasing `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs`
endif

MAKEFLAGS = --jobs=4

OBJS = firmata_test.o serial.o

all: $(FINAL_TARGET)

$(PROG): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LIBS)

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

