CC = mpicc
CFLAGS = -g
INCLUDES = -I../../../include
LIBS = -L$(HOME)/hpx5_rethread/lib -lhpx

SRC = $(shell ls *.c)
OBJ = $(SRC:.c=.o)
TARGET = luleshHPX

.PHONY: clean help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS) 

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(TARGET) *~
help:
	@echo "Usage: ./$(TARGET) [nDoms] [nx] [maxcycles] [nCores]"

