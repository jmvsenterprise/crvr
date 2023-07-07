# Comment out the sections you don't need.

# Unix
OUTEXT=
OBJ=o
COMPILE_OPTS=-g -O0 -DUNIX=1 -Werror -Wextra -Wall -Wconversion
CXXFLAGS=$(COMPILE_OPTS) -std=c++17
CFLAGS=$(COMPILE_OPTS) -std=c11
LDFLAGS=
LDLIBS=
RM=rm -f

# Windows
include windows.mk
OUTEXT=.exe
OBJ=obj
CXXFLAGS=/Od /Zi /W3 /WX
CFLAGS=/Od /Zi /W3 /WX
LDFLAGS=
LDLIBS=
RM=del
