PHOTHOME=../dapl_overmx
CC=gcc
LD=ld

UTILOBJS=htable.o buffertable.o
LDFLAGS=-shared
INCLUDES=-I. -I/opt/dapl-mx/include -I$(PHOTHOME) 

CFLAGS=-Wall -fPIC
ifdef DEBUG
CFLAGS+=-g
CFLAGS+=-DDEBUG
CFLAGS+=-DCALLTRACE
else
CFLAGS+=-O3
endif

all: $(UTILOBJS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f $(UTILOBJS)
