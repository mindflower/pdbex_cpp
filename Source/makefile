#VSINSTALLDIR="L:\flash2G_my\syser\pdb\tmp"
#CFLAGS= /MT
CFLAGS= /EHsc
LFLAGS= /MANIFEST:NO
#DEBUG= NO

!if "$(DEBUG)" == ""
DEBUG = 1
!endif

!if "$(DEBUG)" == "1"
ODIR = debug
CFLAGS = $(CFLAGS) -Od -D_DEBUG -ZI 
D = d
!else
ODIR = release
CFLAGS = $(CFLAGS) -O1 -DNDEBUG -Zi
!endif


LIBS = \
    $(LIBS) \
    ole32.lib \
    oleaut32.lib


CFLAGS = $(CFLAGS) -MT$(D) -I"$(VSINSTALLDIR)\DIA SDK\include"

CFLAGS   = $(CFLAGS)   -nologo -c -Fd$(ODIR)\ -W3
LFLAGS   = $(LFLAGS)   -map -debug -PDB:$(ODIR)\pdbex_cpp.pdb "-libpath:$(VSINSTALLDIR)\DIA SDK\lib"


!if "$(VERBOSE)" == "1"
!message DEBUG=$(DEBUG)
!endif

OBJS = \
    $(ODIR)\main.obj    \
    $(ODIR)\PDB.obj        \
    $(ODIR)\PDBExtractor.obj \
    $(ODIR)\PDBHeaderReconstructor.obj


##### Inference Rules

all : $(ODIR)\pdbex_cpp.exe

#use as prefix for cl
#D:\LLVM-9.0.0-win32\bin\clang-
#D:\LLVM-9.0.0-win64\bin\clang-

{}.cpp{$(ODIR)\}.obj::
    cl $(CFLAGS) /std:c++20 $(MPBUILDFLAGS) $(PCHFLAGS) -Fo$(ODIR)\ -FR$(ODIR)\ $<

$(ODIR)\pdbex_cpp.exe : $(ODIR) $(PCHNAME) $(OBJS)
    link -out:$(ODIR)\pdbex_cpp.exe $(OBJS) $(LFLAGS) $(LIBS)

$(ODIR):
    -md $(ODIR)

clean :
    del /q $(ODIR)
