CC = mpicc
CFLAGS = -g -std=gnu99
INCLUDES = -I/home/manderson/hpx5_gitlab/include
LIBS = -L/home/manderson/hpx5_gitlab/lib -lhpx -lhpx_jemalloc -lphoton -lm

SRC = $(shell ls *.c)
OBJ = $(SRC:.c=.o)
TARGET = lulesh

.PHONY: clean help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS) 

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(TARGET) *~
help:
	@echo "Usage: ./$(TARGET) [options]"

