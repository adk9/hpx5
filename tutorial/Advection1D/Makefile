CC = gcc
LIBS = $(shell pkg-config --libs hpx) -lm 
CFLAGS = $(shell pkg-config --cflags hpx) -std=gnu99 -g
TARGETS = advection4 advection3 advection2 advection1 advection0

all:$(TARGETS)

$(TARGETS): %: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf $(TARGETS) *~ *.txt

