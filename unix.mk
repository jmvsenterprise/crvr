OUTEXT=
OBJ=o
#SANITIZERS=-fsanitize=address -fsanitize=undefined
COMPILE_OPTS=-g -O0 -DUNIX=1 -Werror -Wextra -Wall -Wconversion $(SANITIZERS)
CXXFLAGS=$(COMPILE_OPTS) -std=c++17
CFLAGS=$(COMPILE_OPTS) -std=c17
LDFLAGS=$(SANITIZERS)
LDLIBS=
RM=rm -f
