CC = mpicc
CPPFLAGS = -I../common
CFLAGS = -g
LIBS = -lhpx

VPATH = ../common
SRC = common.c $(shell ls *.c)
OBJ = $(SRC:.c=.o)

.PHONY: all clean help

all: $(TARGET)

clean:
	$(RM) $(OBJ) $(TARGET) *~

help:
	@echo "Usage: ./$(TARGET) [nDoms] [maxcycles] [nCores]"

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS) 

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

