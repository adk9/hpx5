CC = mpicc
CFLAGS = -g
INCLUDES = -I/home/manderson/hpx5_gitlab/include
LIBS = -L/home/manderson/hpx5_gitlab/lib -lhpx -lrt

SRC = $(shell ls *.c)
OBJ = $(SRC:.c=.o)
TARGET = test

.PHONY: clean help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS) 

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(TARGET) *~
help:
	@echo "Usage: ./$(TARGET) [nCores] [nThreads] [iterations]"

