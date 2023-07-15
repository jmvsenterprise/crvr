# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=server$(OUTEXT)
OBJS=server.$(OBJ) pool.$(OBJ)

all: server

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	
.PHONY: all clean
