#PROFILING = -DPROFILING #-DCOMM_DISABLED
#PROFILING_LIBS = -L../../../ipm/lib
#PROFILING_LIB = -lipm

FLAGS = -O0 -g -DWITH_XSP $(PROFILING)
LIBS = $(PROFILING_LIBS)

PHOTON_INC = -I/home/kissel/xspd/include -I/home/kissel/Photon/include
PHOTON_LIBS = -L/home/kissel/xspd/lib -lxsp_client -L/home/kissel/Photon/lib/ -lphoton -lpthread 

MPICC = mpicc
RUN = srun --exclusive

all: mpiiox photonx localx dummyx

mpiiox: diffusion2d.c mpiio.c
	$(MPICC) -c $(FLAGS) mpiio.c
	$(MPICC) -o mpiiox $(FLAGS) $(LIBS) diffusion2d.c mpiio.o

photonx: diffusion2d.c photon.c $(PHOTON_LIB)
	$(MPICC) -c $(FLAGS) $(PHOTON_INC) photon.c
	$(MPICC) -o photonx $(FLAGS) $(LIBS) $(PHOTON_INC) $(PHOTON_LIBS) diffusion2d.c photon.o

localx: diffusion2d.c local.c
	$(MPICC) -c $(FLAGS) local.c
	$(MPICC) -o localx $(FLAGS) $(LIBS) diffusion2d.c local.o
	
dummyx: diffusion2d.c dummy.c
	$(MPICC) -c $(FLAGS) dummy.c
	$(MPICC) -o dummyx $(FLAGS) $(LIBS) diffusion2d.c dummy.o

clean:
	rm -f *x *.o *~ out.*
