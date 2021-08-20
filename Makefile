APP:=rtspserver
APP_INSTALL_DIR?=/usr/local/bin

SRCS:= $(wildcard *.c)
INCS:= $(wildcard *.h)

PKGS:= gstreamer-1.0 gstreamer-video-1.0 gstreamer-rtsp-server-1.0 json-glib-1.0

OBJS:= $(SRCS:.c=.o)

CFLAGS+= `pkg-config --cflags $(PKGS)`
LIBS+= `pkg-config --libs $(PKGS)`

all: $(APP)

%.o: %.c $(INCS) Makefile
	$(CC) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CC) -o $(APP) $(OBJS) $(LIBS)

install: $(APP)
	mkdir -p $(APP_INSTALL_DIR)
	cp -r $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(OBJS)
