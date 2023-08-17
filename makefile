# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=crvr$(OUTEXT)
OBJS=crvr.$(OBJ) pool.$(OBJ) asl.$(OBJ) http.$(OBJ) utils.$(OBJ) socket_layer.$(OBJ)

all: crvr

pkg: crvr.tar.xz

crvr.tar.xz: crvr asl.html asl_done.html
	tar -cf crvr.tar crvr asl.html asl_done.html
	xz crvr.tar

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

analyze: crvr.c asl.c
	clang-tidy crvr.c asl.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	$(RM) crvr.tar.xz
	
.PHONY: all clean
