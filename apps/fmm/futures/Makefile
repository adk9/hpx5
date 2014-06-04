CC = icc
CFLAGS = -std=gnu99 -fast
INCLUDES = -I/u/zhang416/hpx/build/include
LIBS = -L/u/zhang416/hpx/build/lib/ -lhpx -lm

SRC = fmm-test.c fmm-graph.c fmm-laplace.c
OBJ = fmm-test.o fmm-graph.o fmm-laplace.o
PROG = fmm

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) -o $(PROG) $(OBJ) $(LIBS)

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(PROG) *~

