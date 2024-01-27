.PHONY: all clean install uninstall

COMMON_INCLUDES=-I.

# Predefine all to be empty before calling config.mk because some .mk files add
# recipes for their platforms.
all:
	
# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=crvr$(OUTEXT)
OBJS=crvr.$(OBJ) asl/asl.$(OBJ) http.$(OBJ) utils.$(OBJ) socket_layer.$(OBJ) base_defs.$(OBJ)

# Redefine all to what we actually want now that we've included all .mk files.
all: $(OUT)

pkg: crvr.tar.xz

crvr.tar.xz: crvr asl.html asl_done.html
	tar -cf crvr.tar crvr asl.html asl_done.html
	xz crvr.tar

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

analyze: crvr.o asl.o
	clang-tidy crvr.c asl.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

clean:
	$(RM) $(OUT)
	$(RM) $(OBJS)
	$(RM) crvr.tar.xz

install: crvr
	mkdir -p /usr/local/bin
	cp crvr /usr/local/bin

uninstall:
	if [ -e /usr/local/bin/crvr ]; rm /usr/local/bin/crvr
	
.PHONY: all clean
