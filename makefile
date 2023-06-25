include config.mk

.PHONY: clean all

all: server

server: server.cpp

clean:
	rm -f server
