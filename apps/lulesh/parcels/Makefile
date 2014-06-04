CC = mpicc
CFLAGS = -g
INCLUDES = -I/home/manderson/hpx_3jun2014/hpx5/include
LIBS = -L/home/manderson/hpx_3jun2014/hpx5/lib -lhpx

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

