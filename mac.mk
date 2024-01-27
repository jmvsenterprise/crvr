OUTEXT=
OBJ=o

COMMON_FLAGS=$(COMMON_INCLUDES) -DMAC=1 -Werror -Wextra -Wall -Wconversion
#SANITIZERS=-fsanitize=address -fsanitize=undefined
DEBUG_FLAGS=-g -O0 $(COMMON_FLAGS) $(SANITIZERS)
RELEASE_FLAGS=-Os $(COMMON_FLAGS)

BUILD=$(DEBUG_FLAGS)

CFLAGS=$(BUILD) -std=c17 -Ibase

LDFLAGS=$(SANITIZERS)
LDLIBS=-lm
RM=rm -f

scan:
	scan-build make
