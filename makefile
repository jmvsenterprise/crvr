# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=crvr$(OUTEXT)
OBJS=crvr.$(OBJ) pool.$(OBJ) asl.$(OBJ)

all: crvr

pkg: crvr.tar.xz

server.tar.xz: server asl.html asl_done.html
	tar -cf server.tar server asl.html asl_done.html
	xz server.tar

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

analyze: crvr.c asl.c
	clang-tidy crvr.c asl.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	$(RM) crvr.tar.xz
	
.PHONY: all clean
