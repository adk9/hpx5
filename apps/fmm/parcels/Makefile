CC     = mpicc
CFLAGS = -std=gnu99 -Wall -pedantic
LIBS   = /u/zhang416/installed/hpx/lib/libhpx.a -lpthread -lrt -lm 

SRC    = $(shell ls *.c)
OBJ    = $(SRC:.c=.o)
TARGET = fmm 

all: normal

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *~ *o

debug: CFLAGS := $(CFLAGS) -O0 -g
debug: $(TARGET)

fast: CFLAGS := $(CFLAGS) -O3 -g
fast: $(TARGET)

normal: CFLAGS := $(CFLAGS) -O2
normal: $(TARGET)