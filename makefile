.PHONY: all clean install uninstall test tools
	
# config.mk doesn't exist by default. Either copy unix.mk or windows.mk to
# config.mk or symlink it.
include config.mk

PREFIX ?= /usr/local/bin

LIBRARY := libcrvr.a

LDFLAGS := $(LDFLAGS) -L.
LDLIBS := $(LDLIBS) -lcrvr
OUT=crvr$(OUTEXT)
OBJS=\
	asl.$(OBJ) \
	http.$(OBJ) \
	utils.$(OBJ) \
	socket_layer.$(OBJ) \
	base_defs.$(OBJ) \
	plugins.$(OBJ)

PLUGINS:=\
	quizzer.$(SO)

CONVERTED_PAGES := \
	plugins/quizzer/quiz_page.h \
	plugins/quizzer/select_quiz_page.h \
	plugins/quizzer/startup_page.h

TOOLS := \
	tools/html2c \
	tools/html2cpp

CONV := tools/html2c

all: $(LIBRARY) $(OUT) $(TOOLS) $(PLUGINS)

pkg: crvr.tar.xz

crvr.tar.xz: crvr asl.html asl_done.html
	tar -cf crvr.tar crvr asl.html asl_done.html
	xz crvr.tar

$(LIBRARY): $(OBJS)
	$(AR) -crs $@ $(OBJS)

$(OUT): crvr.o $(LIBRARY)
	$(CC) $(CFLAGS) crvr.o -o $@ $(LDFLAGS) $(LDLIBS)

%.h: %.html
	tools/html2cpp $^ > $@

%.$(SO): plugins/%.c
	$(CC) $(CFLAGS) $(DYLIB_FLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

quizzer.$(SO): plugins/quizzer/quizzer.cpp $(CONVERTED_PAGES)
	$(CXX) $(CXXFLAGS) $(DYLIB_FLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

analyze: crvr.c asl.c
	clang-tidy crvr.c asl.c -checks=-*,cert-*,clang-analyzer-*,linuxkernel-*,performance-*,portability-*,readability-*

test: $(CONV) test.sh
	./test.sh

tests/asl_done.html: asl_done.html
	cp -f $^ $@

tests/asl.html: asl.html
	cp -f $^ $@

tests/index.html: index.html
	cp -f $^ $@

tests/image.png:
	scrot tests/image.png

tests:
	mkdir tests

clean:
	$(RM) $(OUT)
	$(RM) *.$(OBJ)
	$(RM) crvr.tar.xz
	$(RM) -f $(CONVERTED_PAGES)

install: crvr
	mkdir -p $(PREFIX)
	cp crvr $(PREFIX)
	cp $(PLUGINS) $(PREFIX)

uninstall:
	if [ -e /usr/local/bin/crvr ]; rm /usr/local/bin/crvr
	
.PHONY: all clean
