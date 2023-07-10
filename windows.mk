OUTEXT=.exe
OBJ=obj
CXXFLAGS=/Od /Zi /W3 /WX
CFLAGS=/Od /Zi /W3 /WX
LDFLAGS=
LDLIBS=
RM=del

.exe.c:
	cl.exe $(CFLAGS) $(LDFLAGS) $(LDLIBS) $< /Fo$@
