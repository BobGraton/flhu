# Dependencies
ifeq (, $(shell fltk-config --api-version | grep "^1.3"))
 $(error "FLTK 1.3.x is not installed on your system (install libfltk1.3-dev package)")
endif

ifeq (, $(shell ldconfig -p | grep libgps))
 $(error "libgps is not installed on your system (install libgps-dev package)")
endif

ifeq (, $(shell pkg-config --exists libmpdclient && echo YES))
 $(error "libmpdclient is not installed on your system (install libmpdclient-dev package)")
endif

CXX     := $(shell fltk-config --cxx)

LINK     := $(CXX)
TARGET   := media_ui
OBJS     := media_ui.o sunriset.o
SRCS     := media_ui.cpp sunriset.c sunriset.h ../simpleini/SimpleIni.h
#ARCH     := $(shell getconf LONG_BIT)
# Initial single file compilation command:
# CXXFLAGS  = -Wall $(DEBUG) -std=c++11 -pthread
# fltk-config --cxxflags --ldflags --use-images --compile media_ui.cpp


CXXFLAGS := $(shell fltk-config --use-images --use-forms --cxxflags --optim) $(shell pkg-config libgps --cflags) $(shell pkg-config libmpdclient --cflags) -I. -I../gpiod/gpiod -I../simpleini
CFLAGS   := $(shell fltk-config --use-images --use-forms --cflags --optim) $(shell pkg-config libgps --cflags) $(shell pkg-config libmpdclient --cflags) -I. -I../gpiod/gpiod -I../simpleini

LDFLAGS  := $(shell fltk-config --use-images --ldflags) $(shell pkg-config libgps --libs) $(shell pkg-config libmpdclient --libs) -pthread -lstdc++ -lm

#CXXFLAGS_32 := -D_32BITS_
#CXXFLAGS_64 := -D_64BITS_ -m64
#CXXFLAGS    += $(CXXFLAGS_$(ARCH))
#LDFLAGS_32  :=
#LDFLAGS_64  := -m64
#CFLAGS      += $(CFLAGS_$(ARCH))
#LDFLAGS     += $(LDFLAGS_$(ARCH))

.SUFFIXES: .o .cpp
%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)


.SUFFIXES: .o .c
%.o: %.c
	$(CXX) -c -o $@ $< $(CXXFLAGS)


all: $(TARGET) dostrip


dostrip:
	strip $(TARGET)


$(TARGET): $(SRCS) $(OBJS)
	$(LINK) -o $(TARGET) $(OBJS) $(LDFLAGS) 


debug: CXXFLAGS += -DDEBUG -ggdb
debug: CXXFLAGS := $(shell echo $(CXXFLAGS) | sed -e 's/\-O2/\-O0/g' -e 's/\"/\\\"/g')
debug: CFLAGS += -DEBUG -gdb
debug: CFLAGS := $(shell echo $(CFLAGS) | sed -e 's/\-O2/\-O0/g' -e 's/\"/\\\"/g')
debug: $(TARGET)


clean:
	rm -f $(OBJS) > /dev/null 2<&1
	rm -f $(TARGET) > /dev/null 2<&1


stop:
	$(shell killall -9 media_ui > /dev/null 2<&1)
#	# Gné ?
	$(shell export DISPLAY=:0.0)
#	# $(shell rm media_ui)


install: stop all
	mkdir -p  ~/.flhu/mpd/music
	mkdir -p  ~/.flhu/mpd/playlists
	mkdir -p  ~/.flhu/applelogo	
	mkdir -p  ~/.flhu/icons
	mkdir -p ~/.flhu/bat/new
	mkdir -p ~/.config/autostart	
	@#cp ./BluetoothMAC.txt ~/.flhu
	cp -r ./flhu/bat/* ~/.flhu/bat	
	cp -r ./flhu/applelogo/* ~/.flhu/applelogo
	cp -r ./flhu/icons/* ~/.flhu/icons	
	cp ./media_ui.desktop ~/.config/autostart
	cp ./media_ui.desktop ~/Desktop
	test ! -f /usr/share/fonts/truetype/chicago/Chicago.ttf && (sudo mkdir -p /usr/share/fonts/truetype/chicago; sudo cp ./fonts/Chicago.ttf /usr/share/fonts/truetype/chicago/Chicago.ttf) || /bin/true

start: stop all
	$(shell chmod oug+x media_ui)
	$(warning ------- media_ui ---------)
	$(shell ls -al media_ui)
	$(warning ------------------------)
	$(warning media_ui compiled)
	$(shell rm -f /tmp/media_ui.log)
	@#$(shell ./media_ui)
