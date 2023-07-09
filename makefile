# config.mk doesn't exist by default. Copy config.def.mk, and mod it to work
# for your system.
include config.mk

OUT=server$(OUTEXT)

all: server

$(OUT): server.$(OBJ) pool.$(OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	
.PHONY: all clean
