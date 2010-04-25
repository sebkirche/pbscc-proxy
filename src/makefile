!include <win32.mak>

PRJNAME=pbscc

LIB_FILES= KERNEL32.lib USER32.lib SHELL32.LIB ADVAPI32.LIB wininet.lib


CPP_FILES= pbscc.cpp todo.cpp easystr.cpp entries.cpp filecmp.cpp conproc.cpp login.cpp

OBJS= $(CPP_FILES:.cpp=.obj)

all: x_res x_exe


.cpp.obj:
	$(cc) -Ox $(cflags) $(cvarsdll) -DSTRICT -YX -DNODEBUG $<

x_res: 
	rc $(rcvars) $(rcflags) -r -fo $(PRJNAME).res $(PRJNAME).rc

x_exe: $(OBJS)
	$(link) /dll /RELEASE -out:$(PRJNAME).dll $(OBJS) $(PRJNAME).res $(LIB_FILES) /DEF:$(PRJNAME).def

clean:
	-del *.obj
	-del *.pch
	-del *.exp
	-del *.lib
	-del *.res
