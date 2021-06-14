################################################################################
#
# Copyright (c) 2020-2021, Loopinno. All rights reserved.
#
################################################################################

APP:=rtspd
APP_INSTALL_DIR?=/usr/local/bin

SRCS:= $(wildcard *.c)
INCS:= $(wildcard *.h)

PKGS:= gstreamer-1.0 gstreamer-video-1.0 gstreamer-rtsp-server-1.0

OBJS:= $(SRCS:.c=.o)

CFLAGS+= `pkg-config --cflags $(PKGS)`
LIBS+= `pkg-config --libs $(PKGS)`

all: $(APP)

%.o: %.c $(INCS) Makefile
	$(CC) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CC) -o $(APP) $(OBJS) $(LIBS)

install: $(APP)
	cp -rv $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(OBJS)