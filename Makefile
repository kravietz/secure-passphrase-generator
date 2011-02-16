CC=gcc
LD=gcc
CFLAGS=-O2
LDFLAGS=-O2 -s -mwindows
#CFLAGS=-ggdb3
#LDFLAGS=-ggdb3 -mwindows
LIBS=-lshlwapi -lgdi32 -lcomctl32

spg.exe:	polish.o english.o spg.o
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		
spg.o:	spg.c english.h polish.h

english.o:	english.c

polish.o:	english.c

clean:
	-rm -f *.o *.exe
	-del *.o *.exe
