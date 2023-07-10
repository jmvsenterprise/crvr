# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

OUT=server$(OUTEXT)

all: server

$(OUT): server.$(OBJ) pool.$(OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	
.PHONY: all clean
