# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=server$(OUTEXT)
OBJS=server.$(OBJ) pool.$(OBJ)

all: server

pkg: server.tar.xz

server.tar.xz: server asl.html asl_done.html
	tar -cf server.tar server asl.html asl_done.html
	xz server.tar

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

analyze: server.c
	clang-tidy server.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	$(RM) server.tar.xz
	
.PHONY: all clean
