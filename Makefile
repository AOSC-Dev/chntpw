#
# Makefile for the Offline NT Password Editor
#
#
# Change here to point to the needed OpenSSL libraries & .h files
# See INSTALL for more info.
#

.PHONY: clean all install

DESTDIR=
PREFIX=/usr/local

OSSLPATH=/usr
OSSLINC=$(OSSLPATH)/include
CC=gcc
AR=gcc-ar

# Force m32?
FORCE32=0
CFLAGS_FORCE32_0=
CFLAGS_FORCE32_1=-m32

# Force lib64?
FORCE64=0
OSSLLIB_FORCE64_0=
OSSLLIB_FORCE64_1=64

# Do crypto/enable setting password/use OSSL?
DOCRYPTO=0
CFLAGS_DOCRYPTO_0=
CFLAGS_DOCRYPTO_1=-DDOCRYPTO
LIBS_DOCRYPTO_0=
# for forced static: $(OSSLLIB)/libcrypto.a
# gcc/ld normally can search for it
LIBS_DOCRYPTO_1=-lcrypto

# Make it static? (Consider using a small libc)
STATIC=0
LDFLAGS_STATIC_0=
LDFLAGS_STATIC_1=-static

CFLAGS= -fpic -g -O2 -I. -I$(OSSLINC) -Wall -Wextra $(CFLAGS_FORCE32_$(FORCE32)) $(CFLAGS_DOCRYPTO_$(DOCRYPTO))
OSSLLIB=$(OSSLPATH)/lib$(OSSLLIB_FORCE64_$(FORCE64))

# This is to link with whatever we have
LIBS= -L$(OSSLLIB) $(LDFLAGS_STATIC_$(STATIC)) $(LIBS_DOCRYPTO_$(DOCRYPTO))
LDFLAGS= -O2 -Wl,-O1,--sort-common,--as-needed

all: chntpw cpnt reged samusrgrp sampasswd samunlock libsam.so

chntpw: chntpw.o ntreg.o edlib.o libsam.o
	$(CC) $(LDFLAGS) -o chntpw chntpw.o ntreg.o edlib.o libsam.o $(LIBS)

cpnt: cpnt.o
	$(CC) $(LDFLAGS) -o cpnt cpnt.o

reged: reged.o ntreg.o edlib.o
	$(CC) $(LDFLAGS) -o reged reged.o ntreg.o edlib.o

samusrgrp: samusrgrp.o ntreg.o libsam.o
	$(CC) $(LDFLAGS) -o samusrgrp samusrgrp.o ntreg.o libsam.o

sampasswd: sampasswd.o ntreg.o libsam.o
	$(CC) $(LDFLAGS) -o sampasswd sampasswd.o ntreg.o libsam.o

samunlock: samunlock.o ntreg.o libsam.o
	$(CC) $(LDFLAGS) -o samunlock samunlock.o ntreg.o libsam.o

libsam.so: libsam.o ntreg.o dummy_gverbose.o
	$(CC) $(LDFLAGS) -shared -o libsam.so $^

libsam.a: libsam.o ntreg.o dummy_gverbose.o
	$(AR) rcs libsam.so $^

#ts: ts.o ntreg.o
#	$(CC) $(LDFLAGS) -nostdlib -o ts ts.o ntreg.o $(LIBS)

# -Wl,-t

.c.o:
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f *.o chntpw cpnt reged samusrgrp sampasswd samunlock libsam.so *~

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp chntpw cpnt reged samusrgrp sampasswd samunlock $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp libsam.so $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp sam.h ntreg.h $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(PREFIX)/doc/chntpw
	cp *.txt $(DESTDIR)$(PREFIX)/doc/chntpw

print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true
