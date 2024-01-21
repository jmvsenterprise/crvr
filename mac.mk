OUTEXT=
OBJ=o

COMMON_FLAGS=-DMAC=1 -Werror -Wextra -Wall -Wconversion
#SANITIZERS=-fsanitize=address -fsanitize=undefined
DEBUG_FLAGS=-g -O0 $(COMMON_FLAGS) $(SANITIZERS)
RELEASE_FLAGS=-Os $(COMMON_FLAGS)

BUILD=$(DEBUG_FLAGS)

CXXFLAGS=$(BUILD) -std=c++17
CFLAGS=$(BUILD) -std=c17 -Ibase

LDFLAGS=$(SANITIZERS)
LDLIBS=-lm
RM=rm -f
