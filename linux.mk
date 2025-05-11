OUTEXT=
OBJ=o
SO=so

COMMON_FLAGS=-DLINUX=1 -Werror -Wextra -Wall -Wconversion -mshstk -fanalyzer
DYLIB_FLAGS := -I.
#SANITIZERS=-fsanitize=address -fsanitize=undefined
DEBUG_FLAGS=-g -O0 $(COMMON_FLAGS) $(SANITIZERS)
RELEASE_FLAGS=-Os $(COMMON_FLAGS)

BUILD=$(DEBUG_FLAGS)

CXXFLAGS=$(BUILD) -std=c++17 -fPIC
CFLAGS=$(BUILD) -std=c17 -Ibase -fPIC

LDFLAGS=$(SANITIZERS)
LDLIBS=-lcrvr -lm
RM=rm -f
