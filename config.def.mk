# Comment out the sections you don't need.

# Unix
OUTEXT=
CFLAGS=-Werror -Wextra -Wall -Wconversion -DUNIX=1 -g -O0
LDFLAGS=
LDLIBS=
RM=rm -f

# Windows
include windows.mk
OUTEXT=.exe
CFLAGS=/Od /Zi /W3 /WX
LDFLAGS=
LDLIBS=
RM=del
