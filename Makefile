# Cross compile for Windows: make WIN=y
# If you're looking for a cross compiler for Windows designed to
# run on any Unix sytem (Linux, Mac OS X, *BSD...): http://mxe.cc

PRGNAME=bsa
INSTALLPATH=/usr/local/bin




ifeq ($(WIN),y)
   CROSS=i686-w64-mingw32.static-
   CC=gcc
   EXE=$(PRGNAME).exe
else
   ARCH:=$(shell ./test-arch.sh)
   EXE=$(PRGNAME)
endif


$(EXE):	$(PRGNAME).c
	$(CROSS)$(CC) -std=c99 -Wall $(ARCH) $< -o $@

clean:
	rm -f $(EXE)

install:
	install $(EXE) $(INSTALLPATH)
