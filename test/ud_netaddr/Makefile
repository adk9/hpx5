CC = gcc
BASIC_FILES = 
BASIC_HEADERS = 

PROGS=mckey ud_blast ud_pingpong rc_pingpong

all: $(PROGS)
all: BASIC_FILES += pingpong.c
all: LOADLIBS += -libverbs -lrdmacm

${PROGS}: %: %.c ${BASIC_FILES} ${BASIC_HEADERS}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< ${BASIC_FILES} $(LOADLIBS) $(XSP_LIBS) $(LDLIBS) -o $@

clean:
	$(foreach fname,${PROGS}, rm -f ${fname})
.DELETE_ON_ERROR:
.PHONY: all clean
