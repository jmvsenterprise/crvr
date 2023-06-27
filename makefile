# config.mk doesn't exist by default. Copy config.def.mk, and mod it to work
# for your system.
include config.mk

OUT=server$(OUTEXT)

all: server

$(OUT): server.cpp

clean:
	$(RM) $(OUT)
	
.PHONY: all clean
