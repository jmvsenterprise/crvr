# config.mk doesn't exist by default. Copy config.def.mk, and mod it to work
# for your system.
include config.mk

.PHONY: all clean

all: server

server: server.cpp

clean:
	$(RM) server