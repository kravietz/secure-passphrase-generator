CC=gcc
LD=gcc
CFLAGS=
LDFLAGS=
LIBS=-lshlwapi -lgdi32

spg.exe:	polish.o english.o spg.o
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		
spg.o:	spg.c english.h polish.h

english.o:	english.c

polish.o:	english.c

clean:
	@rm -f *.o *.exe
	@del *.o *.exe