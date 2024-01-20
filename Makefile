.PHONY: all clean install uninstall
	
# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=crvr$(OUTEXT)
OBJS=crvr.$(OBJ) asl.$(OBJ) http.$(OBJ) utils.$(OBJ) socket_layer.$(OBJ) base_defs.$(OBJ)

all: $(OUT)

pkg: crvr.tar.xz

crvr.tar.xz: crvr asl.html asl_done.html
	tar -cf crvr.tar crvr asl.html asl_done.html
	xz crvr.tar

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

analyze: crvr.c asl.c
	clang-tidy crvr.c asl.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	$(RM) crvr.tar.xz

install: crvr
	mkdir -p /usr/local/bin
	cp crvr /usr/local/bin

uninstall:
	if [ -e /usr/local/bin/crvr ]; rm /usr/local/bin/crvr
	
.PHONY: all clean
