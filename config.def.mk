# Comment out the sections you don't need.

# Unix
OUTEXT=
CXXFLAGS=-Werror -Wextra -Wall -Wconversion -DUNIX=1 -g -O0 -std=c++17
LDFLAGS=
LDLIBS=
RM=rm -f

# Windows
include windows.mk
OUTEXT=.exe
CXXFLAGS=/Od /Zi /W3 /WX
LDFLAGS=
LDLIBS=
RM=del
